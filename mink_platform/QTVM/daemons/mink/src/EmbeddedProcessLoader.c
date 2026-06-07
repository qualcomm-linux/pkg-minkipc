// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "EmbeddedProcessLoader.h"
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "CDemuraTnService.h"
#include "CTCDriverCBService.h"
#include "CTMemoryService_open.h"
#include "CTProcessLoader.h"
#include "CTProcessLoader_open.h"
#include "CTUICoreService.h"
#include "CTouchInput.h"
#include "CVMFileTransferService.h"
#include "EmbeddedProcessIDs.h"
#include "EmbeddedProcessList.h"
#include "ITMemoryService.h"
#include "ITProcessLoader.h"
#include "MSMem.h"
#include "TUtils.h"
#include "fdwrapper.h"
#include "heap.h"
#include "vmmem_wrapper.h"
#ifdef STUB
#include "BufferAllocatorWrapper.h"
#else
#include <BufferAllocator/BufferAllocatorWrapper.h>
#endif

// membuf doesn't map physical pages without 2mb alignment
#define SIZE_2MB 0x200000

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Description: Given a UID, populate the corresponding ELF into a file.
 *
 * In:          uid: Unique ID of requested service.
 *
 * Out:         fd: File descriptor containing the corresponding ELF.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t _getBinary(uint32_t uid, cid_t *cid, Object memPoolObj, Object *memObj)
{
    uint32_t idx;
    int32_t ret = Object_OK, bufFd = -1;
    char *fileName = NULL;
    FILE *pFile = NULL;
    size_t allocSize;
    void *ptr = NULL;
    BufferAllocator *bufferAllocator = CreateDmabufHeapBufferAllocator();

    T_CHECK(bufferAllocator != NULL);

    // Find CID
    for (idx = 0; idx < embeddedProcessIDCount; idx++) {
        if (embeddedProcessIDList[idx].uid == uid) {
            *cid = embeddedProcessIDList[idx].cid;
            break;
        }
    }

    // Find file path
    for (idx = 0; idx < embeddedProcessCount; idx++) {
        if (embeddedProcessList[idx].cid == *cid) {
            fileName = embeddedProcessList[idx].path;
            break;
        }
    }
    T_CHECK(fileName != NULL);

    pFile = fopen(fileName, "rb");
    T_CHECK(pFile != NULL);

    // Get file size
    struct stat st;
    stat(fileName, &st);
    size_t fileLen = st.st_size;
    LOG_MSG("File %s size: %lu", fileName, fileLen);

    allocSize = (fileLen + (SIZE_2MB - 1)) & (~(SIZE_2MB - 1));

    LOG_MSG("EPL needs %lu bytes to load the embedded process", allocSize);

    T_CALL(ITMemPool_allocateBuffer(memPoolObj, allocSize, memObj));
    T_CHECK(isMSMem(*memObj, &bufFd));
    LOG_MSG("bufFd = %d", bufFd);
    T_CHECK(!(bufFd <= 0));

    ptr = mmap(NULL, fileLen, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(!(ptr == MAP_FAILED));

    // Read file into buffer
    int n = fread(ptr, 1, fileLen, (FILE *)pFile);
    T_CHECK(fileLen == n);

    // Make buffer read-only to protect against tampering after populating buffer.
    T_CHECK(!mprotect(ptr, fileLen, PROT_READ));

#ifdef STUB
    // return a O_RDONLY fd
    bufFd = RefreshMemFd(bufFd, O_RDONLY);
    LOG_MSG("Read only bufFd = %d", bufFd);
    MSChangeToROFd(*memObj, bufFd);
#else

    DmabufHeapCpuSyncStart(bufferAllocator, bufFd, kSyncReadWrite, NULL, NULL);

    DmabufHeapCpuSyncEnd(bufferAllocator, bufFd, kSyncReadWrite, NULL, NULL);
#endif

exit:
    if (ret) {
        perror("Error: ");
    }

    if (ptr != NULL) {
        munmap(ptr, fileLen);
    }

    if (pFile) {
        fclose(pFile);
    }

    if (bufferAllocator) {
        FreeDmabufHeapBufferAllocator(bufferAllocator);
    }

    return ret;
}

/**
 * Description: Test if service is hosted within an embedded process.
 *
 * In:          uid: Unique ID of requested service.
 *
 * Return:      true on success.
 *              false on failure.
 */
bool EmbeddedProcessLoader_isUIDSupported(uint32_t uid)
{
    uint32_t i;

    for (i = 0; i < embeddedProcessIDCount; i++) {
        if (uid == embeddedProcessIDList[i].uid) {
            LOG_MSG("UID is an EmbeddedProcess");
            return true;
        }
    }

    return false;
}

/**
 * Description: Load embedded process which hosts requested service.
 *
 * In:          uid: Unique ID of requested service.
 *              credentials: ICredentials object of caller.
 *
 * Return:      EmbeddedProcessLoader_PROCESS_LOADED on success.
 *              All else on failure.
 */
int32_t EmbeddedProcessLoader_load(uint32_t uid, Object credentials)
{
    int32_t ret = Object_ERROR;
    Object tProcLoaderObj = Object_NULL;
    Object tProcCtrlObj = Object_NULL;
    Object memObj = Object_NULL;
    Object memFactoryObj = Object_NULL;
    Object memPoolObj = Object_NULL;
#ifdef STUB
    int32_t fd = -1;
#endif
    uint32_t poolMemSize = 10 * 1024 * 1024;  // 10MB
    cid_t cid = -1;

    pthread_mutex_lock(&gLock);

    T_CALL(CTMemPoolFactory_open(uid, credentials, &memFactoryObj));

    T_CALL(ITMemPoolFactory_createPool(memFactoryObj, NULL, poolMemSize, &memPoolObj));

    T_CALL(_getBinary(uid, &cid, memPoolObj, &memObj));

    T_CALL(CTProcessLoader_openEmbedded(cid, credentials, &tProcLoaderObj));

    T_CALL(ITProcessLoader_loadFromBuffer(tProcLoaderObj, memObj, &tProcCtrlObj));

    LOG_MSG("Successfully loaded embedded app");

exit:
#ifdef STUB
    T_TRACE((isMSMem(memObj, &fd)));
    close_offtarget_unlink(fd);
#endif

    /* Callers do not have control over lifetimes of embedded processes. Unless
     * 'neverUnload' is set, the process will shut down immediately. */
    Object_ASSIGN_NULL(tProcCtrlObj);
    Object_ASSIGN_NULL(tProcLoaderObj);
    Object_ASSIGN_NULL(memObj);
    Object_ASSIGN_NULL(memPoolObj);
    Object_ASSIGN_NULL(memFactoryObj);

    pthread_mutex_unlock(&gLock);

    return ret;
}
