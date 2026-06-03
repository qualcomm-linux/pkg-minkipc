// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "memscpy.h"

#include "CAVMTestService.h"
#include "CAllPrivilegeTestService.h"
#include "CAppClient.h"
#include "CCommonTestService.h"
#include "CCredDealerTestService.h"
#include "CEmbeddedAllPrivilegeTestService.h"
#include "CEmbeddedCommonTestService.h"
#include "CEmbeddedMissingNeverUnload.h"
#include "CEmbeddedNormalDeathTestService.h"
#include "CEmbeddedSpareTestService.h"
#include "CEmbeddedStressTestService.h"
#include "CEmbeddedWrongTestService.h"
#include "COEMVMPlatformInfo.h"
#include "CQTVMPlatformInfo.h"
#include "CSpareTestService.h"
#include "CStressTestService.h"
#include "CTAccessControl.h"
#include "CTOEMVMAccessControl.h"
#include "CTMemoryService.h"
#include "CTOEMVMMemoryService.h"
#include "CTOEMVMPowerService.h"
#include "CTPowerService.h"
#include "CVMDeviceUniqueKey.h"
#include "CAppLoader.h"
#include "IAVMTestService.h"
#include "IAppClient.h"
#include "ICredentials.h"
#include "ITEnv.h"
#include "ITMemoryService.h"
#include "ITPowerService.h"
#include "ITestService.h"
#include "ITestService_invoke.h"
#include "IVMDeviceUniqueKey.h"
#include "IAppLoader.h"
#include "ITestInterface.h"
#include "ITestMemManager.h"
#include "TUtils.h"
#include "assert.h"
#include "heap.h"
#include "moduleAPI.h"
#include "object.h"
#include "vmuuid.h"
#include "tzecotestapp_uids.h"

#ifndef OFFTARGET
#include "version.h"
#include <minkipc/Profiling.h>
#else
#include "../../../../../mink/Platform/QTVM/daemons/mink/inc/version.h"
#include "../../../../../mink/Transport/MinkSocket/sock/Profiling.h"
#endif

#define NOT_USED_TEST_UID ((int32_t)0xffffffff)
#define MAX_STORED_CREDS 5
#define MAX_TEST_PARALLELISM 5
#define TEST_OPEN_POWER_SERVICE 0
#define TEST_ACQUIRE_WAKE_LOCK 1
#define TEST_CLOSE_POWER_SERVICE 2
#define NUM_TEST_THREADS 5
#define QTVM_ALLOC_ITERATION 20

typedef int32_t (*__servFunc)(Object);

typedef struct {
    uint32_t uid;
    int32_t (*func)(Object);
} serviceUnit;

typedef struct {
    Object cred;
} storedCredUnit;

typedef struct {
    int32_t refs;
    Object credentials;
} TestService;

static storedCredUnit gCreds[MAX_STORED_CREDS];
// Points to most recently filled position
static int32_t gCredsCnt = -1;

static enum RemoteConnection {
    QTVM = 0,
    OEMVM,
} gRemoteConn = QTVM;

typedef struct {
    bool same;
    int32_t idx;
} thread_data;

typedef struct stressThreadData {
    uint32_t targetUid;
    int32_t ret;
    TestService *me;
    Object opener;
    Object *myProcLoaderObj;
    Object *myProcObj;
    Object serviceObj;
} stressThreadData;

/* clang-format off */
static uint32_t gMemoryFactoryUid[] = {
    CTMemPoolFactory_UID,
    CTOEMVMMemPoolFactory_UID
};

static uint32_t gPowerServiceUid[] = {
    CTPowerService_UID,
    CTOEMVMPowerService_UID
};

static uint32_t gPlatformInfoServiceUid[] = {
    CQTVMPlatformInfo_UID,
    COEMVMPlatformInfo_UID
};

static uint32_t gTAccessControlUid[] = {
    CTAccessControl_UID,
    CTOEMVMAccessControl_UID
};

static serviceUnit callSameServices[] = {
    {CEmbeddedStressTestService_UID, ITestService_printHello},
    {CEmbeddedStressTestService_UID, ITestService_printHello},
    {CEmbeddedStressTestService_UID, ITestService_printHello},
    {CEmbeddedStressTestService_UID, ITestService_printHello},
    {CEmbeddedStressTestService_UID, ITestService_printHello},
};

static serviceUnit callDiffServices[] = {
    {CEmbeddedAllPrivilegeTestService_UID, ITestService_printHello},
    {CEmbeddedCommonTestService_UID,       ITestService_printHello},
    {CEmbeddedStressTestService_UID,       ITestService_printHello},
    {CEmbeddedNormalDeathTestService_UID,  ITestService_printHello},
    {CEmbeddedSpareTestService_UID,        ITestService_printHello},
};

static const uint32_t gTrustedUid[] = {
    CAllPrivilegeTestService_UID,
    CCommonTestService_UID,
    CStressTestService_UID,
    CCredDealerTestService_UID,
    CSpareTestService_UID
};
/* clang-format on */

struct {
    char *name;
    uint32_t uid;
} gAppPair[] = {
    {"EmbeddedAllPrivilegeTestService", CEmbeddedAllPrivilegeTestService_UID},
    {"EmbeddedCommonTestService", CEmbeddedCommonTestService_UID},
    {"EmbeddedMissingNeverUnload", CEmbeddedMissingNeverUnload_UID},
    {"EmbeddedNormalDeathTestService", CEmbeddedNormalDeathTestService_UID},
    {"EmbeddedSpareTestService", CEmbeddedSpareTestService_UID},
    {"EmbeddedStressTestService", CEmbeddedStressTestService_UID},
    {"AllPrivilegeTestService", CAllPrivilegeTestService_UID},
    {"CommonTestService", CCommonTestService_UID},
    {"CredDealerTestService", CCredDealerTestService_UID},
    {"SpareTestService", CSpareTestService_UID},
    {"StressTestService", CStressTestService_UID},
};

static int32_t gAppIndex;

// ------------------------------------------------------------------------
// Global variable definitions
// ------------------------------------------------------------------------
static sem_t gShutdownLock;
#ifdef EMBEDDED_PROC
static int32_t gServiceCount;
#endif
static const char *gSharedMsg = "My shared message";
static Object gPowerServiceFactory = Object_NULL;
static Object gWakeLock = Object_NULL;
static bool gSynchronizeShutdown = false;
static Object gRegister = Object_NULL;
static Object gRegisterNotifyCB = Object_NULL;

int32_t tProcessOpen(uint32_t uid, Object cred, Object *objOut);
static int32_t CTestService_runMemSharingTest(TestService *me, uint32_t targetUid);
// ------------------------------------------------------------------------
// Methods
// ------------------------------------------------------------------------
/**
 * Description: The main function.
 *
 * In:          argc: The number of arguments.
 *              argv: The argument values.
 * Out:         void
 * Return:      0 on success.
 */
int32_t main(int32_t argc, char *argv[])
{
    int32_t ret = 0;
    Object serviceObj = Object_NULL;
    char vmuuidStr[VMUUID_MAX_SIZE * 2 + 1] = {0};
    Profiling_configProfile();

    if (sem_init(&gShutdownLock, 0, 0) != 0) {
        printf("Failed to initialize semaphore");
        return -1;
    }

    // Initialize structures or connections before service becomes available to
    // other processes.

    for (int32_t i = 0; i < sizeof(gAppPair) / sizeof(gAppPair[0]); ++i) {
        if (memcmp(argv[0], gAppPair[i].name, strlen(gAppPair[i].name)) == 0) {
            gAppIndex = i;
            break;
        }
    }

    LOG_MSG("Process name : %s : %s ", argv[0], gAppPair[gAppIndex].name);

#ifdef OFFTARGET
    if (gAppIndex == 4) {
        if (tProcessOpen(0, Object_NULL, &gRegister)) {
            LOG_MSG("Failed to generate gRegister");
            return -1;
        }

        for (int32_t i = 0; i < 10; ++i) {
            T_CALL_NO_CHECK(ret, ITEnv_open(gTVMEnv, CEmbeddedCommonTestService_UID, &serviceObj));
            if (Object_isOK(ret)) {
                T_CALL_NO_CHECK(ret, ITestService_registerNotifyCB(serviceObj, gRegister));
                break;
            } else {
                // EmbeddedSpareTestService may not add to TModule List and it will make
                // getLocalModule function failed. In this situation, it should add some delay for
                // waiting Mink to add EmbeddedSpareTestService to TModule list.
                usleep(50000);
            }

            LOG_MSG("Open CEmbeddedCommonTestService_UID Failed(%x)", ret);
        }

        LOG_MSG("Finish Load CEmbeddedCommonTestService_UID(%d).", ret);
    }
#endif

    // Decrement (lock) the semaphore. Put to sleep indefinitely.
    if (sem_wait(&gShutdownLock) != 0) {
        printf("Failed to wait on semaphore\n");
        return -1;
    }

    if (gSynchronizeShutdown) {
        // Wait for the synchronization signal
        LOG_MSG("Paused.... waiting for signal to resume.");
        usleep(10000);
        raise(SIGSTOP);
    }

exit:
    Object_ASSIGN_NULL(gWakeLock);
    Object_ASSIGN_NULL(gPowerServiceFactory);
    Object_ASSIGN_NULL(gRegister);
    Object_ASSIGN_NULL(serviceObj);
    Object_ASSIGN_NULL(gRegisterNotifyCB);
    LOG_MSG("work after SIGSTOP");

    return ret;
}

/**
 * Description: Release any remaining objects before process is killed.
 *
 * In:          void
 * Out:         void
 * Return:      void
 */
void tProcessShutdown(void)
{
    // Increment (unlock) semaphore. Allow main thread to complete.
    sem_post(&gShutdownLock);
    LOG_MSG("Posted on semaphore. Beginning process exit.");
}

static bool _isEmebddedSevice(uint32_t uid)
{
    for (int32_t i = 0; i < sizeof(gAppPair) / sizeof(gAppPair[0]); ++i) {
        if (gAppPair[i].uid == uid) {
            return true;
        }
    }

    return false;
}

static uint64_t _getAverage(const uint64_t arr[], uint32_t size)
{
    uint64_t sum = 0;
    if (size == 0) {
        return sum;
    }
    for (int i = 0; i < size; i++) {
        if (__builtin_add_overflow(sum, arr[i], &sum)) {
            LOG_MSG("overflow occured \n");
            return 0;
        }
    }
    return sum / size;
}

/**
 * Description: Get QTVM wakelock.
 *
 * Out: wakelock                    the wakelock object of qtvm
 * Return:                          Object_OK when it succeeds.
 */
static int32_t _getQTVMWakeLock(Object *wakeLock)
{
    int32_t ret = Object_OK;
    Object powerService = Object_NULL;

    T_GUARD(ITEnv_open(gTVMEnv, CTPowerService_UID, &powerService));
    T_GUARD(ITPowerService_acquireWakeLock(powerService, wakeLock));

exit:
    Object_ASSIGN_NULL(powerService);
    return ret;
}

static void *callServiceUnit(void *args)
{
    uint32_t ret = Object_OK;
    uint64_t rvalue = -1;
    thread_data *data = (thread_data *)args;
    int32_t idx = data->idx;
    uint32_t uid = 0;
    __servFunc func = NULL;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;
    serviceUnit *service_arr = data->same ? callSameServices : callDiffServices;

    uid = service_arr[idx].uid;
    func = service_arr[idx].func;

    LOG_MSG("subthread #%d: uid=%d,func=%p.", idx, uid, func);

    // When it tries to open embedded service which runs on QTVM, it will get the wakelock in order
    // that the QTVM won't fall into sleep.
    if (_isEmebddedSevice(uid)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }

    T_GUARD(ITEnv_open(gTVMEnv, uid, &testServiceObj));
    T_GUARD(func(testServiceObj));

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    rvalue = ret;
    return (void *)rvalue;
}

/* ########## Unit test for Local Memory Service in QTVM. ########## */
#define MAX_ENTRIES_FOR_MS_TEST 9
#define MAX_COUNT_AVAILABLE_HEAPS 8

Object ms_memPoolFactoryObj[MAX_ENTRIES_FOR_MS_TEST] = {Object_NULL};
Object ms_memPoolObj[MAX_ENTRIES_FOR_MS_TEST] = {Object_NULL};
Object ms_memObj[MAX_ENTRIES_FOR_MS_TEST] = {Object_NULL};

// This is the teardown function of each sub-case.
static void MSTest_Cleanup()
{
    int32_t idx;
    for (idx = 0; idx < MAX_ENTRIES_FOR_MS_TEST; idx++) {
        Object_ASSIGN_NULL(ms_memObj[idx]);
        Object_ASSIGN_NULL(ms_memPoolObj[idx]);
        Object_ASSIGN_NULL(ms_memPoolFactoryObj[idx]);
    }

    // Sleep 100 ms to give some time on completion of recycling in TVMMink.
    usleep(100 * 1000);
}

static int32_t MSTest_GetMemPool(const ITAccessPermissions_rules *confRulesPtr,
                                 Object *memPoolFactoryObj, Object *memPoolObj, uint64_t xMegaBytes)
{
    int32_t ret = Object_OK;
    uint64_t poolMemSize = xMegaBytes * 1024 * 1024;

    T_GUARD(ITEnv_open(gTVMEnv, gMemoryFactoryUid[gRemoteConn], memPoolFactoryObj));
    T_CALL(ITMemPoolFactory_createPool(*memPoolFactoryObj, confRulesPtr, poolMemSize, memPoolObj));

exit:
    return ret;
}

static int32_t MSTest_AllocateMemObj(Object memPoolObj, Object *memObj, uint64_t xMegaBytes)
{
    int32_t ret = Object_OK;
    uint64_t bufSize = xMegaBytes * 1024 * 1024;

    T_CALL(ITMemPool_allocateBuffer(memPoolObj, bufSize, memObj));

exit:
    return ret;
}

static int32_t MSTest_InitMemObj(Object *memObj, uint64_t xMegaBytes)
{
    uint64_t bufSize = xMegaBytes * 1024 * 1024;
    int32_t bufFd = -1, ret = Object_OK;
    void *ptr = NULL;

    // at test client side, memObj will be a fdwrapper object
    T_GUARD(Object_unwrapFd(*memObj, &bufFd));
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);
    memset(ptr, 0, bufSize);
    ret = munmap(ptr, bufSize);
    T_CHECK(0 == ret);

exit:
    return ret;
}

typedef struct memThreadData {
    Object memPoolObj;
    Object *memObj;
    uint32_t xMegaBytes;
    int32_t ret;
} memThreadData;

static void *MSTest_AllocateMemObj_Parallel(void *args)
{
    memThreadData *data = (memThreadData *)args;
    data->ret = MSTest_AllocateMemObj(data->memPoolObj, data->memObj, data->xMegaBytes);
    data->ret = MSTest_InitMemObj(data->memObj, data->xMegaBytes);
    return NULL;
}

typedef struct poolThreadData {
    uint32_t poolSize;
    Object *memPoolFactoryObj;
    Object *memPoolObj;
    int32_t ret;
    const ITAccessPermissions_rules *confRulesPtr;
} poolThreadData;

static void *MSTest_GetMemPool_Parallel(void *args)
{
    poolThreadData *data = (poolThreadData *)args;
    data->ret = MSTest_GetMemPool(data->confRulesPtr, data->memPoolFactoryObj, data->memPoolObj,
                                  data->poolSize);
    return NULL;
}

/**
 * @brief tprocess1 asks tprocess2 for a shared memory buffer which is actually from memory service.
 * we will check the validity of the shared memory buffer and
 * the consistency of shared message stored in the shared memory buffer.
 */
static int32_t _memSharing(TestService *me, uint32_t targetUid)
{
    int32_t ret = Object_OK;
    Object targetService = Object_NULL;
    Object wakeLock = Object_NULL;

    if (_isEmebddedSevice(targetUid)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }

    T_GUARD(ITEnv_open(gTVMEnv, targetUid, &targetService));
    T_GUARD(CTestService_runMemSharingTest(me, targetUid));

exit:
    Object_ASSIGN_NULL(targetService);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

/**
 * @brief helper function to launch the local memory sharing test in an individual thread using
 * pthread_create.
 *
 */
static void *_memSharingParallel(void *args)
{
    stressThreadData *data = (stressThreadData *)args;
    data->ret = _memSharing(data->me, data->targetUid);
    // FIXME: Object_ERROR_UNAVAIL says that retrying the operation again might result in success
    if (data->ret == Object_ERROR_UNAVAIL) {
        LOG_MSG("Retrying localMemSharing once due to Object_ERROR_UNAVAIL");
        data->ret = _memSharing(data->me, data->targetUid);
    }

    return NULL;
}

/**
 * @brief send SIGTERM to process and test to make sure it terminates and is no longer available
 *
 */
static int32_t _processAbnormalDeath(uint32_t uid)
{
    int32_t ret = Object_OK;
    Object testService = Object_NULL;
    Object wakeLock = Object_NULL;

    if (_isEmebddedSevice(uid)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }

    // embedded service now opened by TA
    T_GUARD(ITEnv_open(gTVMEnv, uid, &testService));

    T_GUARD(ITestService_printHello(testService));
    T_CALL_CHECK(ITestService_raiseSignal(testService, SIGTERM), ret == Object_ERROR_DEFUNCT);
    T_CALL_CHECK(ITestService_printHello(testService), ret == Object_ERROR_UNAVAIL);
    // If none of the four T_CALL macros jumps to exit, then we have passed this test
    ret = Object_OK;

exit:
    Object_ASSIGN_NULL(testService);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

/**
 * @brief helper function to launch the processAbnormalDeath test in an individual thread using
 * pthread_create.
 *
 */
static void *_processAbnormalDeathParallel(void *args)
{
    stressThreadData *data = (stressThreadData *)args;
    data->ret = _processAbnormalDeath(data->targetUid);
    // FIXME: Object_ERROR_UNAVAIL says that retrying the operation again might result in success
    if (data->ret == Object_ERROR_UNAVAIL) {
        LOG_MSG("Retrying processAbnormalDeath once due to Object_ERROR_UNAVAIL");
        data->ret = _processAbnormalDeath(data->targetUid);
    }

    return NULL;
}

static int32_t _embeddedNormalDeath(void)
{
    int32_t ret = Object_OK;
    Object testService = Object_NULL;
    Object wakeLock = Object_NULL;

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedNormalDeathTestService_UID, &testService));

    // A method could be failed here because a process could be dead accidentally anytime
    // so there is no need to check return value.
    ITestService_printHello(testService);

exit:
    // Kill the process and the others will not know it died until they call their printHello
    Object_ASSIGN_NULL(testService);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static void *_embeddedNormalDeathParallel(void *args)
{
    stressThreadData *data = (stressThreadData *)args;
    data->ret = _embeddedNormalDeath();
    // FIXME: Object_ERROR_UNAVAIL says that retrying the operation again might result in success
    if (data->ret == Object_ERROR_UNAVAIL) {
        LOG_MSG("Retrying embeddedNormalDeath once due to Object_ERROR_UNAVAIL");
        data->ret = _embeddedNormalDeath();
    }

    return NULL;
}

/**
 * @brief load one embedded process
 */
static int32_t _openEmbeddedService(uint32_t uid, Object *testService)
{
    int32_t ret = Object_OK;

    LOG_MSG("Attemping to open remote/local embedded service %x", uid);
    T_GUARD(ITEnv_open(gTVMEnv, uid, testService));
    T_GUARD(ITestService_printHello(*testService));

exit:
    return ret;
}

/**
 * @brief helper function to launch openEmbeddedService test in an individual thread using
 * pthread_create.
 *
 */
static void *_openEmbeddedServiceParallel(void *args)
{
    stressThreadData *data = (stressThreadData *)args;
    data->ret = _openEmbeddedService(data->targetUid, &data->serviceObj);
    // FIXME: Object_ERROR_UNAVAIL or Object_ERROR_DEFUNCT says that retrying the operation again
    // might result in success
    if (data->ret == Object_ERROR_UNAVAIL || data->ret == Object_ERROR_DEFUNCT) {
        LOG_MSG(
            "Retrying openEmbeddedService once due to Object_ERROR_UNAVAIL or "
            "Object_ERROR_DEFUNCT");
        data->ret = _openEmbeddedService(data->targetUid, &data->serviceObj);
    }

    return NULL;
}

static int32_t _callPowerService(uint32_t uid)
{
    int32_t ret = Object_OK;
    Object testService = Object_NULL;
    Object wakeLock = Object_NULL;

    if (_isEmebddedSevice(uid)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }

    T_GUARD(ITEnv_open(gTVMEnv, uid, &testService));
    T_GUARD(ITestService_wakeLockTest(testService, TEST_OPEN_POWER_SERVICE));
    T_GUARD(ITestService_wakeLockTest(testService, TEST_ACQUIRE_WAKE_LOCK));
    T_GUARD(ITestService_wakeLockTest(testService, TEST_CLOSE_POWER_SERVICE));

    srand(time(NULL));

    int32_t count = (rand() % 10) + 2;
    // mutiple get lock and release lock
    T_GUARD(ITestService_wakeLockTest(testService, TEST_OPEN_POWER_SERVICE));

    for (int32_t i = 0; i < count; ++i) {
        T_GUARD(ITestService_wakeLockTest(testService, TEST_ACQUIRE_WAKE_LOCK));
        T_GUARD(ITestService_wakeLockTest(testService, TEST_CLOSE_POWER_SERVICE));
    }

exit:
    Object_ASSIGN_NULL(testService);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static void *_localPowerServiceParallel(void *args)
{
    stressThreadData *data = (stressThreadData *)args;
    data->ret = _callPowerService(data->targetUid);
    return NULL;
}

/* ########## Positive Test Cases for Local Memory Service ########## */

/**
 * @brief Positive functional
 *
 * test client asks memory service for a shm obj
 *
 */
static int32_t PositiveAllocateMemObj(const ITAccessPermissions_rules *confRulesPtr)
{
    int32_t ret = Object_OK;

    T_GUARD(MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0], 10));
    for (int32_t idx = 0; idx < MAX_ENTRIES_FOR_MS_TEST; idx++) {
        T_GUARD(MSTest_AllocateMemObj(ms_memPoolObj[0], &ms_memObj[idx], 1));
        T_GUARD(MSTest_InitMemObj(&ms_memObj[idx], 1));
    }

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_PositiveAllocateMemObj()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(PositiveAllocateMemObj(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(PositiveAllocateMemObj(&confRules));

exit:

    return ret;
}

/**
 * @brief Positive functional
 *
 * test client asks memory service for a shm obj(requestedSize == poolSize)
 *
 */
static int32_t PositiveAllocateMemObjLimit(const ITAccessPermissions_rules *confRulesPtr)
{
    int32_t ret = Object_OK;

    // The pool size (in MB) is equal to MAX_ENTRIES_FOR_MS_TEST
    T_GUARD(MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0],
                              MAX_ENTRIES_FOR_MS_TEST));
    for (int32_t idx = 0; idx < MAX_ENTRIES_FOR_MS_TEST; idx++) {
        T_GUARD(MSTest_AllocateMemObj(ms_memPoolObj[0], &ms_memObj[idx], 1));
        T_GUARD(MSTest_InitMemObj(&ms_memObj[idx], 1));
    }

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_PositiveAllocateMemObjLimit()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(PositiveAllocateMemObjLimit(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(PositiveAllocateMemObjLimit(&confRules));

exit:

    return ret;
}

/**
 * @brief Positive functional
 *
 * test client asks memory service for multiple memory pools
 *
 */
static int32_t PositiveGetMultiMemPool(const ITAccessPermissions_rules *confRulesPtr)
{
    int32_t ret = Object_OK;

    for (int32_t idx = 0; idx < MAX_COUNT_AVAILABLE_HEAPS; idx++) {
        T_GUARD(
            MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[idx], &ms_memPoolObj[idx], 3));
    }

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_PositiveGetMultiMemPool()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(PositiveGetMultiMemPool(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(PositiveGetMultiMemPool(&confRules));

exit:

    return ret;
}

/**
 * @brief Positive functional
 *
 * test client asks memory service for a mem pool repetitively
 *
 */
static int32_t PositiveRequestOnePoolRepetitively(const ITAccessPermissions_rules *confRulesPtr)
{
    int32_t ret = Object_OK, maxTimes = 5;

    for (int32_t idx = 0; idx < maxTimes; idx++) {
        T_GUARD(MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0], 3));
        Object_ASSIGN_NULL(ms_memPoolObj[0]);
        Object_ASSIGN_NULL(ms_memPoolFactoryObj[0]);
    }

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_PositiveRequestOnePoolRepetitively()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(PositiveRequestOnePoolRepetitively(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(PositiveRequestOnePoolRepetitively(&confRules));

exit:

    return ret;
}

/**
 * @brief Positive functional
 *
 * Test lifecycles of memObj and memPool
 *
 */
static int32_t PositiveTestPoolCriticalCondition(const ITAccessPermissions_rules *confRulesPtr)
{
    uint32_t bufSize = 3 * 1024 * 1024;
    int32_t bufFd = -1, ret = Object_OK;
    void *ptr = NULL;

    T_GUARD(MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0], 5));
    T_GUARD(ITMemPool_allocateBuffer(ms_memPoolObj[0], bufSize, &ms_memObj[0]));

    // memPool is not expected to be freed cause there is an ALIVE mMemObj in it
    Object_ASSIGN_NULL(ms_memPoolObj[0]);
    Object_ASSIGN_NULL(ms_memPoolFactoryObj[0]);

    // although we TRIED to release memPool before,
    // the memObj should be still alive and valid logically
    T_GUARD(Object_unwrapFd(ms_memObj[0], &bufFd));
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);
    memset(ptr, 0, bufSize);
    ret = munmap(ptr, bufSize);
    T_CHECK(0 == ret);

    // now we let mMemObj[0] and mMemPoolObj[0] to be really released
    Object_ASSIGN_NULL(ms_memObj[0]);

    // we use SAME objects to get memPool and memObj
    T_GUARD(MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0], 5));
    T_GUARD(ITMemPool_allocateBuffer(ms_memPoolObj[0], bufSize, &ms_memObj[0]));

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_PositiveTestPoolCriticalCondition()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(PositiveTestPoolCriticalCondition(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(PositiveTestPoolCriticalCondition(&confRules));

exit:

    return ret;
}

/* ########## Negative Test Cases for Local Memory Service ########## */

/**
 * @brief Negative functional
 *
 * test client asks memory service for a shm obj (requestedSize > poolSize)
 *
 */
static int32_t NegativeAllocateMemObjTooLarge(const ITAccessPermissions_rules *confRulesPtr)
{
    uint32_t bufSize = 7 * 1024 * 1024;
    int32_t ret = Object_OK;

    T_GUARD(MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0], 5));
    T_CHECK(Object_ERROR_MEM == ITMemPool_allocateBuffer(ms_memPoolObj[0], bufSize, &ms_memObj[0]));

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_NegativeAllocateMemObjTooLarge()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(NegativeAllocateMemObjTooLarge(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(NegativeAllocateMemObjTooLarge(&confRules));

exit:

    return ret;
}

/**
 * @brief Negative functional
 *
 * test client asks memory service for a shm obj (lastTimeRequestedSize >
 * remainPoolSize)
 *
 */
static int32_t NegativeAllocateMemObjTooMuch(const ITAccessPermissions_rules *confRulesPtr)
{
    int32_t ret = Object_OK;
    uint32_t bufSize = 3 * 1024 * 1024;

    T_GUARD(MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0], 5));
    for (int32_t idx = 0; idx < 4; idx++) {
        T_GUARD(MSTest_AllocateMemObj(ms_memPoolObj[0], &ms_memObj[idx], 1));
        T_GUARD(MSTest_InitMemObj(&ms_memObj[idx], 1));
    }

    T_CHECK(Object_ERROR_MEM == ITMemPool_allocateBuffer(ms_memPoolObj[0], bufSize, &ms_memObj[4]));

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_NegativeAllocateMemObjTooMuch()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(NegativeAllocateMemObjTooMuch(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(NegativeAllocateMemObjTooMuch(&confRules));

exit:

    return ret;
}

/**
 * @brief Negative functional
 * test client asks memory service for too many mem pools(requestedPoolCount >
 * MAX_POOLS_COUNT)
 *
 */
static int32_t NegativeRequestTooManyPool(const ITAccessPermissions_rules *confRulesPtr)
{
    int32_t ret = Object_OK;
    uint32_t poolMemSize = 3 * 1024 * 1024;
    for (int32_t idx = 0; idx < MAX_COUNT_AVAILABLE_HEAPS; idx++) {
        T_GUARD(
            MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[idx], &ms_memPoolObj[idx], 3));
    }

    T_GUARD(ITEnv_open(gTVMEnv, gMemoryFactoryUid[gRemoteConn],
                       &ms_memPoolFactoryObj[MAX_COUNT_AVAILABLE_HEAPS]));
    T_CHECK(ITMemPoolFactory_ERROR_NO_AVAILABLE_HEAP ==
            ITMemPoolFactory_createPool(ms_memPoolFactoryObj[MAX_COUNT_AVAILABLE_HEAPS],
                                        confRulesPtr, poolMemSize,
                                        &ms_memPoolObj[MAX_COUNT_AVAILABLE_HEAPS]));

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_NegativeRequestTooManyPool()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(NegativeRequestTooManyPool(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(NegativeRequestTooManyPool(&confRules));

exit:

    return ret;
}

/**
 * @brief Negative functional
 * test client asks memory service for a mem pool with
 * specialRules=removeSelfAccess
 *
 */
static int32_t MSTest_NegativeRequestMemPoolWithInvalidSpecialRules()
{
    int32_t ret = Object_OK;
    uint32_t poolMemSize = 3 * 1024 * 1024;
    ITAccessPermissions_rules confRules = {0};
    confRules.specialRules = ITAccessPermissions_removeSelfAccess;

    T_GUARD(ITEnv_open(gTVMEnv, gMemoryFactoryUid[gRemoteConn], &ms_memPoolFactoryObj[0]));
    T_CHECK(ITMemPoolFactory_ERROR_INVALID_CONFINEMENT ==
            ITMemPoolFactory_createPool(ms_memPoolFactoryObj[0], &confRules, poolMemSize,
                                        &ms_memPoolObj[0]));

exit:
    MSTest_Cleanup();
    return ret;
}

/* ########## Stress Test Cases for Local Memory Service ########## */

/**
 * @brief Stress functional
 *
 * multiple test clients ask memory service for shm objs from ONE pool
 *
 */
static int32_t StressOnePoolAllocateMemObj(const ITAccessPermissions_rules *confRulesPtr)
{
    int32_t ret = Object_OK, idx = -1;
    int32_t maxTestThreadsCount = MAX_ENTRIES_FOR_MS_TEST;
    pthread_t client[MAX_ENTRIES_FOR_MS_TEST];
    memThreadData args[MAX_ENTRIES_FOR_MS_TEST];

    T_GUARD(MSTest_GetMemPool(confRulesPtr, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0], 10));
    for (idx = 0; idx < maxTestThreadsCount; idx++) {
        args[idx].memPoolObj = ms_memPoolObj[0];
        args[idx].memObj = &ms_memObj[idx];
        args[idx].xMegaBytes = 1;
        args[idx].ret = Object_ERROR;
        T_CHECK(0 ==
                pthread_create(&client[idx], NULL, MSTest_AllocateMemObj_Parallel, &args[idx]));
    }

    for (idx = 0; idx < maxTestThreadsCount; idx++) {
        T_CHECK(0 == pthread_join(client[idx], NULL));
        T_CHECK(Object_OK == args[idx].ret);
    }

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_StressOnePoolAllocateMemObj()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(StressOnePoolAllocateMemObj(&confRules));

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(StressOnePoolAllocateMemObj(&confRules));

exit:

    return ret;
}

/**
 * @brief Stress functional
 *
 * multiple test clients ask memory service for shm objs from different pools
 *
 */
static int32_t StressMultiPoolAllocateMemObj(const ITAccessPermissions_rules *confRulesPtr)
{
    int32_t ret = Object_OK, idx = -1;
    pthread_t memClient[MAX_COUNT_AVAILABLE_HEAPS];
    pthread_t poolClient[MAX_COUNT_AVAILABLE_HEAPS];
    memThreadData memArgs[MAX_COUNT_AVAILABLE_HEAPS];
    poolThreadData poolArgs[MAX_COUNT_AVAILABLE_HEAPS];

    for (idx = 0; idx < MAX_COUNT_AVAILABLE_HEAPS; idx++) {
        poolArgs[idx].poolSize = 3;
        poolArgs[idx].memPoolFactoryObj = &ms_memPoolFactoryObj[idx];
        poolArgs[idx].memPoolObj = &ms_memPoolObj[idx];
        poolArgs[idx].ret = Object_ERROR;
        poolArgs[idx].confRulesPtr = confRulesPtr;
        T_CHECK(0 ==
                pthread_create(&poolClient[idx], NULL, MSTest_GetMemPool_Parallel, &poolArgs[idx]));
    }

    for (idx = 0; idx < MAX_COUNT_AVAILABLE_HEAPS; idx++) {
        T_CHECK(0 == pthread_join(poolClient[idx], NULL));
        T_CHECK(Object_OK == poolArgs[idx].ret);
        T_CHECK(!Object_isNull(ms_memPoolObj[idx]));
    }

    for (idx = 0; idx < MAX_COUNT_AVAILABLE_HEAPS; idx++) {
        memArgs[idx].memPoolObj = ms_memPoolObj[idx];
        memArgs[idx].memObj = &ms_memObj[idx];
        memArgs[idx].xMegaBytes = 2;
        memArgs[idx].ret = Object_ERROR;
        T_CHECK(0 == pthread_create(&memClient[idx], NULL, MSTest_AllocateMemObj_Parallel,
                                    &memArgs[idx]));
    }

    for (idx = 0; idx < MAX_COUNT_AVAILABLE_HEAPS; idx++) {
        T_CHECK(0 == pthread_join(memClient[idx], NULL));
        T_CHECK(Object_OK == memArgs[idx].ret);
        T_CHECK(!Object_isNull(ms_memObj[idx]));
    }

exit:
    MSTest_Cleanup();
    return ret;
}

static int32_t MSTest_StressMultiPoolAllocateMemObj()
{
    int32_t ret = Object_OK;
    ITAccessPermissions_rules confRules = {0};

    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(StressMultiPoolAllocateMemObj(&confRules));

    usleep(500 * 1000); // Ensure all pools can be released

    confRules.specialRules = ITAccessPermissions_qcomDisplayDmabuf;
    T_GUARD(StressMultiPoolAllocateMemObj(&confRules));

exit:

    return ret;
}

// ############### TestService ###############
static int32_t CTestService_retain(TestService *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t CTestService_release(TestService *me)
{
    if (!me) {
        return Object_OK;
    }

    if (atomicAdd(&me->refs, -1) == 0) {
        Object_RELEASE_IF(me->credentials);
        HEAP_FREE_PTR(me);
#ifdef EMBEDDED_PROC
        // If no more service Object references exist, shutdown
        if (atomicAdd(&gServiceCount, -1) == 0) {
            T_TRACE(tProcessShutdown());
        }
#endif
    }

    return Object_OK;
}

static int32_t CTestService_QRTRConn(TestService *me, uint32_t TestType)
{
    int32_t ret = Object_ERROR;
    uint8_t buff[IAVMTestService_MAXSENDSTRINGLEN] = "asdgfggfdlkjiernafighn";
    uint8_t out_buff[IAVMTestService_MAXSENDSTRINGLEN] = {0};
    size_t buff_len = strlen(buff);
    size_t out_buff_len = 0;
    Object testQRTRServ = Object_NULL;

    if (TestType == ITestService_POSITIVETEST) {
        T_CALL(ITEnv_open(gTVMEnv, CAVMCommonTestService_UID, &testQRTRServ));
        T_CHECK(!Object_isNull(testQRTRServ));
        T_CALL(IAVMTestService_open(testQRTRServ));
    } else if (TestType == ITestService_NEGATIVETEST) {
        T_CALL(ITEnv_open(gTVMEnv, NOT_USED_TEST_UID, &testQRTRServ));
        T_CHECK(!Object_isNull(testQRTRServ));
        T_CALL(IAVMTestService_open(testQRTRServ));
    } else if (TestType == ITestService_STRESSTEST) {
        T_CALL(ITEnv_open(gTVMEnv, CAVMStressTestService_UID, &testQRTRServ));
        T_CHECK(!Object_isNull(testQRTRServ));
        T_CALL(IAVMTestService_QRTRSendBuff(testQRTRServ, buff, buff_len, out_buff,
                                            IAVMTestService_MAXSENDSTRINGLEN, &out_buff_len));
        T_CHECK(0 == strcmp(buff, out_buff));
    }
    ret = Object_OK;

exit:
    Object_ASSIGN_NULL(testQRTRServ);
    return ret;
}

static int32_t CTestService_printHello(TestService *me)
{
    LOG_MSG("%s Hello!", gAppPair[gAppIndex].name);
    return Object_OK;
}

static int32_t CTestService_wakeLockTest(TestService *me, uint32_t cmdId)
{
    int32_t ret = Object_OK;
    switch (cmdId) {
        case TEST_OPEN_POWER_SERVICE:
            if (Object_isNull(gPowerServiceFactory)) {
                T_CALL(ITEnv_open(gTVMEnv, gPowerServiceUid[gRemoteConn], &gPowerServiceFactory));
            }
            break;
        case TEST_ACQUIRE_WAKE_LOCK:
            T_CHECK(!Object_isNull(gPowerServiceFactory));
            if (Object_isNull(gWakeLock)) {
                T_CALL(ITPowerService_acquireWakeLock(gPowerServiceFactory, &gWakeLock));
            }
            break;
        case TEST_CLOSE_POWER_SERVICE:
            Object_ASSIGN_NULL(gWakeLock);
            break;
        default:
            ret = Object_ERROR;
            break;
    }

exit:
    return ret;
}

static int32_t CTestService_raiseSignal(TestService *me, int32_t sigNum)
{
    uint32_t ret = Object_OK;

    T_CALL(raise(sigNum));

exit:
    return ret;
}

static int32_t CTestService_shutdown(TestService *me, int8_t bWaitForSIGCONT)
{
    gSynchronizeShutdown = (bWaitForSIGCONT == 0) ? false : true;

    tProcessShutdown();
    return Object_OK;
}

static int32_t CTestService_backtracetest(TestService *me, uint32_t sigNum)
{
    const static uint8_t illegalFunctionAddr[] = {0xff, 0xff, 0xff, 0xff};
    char *p = NULL;
    Object object;
    __servFunc func = NULL;

    LOG_MSG("Raise Signal %d:%s\n", sigNum, strsignal(sigNum));
    switch (sigNum) {
        case SIGABRT:
            abort();
            break;
        case SIGSEGV:
            raise(SIGSEGV);
            abort();
            break;
        case SIGFPE:
            raise(SIGFPE);
            // Because using raise function to generate the SIGFPE signal won't make service die,
            // here will call abort function to make it die.
            abort();
            break;
        case SIGILL:
            func = (__servFunc)illegalFunctionAddr;
            func(object);
            break;
        default:
            break;
    }

    return Object_OK;
}

static int32_t CTestService_callService(TestService *me, uint32_t targetUid)
{
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;
    int32_t ret = -1;

    LOG_MSG("Calling another service!");

    if (_isEmebddedSevice(targetUid)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }

    // get service object
    T_GUARD(ITEnv_open(gTVMEnv, targetUid, &testServiceObj));
    // call service method
    T_GUARD(ITestService_printHello(testServiceObj));

exit:
    // release service object
    T_TRACE(Object_ASSIGN_NULL(testServiceObj));
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static inline int32_t _parallelCallXService(bool same)
{
    int32_t ret = -1, i;
    pthread_t mthread[MAX_TEST_PARALLELISM];
    void *status[MAX_TEST_PARALLELISM];
    int32_t threadsRC[MAX_TEST_PARALLELISM], threads_passed = 0;
    int32_t parallelism = sizeof(callSameServices) / sizeof(serviceUnit);
    thread_data args[MAX_TEST_PARALLELISM];

    LOG_MSG("parallelism=%d.", parallelism);

    for (i = 0; i < parallelism; i++) {
        args[i].same = same;
        args[i].idx = i;
        threadsRC[i] = pthread_create(&mthread[i], NULL, callServiceUnit, &args[i]);
        assert(0 == threadsRC[i]);
    }

    for (i = 0; i < parallelism; i++) {
        threadsRC[i] = pthread_join(mthread[i], &status[i]);
        assert(0 == threadsRC[i]);
        if (Object_isOK(status[i])) {
            threads_passed++;
        }
    }

exit:
    LOG_MSG("threads_passed=%d,parallelism=%d.", threads_passed, parallelism);
    return (threads_passed == parallelism) ? Object_OK : Object_ERROR;
}

static int32_t CTestService_parallelCallSameService(TestService *me)
{
    return _parallelCallXService(true);
}

static int32_t CTestService_parallelCallDiffService(TestService *me)
{
    return _parallelCallXService(false);
}

/* Open the CredDealer service, which will add a reference to *this* processes
 * TModule in the CredDealer's global list of credentials. */
static int32_t CTestService_appendCredentials(TestService *me)
{
    Object credDealerObj = Object_NULL;
    int32_t ret = Object_OK;

    LOG_MSG("Start to get credDealerServiceObj...");
    T_GUARD(ITEnv_open(gTVMEnv, CCredDealerTestService_UID, &credDealerObj));
    LOG_MSG("Got credDealerServiceObj!");

exit:
    T_TRACE(Object_ASSIGN_NULL(credDealerObj));
    return ret;
}

/* To check that the credentials are still valid, we simply need to query them
 * without error. */
static int32_t CTestService_checkCredentials(TestService *me)
{
    int32_t ret = Object_ERROR;
    int8_t pid[16] = {0};
    size_t lenOut;
    Object credentials = gCreds[gCredsCnt].cred;

    T_CHECK(!Object_isNull(credentials));
    LOG_MSG("Checking gCreds[%d]", gCredsCnt);

    T_CALL(
        ICredentials_getValueByName(credentials, "pid", strlen("pid"), &pid, sizeof(pid), &lenOut));

    // TODO: add a self-check here. Ideally, it would gather information directly
    // from TA2 during
    // ITestService_appendCredentials and store it in the credDealer and then after
    // TA2 is killed,
    // the same info could be queried from its credentials to show that the
    // remaining credentials do
    // actually belong to the recently deceased TP.

exit:
    return ret;
}

static int32_t CTestService_dequeueBuffer(TestService *me, uint32_t bufSize, Object *objOut_ptr)
{
    int32_t ret = Object_OK;
    Object mMemPoolFactoryObj = Object_NULL;
    Object memPoolObj = Object_NULL;
    Object memObj = Object_NULL;
    uint32_t poolMemSize = 10 * 1024 * 1024;  // 10MB
    int32_t bufFd = -1;
    void *ptr = NULL;
    ITAccessPermissions_rules confRules = {0};
    confRules.specialRules = ITAccessPermissions_keepSelfAccess;

    T_GUARD(ITEnv_open(gTVMEnv, gMemoryFactoryUid[gRemoteConn], &mMemPoolFactoryObj));
    T_GUARD(ITMemPoolFactory_createPool(mMemPoolFactoryObj, &confRules, poolMemSize, &memPoolObj));
    T_GUARD(ITMemPool_allocateBuffer(memPoolObj, bufSize, &memObj));

    T_GUARD(Object_unwrapFd(memObj, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);

    memset(ptr, 0, bufSize);
    memscpy(ptr, strlen(gSharedMsg), gSharedMsg, strlen(gSharedMsg));
    T_GUARD(munmap(ptr, bufSize));

    Object_INIT(*objOut_ptr, memObj);

exit:
    Object_ASSIGN_NULL(memObj);
    Object_ASSIGN_NULL(memPoolObj);
    Object_ASSIGN_NULL(mMemPoolFactoryObj);
    return ret;
}

static int32_t CTestService_testLocalMemoryService(TestService *me)
{
    int32_t ret = Object_OK;

    LOG_MSG("Now ready to run positive test cases of local memory service...");
    T_CALL(MSTest_PositiveAllocateMemObj());
    T_CALL(MSTest_PositiveAllocateMemObjLimit());
    T_CALL(MSTest_PositiveGetMultiMemPool());
    T_CALL(MSTest_PositiveRequestOnePoolRepetitively());
    T_CALL(MSTest_PositiveTestPoolCriticalCondition());
    LOG_MSG("All positive test cases of local memory service pass.");

    LOG_MSG("Now ready to run negative test cases of local memory service...");
    T_CALL(MSTest_NegativeAllocateMemObjTooLarge());
    T_CALL(MSTest_NegativeAllocateMemObjTooMuch());
    T_CALL(MSTest_NegativeRequestTooManyPool());
    T_CALL(MSTest_NegativeRequestMemPoolWithInvalidSpecialRules());
    LOG_MSG("All negative test cases of local memory service pass.");

    LOG_MSG("Now ready to run stress test cases of local memory service...");
    T_CALL(MSTest_StressOnePoolAllocateMemObj());
    T_CALL(MSTest_StressMultiPoolAllocateMemObj());
    LOG_MSG("All stress test cases of local memory service pass.");

exit:
    return ret;
}

static int32_t CTestService_reclaimBufferTest(TestService *me)
{
    int32_t ret = Object_OK, bufFd = -1;
    uint32_t poolMemSize = 10 * 1024 * 1024;  // 10MB
    size_t bufSize = 8 * 1024 * 1024;         // 8MB
    void *ptr = NULL;
    Object mMemPoolFactoryObj = Object_NULL;
    Object memPoolObj = Object_NULL;
    Object memObj = Object_NULL;
    ITAccessPermissions_rules confRules = {0};
    confRules.specialRules = ITAccessPermissions_keepSelfAccess;

    T_GUARD(ITEnv_open(gTVMEnv, gMemoryFactoryUid[gRemoteConn], &mMemPoolFactoryObj));
    T_GUARD(ITMemPoolFactory_createPool(mMemPoolFactoryObj, &confRules, poolMemSize, &memPoolObj));
    T_GUARD(ITMemPool_allocateBuffer(memPoolObj, bufSize, &memObj));

    // Basic usage of the allocated buffer
    T_GUARD(Object_unwrapFd(memObj, &bufFd));
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);

    memset(ptr, 0, bufSize);
    memscpy(ptr, strlen(gSharedMsg), gSharedMsg, strlen(gSharedMsg));
    T_GUARD(munmap(ptr, bufSize));

    // We aim to release the buffer but releasing is asynchronous by now
    Object_ASSIGN_NULL(memObj);

    // TODO: let releasing be synchronous
    // Work-around: waiting for completion of releasing buffer
    usleep(30 * 1000);

    T_GUARD(ITMemPool_allocateBuffer(memPoolObj, bufSize, &memObj));

exit:
    Object_ASSIGN_NULL(memObj);
    Object_ASSIGN_NULL(memPoolObj);
    Object_ASSIGN_NULL(mMemPoolFactoryObj);
    return ret;
}

static int32_t CTestService_enqueueBuffer(TestService *me, uint32_t bufSize, Object memObj)
{
    int32_t bufFd = -1;
    int32_t ret = Object_OK;
    void *ptr;

    T_CHECK(!Object_isNull(memObj));
    T_GUARD(Object_unwrapFd(memObj, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);

    T_GUARD(munmap(ptr, bufSize));

exit:
    return ret;
}

static int32_t CTestService_runMemSharingTest(TestService *me, uint32_t targetUid)
{
    int32_t ret = Object_OK;
    Object memObj = Object_NULL;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;
    uint32_t bufSize = 2 * 1024 * 1024;  // 2MB
    int32_t bufFd = -1;
    void *ptr = NULL;

    if (_isEmebddedSevice(targetUid)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }

    // get service object
    T_GUARD(ITEnv_open(gTVMEnv, targetUid, &testServiceObj));
    // call service method
    T_GUARD(ITestService_dequeueBuffer(testServiceObj, bufSize, &memObj));

    T_GUARD(Object_unwrapFd(memObj, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);

    // See the messages match
    T_CHECK(0 == memcmp(ptr, gSharedMsg, strlen(gSharedMsg)));

    T_GUARD(munmap(ptr, bufSize));

    // call service method
    T_CALL(ITestService_enqueueBuffer(testServiceObj, bufSize, memObj));

exit:
    Object_ASSIGN_NULL(memObj);
    // release service object
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_shareMemoryBuffer(TestService *me, uint32_t blockedMS, uint32_t bufSize,
                                              Object memObj)
{
    int32_t ret = Object_OK, bufFd = -1;
    void *ptr = NULL;

    T_GUARD(Object_unwrapFd(memObj, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);

    T_CHECK(0 == memcmp(ptr, gSharedMsg, strlen(gSharedMsg)));

    if (blockedMS > 0) {
        LOG_MSG("Ready to sleep %d ms...", blockedMS);
        usleep(blockedMS * 1000);
        LOG_MSG("Sleeped %d ms.", blockedMS);
    }

    T_GUARD(munmap(ptr, bufSize));

exit:
    return ret;
}

static int32_t CTestService_furtherShareMemoryBuffer(TestService *me, uint32_t bufSize,
                                                     uint32_t targetUid, Object memObj)
{
    int32_t ret = Object_OK, bufFd = -1;
    void *ptr = NULL;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    T_GUARD(Object_unwrapFd(memObj, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);

    memscpy(ptr, strlen(gSharedMsg), gSharedMsg, strlen(gSharedMsg));
    T_GUARD(munmap(ptr, bufSize));

    if (_isEmebddedSevice(targetUid)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }

    T_GUARD(ITEnv_open(gTVMEnv, targetUid, &testServiceObj));
    T_GUARD(ITestService_shareMemoryBuffer(testServiceObj, 0, bufSize, memObj));

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_lendMemoryBufferNoUnmap(TestService *me, uint32_t blockedMS,
                                                    uint32_t bufSize, Object memObj)
{
    int32_t ret = Object_OK, bufFd = -1;
    void *ptr = NULL;

    T_GUARD(Object_unwrapFd(memObj, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);

    if (blockedMS > 0) {
        LOG_MSG("Ready to sleep %d ms...", blockedMS);
        usleep(blockedMS * 1000);
        LOG_MSG("Sleeped %d ms.", blockedMS);
    }

exit:
    return ret;
}

/** Note: CTestService_registerNotifyCB and CTestService_callNotifyCB functions are used to test
 *        auto-start feature. When TVMMink runs EmbeddedSpareTestService, EmbeddedSpareTestService
 *        will call CTestService_callNotifyCB to register itself to CEmbeddedCommonTestService.
 *        After that, we can open CEmbeddedCommonTestService and call CTestService_callNotifyCB to
 *        run CTestService_printHello of EmbeddedSpareTestService. If it pass, we can verify
 *        auto-start EmbeddedSpareTestService. This is the same thing that CTCDriverCBService does
 *        in its main function.
 *
 */
/**
 * Description: Register the service object. The EmbeddedSpareTestService will call it to register
 *              itself to CEmbeddedCommonTestService. It uses to support CTestService_callNotifyCB
 *
 * In:          object:    register service(EmbeddedSpareTestService service object).
 * Return:      Object_OK on success.
 */
static int32_t CTestService_registerNotifyCB(TestService *me, Object obj)
{
    LOG_MSG("Calling CTestService_registerNotifyCB.");
    Object_ASSIGN(gRegisterNotifyCB, obj);
    return Object_OK;
}

/**
 * Description: Call the CTestService_printHello of gRegisterNotifyCB
 *              when we have register the service object.
 *
 * Return:      Object_OK on success.
 */
static int32_t CTestService_callNotifyCB(TestService *me)
{
    LOG_MSG("Calling CTestService_callNotifyCB.");

    if (Object_isNull(gRegisterNotifyCB)) {
        LOG_MSG("Not Find gRegisterNotifyCB!");
        return Object_ERROR;
    }

    return ITestService_printHello(gRegisterNotifyCB);
}

static int32_t CTestService_queryCred(TestService *me, Object *credential_out)
{
    int32_t ret = Object_ERROR;

    if (!Object_isNull(me->credentials)) {
        Object_ASSIGN(*credential_out, me->credentials);
        ret = Object_OK;
    }

    LOG_MSG("CTestService_queryCred returned: %d!", ret);
    return ret;
}

static void _logBytesAsHex(const char *label, uint8_t *data, size_t len)
{
    char buf[200] = {0};
    int32_t pos = 0;

    for (size_t i = 0; i < len; i++) {
        pos += snprintf(&buf[pos], sizeof(buf) - pos - 1, "%02X", data[i]);
    }

    LOG_MSG("%s : %s\n", label, buf);
}

static void _logString(const char *label, char *data)
{
    LOG_MSG("%s : %s\n", label, data);
}

static int32_t _checkProcessCredentials(Object cred)
{
    int32_t ret = Object_OK;
    uint8_t data[100] = {0};
    size_t lenOut = 0;

    T_GUARD(ICredentials_getValueByName(cred, "n", strlen("n"), data, sizeof(data), &lenOut));
    _logString("appName", (char *)data);

    T_GUARD(ICredentials_getValueByName(cred, "apid", strlen("apid"), data, sizeof(data), &lenOut));
    _logBytesAsHex("appId", data, lenOut);

    T_GUARD(ICredentials_getValueByName(cred, "uid", strlen("uid"), data, sizeof(data), &lenOut));
    _logBytesAsHex("uid", data, lenOut);

    T_GUARD(ICredentials_getValueByName(cred, "pid", strlen("pid"), data, sizeof(data), &lenOut));
    _logBytesAsHex("pid", data, lenOut);

    T_GUARD(ICredentials_getValueByName(cred, "ahsh", strlen("ahsh"), data, sizeof(data), &lenOut));
    _logBytesAsHex("appHash", data, lenOut);

    T_GUARD(ICredentials_getValueByName(cred, "did", strlen("did"), data, sizeof(data), &lenOut));
    _logBytesAsHex("distId", data, lenOut);

    T_GUARD(ICredentials_getValueByName(cred, "dn", strlen("dn"), data, sizeof(data), &lenOut));
    _logString("distName", (char *)data);

    T_GUARD(ICredentials_getValueByName(cred, "dmn", strlen("dmn"), data, sizeof(data), &lenOut));
    _logString("domain", (char *)data);

exit:
    return ret;
}

static int32_t _checkEnvCredentials(Object cred, bool isPlatformInfo)
{
    int32_t ret = Object_OK;
    uint8_t data[100] = {0};
    uint32_t version = 0;
    size_t lenOut = 0;

    T_GUARD(ICredentials_getValueByName(cred, "eid", strlen("eid"), data, sizeof(data), &lenOut));
    _logBytesAsHex("vmuuid", data, lenOut);

    T_GUARD(ICredentials_getValueByName(cred, "edmn", strlen("edmn"), data, sizeof(data), &lenOut));
    _logBytesAsHex("EEDomain", data, lenOut);

    T_GUARD(ICredentials_getValueByName(cred, "edid", strlen("edid"), data, sizeof(data), &lenOut));
    _logBytesAsHex("EEDistinguishedId", data, lenOut);

    T_GUARD(ICredentials_getValueByName(cred, "edn", strlen("edn"), data, sizeof(data), &lenOut));
    _logString("EEDistinguishedName", (char *)data);

    if (isPlatformInfo) {
        T_GUARD(ICredentials_getValueByName(cred, "ever", strlen("ever"), &version,
                                            sizeof(version), &lenOut));
        T_CHECK(version == QTVM_PLATFORM_VERSION);
        LOG_MSG("ever : %ld, major: %d, minor %d, patch: %d\n", version, version >> 28,
                (version >> 16) & 0xFFF, version & 0xFFFF);
    }

exit:
    return ret;
}

static int32_t CTestService_queryTACred(TestService *me, uint32_t uid)
{
    int32_t ret = Object_ERROR;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;
    Object credentialObj = Object_NULL;

    if (_isEmebddedSevice(uid)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }

    T_CALL(ITEnv_open(gTVMEnv, uid, &testServiceObj));
    T_CALL(ITestService_queryCred(testServiceObj, &credentialObj));

    T_GUARD(_checkEnvCredentials(credentialObj, false));
    T_GUARD(_checkProcessCredentials(credentialObj));

exit:
    Object_ASSIGN_NULL(credentialObj);
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_getAppObject(TestService *me, const char *name, size_t nameLen)
{
    int32_t ret = Object_OK;
    Object serviceObject = Object_NULL;
    Object appObj = Object_NULL;

    T_GUARD(ITEnv_open(gTVMEnv, CAppClient_UID, &serviceObject));
    // Get the appObj from my TP
    T_GUARD(IAppClient_getAppObject(serviceObject, name, nameLen, &appObj))

exit:
    Object_ASSIGN_NULL(appObj);
    Object_ASSIGN_NULL(serviceObject);
    return ret;
}

static int32_t CTestService_deriveKey(TestService *me, uint8_t *deriveKey, size_t deriveKeyLen,
                                      size_t *deriveKeyLenOut)
{
    int32_t ret = Object_OK;
    Object serviceObject = Object_NULL;

    T_GUARD(ITEnv_open(gTVMEnv, CVMDeviceUniqueKey_UID, &serviceObject));
    T_GUARD(IVMDeviceUniqueKey_derive(serviceObject, deriveKey, deriveKeyLen, deriveKeyLenOut));

exit:
    Object_ASSIGN_NULL(serviceObject);
    return ret;
}

static int32_t CTestService_positiveOpenEmbeddedService(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling positiveOpenEmbeddedService test. Uid=%X",
            CEmbeddedAllPrivilegeTestService_UID);

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedAllPrivilegeTestService_UID, &testServiceObj));
    T_GUARD(ITestService_printHello(testServiceObj));

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_positiveMemSharing(TestService *me, uint32_t targetUid)
{
    int32_t ret = Object_OK;

    LOG_MSG("Calling positiveMemSharing test");
    T_GUARD(_memSharing(me, targetUid));

exit:
    return ret;
}

static int32_t CTestService_positiveTestMemoryService(TestService *me, uint32_t targetUid)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling positiveTestMemoryService test");
    if (targetUid != gAppPair[gAppIndex].uid) {
        if (_isEmebddedSevice(targetUid)) {
            T_GUARD(_getQTVMWakeLock(&wakeLock));
        }

        T_GUARD(ITEnv_open(gTVMEnv, targetUid, &testServiceObj));
        T_GUARD(ITestService_testLocalMemoryService(testServiceObj));
    } else {
        T_GUARD(CTestService_testLocalMemoryService(me));
    }

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_positiveEmbeddedCallEmbedded(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling positiveEmbeddedCallEmbedded test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    // open embedded process
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedAllPrivilegeTestService_UID, &testServiceObj));
    // now have this embedded process load another embedded process
    T_GUARD(ITestService_callService(testServiceObj, CEmbeddedCommonTestService_UID));

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_positiveProcessAbnormalDeath(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling positiveProcessAbnormalDeath test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    // open embedded process
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedNormalDeathTestService_UID, &testServiceObj));
    // run the process abnormal death test
    T_GUARD(ITestService_printHello(testServiceObj));
    // using T_CALL_CHECK because we expect specific error codes
    T_CALL_CHECK(ITestService_raiseSignal(testServiceObj, SIGTERM), ret == Object_ERROR_DEFUNCT);
    T_CALL_CHECK(ITestService_printHello(testServiceObj), ret == Object_ERROR_UNAVAIL);
    // If we arrive at this point then the test has passsed
    ret = Object_OK;

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_positiveReclaimBufferTest(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;

    LOG_MSG("Calling positiveReclaimBufferTest test");

    if (gRemoteConn == OEMVM) {
        T_GUARD(CTestService_reclaimBufferTest(me));
    } else {
        // open embedded process
        T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedCommonTestService_UID, &testServiceObj));
        // now run ITestService_reclaimBufferTest
        T_GUARD(ITestService_reclaimBufferTest(testServiceObj));
    }

exit:
    Object_ASSIGN_NULL(testServiceObj);
    return ret;
}

static int32_t CTestService_positiveEmbeddedServiceWakeLockTest(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling positiveEmbeddedServiceWakeLockTest test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    // open embedded process
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedAllPrivilegeTestService_UID, &testServiceObj));
    // now run ITestService_wakeLockTest
    T_GUARD(ITestService_wakeLockTest(testServiceObj, TEST_OPEN_POWER_SERVICE));
    T_GUARD(ITestService_wakeLockTest(testServiceObj, TEST_ACQUIRE_WAKE_LOCK));
    T_GUARD(ITestService_wakeLockTest(testServiceObj, TEST_CLOSE_POWER_SERVICE));

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_positiveQTVMWakeLockTest(TestService *me)
{
    int32_t ret = Object_OK;
    Object powerServiceFactory = Object_NULL;
    Object wakeLock = Object_NULL;

    T_CALL(ITEnv_open(gTVMEnv, CTPowerService_UID, &powerServiceFactory));
    T_CALL(ITPowerService_acquireWakeLock(powerServiceFactory, &wakeLock));

exit:
    Object_ASSIGN_NULL(powerServiceFactory);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_positiveMultipleWakeLockTest(TestService *me)
{
    int32_t ret = Object_OK;
    Object testService[2] = {Object_NULL, Object_NULL};
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling positiveMultipleWakeLockTest test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    // open embedded process
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedAllPrivilegeTestService_UID, &testService[0]));
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedCommonTestService_UID, &testService[1]));

    // now run ITestService_wakeLockTest
    T_GUARD(ITestService_wakeLockTest(testService[0], TEST_OPEN_POWER_SERVICE));
    T_GUARD(ITestService_wakeLockTest(testService[1], TEST_OPEN_POWER_SERVICE));

    T_GUARD(ITestService_wakeLockTest(testService[0], TEST_ACQUIRE_WAKE_LOCK));
    T_GUARD(ITestService_wakeLockTest(testService[1], TEST_ACQUIRE_WAKE_LOCK));

    T_GUARD(ITestService_wakeLockTest(testService[0], TEST_CLOSE_POWER_SERVICE));
    T_GUARD(ITestService_wakeLockTest(testService[1], TEST_CLOSE_POWER_SERVICE));

exit:
    Object_ASSIGN_NULL(testService[0]);
    Object_ASSIGN_NULL(testService[1]);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_positiveAutoStartCoreService(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling positiveAutoStartCoreService test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    // open embedded process
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedCommonTestService_UID, &testServiceObj));

    // now run ITestService_callNotifyCB
    T_GUARD(ITestService_callNotifyCB(testServiceObj));

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_positiveRestartEmbeddedAutoStartService(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj[2] = {Object_NULL, Object_NULL};
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling positiveRestartEmbeddedAutoStartService test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    // open embedded process
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedCommonTestService_UID, &testServiceObj[0]));
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedSpareTestService_UID, &testServiceObj[1]));

    // run the process abnormal death test
    T_GUARD(ITestService_printHello(testServiceObj[1]));
    // using T_CALL_CHECK because we expect specific error codes
    T_CALL_CHECK(ITestService_raiseSignal(testServiceObj[1], SIGTERM), ret == Object_ERROR_DEFUNCT);
    T_CALL_CHECK(ITestService_printHello(testServiceObj[1]), ret == Object_ERROR_UNAVAIL);
    Object_ASSIGN_NULL(testServiceObj[1]);

    sleep(2);
    // now run ITestService_wakeLockTest
    T_GUARD(ITestService_callNotifyCB(testServiceObj[0]));

exit:
    Object_ASSIGN_NULL(testServiceObj[0]);
    Object_ASSIGN_NULL(testServiceObj[1]);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_negativeOpenServiceNotPrivileged(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling negativeOpenServiceNotPrivileged test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    // open embedded process
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedCommonTestService_UID, &testServiceObj));
    // now run ITestService_wakeLockTest
    T_CALL_CHECK(ITestService_callService(testServiceObj, CEmbeddedAllPrivilegeTestService_UID),
                 ret == ITEnv_ERROR_PRIVILEGE);

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_negativeEmbeddedMissingNeverUnload(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj[2] = {Object_NULL, Object_NULL};
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling negativeEmbeddedMissingNeverUnload test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedAllPrivilegeTestService_UID, &testServiceObj[0]));
    // now have this embedded process load another embedded process
    T_GUARD(ITestService_callService(testServiceObj[0], CEmbeddedMissingNeverUnload_UID));

exit:
    // Tear down the now-launched process by opening a service and releasing it.
    ITEnv_open(gTVMEnv, CEmbeddedSpareTestService_UID, &testServiceObj[1]);
    Object_ASSIGN_NULL(testServiceObj[0]);
    Object_ASSIGN_NULL(testServiceObj[1]);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_negativeEmbeddedRetryLimit(TestService *me)
{
    int32_t ret = Object_OK;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling negativeEmbeddedRetryLimit test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    // FIXME: remove teardown part and place CEmbeddedWrongTestService_UID in the ITEnv_open call
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedWrongTestService_UID, &testServiceObj));

exit:
    Object_ASSIGN_NULL(testServiceObj);
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_stressMultiPoolsSameTAsMemSharing(TestService *me, uint32_t uid)
{
    int32_t ret = Object_OK;
    const int32_t MAX_POOLS_COUNT = 2;  // leave some pools available to minkDaemon
    pthread_t client[MAX_POOLS_COUNT];
    stressThreadData args[MAX_POOLS_COUNT];

    LOG_MSG("Calling stressMultiPoolsSameTAsLocalMemSharing test");

    memset(args, 0, sizeof(args));

    for (int32_t n = 0; n < MAX_POOLS_COUNT; n++) {
        args[n].me = me;
        args[n].targetUid = uid;
        T_CHECK(0 == pthread_create(&client[n], NULL, _memSharingParallel, &args[n]));
    }

    for (int32_t n = 0; n < MAX_POOLS_COUNT; n++) {
        T_CHECK(0 == pthread_join(client[n], NULL));
        T_CHECK(Object_OK == args[n].ret);
    }

exit:
    return ret;
}

static int32_t CTestService_stressMultiPoolsDiffTAsMemSharing(TestService *me,
                                                              const uint8_t *uidListPtr,
                                                              size_t uidListLen)
{
    int32_t ret = Object_OK;
    const int32_t MAX_POOLS_COUNT = 2;  // leave some pools available to minkDaemon
    pthread_t client[MAX_POOLS_COUNT];
    stressThreadData args[MAX_POOLS_COUNT];
    const uint32_t *uid = (const uint32_t *)uidListPtr;

    LOG_MSG("Calling stressMultiPoolsDiffTAsLocalMemSharing test");

    T_CHECK(uidListPtr != NULL && uidListLen == (sizeof(uint32_t) * MAX_POOLS_COUNT));

    memset(args, 0, sizeof(args));

    for (int32_t n = 0; n < MAX_POOLS_COUNT; n++) {
        args[n].me = me;
        args[n].targetUid = uid[n];
        T_CHECK(0 == pthread_create(&client[n], NULL, _memSharingParallel, &args[n]));
    }

    for (int32_t n = 0; n < MAX_POOLS_COUNT; n++) {
        T_CHECK(0 == pthread_join(client[n], NULL));
        T_CHECK(Object_OK == args[n].ret);
    }

exit:
    return ret;
}

static int32_t CTestService_stressProcessAbnormalDeath(TestService *me)
{
    int32_t ret = Object_OK;
    pthread_t client[NUM_TEST_THREADS];
    const int32_t maxTestThreadsCount = NUM_TEST_THREADS;
    stressThreadData args[NUM_TEST_THREADS];

    LOG_MSG("Calling stressProcessAbnormalDeath test");

    memset(args, 0, sizeof(args));

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        args[n].targetUid = callDiffServices[n].uid;
        T_CHECK(0 == pthread_create(&client[n], NULL, _processAbnormalDeathParallel, &args[n]));
    }

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        T_CHECK(0 == pthread_join(client[n], NULL));
        T_CHECK(Object_OK == args[n].ret);
    }

exit:
    return ret;
}

static int32_t CTestService_stressLoadSameEmbeddedProc(TestService *me)
{
    int32_t ret = Object_OK;
    pthread_t client[NUM_TEST_THREADS];
    const int32_t maxTestThreadsCount = NUM_TEST_THREADS;
    stressThreadData args[NUM_TEST_THREADS];
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling stressLoadSameEmbeddedProc test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    memset(args, 0, sizeof(args));

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        args[n].targetUid = CEmbeddedAllPrivilegeTestService_UID;
        T_CHECK(0 == pthread_create(&client[n], NULL, _openEmbeddedServiceParallel, &args[n]));
    }

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        T_CHECK(0 == pthread_join(client[n], NULL));
        T_CHECK(Object_OK == args[n].ret);
        Object_ASSIGN_NULL(args[n].serviceObj);
    }

exit:
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_stressLoadDiffEmbeddedProc(TestService *me)
{
    int32_t ret = Object_OK;
    pthread_t client[NUM_TEST_THREADS];
    const int32_t maxTestThreadsCount = NUM_TEST_THREADS;
    stressThreadData args[NUM_TEST_THREADS];
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling stressLoadDiffEmbeddedProc test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    memset(args, 0, sizeof(args));

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        args[n].targetUid = callDiffServices[n].uid;
        T_CHECK(0 == pthread_create(&client[n], NULL, _openEmbeddedServiceParallel, &args[n]));
    }

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        T_CHECK(0 == pthread_join(client[n], NULL));
        T_CHECK(Object_OK == args[n].ret);
    }

exit:
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_stressEmbeddedNormalDeath(TestService *me)
{
    int32_t ret = Object_OK;
    pthread_t client[NUM_TEST_THREADS];
    const int32_t maxTestThreadsCount = NUM_TEST_THREADS;
    stressThreadData args[NUM_TEST_THREADS];
    Object wakeLock = Object_NULL;

    LOG_MSG("Calling stressEmbeddedNormalDeath test");

    T_GUARD(_getQTVMWakeLock(&wakeLock));

    memset(args, 0, sizeof(args));

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        args[n].targetUid = callDiffServices[n].uid;
        T_CHECK(0 == pthread_create(&client[n], NULL, _embeddedNormalDeathParallel, &args[n]));
    }

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        T_CHECK(0 == pthread_join(client[n], NULL));
        T_CHECK(Object_OK == args[n].ret);
    }

exit:
    Object_ASSIGN_NULL(wakeLock);
    return ret;
}

static int32_t CTestService_stressAcquireWakeLock(TestService *me)
{
    int32_t ret = Object_OK;
    pthread_t client[NUM_TEST_THREADS];
    const int32_t maxTestThreadsCount = NUM_TEST_THREADS;
    stressThreadData args[NUM_TEST_THREADS];

    LOG_MSG("Calling stressAcquireWakeLock test");

    memset(args, 0, sizeof(args));

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        args[n].targetUid = callDiffServices[n].uid;
        T_CHECK(0 == pthread_create(&client[n], NULL, _localPowerServiceParallel, &args[n]));
    }

    for (int32_t n = 0; n < maxTestThreadsCount; n++) {
        T_CHECK(0 == pthread_join(client[n], NULL));
        T_CHECK(Object_OK == args[n].ret);
    }

exit:
    return ret;
}

static int32_t CTestService_setRunningVM(TestService *me, uint32_t vmType)
{
    int32_t ret = Object_OK;

    T_CHECK(vmType <= OEMVM);

    gRemoteConn = vmType;

    if (gRemoteConn == OEMVM) {
        LOG_MSG("Running on OEMVM VM\n");
    } else {
        LOG_MSG("Running on QTVM VM\n");
    }

exit:
    return ret;
}

/**
 * Description: Load a test TA in QTEE, and passes the Credential obj to QTEE via minkhub.
 *
 * Return:      Object_OK on success.
 */
static int32_t CTestService_passCredToQTEE(TestService *me)
{
    int32_t ret = Object_OK;
    Object appClientObj = Object_NULL;
    Object appObj = Object_NULL;
    const char appName[] = "credtestapp64";

    T_CALL(ITEnv_open(gTVMEnv, CAppClient_UID, &appClientObj));
    T_CALL(IAppClient_getAppObject(appClientObj, appName, strlen(appName), &appObj));

exit:
    Object_ASSIGN_NULL(appObj);
    Object_ASSIGN_NULL(appClientObj);

    return ret;
}

/**
 * Description: HLOS simply shares the memory with QTVM.
 *              Receiver(QTVM TA) wont check if the content is consistent.
 *              Instead, it focuses on other aspect like sharing LARGE buffer
 *                and its releasing latency/timing.
 *
 * Return:      Object_OK on success.
 */
static int32_t CTestService_simpleShareMemoryBuffer(TestService *me, uint64_t bufSize,
                                                    Object memObj)
{
    int32_t ret = Object_ERROR, bufFd = -1;
    void *ptr = NULL;

    T_CALL(Object_unwrapFd(memObj, &bufFd));
    // Ensure bufSize fits in size_t on 32-bit platform
    T_CHECK(bufSize <= SIZE_MAX);

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(MAP_FAILED != ptr);

    if (bufSize > strlen(gSharedMsg)) {
        memscpy(ptr, strlen(gSharedMsg), gSharedMsg, strlen(gSharedMsg));
    }

    ret = munmap(ptr, bufSize);
    T_CHECK(0 == ret);

exit:

    return ret;
}

static int32_t QTVM2TZ_bufferTest(Object *appObj)
{
    int32_t ret = Object_OK;
    uint8_t input[8] = {0}, output[256] = {0};
    size_t outlen = 0;
    Object objTest = Object_NULL;
    memset(input, 1, sizeof(input));

    T_GUARD(IOpener_open(*appObj, CTzEcoTestApp_TestInterface_UID, &objTest));
    T_CHECK(!Object_isNull(objTest));

    T_GUARD(ITestInterface_setBuf(objTest, input, sizeof(input)));

    T_GUARD(ITestInterface_getBuf(objTest, output, sizeof(output), &outlen));
    T_CHECK(outlen == sizeof(input));
    T_CHECK(0 == memcmp(input, output, sizeof(input)));
    T_GUARD(ITestInterface_getBuf(objTest, NULL, 0, &outlen));
    T_GUARD(ITestInterface_getBuf(objTest, output, 0, &outlen));
    // Note: The err code does not match with head file ITestInterface.h
    T_CALL_CHECK(ITestInterface_getBuf(objTest, NULL, sizeof(output), &outlen), ret != Object_OK);

    T_GUARD(ITestInterface_copyBuf(objTest, input, sizeof(input), output, sizeof(output), &outlen));
    T_CHECK(outlen == sizeof(input));
    T_CHECK(0 == memcmp(input, output, sizeof(input)));

exit:
    Object_ASSIGN_NULL(objTest);
    return ret;
}

static int32_t QTVM2TZ_memObjTest(Object *appObj)
{
    int32_t ret = Object_OK;
    int32_t bufFd = -1;
    void *bufPtr = NULL, *alignedPtr = NULL;
    // large memory obj may cause error
    uint64_t poolSizeMB = 2, buffSizeMB = 2;
    Object mmTestObj = Object_NULL;

    ITAccessPermissions_rules confRules = {0};
    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    T_GUARD(MSTest_GetMemPool(&confRules, &ms_memPoolFactoryObj[0], &ms_memPoolObj[0], poolSizeMB));
    T_GUARD(MSTest_AllocateMemObj(ms_memPoolObj[0], &ms_memObj[0], buffSizeMB));
    T_GUARD(MSTest_InitMemObj(&ms_memObj[0], buffSizeMB));
    T_CHECK(!Object_isNull(ms_memObj[0]));

    // set the buffer beginning 8 bytes with certain pattern
    T_GUARD(Object_unwrapFd(ms_memObj[0], &bufFd));
    bufPtr = mmap(NULL, buffSizeMB * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(bufPtr != MAP_FAILED);
    *(uint64_t *)bufPtr = ITestMemManager_TEST_PATTERN1;

    // send memory Obj to TZ app and check return pattern
    LOG_MSG("send buf %" PRIx64 " to TZApp", *(uint64_t *)bufPtr);
    T_GUARD(IOpener_open(*appObj, CTzEcoTestApp_TestMemManager_UID, &mmTestObj));
    ITestMemManager_access(mmTestObj, ms_memObj[0]);
    T_CHECK(*(uint64_t *)bufPtr == ITestMemManager_TEST_PATTERN2);
    LOG_MSG("return buf %" PRIx64 " from TZApp", *(uint64_t *)bufPtr);

exit:
    munmap(bufPtr, buffSizeMB * 1024 * 1024);
    MSTest_Cleanup();
    Object_ASSIGN_NULL(mmTestObj);
    return ret;
}

/**
 * Description: QTVM invoke with TZ
 * step1: Load TZ app with memoryObj SHARE form HLOS
 * step2: Open the appobj with appClientObj
 * step3: Invoke the app in TZ with 2 options:
 *        op1: invocation with buffer
 *        op2: invocation with memoey obj
 * Return: Object_OK on success.
 */
static int32_t CTestService_QTVM2TZtest(TestService *me, Object memObj)
{
    int32_t ret = Object_OK;
    Object appLoaderObj = Object_NULL;
    Object appCtlObj = Object_NULL;
    Object appObj = Object_NULL;

    T_GUARD(ITEnv_open(gTVMEnv, CAppLoader_UID, &appLoaderObj));
    T_GUARD(IAppLoader_loadFromRegion(appLoaderObj, memObj, &appCtlObj));
    T_GUARD(IAppController_getAppObject(appCtlObj, &appObj));
    LOG_MSG("Success to load and get QTEE TA\n");

    T_CALL(QTVM2TZ_bufferTest(&appObj));
    T_CALL(QTVM2TZ_memObjTest(&appObj));

exit:
    Object_ASSIGN_NULL(appObj);
    Object_ASSIGN_NULL(appCtlObj);
    Object_ASSIGN_NULL(appLoaderObj);
    return ret;
}

/**
 * Description: QTVM TA1 alloc large memory and share with QTVM TA2
 *
 * Return: Object_OK on success.
 */
static int32_t CTestService_QTVMAllocMemory(TestService *me, uint64_t poolSizeMB)
{
    int32_t ret = Object_OK;
    uint64_t bufSizeMB = poolSizeMB;
    int32_t bufFd = -1;
    void *ptr = NULL;
    Object testServiceObj = Object_NULL;
    Object wakeLock = Object_NULL;

    uint64_t getPoolTime[QTVM_ALLOC_ITERATION] = {0};
    uint64_t getBufferTime[QTVM_ALLOC_ITERATION] = {0};
    uint64_t freeBufferTime[QTVM_ALLOC_ITERATION] = {0};
    uint64_t freePoolTime[QTVM_ALLOC_ITERATION] = {0};

    ITAccessPermissions_rules confRules = {0};
    confRules.specialRules = ITAccessPermissions_keepSelfAccess;

    if (_isEmebddedSevice(CEmbeddedCommonTestService_UID)) {
        T_GUARD(_getQTVMWakeLock(&wakeLock));
    }
    T_GUARD(ITEnv_open(gTVMEnv, CEmbeddedCommonTestService_UID, &testServiceObj));

    for (size_t i = 0; i < QTVM_ALLOC_ITERATION; i++) {
        // Get pool and allocate buffer
        getPoolTime[i] = LOG_PERF_OP(T_GUARD(MSTest_GetMemPool(&confRules, &ms_memPoolFactoryObj[0],
                                                               &ms_memPoolObj[0], poolSizeMB)),
                                     "get pool size = %" PRIu64 " \n", poolSizeMB);
        getBufferTime[i] =
            LOG_PERF_OP(T_GUARD(MSTest_AllocateMemObj(ms_memPoolObj[0], &ms_memObj[0], bufSizeMB)),
                        "get buffer size = %" PRIu64 " \n", bufSizeMB);
        MSTest_InitMemObj(&ms_memObj[0], bufSizeMB);

        // Copy shared message to memObj
        T_GUARD(Object_unwrapFd(ms_memObj[0], &bufFd));
        ptr = mmap(NULL, bufSizeMB * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
        T_CHECK(MAP_FAILED != ptr);
        memscpy(ptr, strlen(gSharedMsg), gSharedMsg, strlen(gSharedMsg));
        T_GUARD(munmap(ptr, bufSizeMB * 1024 * 1024));

        // Share the memObj to target service
        T_CALL(ITestService_enqueueBuffer(testServiceObj, bufSizeMB, ms_memObj[0]));

        // release pool and buffer
        freeBufferTime[i] = LOG_PERF_OP(Object_ASSIGN_NULL(ms_memObj[0]),
                                        "free pool size = %" PRIu64 " \n", poolSizeMB);
        freePoolTime[i] = LOG_PERF_OP(Object_ASSIGN_NULL(ms_memPoolObj[0]),
                                      "free buffer size = %" PRIu64 " \n", bufSizeMB);

        Object_ASSIGN_NULL(ms_memPoolFactoryObj[0]);

        // Need time to donate memory back to HLOS asynchronously
        usleep(1000 * 1000);
    }

    LOG_MSG("Test TA PROFILE: get mempool sizeMB = %" PRIu64 ", costTime = %" PRIu64 " \n",
            poolSizeMB, _getAverage(getPoolTime, QTVM_ALLOC_ITERATION));
    LOG_MSG("Test TA PROFILE: get buffer sizeMB = %" PRIu64 ", costTime = %" PRIu64 " \n",
            bufSizeMB, _getAverage(getBufferTime, QTVM_ALLOC_ITERATION));
    LOG_MSG("Test TA PROFILE: free buffer sizeMB = %" PRIu64 ", costTime = %" PRIu64 " \n",
            bufSizeMB, _getAverage(freeBufferTime, QTVM_ALLOC_ITERATION));
    LOG_MSG("Test TA PROFILE: free mempool sizeMB = %" PRIu64 ", costTime = %" PRIu64 " \n",
            poolSizeMB, _getAverage(freePoolTime, QTVM_ALLOC_ITERATION));

exit:
    return ret;
}

static int32_t CTestService_localPlatformInfoTest(TestService *me)
{
    Object testServiceObj = Object_NULL;
    int32_t ret = -1;

    LOG_MSG("Calling credential service!");

    // get service object
    T_GUARD(ITEnv_open(gTVMEnv, gPlatformInfoServiceUid[gRemoteConn], &testServiceObj));

    T_GUARD(_checkEnvCredentials(testServiceObj, true));

exit:
    // release service object
    Object_ASSIGN_NULL(testServiceObj);
    return ret;
}

/**
 * Description: QTVM TA receive the buffer from HLOS CA and echo back.
 *
 * Return: Object_OK on success.
 */
static int32_t
CTestService_simpleShareCopyBuffer(TestService *me, void *echo_in, size_t echo_in_len,
                                   void *echo_out, size_t echo_out_len, size_t *echo_out_lenOut)
{
    int32_t ret = Object_OK;
    T_CHECK((echo_in != NULL) && (echo_out != NULL) && (echo_out_lenOut != NULL));

    *echo_out_lenOut = memscpy(echo_out, echo_out_len, echo_in, echo_in_len);

exit:
    return ret;
}

/**
 * Description:
 *   QTVM TA checks if the confinement rules of the memory object is as expected.
 *
 * Return: Object_OK on success.
 */
static int32_t
CTestService_checkAccessPermissions(TestService *me,
                                    void *confRules,
                                    size_t confRules_len,
                                    Object memObj)
{
    int32_t ret = Object_OK;
    Object tACService = Object_NULL;
    ITAccessPermissions_rules *confRulesPtr = NULL;

    T_CHECK((NULL != confRules) && (sizeof(ITAccessPermissions_rules) == confRules_len));
    confRulesPtr = confRules;
    T_CHECK(!Object_isNull(memObj));
    T_GUARD(ITEnv_open(gTVMEnv, gTAccessControlUid[gRemoteConn], &tACService));

    T_CALL(ITAccessControl_checkExclusiveAccess(tACService, confRulesPtr, memObj));

exit:
    Object_ASSIGN_NULL(tACService);
    return ret;
}

static ITestService_DEFINE_INVOKE(CTestService_invoke, CTestService_, TestService *);

/**
 * Description: Open service by providing the services Unique ID as well as the
 *              ICredentials object of the caller, to uniquely identify it.
 *
 * In:          uid:    The unique ID of the requested service.
 *              cred:   The ICredentials object of the caller.
 * Out:         objOut: The service object.
 * Return:      Object_OK on success.
 */
int32_t tProcessOpen(uint32_t uid, Object cred, Object *objOut)
{
    int32_t ret = Object_OK;
    TestService *me = HEAP_ZALLOC_TYPE(TestService);

    T_CHECK_ERR(me, Object_ERROR_MEM);

    LOG_MSG("Opening service with UID = %d", uid);

    Object_ASSIGN(me->credentials, cred);
    me->refs = 1;

#ifdef EMBEDDED_PROC
    // Keep track of all outstanding service objects

    atomicAdd(&gServiceCount, 1);
    LOG_MSG("Embedded Service objects count = %lu", gServiceCount);
#endif

    // In addition, keep a reference in a global list for CredDealer tests
    if (uid == CCredDealerTestService_UID) {
        gCredsCnt++;
        T_CHECK(gCredsCnt < MAX_STORED_CREDS);

        Object_ASSIGN(gCreds[gCredsCnt].cred, cred);
        LOG_MSG("callerUid=%u,gCredsCnt=%d.", uid, gCredsCnt);
    }

    *objOut = (Object){CTestService_invoke, me};

exit:
    if (ret) {
        CTestService_release(me);
    }

    return ret;
}
