// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "CTPreLauncher.h"
#include "CTPrivilegedProcessManager.h"
#include "CTPrivilegedProcessManager_open.h"
#include "Elf.h"
#include "ElfFile.h"
#include "ErrorMap.h"
#include "ITEnv.h"
#include "ITPreLauncher.h"
#include "ITPreLauncher_invoke.h"
#include "ITPrivilegedProcessManager.h"
#include "MSMem.h"
#include "MemSCpy.h"
#include "MetadataInfo.h"
#include "MinkTypes.h"
#include "TEnv.h"
#include "TUtils.h"

#include "cdefs.h"
#include "fdwrapper.h"
#include "heap.h"
#include "libcontainer.h"
#include "minkipc.h"
#include "object.h"

#ifdef ENABLE_AUTH
#include "CSecureImage.h"
#include "IClientEnv.h"
#include "ISecureImage.h"
#include "TZCom.h"

#define QTVM_DOMAIN_UNK_STR "unk"
#define QTVM_DOMAIN_OEM_STR "oem"
#define QTVM_DOMAIN_ALT_STR "alt"
#define QTVM_DOMAIN_QTI_STR "qti"

#ifdef ENABLE_ARB
#include "CKVStore.h"
#include "IKVStore.h"
#endif  // ENABLE_ARB

#else  // ENABLE_AUTH
#define QTVM_ROT_NOTSIGNED UINT32_C(0x10000000)
// 'nos' means 'no signature'
#define QTVM_DOMAIN_NOTSIGNED_STR "nos"
#endif  // ENABLE_AUTH

typedef struct {
    int32_t refs;
    Object credentials;
} TPreLauncher;

typedef struct {
    ElfSource elfSource;
    void *ptr;
    size_t size;
} ProcSource;

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;

Object gRootObj = Object_NULL;

/**
 * Error conversion from ITPPM to ITPreLauncher
 * */
/* clang-format off */
static const ErrorPair gITPPMToITPreLauncherErrors[] = {
    {ITPPM_ERROR_INVALID_BUFFER, ITPreLauncher_ERROR_INVALID_BUFFER},
    {ITPPM_ERROR_HEAP_MALLOC_FAILURE, ITPreLauncher_ERROR_PROC_NOT_LOADED},
    {ITPPM_ERROR_PROC_ALREADY_LOADED, ITPreLauncher_ERROR_PROC_ALREADY_LOADED},
    {ITPPM_ERROR_PROC_NOT_LOADED, ITPreLauncher_ERROR_PROC_NOT_LOADED},
};
/* clang-format on */

static const ErrorMap gITPPMToITPreLauncherErrorMap = {
    .errors = gITPPMToITPreLauncherErrors,
    .length = C_LENGTHOF(gITPPMToITPreLauncherErrors),
    .genericError = Object_ERROR,
    .startConversionAt = Object_ERROR_USERBASE};

static int32_t _iTPPMToITPreLauncherError(int32_t error)
{
    return ErrorMap_convert(&gITPPMToITPreLauncherErrorMap, error);
}

/////////////////////////////////////////////
//        ELF parsing functions          ////
/////////////////////////////////////////////
/**
 * Description: Function to free memory from Elf file
 *
 * In:          ptr: Pointer to heap data
 */
static void ProcSource_freeData(ElfSource *me, const void *ptr)
{
    (void)me;
    void *x;
    if (ptr) {
        x = c_const_cast(void *, ptr);
        heap_free(x);
    }
}

/**
 * Description: Function to copy memory from Elf file at a specific offset
 *
 * In:          cxt:
 *              offset:
 *              size:
 *
 * Out:         meOut: The service object.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t ProcSource_getData(void **meOut, ElfSource *cxt, size_t offset, size_t size)
{
    int32_t ret = Object_ERROR;
    ProcSource *me = (ProcSource *)cxt;
    void *ptr = heap_zalloc(size);

    T_CHECK_ERR(ptr, Object_ERROR_MEM);
    T_CHECK(offset <= me->size && size <= me->size - offset);

    memscpy(ptr, size, (char *)me->ptr + offset, size);
    *meOut = ptr;
    return Object_OK;

exit:
    ProcSource_freeData(&(me->elfSource), ptr);
    return ret;
}

/**
 * Description: Construct custom struct
 *
 * In:          cxt:
 *              offset:
 *              size:
 *
 * Out:         meOut: The service object.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static void ProcSource_ctor(ProcSource *me, void *ptr, size_t size)
{
    me->elfSource.getData = ProcSource_getData;
    me->elfSource.freeData = ProcSource_freeData;
    me->size = size;
    me->ptr = ptr;
}

/**
 * Description: Find the index of the segment which contains the flag
 *
 * In:          ef:      ElfFile struct of buffer.
 *              segFlag: The program header flag in question.
 *
 * Out:         segIdx: The index of the first segment which has the desired
 *                      PHDR flag set.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int _findSegmentIndexByFlag(const ElfFile *ef, uint32_t segFlag, size_t *segIdx)
{
    int32_t ret = Object_OK;
    size_t ii;
    size_t phNum = ElfFile_getSegmentCount(ef);

    // Loop through segments
    ElfSegment seg;
    for (ii = 0; ii < phNum; ii++) {
        // Exit if ElfFile_getSegmentInfo() fails
        T_GUARD(ElfFile_getSegmentInfo(ef, ii, &seg));

        // Test for seg flag
        if (((seg.flags) & PT_FLAG_TYPE_MASK) == segFlag) {
            *segIdx = ii;
            return Object_OK;
        }
    }

// Segment not found
exit:
    return Object_ERROR;
}

/**
 * Description: Find the index of the segment which contains the type
 *
 * In:          ef:      ElfFile struct of buffer.
 *              segType: The program header type in question.
 *
 * Out:         segIdx: The index of the first segment which has the desired
 *                      PHDR type set.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int _findSegmentIndexByType(const ElfFile *ef, uint32_t segType, size_t *segIdx)
{
    int32_t ret = Object_OK;
    size_t ii;
    size_t phNum = ElfFile_getSegmentCount(ef);

    // Loop through segments
    ElfSegment seg;
    for (ii = 0; ii < phNum; ii++) {
        // Exit if ElfFile_getSegmentInfo() fails
        T_GUARD(ElfFile_getSegmentInfo(ef, ii, &seg));

        // Test for seg type
        if (seg.type == segType) {
            *segIdx = ii;
            return Object_OK;
        }
    }

// Segment not found
exit:
    return Object_ERROR;
}

/**
 * Description: Return the source address, given a segment index.
 *
 * In:          ef:      ElfFile struct of buffer.
 *              srcAddr: Address of source buffer.
 *              srcSize: Size of source buffer.
 *              segIdx:  Index of the segment.
 *
 * Out:         segSrcAddr: Address of the segment at the given index.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int _getSegmentSrcAddr(const ElfFile *ef, const uintptr_t srcAddr, const size_t srcSize,
                              const size_t segIdx, uintptr_t *segSrcAddr)
{
    int ret = Object_OK;
    uintptr_t segAddr;
    ElfSegment seg;
    size_t phNum = ElfFile_getSegmentCount(ef);

    // Zero-based index cannot exceed the total number of segments
    T_CHECK(segIdx < phNum);

    T_GUARD(ElfFile_getSegmentInfo(ef, segIdx, &seg));

    segAddr = srcAddr + seg.offset;

    // Bounds checking to ensure that segAddr exists within the source buffer
    T_CHECK(!CHECK_OVERFLOW(srcAddr, srcSize));
    T_CHECK(segAddr <= srcAddr + srcSize && segAddr >= srcAddr);

    // Populate output
    *segSrcAddr = segAddr;

exit:
    return ret;
}

/**
 * Description: Locate and check bounds of manifest segment.
 *
 * In:          ef:      ElfFile struct of buffer.
 *              srcAddr: Address of source buffer.
 *              srcSize: Size of source buffer.
 *
 * Out:         pManifest: Pointer to segment base address within buffer.
 *              pManifestSize: segment size
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int _findSegment(const ElfFile *ef, const uintptr_t srcAddr, const size_t srcSize,
                        size_t segIdx, void **pManifest, uint64_t *pManifestSize)
{
    int32_t ret = Object_OK;
    uintptr_t segSrcAddr = 0;
    ElfSegment seg;

    // Get the address of the manifest segment
    T_GUARD(_getSegmentSrcAddr(ef, srcAddr, srcSize, segIdx, &segSrcAddr));

    /* Bounds checking to verify that the entire manifest segment resides
     * within the source buffer. */
    T_GUARD(ElfFile_getSegmentInfo(ef, segIdx, &seg));
    T_CHECK(!CHECK_OVERFLOW(segSrcAddr, seg.filesz));
    T_CHECK(!CHECK_OVERFLOW(srcAddr, srcSize));
    T_CHECK(segSrcAddr + seg.filesz <= srcAddr + srcSize);

    // Populate outputs
    *pManifest = (void *)segSrcAddr;
    *pManifestSize = seg.filesz;

exit:
    return ret;
}

/**
 * Description: Find the index of the section with a matching name.
 *
 * In:          ef:      ElfFile struct of buffer.
 *              srcAddr: Address of source buffer.
 *              name:    String of section name we are trying to find
 *
 * Out:         secIdx: The index of the first section whose name matches.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int _findSectionIndex(const ElfFile *ef, const uintptr_t srcAddr, char *name, size_t *secIdx)
{
    int32_t ret = Object_OK;
    size_t ii;
    size_t shNum = ElfFile_getSectionCount(ef);

    // Loop through sections
    ElfSection sec;
    for (ii = 0; ii < shNum; ii++) {
        // Exit if ElfFile_getSectionInfo() fails
        T_GUARD(ElfFile_getSectionInfo(ef, ii, &sec));

        // Test for matching section name
        const char *sec_name = (const char *)srcAddr + sec.name_addr;
        if (strlen(name) == strlen(sec_name) && strncmp(sec_name, name, strlen(name)) == 0) {
            *secIdx = ii;
            return Object_OK;
        }
    }

// Section not found
exit:
    return Object_ERROR;
}

/**
 * Description: Locate and check bounds of manifest section.
 *
 * In:          ef:      ElfFile struct of buffer.
 *              srcAddr: Address of source buffer.
 *              srcSize: Size of source buffer.
 *              secType: The program header flag
 *
 * Out:         pManifest: Pointer to section base address within buffer.
 *              pManifestSize: section size
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int _findManifestSection(const ElfFile *ef, const uintptr_t srcAddr, const size_t srcSize,
                                void **pManifest, uint64_t *pManifestSize)
{
    int32_t ret = Object_OK;
    uintptr_t secSrcAddr = 0;
    size_t secIdx = 0;
    ElfSection sec;
    char name[] = ".mink_metadata";

    // Find the manifest section by NAME
    T_GUARD(_findSectionIndex(ef, srcAddr, name, &secIdx));

    // Get the address of the manifest section
    T_GUARD(ElfFile_getSectionInfo(ef, secIdx, &sec));
    secSrcAddr = srcAddr + sec.offset;

    // Bounds checking to ensure that secSrcAddr exists within the source buffer
    T_CHECK(!CHECK_OVERFLOW(srcAddr, srcSize));
    T_CHECK(secSrcAddr <= srcAddr + srcSize && secSrcAddr >= srcAddr);

    /* Bounds checking to verify that the entire manifest section resides
     * within the source buffer. */
    T_CHECK(!CHECK_OVERFLOW(secSrcAddr, sec.size));
    T_CHECK(secSrcAddr + sec.size <= srcAddr + srcSize);

    // Populate outputs
    *pManifest = (void *)secSrcAddr;
    *pManifestSize = sec.size;

exit:
    return ret;
}

#ifdef ENABLE_AUTH
/**
 * Description: Authenticate a downloadable trusted process.
 *
 * In:          memObj:    Memory Buffer of ELF.
 *
 * Out:         bufRot:    Pointer to RoT value.
 *              bufDomain: Pointer to Domain str.
 *              elfArbVer: Pointer to AR version value of the ELF.
 *
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t _authTP(Object memObj, uint32_t *bufRot, char **bufDomain, uint32_t *elfArbVer)
{
    int32_t ret = Object_OK;
    ISecureImage_verifyInputs verifyInputs = {0};
    Object clientEnv = Object_NULL;
    Object secureImgSvc = Object_NULL;
    Object verifiedElfInfo = Object_NULL;

    // TZ-APP-OEM/TZ-APP-QTI
    verifyInputs.sw_id = 0xC;

    // secondary_sw_id is used by TZ to distinguish TA imgs.
    // Let SecureImage/SecureBoot pass secondary_sw_id validation.
    verifyInputs.enforcement_policy = ISecureImage_IGNORE_SECONDARY_SW_ID;

    T_CALL_ERR(TZCom_getClientEnvObject(&clientEnv), ITPreLauncher_ERROR_GET_AUTH_SVC);
    T_CALL_ERR(IClientEnv_open(clientEnv, CSecureImage_UID, &secureImgSvc),
               ITPreLauncher_ERROR_GET_AUTH_SVC);

    T_CALL_ERR(ISecureImage_verifyELFFromMemObj(secureImgSvc, memObj, NULL, 0, &verifyInputs,
                                                &verifiedElfInfo),
               ITPreLauncher_ERROR_ELF_SIGNATURE_ERROR);

    T_CALL_ERR(IVerifiedSecureImageELFInfo_getIntParameter(
                   verifiedElfInfo, ISecureImage_PARAM_OUT_AUTHORITIES, bufRot),
               ITPreLauncher_ERROR_ELF_SIGNATURE_ERROR);

    // We only allow single OEM signing for downloadable TPs
    T_CHECK_ERR(ISecureImage_OEM_SIGNED == *bufRot, ITPreLauncher_ERROR_ELF_SIGNATURE_ERROR);
    *bufDomain = QTVM_DOMAIN_OEM_STR;

    // Images are always signed with an AR version.
    // If signer doesn't provide a value to sectools,
    //   then the image will be signed with the default value of zero.
    // Get the OEM AR version signed in ELF
    T_CALL_ERR(IVerifiedSecureImageELFInfo_getIntParameter(
                   verifiedElfInfo, ISecureImage_PARAM_OUT_OEM_AR_VERSION, elfArbVer),
               ITPreLauncher_ERROR_GET_ARB_VERSION);

exit:
    Object_ASSIGN_NULL(verifiedElfInfo);
    Object_ASSIGN_NULL(secureImgSvc);
    Object_ASSIGN_NULL(clientEnv);

    return ret;
}

#ifdef ENABLE_ARB
/**
 * Description: Perform anti-rollback protection for a downloadable TP.
 *
 * In:          did:        Pointer to the Distinguished ID of TP.
 *              elfArbVer:  AR version value of the ELF.
 *
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t _antiRollBack(DistId *did, uint32_t elfArbVer)
{
    int32_t ret = Object_OK;
    uint8_t storedValBuf[4] = {0};
    size_t bufLenOut = 0;
    uint32_t storedArbVer = 0;
    IKVStoreKey_Info keyInfo = {0};
    Object clientEnv = Object_NULL;
    Object kvstoreSvc = Object_NULL;
    Object fileInstance = Object_NULL;

    T_CALL_ERR(TZCom_getClientEnvObject(&clientEnv), ITPreLauncher_ERROR_GET_RPMB_SVC);
    // We need ADCI to realize below
    T_CALL_ERR(IClientEnv_open(clientEnv, CKVStore_UID, &kvstoreSvc),
               ITPreLauncher_ERROR_GET_RPMB_SVC);

    // Query version number using DID in RPMB
    ret = IKVStore_getKeyHandle(kvstoreSvc, did->val, sizeof(did->val), &fileInstance);
    T_CHECK_ERR(Object_OK == ret || IKVStore_ERROR_KEY_NOT_FOUND == ret,
                ITPreLauncher_ERROR_RPMB_FAILURE);
    if (Object_isOK(ret)) {
        // RPMB has the version info of query DID

        T_CALL_ERR(IKVStoreKey_getInfo(fileInstance, &keyInfo), ITPreLauncher_ERROR_RPMB_FAILURE);
        T_CALL_ERR(IKVStoreKey_getValue(fileInstance, 0, keyInfo.dataSize, (void *)storedValBuf,
                                        sizeof(storedValBuf), &bufLenOut),
                   ITPreLauncher_ERROR_RPMB_FAILURE);

        memscpy(&storedArbVer, sizeof(storedArbVer), storedValBuf, sizeof(storedValBuf));

        // Perform Anti-rollback Protection
        T_CHECK_ERR(elfArbVer >= storedArbVer, ITPreLauncher_ERROR_ROLLBACK_FAILURE);

        if (elfArbVer > storedArbVer) {
            // Update a higher version number of query DID into RPMB

            memscpy(storedValBuf, sizeof(storedValBuf), &elfArbVer, sizeof(elfArbVer));

            // IKVStoreKey_updateValue ONLY overwrites bytes of assigned size
            T_CALL_ERR(IKVStoreKey_updateValue(fileInstance, (void *)storedValBuf,
                                               sizeof(storedValBuf), 0),
                       ITPreLauncher_ERROR_RPMB_FAILURE);
        }
    } else {
        // IKVStore_ERROR_KEY_NOT_FOUND
        // RPMB has no version info of query DID

        // Insert a new entry for version number of query DID in RPMB
        memscpy(storedValBuf, sizeof(storedValBuf), &elfArbVer, sizeof(elfArbVer));

        T_CALL_ERR(IKVStore_createNewKey(kvstoreSvc, did->val, sizeof(did->val), storedValBuf,
                                         sizeof(storedValBuf), IKVStore_NO_CRYPTO_RPMB_SERVICE,
                                         &fileInstance),
                   ITPreLauncher_ERROR_RPMB_FAILURE);
    }

exit:
    Object_ASSIGN_NULL(fileInstance);
    Object_ASSIGN_NULL(kvstoreSvc);
    Object_ASSIGN_NULL(clientEnv);

    return ret;
}
#endif  // ENABLE_ARB

#endif  // ENABLE_AUTH

/**
 * Description: Parse and authenticate ELF; perform rollback check; set relevant
 *              metadata.
 *
 * In:          ptr:      Base address of ELF buffer.
 *              size:     Size of ELF buffer.
 *              mdStrLen: Length of metadata string.
 *
 * Out:         programData: Actual executable file.
 *              mdStrPtr:    Pointer to metadata string.
 *              mdStrLenOut: Actual length of metadata string.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t _parseElf(Object memObj, uint8_t *mdStrPtr, size_t mdStrLen, DistId *did,
                         ITPPM_programData *programData, ITPreLauncher_authData *data_ptr,
                         size_t *mdStrLenOut, uint32_t cid)
{
    int32_t fd = -1;
    FdWrapper *fdw = NULL;
    struct stat st;
    int32_t size;
    void *ptr = NULL;
    ProcSource ElfSourceSEC = {{0}};
    ElfFile ElfFileSEC = {0};
    size_t segIdx;
    bool isSegmentMetaData = false;
    void *pMetaData = NULL;
    uint64_t pMetaDataSize = 0;
    void *pExecutableFile = NULL;
    uint64_t pExecutableFileSize = 0;
    MetadataInfo *mdInfo = NULL;
    size_t valueLenOut = 0;
    int32_t ret = Object_OK;
    uint32_t bufRot = 0;
    char *bufDomain = NULL;
    SHA256_CTX sha256;
    SHA256Hash hashValue;

    // extract process buffer ptr from memory object
    T_CALL_CHECK_ERR(fdw, FdWrapperFromObject(memObj), fdw != NULL, Object_ERROR_BADOBJ);
    fd = fdw->descriptor;

#ifdef ENABLE_AUTH
    uint32_t elfArbVer = 0;

    // Authenticate image before mapping.
    // Only downloadable TPs need to be authenticated.
    if (DL_OEM_SIGNED == cid) {
        T_CALL(_authTP(memObj, &bufRot, &bufDomain, &elfArbVer));
    } else if (cid >= EMBEDDED_BASE) {
        // Settings of Embedded Processes
        bufRot = ISecureImage_OEM_SIGNED;
        bufDomain = QTVM_DOMAIN_OEM_STR;
    }
#else   // ENABLE_AUTH
    bufRot = QTVM_ROT_NOTSIGNED;
    bufDomain = QTVM_DOMAIN_NOTSIGNED_STR;
#endif  // ENABLE_AUTH
    T_CHECK('\0' == bufDomain[sizeof(data_ptr->domain) - 1]);

    T_CALL(fstat(fd, &st));
    size = st.st_size;
    ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    T_CHECK(ptr != MAP_FAILED);

    // parse ELF
    ProcSource_ctor(&ElfSourceSEC, (void *)ptr, size);

    T_GUARD_ERR(ElfFile_ctor(&ElfFileSEC, &ElfSourceSEC.elfSource),
                ITPreLauncher_ERROR_METADATA_INVALID);

    // At the beginning of QTVM SDK, the compiled app was packaged together with the metadata
    // segment into an ELF file. The executable was marked with a special flag so it could be easily
    // identified and extracted from the ELF package and copied into memory for execution. Later,
    // the metadata segment was inserted directly into the compiled app to satisfy pVM security
    // classification requirements. Both situations need to be handled to avoid
    // backwards-compatibility issues.
    ret = _findSegmentIndexByFlag(&ElfFileSEC, PT_FLAG_MANIFEST_TYPE_MASK, &segIdx);
    if (Object_isOK(ret)) {
        T_GUARD(
            _findSegment(&ElfFileSEC, (uintptr_t)ptr, size, segIdx, &pMetaData, &pMetaDataSize));
        isSegmentMetaData = true;
        LOG_MSG("Found metadata in segment.");
    } else {
        T_GUARD_ERR(
            _findManifestSection(&ElfFileSEC, (uintptr_t)ptr, size, &pMetaData, &pMetaDataSize),
            ITPreLauncher_ERROR_METADATA_INVALID);
        LOG_MSG("Found metadata in section.");
    }
    T_CHECK((0 < pMetaDataSize) && (pMetaDataSize <= mdStrLen));
    T_CHECK('\0' == ((char *)pMetaData)[pMetaDataSize - 1]);
    LOG_MSG("pMetaData str = %s", (char *)pMetaData);

    // New metadata Info
    *mdStrLenOut = memscpy(mdStrPtr, mdStrLen, (char *)pMetaData, pMetaDataSize);
    T_CHECK(*mdStrLenOut != 0);

    T_CALL(MetadataInfo_new(&mdInfo, (char *)mdStrPtr, strlen((char *)mdStrPtr)));

    // Set RoT
    T_GUARD(MetadataInfo_setRot(mdInfo, bufRot));
    data_ptr->rot = bufRot;

    // Set domain
    T_CHECK(bufDomain);
    T_GUARD(MetadataInfo_setDomain(mdInfo, bufDomain));
    memscpy((char *)data_ptr->domain, sizeof(data_ptr->domain), bufDomain, strlen(bufDomain) + 1);

    // Perform rollback check using DistId
    T_GUARD(MetadataInfo_getDistId(mdInfo, did));

#if defined(ENABLE_AUTH) && defined(ENABLE_ARB)
    if (DL_OEM_SIGNED == cid) {
        T_CALL(_antiRollBack(did, elfArbVer));
    }
#endif

    // Check if optional flag, neverUnload, is set.
    MetadataInfo_getValueByName(mdInfo, "U", sizeof("U"), (void *)(&programData->neverUnload), 1,
                                &valueLenOut);
    MetadataInfo_getValueByName(mdInfo, "n", sizeof("n"), (void *)(&programData->fileName),
                                sizeof(programData->fileName), &valueLenOut);

    // Get executable file offset
    if (isSegmentMetaData) {
        // Legacy SDK - executable included as ELF segment
        T_GUARD_ERR(_findSegmentIndexByFlag(&ElfFileSEC, PT_FLAG_TPROCESS_TYPE_MASK, &segIdx),
                    ITPreLauncher_ERROR_PROC_INVALID);
        T_GUARD(_findSegment(&ElfFileSEC, (uintptr_t)ptr, size, segIdx, &pExecutableFile,
                             &pExecutableFileSize));
    } else {
        // New SDK - executable is entire buffer
        // At the very least, an executable needs to have an interpreter segment.
        T_GUARD_ERR(_findSegmentIndexByType(&ElfFileSEC, PT_INTERP, &segIdx),
                    ITPreLauncher_ERROR_PROC_INVALID);
        T_GUARD_ERR(_findSegmentIndexByType(&ElfFileSEC, PT_GNU_RELRO, &segIdx),
                    ITPreLauncher_ERROR_PROC_INVALID);
        pExecutableFile = ptr;
        pExecutableFileSize = size;
    }

    T_CHECK(SHA256_Init(&sha256));
    T_CHECK(SHA256_Update(&sha256, pExecutableFile, pExecutableFileSize));
    T_CHECK(SHA256_Final(hashValue.val, &sha256));

    memscpy(data_ptr->hash, sizeof(data_ptr->hash), hashValue.val, sizeof(hashValue.val));

    programData->offset = (uintptr_t)pExecutableFile - (uintptr_t)ptr;
    programData->fileSize = pExecutableFileSize;

exit:
    if (ptr) {
        munmap(ptr, size);
    }

    ElfFile_dtor(&ElfFileSEC);
    MetadataInfo_destruct(mdInfo);

    return ret;
}

/////////////////////////////////////////////
//       PreLauncher definition          ////
/////////////////////////////////////////////
/**
 * Description: Parse, check, and launch a process if not already loaded.
 *
 * In:          memObj:      Memory Object of Elf buffer.
 *              mdStrLen: Length of metadata string.
 *
 * Out:         pidPtr:      PID of launched or existing process.
 *              mdStrPtr:    Null-terminated string containing binary metadata.
 *              mdStrLenOut: Actual length of metadata string.
 *              tProcCtrlObj: Actual length of metadata string.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t CTPreLauncher_launch(TPreLauncher *me, Object memObj, uint32_t cid, uint32_t *pidPtr,
                                    uint8_t *mdStrPtr, size_t mdStrLen, size_t *mdStrLenOut,
                                    ITPreLauncher_authData *data_ptr, Object *tProcCtrlObj,
                                    Object *tProcObj)
{
    int32_t ret = Object_OK;
    ITPPM_programData programData = {0};
    DistId *did = HEAP_ZALLOC_TYPE(DistId);

    T_CHECK_ERR(did, ITPreLauncher_ERROR_HEAP_MALLOC_FAILURE);

    pthread_mutex_lock(&gLock);

    T_CALL(_parseElf(memObj, mdStrPtr, mdStrLen, did, &programData, data_ptr, mdStrLenOut, cid));

    // Any embedded process which does NOT have neverUnload set will immediately
    // shut down. Prevent confusion by returning error instead.
    if (cid >= EMBEDDED_BASE) {
        T_CHECK_ERR(programData.neverUnload, ITPreLauncher_ERROR_EMBED_MISSING_PROPERTY);
    }

    T_CALL_REMAP(ITPPM_launch(gRootObj, memObj, cid, (ITProcess_DistID *)did, &programData, pidPtr,
                              tProcCtrlObj, tProcObj),
                 _iTPPMToITPreLauncherError, "for CID = %d", cid);

    LOG_MSG("PreLauncher Success");

exit:
    if (did) {
        HEAP_FREE_PTR(did);
    }

    pthread_mutex_unlock(&gLock);

    return ret;
}

static int32_t CTPreLauncher_registerNotify(TPreLauncher *me, Object notify)
{
    return ITPPM_registerNotify(gRootObj, notify);
}

static int32_t CTPreLauncher_shutdown(TPreLauncher *me, uint32_t restart, uint32_t force)
{
    return ITPPM_shutdown(gRootObj, restart, force);
}

static int32_t CTPreLauncher_retain(TPreLauncher *me)
{
    atomicAdd(&me->refs, 1);

    return Object_OK;
}

static int32_t CTPreLauncher_release(TPreLauncher *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        Object_RELEASE_IF(me->credentials);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static ITPreLauncher_DEFINE_INVOKE(CTPreLauncher_invoke, CTPreLauncher_, TPreLauncher *);

int32_t CTPreLauncher_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    int32_t ret = Object_OK;
    TPreLauncher *me = HEAP_ZALLOC_TYPE(TPreLauncher);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    me->refs = 1;
    Object_INIT(me->credentials, credentials);

    *objOut = (Object){CTPreLauncher_invoke, me};

exit:
    return ret;
}
