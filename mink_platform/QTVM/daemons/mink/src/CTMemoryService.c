// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#ifdef STUB
#include "BufferAllocatorWrapper.h"
#else
#include <BufferAllocator/BufferAllocatorWrapper.h>
#endif

#include "CTPowerService_open.h"
#include "CTPrivilegedProcessManager.h"
#include "CTPrivilegedProcessManager_open.h"
#include "CTrustedCameraMemory.h"
#include "Elf.h"
#include "ElfFile.h"
#include "ITAccessPermissions.h"
#include "ITBufCallBack_invoke.h"
#include "ITMemoryService.h"
#include "ITMemoryService_invoke.h"
#include "ITPowerService.h"
#include "ITPrivilegedProcessManager.h"
#include "MSMem.h"
#include "MemSCpy.h"
#include "MetadataInfo.h"
#include "MinkTypes.h"
#include "TMemLock.h"
#include "TUtils.h"
#include "cdefs.h"
#include "fdwrapper.h"
#include "heap.h"
#include "object.h"
#include "vmmem_wrapper.h"

typedef struct {
    int32_t refs;
} TMemPoolFactory;

typedef struct {
    int32_t refs;
} TAccessControl;

#define MAX_HEAP_NAME_SIZE 256

// 4KB/2MB block is consistent with kernel page/subsection size
// NOTE: update if kernel allocation behavior changed
#define ALIGN_BLOCK_4KB (1ULL << 12)
#define ALIGN_BLOCK_2MB (1ULL << 21)

typedef struct {
    int32_t refs;
    int32_t poolFd;
    size_t poolSizeinBytes;
    size_t remainInBytes;
    int32_t heapListIdx;
    char heapDstName[MAX_HEAP_NAME_SIZE];
    ITAccessPermissions_rules confRules;
    pthread_mutex_t allocLock;
} TMemPool;

typedef struct {
    int32_t refs;
    Object pool;
    size_t bufSize;
} TBufCallBack;


/**
 * Description: Align size up to particular block size boarder.
 *              Apply in following two cases:
 *              - Buffer allocated from memPool align to 4KB.
 *              - MemPool allocated from heap align to 2MB.
 * In:          requestSize: The actual requested size(bytes).
 *              alignBorder: Type of align border(macro).
 * Return:      Size after aligned to block size.
 */
static inline uint64_t _sizeAlign(uint64_t requestSize, uint64_t alignBorder)
{
    if (ALIGN_BLOCK_4KB != alignBorder && ALIGN_BLOCK_2MB != alignBorder) {
        LOG_ERR("Not supported align border, return default size \n");
        return requestSize;
    }

    if (requestSize >= (UINT64_MAX - (alignBorder - 1))) {
        return UINT64_MAX;
    }

    return ((requestSize + (alignBorder - 1)) & (~(alignBorder - 1)));
}

#define ARRAY_LENGTH(__arr) (sizeof(__arr) / sizeof((__arr)[0]))

/////////////////////////////////////////////
//        DMABUFHEAP configuration       ////
/////////////////////////////////////////////

// One that requests the heap will occupy the heap until
//   all memory buffers allocated from the heap are released.
typedef struct {
    char heapName[MAX_HEAP_NAME_SIZE];
    bool available;
} DmaBufHeap;

// Below settings should be consistent with PRODUCT-vm-dma-heaps.dtsi
/* clang-format off */
static DmaBufHeap gListOfHeaps[] = {
    {"qcom,tui", true},
    {"qcom,ms1", true},
    {"qcom,ms2", true},
    {"qcom,ms3", true},
    {"qcom,ms4", true},
    {"qcom,ms5", true},
    {"qcom,ms6", true},
    {"qcom,ms7", true},
};

static uint64_t gAllowedUids[] = {
    CTrustedCameraMemory_UID
};
/* clang-format on */

pthread_mutex_t gHeapListLock = PTHREAD_MUTEX_INITIALIZER;

// VM name. DO NOT modify it.
#ifndef OEM_VM
static char *gMemBufVmName = "qcom,trusted_vm";
#else
static char *gMemBufVmName = "qcom,oemvm";
#endif

/////////////////////////////////////////////
//           BufCallBack definition      ////
/////////////////////////////////////////////

static int32_t CTBufCallBack_retain(TBufCallBack *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t CTBufCallBack_release(TBufCallBack *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        TMemPool *pool = (TMemPool *)((me->pool).context);
        pthread_mutex_lock(&pool->allocLock);
        pool->remainInBytes += me->bufSize;
        pthread_mutex_unlock(&pool->allocLock);
        Object_RELEASE_IF(me->pool);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static ITBufCallBack_DEFINE_INVOKE(CTBufCallBack_invoke, CTBufCallBack_, TBufCallBack *);

static int32_t CTBufCallBack_new(Object pool, size_t bufSize, Object *objOut)
{
    int32_t ret = Object_OK;

    TBufCallBack *me = HEAP_ZALLOC_TYPE(TBufCallBack);
    if (!me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    Object_INIT(me->pool, pool);
    me->bufSize = bufSize;

    *objOut = (Object){CTBufCallBack_invoke, me};

    return ret;
}

/////////////////////////////////////////////
//           MemPool definition          ////
/////////////////////////////////////////////
// Ensure CTMemPool_invoke available in CTMemPool_allocateBuffer's implementation
static int32_t CTMemPool_allocateBuffer(TMemPool *me, uint64_t size_val, Object *memObjPtr);
static int32_t CTMemPool_retain(TMemPool *me);
static int32_t CTMemPool_release(TMemPool *me);

static ITMemPool_DEFINE_INVOKE(CTMemPool_invoke, CTMemPool_, TMemPool *);
/**
 * Description: Request a memory buffer from the memory pool
 *
 * In:          size_val:   The size(bytes) of requested memory buffer.
 *
 * Out:         memObjPtr: Pointer to a new memObj which is MSMemObj at
 *                     Mink side and which will be FdWrapper at client side.
 *
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t CTMemPool_allocateBuffer(TMemPool *me, uint64_t size_val, Object *memObjPtr)
{
    int32_t ret = Object_OK;
    int32_t bufFd = -1;
    uint64_t requestedBufSize = size_val;
    Object bufCallBack = Object_NULL;
    BufferAllocator *bufferAllocator = NULL;
    Object wakeLockObj = Object_NULL;
    Object powerServiceObj = Object_NULL;

    // Ensure BufSize fits in size_t on 32-bit platform
    if(requestedBufSize > SIZE_MAX) {
        LOG("Buffer size larger than SIZE_MAX is not supported \n");
        return Object_ERROR_MEM;
    }

    uint64_t actualBufSize = _sizeAlign(requestedBufSize, ALIGN_BLOCK_4KB);
    LOG_MSG("Requested bufSize = %u%09u, actual size is %u%09u \n",
            UINT64_HIGH(requestedBufSize), UINT64_LOW(requestedBufSize),
            UINT64_HIGH(actualBufSize), UINT64_LOW(actualBufSize));

    pthread_mutex_lock(&me->allocLock);

    // Ensure VM doesn't go to sleep during call
    T_GUARD(CTPowerServiceFactory_open(0, Object_NULL, &powerServiceObj));
    T_CALL(ITPowerService_acquireWakeLock(powerServiceObj, &wakeLockObj));

    bufferAllocator = CreateDmabufHeapBufferAllocator();
    T_CHECK_ERR(bufferAllocator != NULL, ITMemPool_ERROR_SETUP);

    T_CHECK_ERR(actualBufSize <= me->remainInBytes, Object_ERROR_MEM);

    bufFd = DmabufHeapAlloc(bufferAllocator, me->heapDstName, actualBufSize, 0, 0);
    T_CHECK_ERR(bufFd >= 0, ITMemPool_ERROR_ALLOC);

    me->remainInBytes -= actualBufSize;

    T_CALL(CTBufCallBack_new((Object){CTMemPool_invoke, me}, actualBufSize, &bufCallBack));

    T_CALL(MSMem_new(bufFd, bufCallBack, memObjPtr));
    if(ret != Object_OK){
        LOG_MSG("from poolFd=%d returning MSMem with fd : %d fail", me->poolFd, bufFd);
#ifndef STUB
        close(bufFd);
#else
        close_offtarget_unlink(bufFd);
#endif
    } else {
        LOG_MSG("from poolFd=%d returning MSMem with fd : %d success", me->poolFd, bufFd);
    }

exit:
    pthread_mutex_unlock(&me->allocLock);
    if (bufferAllocator) {
        FreeDmabufHeapBufferAllocator(bufferAllocator);
    }

    Object_ASSIGN_NULL(bufCallBack);
    Object_ASSIGN_NULL(wakeLockObj);
    Object_ASSIGN_NULL(powerServiceObj);
    return ret;
}

static int32_t CTMemPool_retain(TMemPool *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t CTMemPool_release(TMemPool *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        pthread_mutex_lock(&gHeapListLock);
#ifndef STUB
        close(me->poolFd);
#endif
        gListOfHeaps[me->heapListIdx].available = true;
        pthread_mutex_unlock(&gHeapListLock);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

/**
 * Description: Request an available heap
 *
 * In:          heapName: The name of some available heap.
 * Out:         index: index of gListOfHeaps that is selected.
 *
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t getAvailableHeap(char *heapName, int32_t *index)
{
    int32_t ret = Object_ERROR;
    int32_t idx;

    for (idx = 0; idx < sizeof(gListOfHeaps) / sizeof(DmaBufHeap); idx++) {
        if (gListOfHeaps[idx].available) {
            memscpy(heapName, MAX_HEAP_NAME_SIZE, gListOfHeaps[idx].heapName,
                    sizeof(gListOfHeaps[idx].heapName));
            LOG_MSG("using heap: %s ", heapName);
            *index = idx;
            ret = Object_OK;
            goto exit;
        }
    }

    LOG_MSG("NO available heap!");

exit:
    return ret;
}

/**
 * Description: Talk with driver to get the DONATED heap FD
 *
 * In:          confRulesPtr:    The pointer to a permission structure.
 *              heapName:        The name of the requested heap.
 *              size:            Expected size of the requested heap.
 *
 * Out:         poolFd:   The DONATED heap FD returned by driver.
 *
 * Return:       0 on success.
 *               ITMemPoolFactory_ERROR_REMOTE_ALLOC on creating vmmem failure;
 *               others on allocing dma buffer failure.
 */
static int32_t populateHeap(const ITAccessPermissions_rules *confRulesPtr, char *heapName,
                            uint64_t size, int32_t *poolFd)
{
    int ret = Object_OK;
    uint32_t perms = VMMEM_READ | VMMEM_WRITE;
    int32_t retry = 10;
    VmMem *vmmem = NULL;
    VmHandle tvmHandle;

    T_CALL_CHECK_ERR(vmmem, CreateVmMem(), NULL != vmmem, ITMemPoolFactory_ERROR_HEAP_SETUP,
                     "unable to create VM");

    T_CALL_CHECK_ERR(tvmHandle, FindVmByName(vmmem, gMemBufVmName), tvmHandle >= 0,
                     ITMemPoolFactory_ERROR_HEAP_SETUP, "unable to find VM name");

    ret = ITMemPoolFactory_ERROR_REMOTE_ALLOC;
    do {
        if (confRulesPtr->specialRules & ITAccessPermissions_qcomDisplayDmabuf) {
            *poolFd =
                RemoteAllocDmabuf(vmmem, size, &tvmHandle, &perms, 1, "qcom,display", heapName);
        } else {
            *poolFd =
                RemoteAllocDmabuf(vmmem, size, &tvmHandle, &perms, 1, "qcom,system", heapName);
        }

        if (*poolFd >= 0) {
            ret = Object_OK;
            break;
        }

        LOG_MSG("Remote allocation failed to transfer heap rc: retval=%d ", *poolFd);
        retry--;
        usleep(1000);
        LOG_MSG("retrying.. ");
    } while (retry >= 0);

    if (Object_isOK(ret)) {
        LOG_MSG("Succeeded to transfer heap rc: poolFd=%d ", *poolFd);
    } else {
        LOG_MSG("membuf allocation failed to transfer heap rc: %d", *poolFd);
    }

exit:
    if (vmmem) {
        FreeVmMem(vmmem);
    }

    return ret;
}

/**
 * Description: Create a new memory pool
 *
 * In:        confRulesPtr:      The pointer to a permission structure.
 *            poolSize:          Expected size of the new memory pool.
 *
 * Out:       objOut:            The pointer to the new memory pool.
 *
 * Return:    Object_OK on success.
 *            Object_ERROR on failure.
 */
static int32_t CTMemPool_new(const ITAccessPermissions_rules *confRulesPtr, uint64_t poolSize,
                             Object *objOut)
{
    int32_t ret = Object_OK;
    TMemPool *me = HEAP_ZALLOC_TYPE(TMemPool);
    if (!me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    me->poolFd = -1;
    me->poolSizeinBytes = poolSize;
    me->remainInBytes = poolSize;

    memcpy(&(me->confRules), confRulesPtr, sizeof(ITAccessPermissions_rules));

    pthread_mutex_init(&(me->allocLock), NULL);

    pthread_mutex_lock(&gHeapListLock);

    T_CALL_ERR(getAvailableHeap(me->heapDstName, &(me->heapListIdx)),
               ITMemPoolFactory_ERROR_NO_AVAILABLE_HEAP);

    T_CALL(populateHeap(&me->confRules, me->heapDstName, poolSize, &me->poolFd));

    gListOfHeaps[me->heapListIdx].available = false;

    *objOut = (Object){CTMemPool_invoke, me};

exit:
    pthread_mutex_unlock(&gHeapListLock);
    if (ret) {
        HEAP_FREE_PTR(me);
    }

    return ret;
}

/////////////////////////////////////////////
//       TMemPoolFactory definition       ////
/////////////////////////////////////////////

static int32_t CTMemPoolFactory_retain(TMemPoolFactory *me)
{
    atomicAdd(&me->refs, 1);

    return Object_OK;
}

static int32_t CTMemPoolFactory_release(TMemPoolFactory *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

/**
 * Description: Create a new memory pool
 *
 * In:        confRulesPtr:     The pointer to a permission structure.
 *            size_val:         Expected size of the new memory pool.
 *
 * Out:       poolObjPtr:       The pointer to the new memory pool.
 *
 * Return:    Object_OK on success.
 *            Object_ERROR on failure.
 */
static int32_t CTMemPoolFactory_createPool(TMemPoolFactory *me,
                                           const ITAccessPermissions_rules *confRulesPtr,
                                           uint64_t size_val, Object *poolObjPtr)
{
    int32_t ret = Object_OK;
    Object wakeLockObj = Object_NULL;
    Object powerServiceObj = Object_NULL;
    uint64_t poolSize = size_val;

    // Ensure PoolSize fits in size_t on 32-bit platform
    T_CHECK(poolSize <= SIZE_MAX);
    uint64_t actualPoolSize = _sizeAlign(poolSize, ALIGN_BLOCK_2MB);
    LOG_MSG("Requested poolSize = %u%09u, actual size is %u%09u \n",
            UINT64_HIGH(poolSize), UINT64_LOW(poolSize),
            UINT64_HIGH(actualPoolSize), UINT64_LOW(actualPoolSize));

    ITAccessPermissions_rules defaultRules = {0};
    defaultRules.specialRules = ITAccessPermissions_keepSelfAccess;

    // Ensure VM doesn't go to sleep during call
    T_GUARD(CTPowerServiceFactory_open(0, Object_NULL, &powerServiceObj));
    T_CALL(ITPowerService_acquireWakeLock(powerServiceObj, &wakeLockObj));

    // Check for confinement rules
    if (NULL != confRulesPtr) {
        T_CHECK_ERR((ITAccessPermissions_keepSelfAccess == confRulesPtr->specialRules) ||
                        (ITAccessPermissions_qcomDisplayDmabuf & confRulesPtr->specialRules),
                    ITMemPoolFactory_ERROR_INVALID_CONFINEMENT);
        T_GUARD(CTMemPool_new(confRulesPtr, actualPoolSize, poolObjPtr));
    } else {
        T_GUARD(CTMemPool_new(&defaultRules, actualPoolSize, poolObjPtr));
    }

exit:
    Object_ASSIGN_NULL(wakeLockObj);
    Object_ASSIGN_NULL(powerServiceObj);
    return ret;
}

static ITMemPoolFactory_DEFINE_INVOKE(CTMemPoolFactory_invoke, CTMemPoolFactory_,
                                      TMemPoolFactory *);

int32_t CTMemPoolFactory_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    (void)credentials;
    int32_t ret = Object_OK;
    TMemPoolFactory *me = HEAP_ZALLOC_TYPE(TMemPoolFactory);

    if (!me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;

    *objOut = (Object){CTMemPoolFactory_invoke, me};

    return ret;
}

/////////////////////////////////////////////
//       TAccessControl definition        ////
/////////////////////////////////////////////

static int32_t CTAccessControl_retain(TAccessControl *me)
{
    atomicAdd(&me->refs, 1);

    return Object_OK;
}

static int32_t CTAccessControl_release(TAccessControl *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static int32_t CTAccessControl_acquireLock(TAccessControl *me,
                                           const ITAccessPermissions_rules *confRulesPtr,
                                           Object memObj, Object *lockObjPtr)
{
    (void)me;
    int32_t ret = ITAccessControl_ERROR_INVALID_CONFINEMENT;
    int32_t index;
    const ITAccessPermissions_uidBasedPerms *uidPermissionPtr = NULL;

#ifndef OEM_VM
    T_CHECK_ERR(NULL != confRulesPtr, ITAccessControl_ERROR_INVALID_CONFINEMENT);

    // Note that only 1st element of uidPermsList in confRules will be checked for now.
    uidPermissionPtr = &confRulesPtr->uidPermsList[0];

    // check if uid belongs to gAllowedUids
    for (index = 0; index < ARRAY_LENGTH(gAllowedUids); index++) {
        if (gAllowedUids[index] == uidPermissionPtr->uid) {
            T_CALL_ERR(TMemLock_new(memObj, uidPermissionPtr->uid, lockObjPtr),
                       ITAccessControl_ERROR_ACQUIRE_LOCK);
            break;
        }
    }
#else
    (void)uidPermissionPtr;
    (void)index;
    (void)gAllowedUids;
    LOG_ERR("acquireLock is not defined for OEMVM TAccessControl service.");
    ret = Object_ERROR;
#endif

exit:
    return ret;
}

static int32_t
CTAccessControl_checkExclusiveAccess(TAccessControl *me,
                                     const ITAccessPermissions_rules *confRulesPtr,
                                     Object memObj)
{
    int32_t ret = Object_OK, memFd = -1;
    uint64_t expectedSpecialRules = 0;
    MSMem *msmem = NULL;
    (void)me;

    T_CHECK_ERR(NULL != confRulesPtr, ITAccessControl_ERROR_INVALID_CONFINEMENT);
    expectedSpecialRules = confRulesPtr->specialRules;

    T_CHECK_ERR(isMSMem(memObj, &memFd), ITAccessControl_ERROR_BAD_MEMOBJ);
    msmem = MSMemFromObject(memObj);
    T_CHECK_ERR(NULL != msmem, ITAccessControl_ERROR_BAD_MEMOBJ);

    // Only specialRules in confRules will be checked for now.
    T_CHECK_ERR(expectedSpecialRules == msmem->confRules.specialRules,
                ITAccessControl_ERROR_ACCESS_CHECK_FAILED_SPECIALRULES);

exit:
    return ret;
}

static ITAccessControl_DEFINE_INVOKE(CTAccessControl_invoke, CTAccessControl_, TAccessControl *);

int32_t CTAccessControl_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    (void)credentials;
    int32_t ret = Object_OK;

    TAccessControl *me = HEAP_ZALLOC_TYPE(TAccessControl);
    if (!me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;

    *objOut = (Object){CTAccessControl_invoke, me};

    return ret;
}
