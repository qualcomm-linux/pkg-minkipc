// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <gtest/gtest.h>
#include <inttypes.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>

extern "C" {
#include "CAllPrivilegeTestService.h"
#include "CAppClient.h"
#include "CAppLoader.h"
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
#include "CSecureImage.h"
#include "CSpareTestService.h"
#include "CStressTestService.h"
#include "CTOEMVMPowerService.h"
#include "CTOEMVMProcessLoader.h"
#include "CTPowerService.h"
#include "CTProcessLoader.h"
#include "CTRebootOEMVM.h"
#include "CTRebootVM.h"

#include "CAVMTestService.h"
#include "CDeviceAttestation.h"
#include "CHLOSPlatformEnv.h"
#include "CMinkdRegister.h"
#include "CTRegisterModule.h"
#include "CVMDeviceUniqueKey.h"
#include "IAVMTestService.h"
#include "IAppClient.h"
#include "IAppLoader.h"
#include "IMinkdRegister.h"
#include "IModule.h"
#include "IObject.h"
#include "ITEnv.h"
#include "ITPowerService.h"
#include "ITProcessController.h"
#include "ITProcessLoader.h"
#include "ITRebootVM.h"
#include "ITestService.h"
#include "RemoteShareMemory.h"
#include "fdwrapper.h"
#include "minkipc.h"
#include "osIndCredentials.h"
#include "version.h"

#ifndef OFFTARGET
#include "BufferAllocator/BufferAllocatorWrapper.h"
#include "CDiagnostics.h"
#include "IClientEnv.h"
#include "ITzdRegister.h"
#include "TZCom.h"
#include "vmmem_wrapper.h"
#else
#include "BufferAllocatorWrapper.h"
#include "ICredentials.h"
#endif
}

#include "Profiling.h"
#include "TVMMonitor.hpp"
#include "minkipc.h"

using namespace std;

/*
 *  ----------------------- GLOBAL BACKGROUND --------------------------
 */

#define VAR2STR(s) #s
#define VAL2STR(s) VAR2STR(s)

#ifdef OFFTARGET
#define SLEEP_MS(x)                                       \
    {                                                     \
        printf("\nSleeping for %d milliseconds.\n\n", x); \
        usleep(1000 * x);                                 \
    }
#else
#define SLEEP_MS(x) usleep(1000 * x);
#endif

// Distinguish the test case which run on-target, off-target or both.
#define COMMON_TEST(testcase) testcase
// This macro is used for the unstable testcases which are disable for the moment.
#define UNSTABLE_TEST(testcase) DISABLED_##testcase

#ifdef OFFTARGET
#define ONTARGET_TEST(testcase) DISABLED_##testcase
#define OFFTARGET_TEST(testcase) testcase
#else
#define ONTARGET_TEST(testcase) testcase
#define OFFTARGET_TEST(testcase) DISABLED_##testcase
#endif

#define HLOS_PATH hlos
#define QTVM_PATH out
#define OEMVM_PATH oemvm
#define HLOSMinkD minkdaemon
#define PRELAUNCHER TVMPrelauncher
#define MINK TVMMink
#define XVM_MINK XVMMink
#define XVM_PRELAUNCHER XVMPrelauncher

#ifndef OFFTARGET
#define MINK_UNIX_LA_SERVICE "/dev/socket/hlos_mink_opener"
#define LEGACY_UNIX_LA_SERVICE "/dev/socket/ssgtzd"
#define MINK_QRTR_LE_SERVICE 5008
#define MINK_QRTR_LE_SERVICE_OEMVM 5010
#define APP_ROOT_DIR "/vendor/bin/"
#else
#define APP_ROOT_DIR VAL2STR(OEMVM_PATH) "/"
#define MINK_UNIX_LA_SERVICE "simulated_hlos_mink_opener"
#define MINK_QRTR_LE_SERVICE "tvm_qrtr_simulated_socket"
#define MINK_QRTR_LE_SERVICE_OEMVM "oemvm_qrtr_simulated_socket"
#endif

#define ITEST_OPEN(self, uid, cred, service, cond) \
    cond ? IOpener_open(self, uid, service) : IModule_open(self, uid, cred, service)
#define CUnregisteredService_UID 1357
#define NUM_THREADS 5
// These two values are defined in the ta_config.json
#define MAX_QTEE_SYSTEM_CLIENTS_ALLOW 12
#define MAX_QTEEOBJECT_PER_SYSTEM_CLIENT_ALLOWED 15
#define SIZE_2MB 0x200000ULL
#define TEST_OPEN_POWER_SERVICE 0
#define TEST_ACQUIRE_WAKE_LOCK 1
#define TEST_CLOSE_POWER_SERVICE 2
#define isOk(__subroutine__) ASSERT_NO_FATAL_FAILURE(__subroutine__)
#define NOT_USED_TEST_UID ((uint32_t)0xfffffffe)

static const char *gSharedMsg = "My shared message";
class TvmdTestEnvironment;
// need a global pointer to access environment
TvmdTestEnvironment *gEnv = nullptr;

/*
 *  ----------------- MEMORY OBJECT ASSISTANT -----------------------------
 */

// Continuous, used to load downloadable TP
#define DEFAULT_DMA_BUF_HEAP "qcom,display"

// Scattered, allocate large buffer
#define SYSTEM_DMA_BUF_HEAP "qcom,system"

extern int32_t CAVMTestIModule_new(Object *objOut);

static ITAccessPermissions_rules _getSimpleConfinement(bool isShared)
{
    if (isShared) {
        return (ITAccessPermissions_rules){{{0, 0}}, {{0, 0}}, ITAccessPermissions_keepSelfAccess};
    } else {
        return (ITAccessPermissions_rules){
            {{0, 0}}, {{0, 0}}, ITAccessPermissions_removeSelfAccess};
    }
}

#ifndef OFFTARGET
static void _attachLendConfinement(Object *memObj)
{
    const ITAccessPermissions_rules confRules = _getSimpleConfinement(false);

    ASSERT_EQ(Object_OK, RemoteShareMemory_attachConfinement(&confRules, memObj));
}
#endif

static uint64_t _getAverage(vector<uint64_t> &v)
{
    uint64_t sum = 0;
    size_t len = v.size();
    if (len == 0) {
        return sum;
    }
    for (size_t i = 0; i < len; i++) {
        if (__builtin_add_overflow(sum, v[i], &sum)) {
            printf("overflow occured \n");
            return 0;
        }
    }
    return sum / v.size();
}

static void _getShm(FILE *pfile, uint64_t filelen, Object *memObj, bool explicitForLend,
                    const char *srcHeap)
{
    int32_t bufFd = -1;
    ASSERT_FALSE((filelen > UINT64_MAX - (SIZE_2MB - 1)));
    uint64_t shmSize = (filelen + (SIZE_2MB - 1)) & (~(SIZE_2MB - 1));
    // Ensure shmSize fits in size_t on 32-bit platform
    ASSERT_FALSE(shmSize > SIZE_MAX);

    BufferAllocator *bufferAllocator = CreateDmabufHeapBufferAllocator();
    ASSERT_FALSE(nullptr == bufferAllocator);

    bufFd = DmabufHeapAlloc(bufferAllocator, srcHeap, shmSize, 0, 0);
    ASSERT_FALSE(bufFd < 0);

    void *ptr = mmap(NULL, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_NE(MAP_FAILED, ptr);

    memset(ptr, 0, shmSize);

    if (pfile != NULL) {
        size_t n = fread(ptr, 1, filelen, (FILE *)pfile);
        ASSERT_EQ(n, filelen);
    }
#ifdef OFFTARGET
    bufFd = RefreshMemFd(bufFd, O_RDONLY);
#endif
    ASSERT_EQ(0, munmap(ptr, shmSize));

    *memObj = FdWrapper_new(bufFd);
    ASSERT_FALSE(Object_isNull(*memObj));

    if (explicitForLend) {
#ifndef OFFTARGET
        isOk(_attachLendConfinement(memObj));
#endif
    }

    if (bufferAllocator) {
        FreeDmabufHeapBufferAllocator(bufferAllocator);
    }
}

static void FreeShm(Object memObj)
{
#ifdef OFFTARGET
    FdWrapper *fdw = NULL;

    fdw = FdWrapperFromObject(memObj);

    if (fdw != nullptr) {
        close_offtarget_unlink(fdw->descriptor);
    }
#endif
    Object_ASSIGN_NULL(memObj);
}

static void _simulatedHubGetCallerCred(Object *objOut)
{
    uint8_t callerVMuuid[VMUUID_MAX_SIZE] = {CLIENT_VMUID_TUI};
    Object callerInfoCred = Object_NULL;
    uint32_t appDebug = 1;
    uint32_t appVersion = 2;
    char testDomain[] = "multiplevm";
    char testAppName[] = "testing123";
    uint64_t testPermissions = 5;
    SHA256Hash appHash_1[SHA256_DIGEST_LENGTH] = {{1}, {2}, {3}, {4}, {0xAA}, {0xFF}};
    SHA256Hash appHash_2[SHA256_DIGEST_LENGTH] = {{1}, {2}, {3}, {4}, {0xAA}, {0xFF}};
    uint8_t legacyCBOR_1[32] = {1, 2, 3, 4, 0xAA, 0xFF};
    uint8_t legacyCBOR_2[32] = {1, 2, 3, 4, 0xAA, 0xFF};
    SHA256Hash *testHashes[] = {appHash_1, appHash_2};
    uint8_t *testLegacyCBOR[] = {legacyCBOR_1, legacyCBOR_2};
    Object simulatedHubEnvCred = Object_NULL;

    ASSERT_FALSE(nullptr == objOut);

    ASSERT_EQ(Object_OK, OSIndCredentials_newEnvCred("osID", callerVMuuid, NULL, "vmDomain", 0,
                                                     &simulatedHubEnvCred));

    ASSERT_EQ(Object_OK, OSIndCredentials_newProcessCred(
                             testPermissions, testAppName, testHashes[0], appDebug, appVersion,
                             testLegacyCBOR[0], sizeof(testLegacyCBOR[0]), testDomain, getpid(),
                             getuid(), &callerInfoCred));

    ASSERT_EQ(Object_OK,
              OSIndCredentials_WrapCredentials(&callerInfoCred, &simulatedHubEnvCred, objOut));
    Object_ASSIGN_NULL(callerInfoCred);
    Object_ASSIGN_NULL(simulatedHubEnvCred);
}

/*
 * ------------------- TEST SUITE DEFINITION ------------------------------
 */
/* clang-format off */
// Note: tzecotestapp.mbn need to be push to /vendor/bin/ manually
static const char *gFileName[] = {
    APP_ROOT_DIR "AllPrivilegeTestService",
    APP_ROOT_DIR "CommonTestService",
    APP_ROOT_DIR "StressTestService",
    APP_ROOT_DIR "CredDealerTestService",
    APP_ROOT_DIR "SpareTestService",
    APP_ROOT_DIR "credtestapp64.mbn",
    APP_ROOT_DIR "tzecotestapp.mbn"
};

static const int gTrustedUid[] = {
    CAllPrivilegeTestService_UID,
    CCommonTestService_UID,
    CStressTestService_UID,
    CCredDealerTestService_UID,
    CSpareTestService_UID
};

static const int gEmbeddedUid[] = {
    CEmbeddedAllPrivilegeTestService_UID,
    CEmbeddedCommonTestService_UID,
    CEmbeddedStressTestService_UID,
    CEmbeddedNormalDeathTestService_UID,
    CEmbeddedSpareTestService_UID
};
/* clang-format on */

struct ShmUnit {
    FILE *pfile;
    Object memObj;
    ShmUnit()
    {
        pfile = NULL;
        memObj = Object_NULL;
    }
};

struct ShmList {
    vector<ShmUnit> shmList;
    int32_t counter;

    void constructor()
    {
        counter = 0;
        shmList.clear();
    }

    void destructor()
    {
        for (uint32_t i = 0; i < shmList.size(); i++) {
            isOk(FreeShm(shmList[i].memObj));
            fclose(shmList[i].pfile);
        }
    }

    // Note that opening ELF might fail i.e. Object_NULL is returned.
    // For convenience, we leave checking of retObj later(e.g. in loadTrustedProcess()).
    Object getShm(const char *fileName, bool explicitForLend)
    {
        ShmUnit tmpUnit;
        struct stat st;
        uint64_t filelen;

        tmpUnit.pfile = fopen(fileName, "rb");

        if (tmpUnit.pfile == NULL) {
            printf("failed to fopen %s\n", fileName);
            return Object_NULL;
        }

        stat(fileName, &st);
        filelen = st.st_size;

        _getShm(tmpUnit.pfile, filelen, &(tmpUnit.memObj), explicitForLend, DEFAULT_DMA_BUF_HEAP);
        if (!Object_isNull(tmpUnit.memObj)) {
            shmList.push_back(tmpUnit);
        }

        return tmpUnit.memObj;
    }
};

static enum RemoteConnection {
    QTVM = 0,
    OEMVM,
} gRemoteConn = QTVM;

static bool gMinkConn = true;
static uint32_t gProcessLoaderUid[] = {CTProcessLoader_UID, CTOEMVMProcessLoader_UID};
static uint32_t gPowerServiceUid[] = {CTPowerService_UID, CTOEMVMPowerService_UID};
static uint32_t gPlatformInfoServiceUid[] = {CQTVMPlatformInfo_UID, COEMVMPlatformInfo_UID};
static uint32_t gHLOSTestServiceUid[] = {CAVMCommonTestService_UID, CAVMStressTestService_UID};

#ifndef OFFTARGET
static uint32_t gRemoteQRTRPort[] = {MINK_QRTR_LE_SERVICE, MINK_QRTR_LE_SERVICE_OEMVM};
#endif

/*
 *  Global environment class for command-specific variables,
 *  extend the class member to cover more scenario.
 */
class TvmdTestEnvironment : public testing::Environment
{
   public:
    TvmdTestEnvironment(const uint64_t customMemorySizeMB) : memorySizeMB(customMemorySizeMB)
    {
    }
    // global setup, only called once (for this program)
    void SetUp() override
    {
        if (gMinkConn) {
            ASSERT_EQ(Object_OK, CAVMTestIModule_new(&mIModule));
        }

        if (mVMMonitor != nullptr) {
            delete mVMMonitor;
        }

        mVMMonitor = new TVMMonitor();
        if (mVMMonitor != nullptr) {
            ASSERT_EQ(0, mVMMonitor->startVM(TRUSTED_VM));

            if (gRemoteConn == OEMVM) {
                mVMMonitor->startVM(OEM_VM);
            }
        }

        SLEEP_MS(200);
    }
    // global teardown, only called once (for this program)
    void TearDown() override
    {
        SLEEP_MS(100);
        Object_ASSIGN_NULL(mIModule);

        if (mVMMonitor != nullptr) {
            delete mVMMonitor;
            mVMMonitor = nullptr;
        }
    }

    // set memory size from command args for share/lend test
    uint64_t memorySizeMB;
    Object mIModule = Object_NULL;
    TVMMonitor *mVMMonitor;
};

class TvmdFixture : public testing::Test
{
   protected:
    // global setup, only called once (for this test suite).
    static void SetUpTestCase()
    {
#ifdef OFFTARGET
        printf("Killing " VAL2STR(HLOSMinkD) " before starting test\n");
        system("pkill " VAL2STR(HLOSMinkD) " || true");
        printf("clean up share memory before starting test\n");
        system("rm -rf /dev/shm/tvmd*");

        // Launch HLOS Mink daemon "minkdaemon"
        printf("Starting " VAL2STR(HLOS_PATH) "/" VAL2STR(HLOSMinkD) "\n");
        system(VAL2STR(HLOS_PATH) "/" VAL2STR(HLOSMinkD) " &");
#endif
        printf("%d Starting Test Suite\n", getpid());
        printf("Test with memory size = %" PRIu64 " MB\n", gEnv->memorySizeMB);
    }

    // global teardown, only called once (for this test suite).
    static void TearDownTestCase()
    {
#ifdef OFFTARGET
        system("echo Test Suite completed. Killing " VAL2STR(HLOSMinkD) "...");
        system("pkill " VAL2STR(HLOSMinkD));
        system("rm -rf wake_*");
#endif
    }

    // local setup, called each time for each instance of test fixture.
    void SetUp() override
    {
        int32_t retry = 5;

#ifdef HLOSMINKD_UNIX_TZD_SERVER
        do {
            mSsgtzdOpenerConn = MinkIPC_connect(LEGACY_UNIX_LA_SERVICE, &mSsgtzdOpener);
            SLEEP_MS(100);
            --retry;
        } while ((!mSsgtzdOpenerConn) && retry > 0);
        ASSERT_FALSE(nullptr == mSsgtzdOpenerConn);
        ASSERT_FALSE(Object_isNull(mSsgtzdOpener));

        retry = 5;
#endif  // HLOSMINKD_UNIX_TZD_SERVER
        do {
#ifndef OFFTARGET
            if (!gMinkConn) {
                if (gRemoteConn == QTVM) {
                    mOpenerConn = MinkIPC_connect_QRTR(MINK_QRTR_LE_SERVICE, &mEnvOpener);
                } else if (gRemoteConn == OEMVM) {
                    mOpenerConn = MinkIPC_connect_QRTR(MINK_QRTR_LE_SERVICE_OEMVM, &mEnvOpener);
                }
#else
            if (!gMinkConn) {
                if (gRemoteConn == QTVM) {
                    mOpenerConn =
                        MinkIPC_connectModule_simulated(MINK_QRTR_LE_SERVICE, &mEnvOpener);
                } else if (gRemoteConn == OEMVM) {
                    mOpenerConn =
                        MinkIPC_connectModule_simulated(MINK_QRTR_LE_SERVICE_OEMVM, &mEnvOpener);
                }
#endif
            } else {
                mOpenerConn = MinkIPC_connect(MINK_UNIX_LA_SERVICE, &mEnvOpener);
            }

            SLEEP_MS(100);
            --retry;
        } while ((!mOpenerConn) && retry > 0);

        ASSERT_FALSE(nullptr == mOpenerConn);
        ASSERT_FALSE(Object_isNull(mEnvOpener));

        if (gMinkConn) {
            ASSERT_EQ(Object_OK, ITEST_OPEN(mEnvOpener, CHLOSPlatformEnv_UID, mSimulatedHubCred,
                                            &mClientOpener, gMinkConn));
            ASSERT_FALSE(Object_isNull(mClientOpener));
            ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CMinkdRegister_UID, mSimulatedHubCred,
                                            &mRegObj, gMinkConn));
            ASSERT_FALSE(Object_isNull(mRegObj));
        } else {
            _simulatedHubGetCallerCred(&mSimulatedHubCred);
            Object_INIT(mClientOpener, mEnvOpener);
            ASSERT_FALSE(Object_isNull(mClientOpener));
        }

        ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, gPowerServiceUid[gRemoteConn],
                                        mSimulatedHubCred, &mPowerService, gMinkConn));
        ASSERT_EQ(Object_OK, ITPowerService_acquireWakeLock(mPowerService, &mWakeLock));
    }

    // local teardown, called each time for each instance of test fixture.
    void TearDown() override
    {
        // Clean-up ProcessManager
        Object_ASSIGN_NULL(mProcManObj);

        // Release wake lock before cleaning up connections
        Object_ASSIGN_NULL(mWakeLock);
        Object_ASSIGN_NULL(mPowerService);

        // Clean-up mEnvOpener and minksocket connection
        Object_ASSIGN_NULL(mRegObj);
        Object_ASSIGN_NULL(mSsgtzdOpener);
        Object_ASSIGN_NULL(mClientOpener);
        Object_ASSIGN_NULL(mEnvOpener);
        Object_ASSIGN_NULL(mSimulatedHubCred);
        if (mOpenerConn) {
            MinkIPC_release(mOpenerConn);
        }
        if (mSsgtzdOpenerConn) {
            MinkIPC_release(mSsgtzdOpenerConn);
        }
    }

    MinkIPC *mOpenerConn = nullptr;
    MinkIPC *mSsgtzdOpenerConn = nullptr;
    Object mSsgtzdOpener = Object_NULL;
    Object mEnvOpener = Object_NULL;
    Object mClientOpener = Object_NULL;
    Object mRegObj = Object_NULL;
    Object mProcManObj = Object_NULL;
    Object mTestServiceObj[NUM_THREADS] = {{Object_NULL}};
    Object mProcLoaderObj[NUM_THREADS] = {{Object_NULL}};
    Object mProcObj[NUM_THREADS] = {{Object_NULL}};
    ShmList mShmList;
    bool mIsLoadSame = false;
    Object mPowerService = Object_NULL;
    Object mWakeLock = Object_NULL;
    Object mSimulatedHubCred = Object_NULL;
};

class TvmdTest : public TvmdFixture
{
   protected:
    // local setup, called each time for each instance of test fixture.
    void SetUp() override
    {
        TvmdFixture::SetUp();
        isOk(mShmList.constructor());
    }

    // local teardown, called each time for each instance of test fixture.
    void TearDown() override
    {
        isOk(mShmList.destructor());
        for (int32_t i = 0; i < NUM_THREADS; i++) {
            // Clean up objects unconditionally
            Object_ASSIGN_NULL(mTestServiceObj[i]);
            Object_ASSIGN_NULL(mProcLoaderObj[i]);
            // Clean up objects conditionally
            if (!Object_isNull(mProcObj[i])) {
                Object_ASSIGN_NULL(mProcObj[i]);
                SLEEP_MS(100);
            }
        }

        // Give signal handler enough time to process all deaths
        SLEEP_MS(400);
#ifdef OFFTARGET
        system("rm -rf wake_*");
#endif
        TvmdFixture::TearDown();
    }
};

class ProfilingTest : public TvmdTest
{
    void SetUp() override
    {
        TvmdTest::SetUp();
    }

    void TearDown() override
    {
        TvmdTest::TearDown();
    }
};

/*
 *  --------------------- START_ROUTINE DEFINITION ------------------------
 */
typedef struct threadData {
    int index;
    Object opener;
    Object *myServiceObj[NUM_THREADS];
    Object *myProcLoaderObj;
    Object *myProcObj;
    Object myMemObj;
    Object mySimulatedHubCred;
} threadData;

typedef struct registerData {
    int32_t index;
    int32_t *uids;
    size_t uid_len;
    pid_t *pid;
} registerData;

/*
 * Note:
 *  Create multi-thread to call service through QRTR.
 *
 */
static void QRTRConnParallel(const uint32_t uid, Object opener)
{
    if (uid == CAVMCommonTestService_UID) {
        ASSERT_EQ(Object_OK, ITestService_QRTRConn(opener, ITestService_POSITIVETEST));
    } else if (uid == CAVMStressTestService_UID) {
        ASSERT_EQ(Object_OK, ITestService_QRTRConn(opener, ITestService_STRESSTEST));
    }
}

static void *QRTRConnPreprocess(void *args)
{
    threadData *tdata = (threadData *)args;
    QRTRConnParallel((const uint32_t)tdata->index, tdata->opener);
    return NULL;
}

void openServiceParallel(int32_t conn, Object opener, Object cred, Object *imodule)
{
    if (conn == QTVM || conn == OEMVM) {
        ASSERT_EQ(Object_OK,
                  ITEST_OPEN(opener, CAllPrivilegeTestService_UID, cred, imodule, gMinkConn));
        ASSERT_EQ(Object_OK, ITestService_QRTRConn(*imodule, ITestService_POSITIVETEST));
    } else {
        ASSERT_EQ(Object_OK,
                  ITEST_OPEN(opener, CAVMCommonTestService_UID, cred, imodule, gMinkConn));
    }
    ASSERT_FALSE(Object_isNull(*imodule));
}

static void *openServiceCrossAVMPreprocess(void *args)
{
    threadData *tdata = (threadData *)args;
    openServiceParallel(tdata->index, tdata->opener, tdata->mySimulatedHubCred, tdata->myProcObj);

    return NULL;
}

#ifndef OFFTARGET
static void qteeObjectAcessParallel(int32_t uid, Object opener)
{
    int32_t ret = Object_OK;
    Object qteeObject[MAX_QTEEOBJECT_PER_SYSTEM_CLIENT_ALLOWED] = {{ Object_NULL }};

    for (size_t i = 0; i < MAX_QTEEOBJECT_PER_SYSTEM_CLIENT_ALLOWED; i++) {
        ret = IClientEnv_open(opener, CDiagnostics_UID, &qteeObject[i]);
        EXPECT_TRUE((ret == Object_OK) || (ret == Object_ERROR_NOSLOTS));
    }

    for (size_t i = 0; i < MAX_QTEEOBJECT_PER_SYSTEM_CLIENT_ALLOWED; i++) {
        Object_ASSIGN_NULL(qteeObject[i]);
    }
}

/*
 * Note:
 *  Access to QTEE client Object in LA.
 *
 */
static void *qteeObjectAcessCrossPreprocess(void *args)
{
    threadData *data = (threadData *)args;

    qteeObjectAcessParallel(data->index, data->opener);
    return NULL;
}
#endif

/*
 * Note:
 *  The one constraint is that assertions that generate a fatal failure (FAIL* and ASSERT_*)
 *      can only be used in void-returning functions.
 */
static void loadTrustedProcess(int uid, Object opener, Object mSimulatedHubCred,
                               Object *procLoaderObj, Object *procObj, Object *testServiceObj,
                               Object memObj)
{
    int32_t ret = -1;

    // If below assertion is failed, here is a typical reason: opening ELF failed.
    ASSERT_FALSE(Object_isNull(memObj));

    ASSERT_EQ(Object_OK, ITEST_OPEN(opener, gProcessLoaderUid[gRemoteConn], mSimulatedHubCred,
                                    procLoaderObj, gMinkConn));

    // Only first client can get a valid TProcess Controller Object (procObj here)
    // If a process is loaded, the client will get a ERROR_PROC_ALREADY_LOADED
    ret = ITProcessLoader_loadFromBuffer(*procLoaderObj, memObj, procObj);
    EXPECT_TRUE((ret == Object_OK) || (ret == ITProcessLoader_ERROR_PROC_ALREADY_LOADED));

    ASSERT_EQ(Object_OK, ITEST_OPEN(opener, uid, mSimulatedHubCred, testServiceObj, gMinkConn));
    ASSERT_EQ(Object_OK, ITestService_setRunningVM(*testServiceObj, gRemoteConn));
    ASSERT_EQ(Object_OK, ITestService_printHello(*testServiceObj));
}

static void *loadTrustedProcessParallel(void *args)
{
    threadData *data = (threadData *)args;
    loadTrustedProcess(data->index, data->opener, data->mySimulatedHubCred, data->myProcLoaderObj,
                       data->myProcObj, data->myServiceObj[0], data->myMemObj);
    return NULL;
}

#define MEMORY_BUFFER_CROSS_AVM_NORMAL 101
#define MEMORY_BUFFER_CROSS_AVM_LEND_NO_UNMAP 102

typedef struct {
    uint32_t type;
    bool isShared;
    uint32_t blockedMS;
    uint64_t bufSize;
    Object *memObj;
    Object testServiceObj;
} mbCrossingData;

static void memoryBufferCrossAVM(Object *memObj, uint32_t blockedMS, uint32_t bufSize,
                                 const ITAccessPermissions_rules *confRules, Object testServiceObj)
{
    ASSERT_EQ(Object_OK, RemoteShareMemory_attachConfinement(confRules, memObj));
    ASSERT_EQ(Object_OK,
              ITestService_shareMemoryBuffer(testServiceObj, blockedMS, bufSize, *memObj));
}

static void memoryBufferCrossAVMNoUnmap(Object *memObj, uint32_t blockedMS, uint32_t bufSize,
                                        const ITAccessPermissions_rules *confRules,
                                        Object testServiceObj)
{
    if (ITAccessPermissions_removeSelfAccess == confRules->specialRules) {
        ASSERT_EQ(Object_OK, RemoteShareMemory_attachConfinement(confRules, memObj));
    }

    ASSERT_EQ(Object_OK,
              ITestService_lendMemoryBufferNoUnmap(testServiceObj, blockedMS, bufSize, *memObj));
}

static void *memoryBufferCrossAVM_Preprocess(void *args)
{
    mbCrossingData *data = (mbCrossingData *)args;
    const ITAccessPermissions_rules confRules = _getSimpleConfinement(data->isShared);

    switch (data->type) {
        case MEMORY_BUFFER_CROSS_AVM_NORMAL:
            memoryBufferCrossAVM(data->memObj, data->blockedMS, data->bufSize, &confRules,
                                 data->testServiceObj);
            break;
        case MEMORY_BUFFER_CROSS_AVM_LEND_NO_UNMAP:
            memoryBufferCrossAVMNoUnmap(data->memObj, data->blockedMS, data->bufSize, &confRules,
                                        data->testServiceObj);
            break;
        default:
            printf("WARNING: not supported type!!! so ignored.\n");
            break;
    }

    return NULL;
}

/* isShared:
 *   true  - AVM CA shares a memory buffer with QTVM TA
 *   false - AVM CA lends a memory buffer to QTVM TA
 */
static void memoryBufferCrossAVM_Monitor(bool isShared, Object opener, Object *testServiceObj)
{
    uint64_t bufSize = 5 * 1024 * 1024;  // 5MB
    int bufFd = -1, failCnt = 0;
    const int maxTestIterations = 100;
    void *ptr = NULL;
    pthread_t crosser;
    mbCrossingData args;
    Object memObj = Object_NULL;

    isOk(_getShm(NULL, bufSize, &memObj, false, DEFAULT_DMA_BUF_HEAP));

    ASSERT_EQ(0, Object_unwrapFd(memObj, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_NE(MAP_FAILED, ptr);
    memcpy(ptr, gSharedMsg, strlen(gSharedMsg));
    ASSERT_EQ(0, munmap(ptr, bufSize));

    args.type = MEMORY_BUFFER_CROSS_AVM_NORMAL;
    args.isShared = isShared;
    args.blockedMS = 0U;
    args.bufSize = bufSize;
    args.memObj = &memObj;
    args.testServiceObj = *testServiceObj;

    ASSERT_EQ(0, pthread_create(&crosser, NULL, memoryBufferCrossAVM_Preprocess, &args));

    if (isShared) {
        for (int i = 0; i < maxTestIterations; i++) {
            ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
            if (MAP_FAILED == ptr) {
                ++failCnt;
            } else {
                ASSERT_TRUE(0 == memcmp(ptr, gSharedMsg, strlen(gSharedMsg)));
                ASSERT_EQ(0, munmap(ptr, bufSize));
            }
        }
    }

    ASSERT_EQ(0, pthread_join(crosser, NULL));

    if (isShared) {
        ASSERT_TRUE(0 == failCnt);
    }

    printf("Ready to sleep 1s to wait for completion of reclaiming back the buffer.\n");
    SLEEP_MS(1000);
    printf("Sleeped 1s and the buffer SHOULD be reclaimed back expectedly.\n");

    // After normal/successful sharing/lending, AVM CA should be able to reclaim back the ownership
    // of the buffer.
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_NE(MAP_FAILED, ptr);
    ASSERT_EQ(0, munmap(ptr, bufSize));

    Object_ASSIGN_NULL(memObj);
}

/* AVM CA shares/lends a memory buffer to QTVM TA1.
 * After QTVM TA1 gets the shared/lent memory buffer,
 *   it shares/lends it to QTVM TA2 further.
 */
static void FurtherShareMemoryBuffer(bool isShared, Object opener, Object *testServiceObj)
{
    uint64_t bufSize = 5 * 1024 * 1024;  // 5MB
    int bufFd = -1;
    void *ptr = NULL;
    const ITAccessPermissions_rules confRules = _getSimpleConfinement(isShared);
    Object memObj = Object_NULL;
    uint32_t targetUid =
        gRemoteConn == OEMVM ? CCommonTestService_UID : CEmbeddedCommonTestService_UID;

    isOk(_getShm(NULL, bufSize, &memObj, false, DEFAULT_DMA_BUF_HEAP));
    if (ITAccessPermissions_removeSelfAccess == confRules.specialRules) {
        ASSERT_EQ(Object_OK, RemoteShareMemory_attachConfinement(&confRules, &memObj));
    }

    ASSERT_EQ(0, Object_unwrapFd(memObj, &bufFd));
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_NE(MAP_FAILED, ptr);
    ASSERT_EQ(0, munmap(ptr, bufSize));

    ASSERT_EQ(Object_OK,
              ITestService_furtherShareMemoryBuffer(*testServiceObj, bufSize, targetUid, memObj));

    Object_ASSIGN_NULL(memObj);
}

static void initMemoryObj(Object *memObj, bool isShared, uint64_t *bufSize, uint64_t *costTime)
{
    // Release of memObj should be called in upper layer
    int bufFd = -1;
    void *ptr = NULL;
    uint64_t startTime = 0, endTime = 0;
    startTime = vm_osal_getCurrentTimeUs();
    isOk(_getShm(NULL, *bufSize, memObj, !isShared, SYSTEM_DMA_BUF_HEAP));
    endTime = vm_osal_getCurrentTimeUs();
    *costTime = endTime - startTime;

    ASSERT_EQ(0, Object_unwrapFd(*memObj, &bufFd));
    ptr = mmap(NULL, *bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_NE(MAP_FAILED, ptr);
    memcpy(ptr, gSharedMsg, strlen(gSharedMsg));
    ASSERT_EQ(0, munmap(ptr, *bufSize));
}

static void passLargeCopyBufferToQTVM(Object *TestServiceObj, uint32_t bufSizeKB, uint32_t sleepMs,
                                      uint32_t iteration)
{
    const uint32_t targetUid = CAllPrivilegeTestService_UID;
    const uint32_t buffSize = bufSizeKB * 1024;

    char *echoIn = (char *)malloc(buffSize * sizeof(char));
    ASSERT_NE(echoIn, nullptr);
    char *echoOut = (char *)malloc(buffSize * sizeof(char));
    ASSERT_NE(echoOut, nullptr);

    size_t echoInLen = buffSize * sizeof(char);
    size_t echoOutLen = buffSize * sizeof(char);

    if (echoIn == NULL || echoOut == NULL) {
        if (echoIn != NULL) {
            free(echoIn);
        }

        if (echoOut != NULL) {
            free(echoOut);
        }

        return;
    }

    for (uint32_t it = 0; it < iteration; it++) {
        memset(echoIn, it, echoInLen);
        memset(echoOut, 0, echoOutLen);
        ASSERT_EQ(Object_OK, ITestService_simpleShareCopyBuffer(*TestServiceObj, echoIn, echoInLen,
                                                                echoOut, echoOutLen, &echoOutLen));
        ASSERT_EQ(echoOutLen, echoInLen);
        ASSERT_EQ(0, memcmp(echoIn, echoOut, echoOutLen));
        if (sleepMs > 0) {
            SLEEP_MS(sleepMs);
        }
    }

    free(echoIn);
    free(echoOut);
}

static void passLargeDMABufferToQTVM(bool isShared, bool isMemReused, Object *testServiceObj,
                                     uint32_t iterations)
{
    uint64_t getMemObjTime = 0;
    uint64_t bufSize = gEnv->memorySizeMB * 1024 * 1024;
    vector<uint64_t> iterationResult;
    Object memObj = Object_NULL;
    ASSERT_TRUE(bufSize > 0);

    initMemoryObj(&memObj, isShared, &bufSize, &getMemObjTime);

    for (uint32_t it = 0; it < iterations; it++) {
        if (isMemReused) {
            // share/lend same memObj repeatedly
            ASSERT_EQ(Object_OK,
                      ITestService_simpleShareMemoryBuffer(*testServiceObj, bufSize, memObj));
        } else {
            // share/lend different memObj repeatedly
            Object_ASSIGN_NULL(memObj);
            initMemoryObj(&memObj, isShared, &bufSize, &getMemObjTime);
            iterationResult.push_back(getMemObjTime);
            ASSERT_EQ(Object_OK,
                      ITestService_simpleShareMemoryBuffer(*testServiceObj, bufSize, memObj));
            Object_ASSIGN_NULL(memObj);
        }
        // stepwise growth sleeping time, 500ms for 512MB increment
        SLEEP_MS(500 * (((gEnv->memorySizeMB + 511) & (~511)) >> 9));
    }

    Object_ASSIGN_NULL(memObj);

    printf("AllocTime: dmabuff size =  %" PRIu64 ", iter = %d, avg Time =  %" PRIu64 " \n", bufSize,
           iterations, _getAverage(iterationResult));
}

static void QTVMAllocMem(Object opener, Object mSimulatedHubCred, Object *TestServiceObj,
                         size_t poolSize)
{
    ASSERT_EQ(Object_OK, ITEST_OPEN(opener, CAllPrivilegeTestService_UID, mSimulatedHubCred,
                                    TestServiceObj, gMinkConn));
    ASSERT_EQ(Object_OK, ITestService_QTVMAllocMemory(*TestServiceObj, poolSize));
}

static void _printBytesAsHex(const char *label, uint8_t *data, size_t len)
{
    char buf[200] = {0};
    int32_t pos = 0;

    for (size_t i = 0; i < len; i++) {
        pos += snprintf(&buf[pos], sizeof(buf) - pos - 1, "%02X", data[i]);
    }

    printf("%s : %s\n", label, buf);
}

static void _printString(const char *label, char *data)
{
    printf("%s : %s\n", label, data);
}

static void _envCredentialsTest(Object cred, bool isPlatformInfo)
{
    uint8_t data[100] = {0};
    uint32_t version = 0;
    size_t lenOut = 0;

    ASSERT_EQ(Object_OK,
              ICredentials_getValueByName(cred, "eid", strlen("eid"), data, sizeof(data), &lenOut));
    _printBytesAsHex("vmuuid", data, lenOut);

    ASSERT_EQ(Object_OK, ICredentials_getValueByName(cred, "edmn", strlen("edmn"), data,
                                                     sizeof(data), &lenOut));
    _printBytesAsHex("EEDomain", data, lenOut);

    ASSERT_EQ(Object_OK, ICredentials_getValueByName(cred, "edid", strlen("edid"), data,
                                                     sizeof(data), &lenOut));
    _printBytesAsHex("EEDistinguishedId", data, lenOut);

    ASSERT_EQ(Object_OK,
              ICredentials_getValueByName(cred, "edn", strlen("edn"), data, sizeof(data), &lenOut));
    _printString("EEDistinguishedName", (char *)data);

    if (isPlatformInfo) {
        ASSERT_EQ(Object_OK, ICredentials_getValueByName(cred, "ever", strlen("ever"), &version,
                                                         sizeof(version), &lenOut));
        ASSERT_EQ(version, QTVM_PLATFORM_VERSION);
        printf("ever : %d, major: %d, minor %d, patch: %d\n", version, version >> 28,
               (version >> 16) & 0xFFF, version & 0xFFFF);
    }
}

/*
 * --------------- TEST CASES DEFINITION ---------------
 */
/**
 * @brief Composite secenario opening embedded process testing
 *
 * Open embedded process successfully when MinkPlatformTest connects to QTVM and fail to open
 * embedded process when MinkPlatformTest connects to OEMVM
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveOpenEmbeddedServiceRemote))
{
    if (!gMinkConn && gRemoteConn != QTVM) {
        ASSERT_EQ(ITEnv_ERROR_NOT_FOUND,
                  ITEST_OPEN(mClientOpener, CEmbeddedAllPrivilegeTestService_UID, mSimulatedHubCred,
                             &mTestServiceObj[0], gMinkConn));
    } else {
        ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CEmbeddedAllPrivilegeTestService_UID,
                                        mSimulatedHubCred, &mTestServiceObj[0], gMinkConn));
        ASSERT_EQ(Object_OK, ITestService_printHello(mTestServiceObj[0]));
    }
}

/*
 * --------------- POSITIVE TEST CASES ---------------
 */

/**
 * @brief Positive functional
 *
 * Open local service in LA.
 *
 */
TEST_F(TvmdTest, UNSTABLE_TEST(PositiveOpenLocalServiceTest))
{
    size_t uid_len = sizeof(gHLOSTestServiceUid) / sizeof(uint32_t);

    if (gMinkConn) {
        ASSERT_EQ(Object_OK, IMinkdRegister_registerServices(mRegObj, gHLOSTestServiceUid, uid_len,
                                                             gEnv->mIModule));

        // open local service
        ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CAVMCommonTestService_UID, mSimulatedHubCred,
                                        &mTestServiceObj[0], gMinkConn));
        ASSERT_FALSE(Object_isNull(mTestServiceObj[0]));
    }
}

/**
 * @brief Positive functional
 *
 * Re-register a service in LA.
 *
 */
TEST_F(TvmdTest, UNSTABLE_TEST(PositiveReregisterLocalServiceTest))
{
    size_t uid_len = sizeof(gHLOSTestServiceUid) / sizeof(uint32_t);

    if (gMinkConn) {
        // register a service
        ASSERT_EQ(Object_OK, IMinkdRegister_registerServices(mRegObj, gHLOSTestServiceUid, uid_len,
                                                             gEnv->mIModule));

        SLEEP_MS(100);
        // re-register a service
        ASSERT_EQ(Object_OK, IMinkdRegister_registerServices(mRegObj, gHLOSTestServiceUid, uid_len,
                                                             gEnv->mIModule));

        // open local service
        ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CAVMCommonTestService_UID, mSimulatedHubCred,
                                        &mTestServiceObj[0], gMinkConn));
        ASSERT_FALSE(Object_isNull(mTestServiceObj[0]));
    }
}

/**
 * @brief Positive functional
 *
 * The QRTR connection test case which establish a connection from QTVM/OEMVM client to AVM service
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveRemoteTVMToAVMQRTRTest))
{
    size_t uid_len = sizeof(gHLOSTestServiceUid) / sizeof(uint32_t);

    if (gMinkConn) {
        isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                                &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                                mShmList.getShm(gFileName[0], true)));
        // register a service
        ASSERT_EQ(Object_OK, IMinkdRegister_registerServices(mRegObj, gHLOSTestServiceUid, uid_len,
                                                             gEnv->mIModule));

        // test code
        ASSERT_EQ(Object_OK, ITestService_QRTRConn(mTestServiceObj[0], ITestService_POSITIVETEST));
    }
}

/**
 * @brief Verify whether it can discovery the Service which run on QTEE.
 */
#ifndef OFFTARGET
TEST_F(TvmdTest, ONTARGET_TEST(PositiveOpenRemoteQTEEService))
{
    if (gMinkConn) {
        ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CAppClient_UID, mSimulatedHubCred,
                                        &mTestServiceObj[0], gMinkConn));
    }
}
#endif

/**
 * @brief Positive functional
 *
 * The SMCINVOKE connection test case which AVM client
 * connect to QTEE service with ITzdRegister_getClientEnv interface.
 */
#if !defined(OFFTARGET) && defined(HLOSMINKD_UNIX_TZD_SERVER)
TEST_F(TvmdTest, ONTARGET_TEST(PositiveONTARGETUntrustedClientServiceDiscovery))
{
    if (gMinkConn) {
        int32_t ret = Object_OK;
        const int8_t clientId[] = "untrusted_client";
        // {143, 62}
        uint32_t const wl_list[] = {CDiagnostics_UID, CAppLoader_UID};
        uint8_t const Cred[] = {0xa1, 0x01, 0x19, 0x03, 0xe8};

        ret = ITzdRegister_getClientEnv(mSsgtzdOpener, (const signed char *)clientId,
                                        sizeof(clientId), Cred, sizeof(Cred), wl_list,
                                        sizeof(wl_list) / sizeof(uint32_t), &mProcObj[0]);
        ASSERT_TRUE(ret == Object_OK);

        ASSERT_FALSE(Object_isNull(mProcObj[0]));
        ASSERT_EQ(Object_OK, IClientEnv_open(mProcObj[0], CDiagnostics_UID, &mTestServiceObj[0]));
        ASSERT_FALSE(Object_isNull(mTestServiceObj[0]));
    }
}

/**
 * @brief Positive functional
 *
 * The SMCINVOKE connection test case which AVM client
 * connect to QTEE service with ITzdRegister_getTrustedClientEnv interface.
 */
TEST_F(TvmdTest, ONTARGET_TEST(PositiveONTARGETTrustedClientServiceDiscovery))
{
    if (gMinkConn) {
        int32_t ret = Object_OK;
        const int8_t clientId[] = "trusted_client";
        uint8_t const Cred[] = {0xa1, 0x01, 0x19, 0x03, 0xe8};

        ret = ITzdRegister_getTrustedClientEnv(mSsgtzdOpener, (const signed char *)clientId,
                                               sizeof(clientId), Cred, sizeof(Cred), &mProcObj[0]);
        ASSERT_TRUE(ret == Object_OK);

        ASSERT_FALSE(Object_isNull(mProcObj[0]));
        ASSERT_EQ(Object_OK, IClientEnv_open(mProcObj[0], CDiagnostics_UID, &mTestServiceObj[0]));
        ASSERT_FALSE(Object_isNull(mTestServiceObj[0]));
    }
}
#endif

/**
 * @brief Positive functional
 *
 * load an embedded process
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveOpenEmbeddedService))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveOpenEmbeddedService(mTestServiceObj[0]));
}

/**
 * @brief Positive functional
 *
 * load a process from buffer
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveLoadTrustedProcess))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
}

/**
 * @brief Positive functional
 *
 * tprocess1 shares a memObj with tprocess2
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveLocalMemSharing))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[1], true)));
    ASSERT_EQ(Object_OK,
              ITestService_positiveMemSharing(mTestServiceObj[0], CCommonTestService_UID));
}

/**
 * @brief Positive functional
 *
 * tprocess1 shares a memObj with tprocess2
 *
 */
TEST_F(TvmdTest, OFFTARGET_TEST(PositiveRemoteMemSharing))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK,
              ITestService_positiveMemSharing(mTestServiceObj[0], CEmbeddedCommonTestService_UID));
}

/**
 * @brief Positive functional
 *
 * Invoke TA to run unit test of local memory service in QTVM
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveTestLocalMemoryService))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveTestMemoryService(mTestServiceObj[0],
                                                                CAllPrivilegeTestService_UID));
}

/**
 * @brief Positive functional
 *
 * Invoke TA to run unit test of remote memory service in QTVM
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveTestRemoteMemoryService))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveTestMemoryService(
                             mTestServiceObj[0], CEmbeddedAllPrivilegeTestService_UID));
}

/**
 * @brief Positive functional
 *
 * a test service of credential
 *
 */
TEST_F(TvmdTest, UNSTABLE_TEST(PositiveCredentialsDealerServiceBasicUsage))
{
    // load credDealerService of which neverUnload=TRUE
    isOk(loadTrustedProcess(CCredDealerTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[3], true)));

    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CEmbeddedAllPrivilegeTestService_UID,
                                    mSimulatedHubCred, &mTestServiceObj[0], gMinkConn));

    // store a credentials in credDealer
    ASSERT_EQ(Object_OK, ITestService_appendCredentials(mTestServiceObj[0]));

    // query a credentials which is stored in credDealer
    ASSERT_EQ(Object_OK, ITestService_checkCredentials(mTestServiceObj[1]));

    // query a credentials which is NOT stored in credDealer
    ASSERT_EQ(Object_ERROR, ITestService_checkCredentials(mTestServiceObj[1]));

    // terminate credDealerService of which neverUnload=TRUE
    ASSERT_EQ(Object_ERROR_UNAVAIL, ITestService_raiseSignal(mTestServiceObj[1], SIGKILL));
    // tell teardown not to check if it is dead
    mProcObj[1] = Object_NULL;
}

/**
 * @brief Positive functional
 *
 * an embedded process's credential should be correct for else process even when it is dead
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveCredentialsAccessAfterClose))
{
    // load credDealerService
    isOk(loadTrustedProcess(CCredDealerTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[3], true)));
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));

    ASSERT_EQ(Object_OK, ITestService_appendCredentials(mTestServiceObj[0]));

    // Release object to cause the process to shut down
    Object_ASSIGN_NULL(mProcObj[0]);
    SLEEP_MS(100);

    ASSERT_EQ(Object_OK, ITestService_checkCredentials(mTestServiceObj[1]));
}

/**
 * @brief Positive functional
 *
 * an embedded process loads itself and calls service in it
 *
 */

// There is a known issue in minksocket. It will be fixed on Pakala, so I need to disable this test
// case.
TEST_F(TvmdTest, UNSTABLE_TEST(PositiveOneServiceCallSelfService))
{
    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CEmbeddedCommonTestService_UID,
                                    mSimulatedHubCred, &mTestServiceObj[0], gMinkConn));
    ASSERT_EQ(Object_OK,
              ITestService_callService(mTestServiceObj[0], CEmbeddedCommonTestService_UID));
}

/**
 * @brief Positive functional
 *
 * an embedded process loads another embedded process and calls service in it
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveEmbeddedCallEmbedded))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveEmbeddedCallEmbedded(mTestServiceObj[0]));
}

/**
 * @brief Positive functional
 *
 * a loaded process opens an embedded process and invokes method on it. The TP runs in OEMVM and the
 * embedded service runs in QTVM.
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveLoadedCallEmbedded))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK,
              ITestService_callService(mTestServiceObj[0], CEmbeddedCommonTestService_UID));
}

/**
 * @brief Positive functional
 *
 * an process kills itself
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveProcessAbnormalDeath))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveProcessAbnormalDeath(mTestServiceObj[0]));
}

/**
 * Ensure a new TModule is created for new instances of the same binary.
 *
 * Caveat: We cannot directly observe TProcess or TModule in zombie state, but they should both be
 * zombies after step 3.
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveUniqueTModulePerProcessInstance))
{
    Object memObj = Object_NULL;

    // 1. Load TA1 and CredDealer
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(loadTrustedProcess(CCredDealerTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[3], &mProcObj[3], &mTestServiceObj[3],
                            mShmList.getShm(gFileName[3], true)));

    // 2. TA1 calls CredDealer service
    ASSERT_EQ(Object_OK, ITestService_appendCredentials(mTestServiceObj[0]));

    // 3. TA1_forceClose() -> TProcess to zombie state
    ASSERT_EQ(Object_OK, ITProcessController_forceClose(mProcObj[0]));
    SLEEP_MS(100);
    ASSERT_EQ(Object_ERROR_UNAVAIL, ITestService_printHello(mTestServiceObj[0]));

    // 4. Another caller spins up TA1 -> a new TProcessController object created
    memObj = mShmList.getShm(gFileName[0], true);
    ASSERT_FALSE(Object_isNull(memObj));
    ASSERT_EQ(Object_OK, ITProcessLoader_loadFromBuffer(mProcLoaderObj[0], memObj, &mProcObj[1]));
    ASSERT_FALSE(Object_isNull(mProcObj[1]));

    // 5. A new TModule created, but the "zombie" TModule still exists
    ASSERT_EQ(Object_OK, ITestService_checkCredentials(mTestServiceObj[3]));

    // 6. See that the only the new TModule will be found during TModule_open()
    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CAllPrivilegeTestService_UID, mSimulatedHubCred,
                                    &mTestServiceObj[1], gMinkConn));
    ASSERT_EQ(Object_OK, ITestService_printHello(mTestServiceObj[1]));

    // Release process object to avoid failure during TearDown
    Object_ASSIGN_NULL(mProcObj[0]);
}

/**
 * @brief Positive functional
 *
 * AVM CA shares a memory buffer with QTVM/OEMVM TA.
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(PositiveONTARGETShareMemoryBufferWithVM))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(memoryBufferCrossAVM_Monitor(true, mClientOpener, &mTestServiceObj[0]));
}

/**
 * @brief Positive functional
 *
 * AVM CA lends a memory buffer to QTVM/OEMVM TA.
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(PositiveONTARGETLendMemoryBufferToVM))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(memoryBufferCrossAVM_Monitor(false, mClientOpener, &mTestServiceObj[0]));
}

// TODO: in phase 3, after we have access control, this should be negative.
/**
 * @brief Positive functional
 *
 * AVM CA shares a memory buffer with QTVM/OEMVM TA1. And then TA1 shares it with the TA2 which is
 * running on the same VM.
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(PositiveONTARGETFurtherShareMemoryBufferInVM))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[1], true)));
    isOk(FurtherShareMemoryBuffer(true, mClientOpener, &mTestServiceObj[0]));
}

// TODO: in phase 3, after we have access control, this should be negative.
/**
 * @brief Positive functional
 *
 * AVM CA lends a memory buffer to QTVM/OEMVM TA1. And then QTVM TA1 lends it to the TA2 which is
 * running on the same VM..
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(PositiveONTARGETFurtherLendMemoryBufferInVM))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[1], true)));
    isOk(FurtherShareMemoryBuffer(false, mClientOpener, &mTestServiceObj[0]));
}

/**
 * @brief Positive functional
 *
 *   After QTVM TA releases the memory buffer(allocated from QTVM memory service),
 *   it should trigger to give the memory buffer back to the memory heap in QTVM.
 *   i.e. remainingBytes of the memory heap should increase
 *     by the size of given-back memory buffer.
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveReclaimBufferTest))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveReclaimBufferTest(mTestServiceObj[0]));
}

/**
 * @brief Positive functional
 *
 * raise different abnormal signals and check whether it can print its backtrace.
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveBackTraceTest))
{
    int32_t signumArray[] = {
        SIGSEGV,
        SIGFPE,
        SIGABRT,
        SIGILL,
    };

    for (uint32_t i = 0; i < sizeof(signumArray) / sizeof(int32_t); ++i) {
        const char *signalStr = strsignal(signumArray[i]);
        printf("Raise Signal %d:%s\n", signumArray[i], signalStr ? signalStr : "Unknown Signal");
        isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                                &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                                mShmList.getShm(gFileName[0], true)));

        ASSERT_EQ(Object_ERROR_DEFUNCT,
                  ITestService_backtracetest(mTestServiceObj[0], signumArray[i]));
        Object_ASSIGN_NULL(mProcLoaderObj[0]);
        Object_ASSIGN_NULL(mProcObj[0]);
        Object_ASSIGN_NULL(mTestServiceObj[0]);
    }
}

/**
 * @brief Positive functional
 *
 * an embedded process call for power service to get wakelock
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveEmbeddedServiceWakeLockTest))
{
    // Wakelock tests assume that no one is currently holding the wake lock, so release it.
    Object_ASSIGN_NULL(mWakeLock);

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveEmbeddedServiceWakeLockTest(mTestServiceObj[0]));
}

/**
 * @brief Positive functional
 *
 * an downloaded process call for power service to get wakelock
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveDownloadedServiceWakeLockTest))
{
    // Wakelock tests assume that no one is currently holding the wake lock, so release it.
    Object_ASSIGN_NULL(mWakeLock);

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));

    ASSERT_EQ(Object_OK, ITestService_wakeLockTest(mTestServiceObj[0], TEST_OPEN_POWER_SERVICE));
    ASSERT_EQ(Object_OK, ITestService_wakeLockTest(mTestServiceObj[0], TEST_ACQUIRE_WAKE_LOCK));
    ASSERT_EQ(Object_OK, ITestService_wakeLockTest(mTestServiceObj[0], TEST_CLOSE_POWER_SERVICE));
}

/**
 * @brief Positive functional
 *
 * an downloaded process call for qtvm's power service to get wakelock
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveQTVMServiceWakeLockTest))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveQTVMWakeLockTest(mTestServiceObj[0]));
}

/**
 * @brief Positive functional
 *
 * two embedded processes call for power service to get wakelock, check if wakelock will release
 * when one process release wakelock.
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveMultipleWakeLockTest))
{
    // Wakelock tests assume that no one is currently holding the wake lock, so release it.
    Object_ASSIGN_NULL(mWakeLock);

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveMultipleWakeLockTest(mTestServiceObj[0]));
}

/**
 * @brief Positive functional
 *
 * a downloadable process retry to acquire wake lock when it failed first time.
 *
 */
TEST_F(TvmdTest, OFFTARGET_TEST(PositiveOFFTARGETWakeLockTestRetryWriteWakeLockFile))
{
    // Wakelock tests assume that no one is currently holding the wake lock, so release it.
    Object_ASSIGN_NULL(mWakeLock);

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_wakeLockTest(mTestServiceObj[0], TEST_OPEN_POWER_SERVICE));

    // Set wake lock to read-only. Acquiring the file with write permissions will fail and
    // return ITPowerService_ERROR_RELEASE_WAKELOCK.
    chmod("wake_lock", S_IRUSR | S_IRGRP | S_IROTH);

    ASSERT_EQ(ITPowerService_ERROR_GET_WAKELOCK,
              ITestService_wakeLockTest(mTestServiceObj[0], TEST_ACQUIRE_WAKE_LOCK));

    // Remove the wake_lock file and it can be acquired this time.
    system("rm -rf wake_lock");
    ASSERT_EQ(Object_OK, ITestService_wakeLockTest(mTestServiceObj[0], TEST_ACQUIRE_WAKE_LOCK));
    ASSERT_EQ(Object_OK, ITestService_wakeLockTest(mTestServiceObj[0], TEST_CLOSE_POWER_SERVICE));
}

/**
 * @brief Positive functional
 *
 * Tunnel Invoke Test
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(PositiveONTARGETGetQTEEAppService))
{
    const char appName[] = "tuiauthapp";

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(IAppClient_ERROR_APP_NOT_FOUND,
              ITestService_getAppObject(mTestServiceObj[0], appName, strlen(appName) + 1));
}

/**
 * @brief Positive functional
 *
 * Tunnel Invoke Test
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(PositiveONTARGETGetQTEEService))
{
    uint8_t *derivedKey = NULL;
    const size_t keySize = 32;
    size_t outputLenout = 0;

    derivedKey = (uint8_t *)malloc(sizeof(uint8_t) * keySize);
    if (derivedKey == NULL) {
        printf("%s : %d malloc failure \n", __func__, __LINE__);
        return;
    }

    memset(derivedKey, 0, keySize);

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));

    ASSERT_EQ(Object_OK,
              ITestService_deriveKey(mTestServiceObj[0], derivedKey, keySize, &outputLenout));
    ASSERT_EQ(outputLenout, keySize);
    printf("Device Key ");
    for (size_t j = 0; j < keySize; j++) {
        printf("0x%x, ", derivedKey[j]);
    }
    printf("\n");

    free(derivedKey);
}

#ifndef OFFTARGET
/**
 * @brief a test for QTVM passing credential to QTEE.
 *
 * Load an QTEE TA, and print credenital contents in qsee log.
 * Temporarily disable this test case due to dependence on PW's script update to push QTEE TA to LA.
 */
TEST_F(TvmdTest, UNSTABLE_TEST(PositiveONTARGETQTVMPassCredToQTEE))
{
    Object clientEnv = Object_NULL;
    Object appLoader = Object_NULL;
    Object appController = Object_NULL;
    struct stat st = {0};
    char taBuffer[50000] = {0};

    FILE *pfile = fopen(gFileName[5], "rb");
    ASSERT_TRUE(pfile != NULL);
    isOk(stat(gFileName[5], &st));
    ASSERT_EQ(fread(taBuffer, 1, st.st_size, pfile), st.st_size);

    ASSERT_EQ(Object_OK, TZCom_getClientEnvObject(&clientEnv));
    ASSERT_EQ(Object_OK, IClientEnv_open(clientEnv, CAppLoader_UID, &appLoader));
    ASSERT_EQ(Object_OK,
              IAppLoader_loadFromBuffer(appLoader, taBuffer, st.st_size, &appController));
    sleep(1);

    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CEmbeddedAllPrivilegeTestService_UID,
                                    mSimulatedHubCred, &mTestServiceObj[0], gMinkConn));
    ASSERT_EQ(Object_OK, ITestService_passCredToQTEE(mTestServiceObj[0]));
}
#endif

/**
 * @brief Positive functional
 *
 * Test auto-start feature.
 *
 */
TEST_F(TvmdTest, OFFTARGET_TEST(PositiveOFFTARGETAutoStartCoreService))
{
    if (!gMinkConn) {
        printf("This test case is not applicable for direct connections.\n");
    } else {
        const char *minkDaemon = gRemoteConn == OEMVM ? VAL2STR(XVM_MINK) : VAL2STR(MINK);
        char command[255] = {0};
        snprintf(command, sizeof(command) - 1,
                 "ps -A |grep -w \"%s\"| awk '{print $1}' | xargs kill -9 ", minkDaemon);
        printf("\nKilling %s\n", minkDaemon);
        printf("\n%s\n", command);
        // wait for restart TVMMink
        SLEEP_MS(2000);
        isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                                &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                                mShmList.getShm(gFileName[0], true)));
        ASSERT_EQ(Object_OK, ITestService_positiveAutoStartCoreService(mTestServiceObj[0]));
    }
}

/**
 * @brief Positive functional
 *
 * Kill Mink daemon and checking whether can re-start successfully.
 *
 */
TEST_F(TvmdTest, OFFTARGET_TEST(PositiveOFFTARGETRestartMink))
{
    if (!gMinkConn) {
        printf("This test case is not applicable when under direct connection.\n");
    } else {
        const char *minkDaemon = gRemoteConn == OEMVM ? VAL2STR(XVM_MINK) : VAL2STR(MINK);
        char command[255] = {0};

        isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                                &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                                mShmList.getShm(gFileName[0], true)));
        snprintf(command, sizeof(command) - 1,
                 "ps -A |grep -w \"%s\"| awk '{print $1}' | xargs kill -9 ", minkDaemon);
        printf("\nKilling %s\n", minkDaemon);
        printf("\n%s\n", command);
        system(command);

        // wait for restart Mink daemon
        SLEEP_MS(2000);

        ASSERT_EQ(Object_ERROR_UNAVAIL, ITestService_printHello(mTestServiceObj[0]));
        isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                                &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                                mShmList.getShm(gFileName[1], true)));
    }
}

/**
 * @brief Positive functional
 *
 * Kill TVMPrelauncher and checking whether can re-start successfully.
 *
 */
TEST_F(TvmdTest, OFFTARGET_TEST(PositiveOFFTARGETRestartPrelauncher))
{
    const char *prelauncherDaemon =
        gRemoteConn == OEMVM ? VAL2STR(XVM_PRELAUNCHER) : VAL2STR(PRELAUNCHER);
    char command[255] = {0};

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[1], true)));

    snprintf(command, sizeof(command) - 1,
             "ps -A |grep -w \"%s\"| awk '{print $1}' | xargs kill -9 ", prelauncherDaemon);
    printf("Killing %s\n", prelauncherDaemon);
    system(command);
    // wait for restart TVMPrelauncher
    SLEEP_MS(2000);

    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[2], &mProcObj[2], &mTestServiceObj[2],
                            mShmList.getShm(gFileName[1], true)));
}

/*
 * @brief Positive functional
 *
 * Test autoStart core service restart feature.
 *
 */
TEST_F(TvmdTest, OFFTARGET_TEST(PositiveRestartEmbeddedAutoStartService))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_positiveRestartEmbeddedAutoStartService(mTestServiceObj[0]));
}

/**
 * @brief
 *
 * PVM triggers QTVM TA invocation with TZ App for both buffer and memObj
 * PW does not support pushing TZ TA to /vendor/bin yet
 */
TEST_F(TvmdTest, UNSTABLE_TEST(PositiveQTVM2TZConnTest))
{
    Object memObj = Object_NULL;

    // It will be released by mShmList.
    memObj = mShmList.getShm(gFileName[6], true);
    ASSERT_FALSE(Object_isNull(memObj));

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_QTVM2TZtest(mTestServiceObj[0], memObj));
}

/**
 * @brief Local PlatformInfo test
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveLocalPlatformInfoTest))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_setRunningVM(mTestServiceObj[0], gRemoteConn));
    ASSERT_EQ(Object_OK, ITestService_localPlatformInfoTest(mTestServiceObj[0]));
}

/**
 * @brief Remote PlatformInfo test
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveRemotePlatformInfoTest))
{
    Object testCredObj = Object_NULL;
    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, gPlatformInfoServiceUid[gRemoteConn],
                                    mSimulatedHubCred, &testCredObj, gMinkConn));
    _envCredentialsTest(testCredObj, true);
    Object_ASSIGN_NULL(testCredObj);
}

/**
 * @brief Positive functional
 *
 * the credentail testcase which remote service opens Mink's TA service
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveRemoteCallerCredentialsTest))
{
    Object cred = Object_NULL;
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));

    ASSERT_EQ(Object_OK, ITestService_queryCred(mTestServiceObj[0], &cred));

    _envCredentialsTest(cred, false);
    Object_ASSIGN_NULL(cred);
}

/**
 * @brief Positive functional
 *
 * the credential testcase that local TA service opens another local TA service
 *
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveLocalCallerCredentialsTest))
{
    // Considering running the two TA services on the same VM, we use two downloadable TPs as test
    // services.
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[1], true)));
    ASSERT_EQ(Object_OK, ITestService_queryTACred(mTestServiceObj[0], CCommonTestService_UID));
}

/**
 * @brief
 *
 * the credentail testcase that the local TA service running on OEMVM opens the TA service running
 * on QTVM.
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveCrossVMCredentialsTest))
{
    // Considering running the two TA services on the different VM, we use one downloadable TPs and
    // one embedded TPs as test services.
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK,
              ITestService_queryTACred(mTestServiceObj[0], CEmbeddedAllPrivilegeTestService_UID));
}

/**
 * @brief
 *
 * Run sample CA + sample apps
 */
TEST_F(TvmdTest, COMMON_TEST(PositiveSampleCode))
{
#if defined(OFFTARGET)
    ASSERT_EQ(0, system("../../mink/SDK/QTVM/apps/sampleCA/QTVMSampleCA "
                        "../../mink/SDK/QTVM/apps/sampleapp/bin/sampleapp qtvm"));
    ASSERT_EQ(0, system("../../mink/SDK/QTVM/apps/sampleCA/QTVMSampleCA "
                        "../../mink/SDK/QTVM/apps/sampleapp_cpp/bin/sampleapp_cpp qtvm"));
    if (gRemoteConn == OEMVM) {
        ASSERT_EQ(0, system("../../mink/SDK/QTVM/apps/sampleCA/QTVMSampleCA "
                            "../../mink/SDK/QTVM/apps/sampleapp/bin/sampleapp oemvm"));
        ASSERT_EQ(0, system("../../mink/SDK/QTVM/apps/sampleCA/QTVMSampleCA "
                            "../../mink/SDK/QTVM/apps/sampleapp_cpp/bin/sampleapp_cpp oemvm"));
    }
#else
    ASSERT_EQ(0, system("QTVMSampleCA sampleapp qtvm"));
    ASSERT_EQ(0, system("QTVMSampleCA sampleapp_cpp qtvm"));
    if (gRemoteConn == OEMVM) {
        ASSERT_EQ(0, system("QTVMSampleCA sampleapp oemvm"));
        ASSERT_EQ(0, system("QTVMSampleCA sampleapp_cpp oemvm"));
    }
#endif
}

/**
 * QTVM unforced shutdown/restart test
 */
TEST_F(TvmdTest, OFFTARGET_TEST(PositiveVMShutdown))
{
    uint32_t targetUid = gRemoteConn == OEMVM ? CTRebootOEMVM_UID : CTRebootVM_UID;
    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, targetUid, mSimulatedHubCred,
                                    &mTestServiceObj[1], gMinkConn));
    // The first arg of ITRebootVM_shutdown is 'restart', it indicates reboot or shutdown the VM.
    // The second arg of ITRebootVM_shutdown is 'force', it indicates whether to shutdown
    // gracefully. When the 'force' parameter is false and there is a service running at the time,
    // the function is expected to return ITRebootVM_ERROR_CANT_SHUTDOWN_GRACEFUL.
    ASSERT_EQ(Object_OK, ITRebootVM_shutdown(mTestServiceObj[1], 0, 0));
}

/**
 * @brief Positive functional
 * Positive unit tests for TAccessControl services in SVM
 */
TEST_F(TvmdTest, UNSTABLE_TEST(PositiveTAccessControlServiceTests))
{
    uint64_t bufSize = 10 * 1024 * 1024;  // 10MB
    const ITAccessPermissions_rules confRulesForShare = _getSimpleConfinement(true);
    const ITAccessPermissions_rules confRulesForLend = _getSimpleConfinement(false);
    Object memObjForShare = Object_NULL, memObjForLend = Object_NULL;

    isOk(_getShm(NULL, bufSize, &memObjForShare, false, DEFAULT_DMA_BUF_HEAP));
    isOk(_getShm(NULL, bufSize, &memObjForLend, false, DEFAULT_DMA_BUF_HEAP));
    ASSERT_EQ(Object_OK, RemoteShareMemory_attachConfinement(&confRulesForLend, &memObjForLend));

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));

    ASSERT_EQ(Object_OK, ITestService_checkAccessPermissions(mTestServiceObj[0], &confRulesForShare,
                                                             sizeof(ITAccessPermissions_rules),
                                                             memObjForShare));
    ASSERT_EQ(Object_OK, ITestService_checkAccessPermissions(mTestServiceObj[0], &confRulesForLend,
                                                             sizeof(ITAccessPermissions_rules),
                                                             memObjForLend));

    Object_ASSIGN_NULL(memObjForShare);
    Object_ASSIGN_NULL(memObjForLend);
}

/*
 * --------------- NEGATIVE TEST CASES ---------------
 */

/**
 * @brief Negative functional
 * Open an unregistered sevice.
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeOpenUnregisteredServiceTest))
{
    ASSERT_NE(Object_OK, ITEST_OPEN(mClientOpener, NOT_USED_TEST_UID, mSimulatedHubCred,
                                    &mTestServiceObj[0], gMinkConn));
}

/**
 * @brief Negative functional
 *
 * The QRTR connection test case which establish a connection from QTVM/OEMVM client to AVM service
 *
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeRemoteTVMToAVMQRTRTest))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_NE(Object_OK, ITestService_QRTRConn(mTestServiceObj[0], ITestService_NEGATIVETEST));
}

/**
 * @brief Negative functional
 *
 * The QTEE connection test case which AVM client
 * connect to QTEE service.
 */
#if !defined(OFFTARGET) && defined(HLOSMINKD_UNIX_TZD_SERVER)
TEST_F(TvmdTest, ONTARGET_TEST(NegativeONTARGETUntrustedClientServiceDiscovery))
{
    int32_t ret = Object_OK;
    const int8_t clientId[] = "trusted_client";
    // {3, 143}
    uint32_t const wl_list[] = {CAppLoader_UID, CDiagnostics_UID};
    uint8_t const Cred[] = {0xa1, 0x01, 0x19, 0x03, 0xe8};

    if (gMinkConn) {
        ret = ITzdRegister_getClientEnv(mSsgtzdOpener, (const signed char *)clientId,
                                        sizeof(clientId), Cred, sizeof(Cred), wl_list,
                                        sizeof(wl_list) / sizeof(uint32_t), &mProcObj[0]);
        ASSERT_TRUE(ret == Object_OK && !Object_isNull(mProcObj[0]));

        ASSERT_NE(Object_OK,
                  IClientEnv_open(mProcObj[0], CDeviceAttestation_UID, &mTestServiceObj[0]));
    }
}
#endif

/**
 * QTVM unforced shutdown/restart test
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeVMShutdown))
{
    uint32_t targetUid = gRemoteConn == OEMVM ? CTRebootOEMVM_UID : CTRebootVM_UID;
    // When the force parameter of ITRebootVM_shutdown is false and there is a service running in
    // the vm at that time, ITRebootVM_shutdown should return
    // ITRebootVM_ERROR_CANT_SHUTDOWN_GRACEFUL, so for testing we need to load a TA and run it.
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[1], true)));

    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, targetUid, mSimulatedHubCred,
                                    &mTestServiceObj[1], gMinkConn));
    // The first arg of ITRebootVM_shutdown is 'restart', it indicates reboot or shutdown the VM.
    // The second arg of ITRebootVM_shutdown is 'force', it indicates whether to shutdown
    // gracefully. When the 'force' parameter is false and there is a service running at the time,
    // the function is expected to return ITRebootVM_ERROR_CANT_SHUTDOWN_GRACEFUL.
    ASSERT_EQ(ITRebootVM_ERROR_CANT_SHUTDOWN_GRACEFUL,
              ITRebootVM_shutdown(mTestServiceObj[1], 0, 0));
    ASSERT_EQ(ITRebootVM_ERROR_CANT_SHUTDOWN_GRACEFUL,
              ITRebootVM_shutdown(mTestServiceObj[1], 1, 0));
}

/**
 * QTVM forced shutdown/restart test. 'Forced' has been disabled, so this will behave the same as
 * unforced.
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeVMShutdownForced))
{
    uint32_t targetUid = gRemoteConn == OEMVM ? CTRebootOEMVM_UID : CTRebootVM_UID;
    // See above
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[1], true)));

    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, targetUid, mSimulatedHubCred,
                                    &mTestServiceObj[1], gMinkConn));
    // See above
    ASSERT_EQ(ITRebootVM_ERROR_CANT_SHUTDOWN_GRACEFUL,
              ITRebootVM_shutdown(mTestServiceObj[1], 0, 1));
    ASSERT_EQ(ITRebootVM_ERROR_CANT_SHUTDOWN_GRACEFUL,
              ITRebootVM_shutdown(mTestServiceObj[1], 1, 1));
}

/**
 * @brief Verify whether it can discovery the Service which doesn't run on QTVM/OEMVM.
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeOpenRemoteQTEEService))
{
    if (!gMinkConn) {
        ASSERT_EQ(ITEnv_ERROR_NOT_FOUND,
                  ITEST_OPEN(mClientOpener, CAppClient_UID, mSimulatedHubCred, &mTestServiceObj[0],
                             gMinkConn));
    }
}

/**
 * Remote caller cannot open local service
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeRemoteCallerLocalService))
{
    // Test services cannot be opened after force close
    ASSERT_EQ(ITEnv_ERROR_LOCAL_EXCLUSIVE,
              ITEST_OPEN(mClientOpener, CTRegisterModule_UID, mSimulatedHubCred,
                         &mTestServiceObj[0], gMinkConn));
}

/**
 * Cannot open a service once a process has been forceClose
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeTrustedProcessForceClose))
{
    Object tmpObj = Object_NULL;

    isOk(loadTrustedProcess(gTrustedUid[1], mClientOpener, mSimulatedHubCred, &mProcLoaderObj[0],
                            &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[1], true)));

    // Close the process
    ASSERT_EQ(Object_OK, ITProcessController_forceClose(mProcObj[0]));

    // Test services cannot be opened after force close
    ASSERT_EQ(ITEnv_ERROR_NOT_FOUND,
              ITEST_OPEN(mClientOpener, gTrustedUid[1], mSimulatedHubCred, &tmpObj, gMinkConn));

    // Release process object to avoid failure during TearDown
    Object_ASSIGN_NULL(mProcObj[0]);
}

/**
 * @brief Negative functional
 *
 * open an unregistered UID
 *
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeCallUnregisteredService))
{
    ASSERT_EQ(ITEnv_ERROR_NOT_FOUND, ITEST_OPEN(mClientOpener, CUnregisteredService_UID,
                                                mSimulatedHubCred, &mTestServiceObj[0], gMinkConn));
}

/**
 * @brief Negative functional
 *
 * pass an invalid memory object to load a process from buffer
 *
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeLoadWithInvalidMemObj))
{
    Object memObj = Object_NULL;

    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, gProcessLoaderUid[gRemoteConn],
                                    mSimulatedHubCred, &mProcLoaderObj[0], gMinkConn));
    ASSERT_EQ(Object_ERROR_BADOBJ,
              ITProcessLoader_loadFromBuffer(mProcLoaderObj[0], memObj, &mProcObj[0]));
}

/**
 * @brief Negative functional
 *
 * an embedded process without proper privilege loads and accesses to another process
 *
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeOpenServiceNotPrivileged))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(ITEnv_ERROR_PRIVILEGE,
              ITestService_negativeOpenServiceNotPrivileged(mTestServiceObj[0]));
}

/**
 * @brief Negative functional
 *
 * an process without proper privilege loads and accesses to another process which runs on
 * QTVM/OEMVM
 *
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeOpenRemoteServiceNotPrivileged))
{
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[1], true)));
    ASSERT_EQ(ITEnv_ERROR_PRIVILEGE,
              ITestService_callService(mTestServiceObj[0], CEmbeddedAllPrivilegeTestService_UID));
}

/**
 * @brief Negative functional
 *
 * Hit retry limit of opening embedded service by mismatching the requested
 * service UID with the binary that it has been paired with.
 *
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeEmbeddedRetryLimit))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(ITEnv_ERROR_EMBED_RETRY, ITestService_negativeEmbeddedRetryLimit(mTestServiceObj[0]));
}

/**
 * @brief Try launching an embedded process which is missing 'neverUnload' property
 */
TEST_F(TvmdTest, COMMON_TEST(NegativeEmbeddedMissingNeverUnload))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(ITEnv_ERROR_EMBED_MISSING_PROPERTY,
              ITestService_negativeEmbeddedMissingNeverUnload(mTestServiceObj[0]));
}

/**
 * @brief Negative functional
 *
 * If the memory buffer is mapping into CA's address space,
 *   then it could not be lent out.
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(NegativeONTARGETLendMappingMemoryBuffer))
{
    uint64_t bufSize = 5 * 1024 * 1024;  // 5MB
    int bufFd = -1;
    void *ptr = NULL;
    Object memObj = Object_NULL;

#if 0
    // OEMVM doesn't support AccessControl.
    if (gRemoteConn == OEMVM) {
        return;
    }
#endif

    isOk(_getShm(NULL, bufSize, &memObj, true, DEFAULT_DMA_BUF_HEAP));

    ASSERT_EQ(0, Object_unwrapFd(memObj, &bufFd));
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_NE(MAP_FAILED, ptr);

    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[1], true)));

    ASSERT_EQ(Object_ERROR_UNAVAIL,
              ITestService_shareMemoryBuffer(mTestServiceObj[0], 0U, bufSize, memObj));

    ASSERT_EQ(0, munmap(ptr, bufSize));
    Object_ASSIGN_NULL(memObj);
}

/**
 * @brief Negative functional
 *
 * If the memory buffer has been lent out and it is still under lending status,
 *   then it could not be mapped to CA's address space.
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(NegativeONTARGETMapLentMemoryBuffer))
{
    uint64_t bufSize = 5 * 1024 * 1024;  // 5MB
    int bufFd = -1;
    void *ptr = NULL;
    pthread_t crosser;
    mbCrossingData args;
    Object memObj = Object_NULL;

    isOk(_getShm(NULL, bufSize, &memObj, false, DEFAULT_DMA_BUF_HEAP));
    ASSERT_EQ(0, Object_unwrapFd(memObj, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_NE(MAP_FAILED, ptr);
    memcpy(ptr, gSharedMsg, strlen(gSharedMsg));
    ASSERT_EQ(0, munmap(ptr, bufSize));

    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[1], true)));

    args.type = MEMORY_BUFFER_CROSS_AVM_NORMAL;
    args.isShared = false;
    args.blockedMS = 1000U;
    args.bufSize = bufSize;
    args.memObj = &memObj;
    args.testServiceObj = mTestServiceObj[0];

    ASSERT_EQ(0, pthread_create(&crosser, NULL, memoryBufferCrossAVM_Preprocess, &args));

    SLEEP_MS(200);
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_EQ(MAP_FAILED, ptr);

    ASSERT_EQ(0, pthread_join(crosser, NULL));

    Object_ASSIGN_NULL(memObj);
}

/**
 * @brief Negative functional
 *
 * If the memory buffer has been lent to QTVM TA and QTVM TA does not
 *   unmap it from its address space, then lender could not reclaim
 *   the lent buffer back successfully.
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(NegativeONTARGETReclaimNoUnmapBuffer))
{
    uint64_t bufSize = 5 * 1024 * 1024;  // 5MB
    int bufFd = -1;
    void *ptr = NULL;
    pthread_t crosser;
    mbCrossingData args;
    Object memObj = Object_NULL;

    isOk(_getShm(NULL, bufSize, &memObj, false, DEFAULT_DMA_BUF_HEAP));
    ASSERT_EQ(0, Object_unwrapFd(memObj, &bufFd));

    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[1], true)));

    args.type = MEMORY_BUFFER_CROSS_AVM_LEND_NO_UNMAP;
    args.isShared = false;
    args.blockedMS = 0U;
    args.bufSize = bufSize;
    args.memObj = &memObj;
    args.testServiceObj = mTestServiceObj[0];

    ASSERT_EQ(0, pthread_create(&crosser, NULL, memoryBufferCrossAVM_Preprocess, &args));

    // invokeRequest is finished now and lender side will start to reclaim the lent buffer.
    ASSERT_EQ(0, pthread_join(crosser, NULL));

    // QTVM TA is still owning the lent buffer.
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    ASSERT_EQ(MAP_FAILED, ptr);

    printf("Ready to let test thread sleep 1s to keep QTVM TA alive\n");
    printf("  to ensure the lent memory buffer is still owned by QTVM TA.\n");
    SLEEP_MS(1000);

    // Note that we need kernel change of removing dma_buf_get()
    //   when ioctl(reclaim) fails.
    // By now, reclaim should fail as expected.
    // Lets shutdown the QTVM TA in advance so as to:
    //   #1. release DMA buf held by QTVM TA.
    //   #2. #1 should be done before final release of the DMA buf.
    Object_ASSIGN_NULL(mTestServiceObj[0]);

    SLEEP_MS(1000);

    Object_ASSIGN_NULL(memObj);
}

/**
 * @brief Negative functional
 * Negative unit tests for TAccessControl services in SVM
 */
TEST_F(TvmdTest, UNSTABLE_TEST(NegativeTAccessControlServiceTests))
{
    uint64_t bufSize = 10 * 1024 * 1024;  // 10MB
    const ITAccessPermissions_rules confRulesForShare = _getSimpleConfinement(true);
    const ITAccessPermissions_rules confRulesForLend = _getSimpleConfinement(false);
    Object memObjForShare = Object_NULL, memObjForLend = Object_NULL;

    isOk(_getShm(NULL, bufSize, &memObjForShare, false, DEFAULT_DMA_BUF_HEAP));
    isOk(_getShm(NULL, bufSize, &memObjForLend, false, DEFAULT_DMA_BUF_HEAP));
    ASSERT_EQ(Object_OK, RemoteShareMemory_attachConfinement(&confRulesForLend, &memObjForLend));

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));

    ASSERT_NE(Object_OK, ITestService_checkAccessPermissions(mTestServiceObj[0], &confRulesForLend,
                                                             sizeof(ITAccessPermissions_rules),
                                                             memObjForShare));
    ASSERT_NE(Object_OK, ITestService_checkAccessPermissions(mTestServiceObj[0], &confRulesForShare,
                                                             sizeof(ITAccessPermissions_rules),
                                                             memObjForLend));

    Object_ASSIGN_NULL(memObjForShare);
    Object_ASSIGN_NULL(memObjForLend);
}

/*
 * --------------- STRESS TEST CASES ---------------
 */

/**
 * @brief Stress functional
 *
 * Maximum HLOS minkdaemon connection test case.
 * Maximum Registration test case.
 *
 */
TEST_F(TvmdTest, UNSTABLE_TEST(StressOpenAVMServiceTest))
{
    int32_t connection[NUM_THREADS] = {-1, -1, -1, QTVM, OEMVM};
    size_t uid_len = sizeof(gHLOSTestServiceUid) / sizeof(uint32_t);
    pthread_t client[NUM_THREADS] = {0};
    threadData args[NUM_THREADS] = {{0}, {0}, {0}, {0}, {0}};

    if (gMinkConn) {
        isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                                &mProcLoaderObj[0], &mProcObj[0], &mProcLoaderObj[1],
                                mShmList.getShm(gFileName[0], true)));

        // register a service through LAMinkdRegister
        ASSERT_EQ(Object_OK, IMinkdRegister_registerServices(mRegObj, gHLOSTestServiceUid, uid_len,
                                                             gEnv->mIModule));

        for (size_t n = 0; n < NUM_THREADS; n++) {
            args[n].index = connection[n];
            args[n].opener = mClientOpener;
            args[n].myProcObj = &mTestServiceObj[n];
            args[n].mySimulatedHubCred = mSimulatedHubCred;
            ASSERT_EQ(0, pthread_create(&client[n], NULL, openServiceCrossAVMPreprocess, &args[n]));
        }

        for (size_t n = 0; n < NUM_THREADS; n++) {
            ASSERT_EQ(0, pthread_join(client[n], NULL));
        }
    }
}

/**
 * @brief Stress functional
 *
 * Maximum QRTR sending buffer test case.
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressRemoteTVMToAVMQRTRTest))
{
    size_t uid_len = sizeof(gHLOSTestServiceUid) / sizeof(int32_t);
    pthread_t client[NUM_THREADS] = {0};
    threadData args[NUM_THREADS] = {{0}, {0}, {0}, {0}, {0}};

    if (gMinkConn) {
        isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                                &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                                mShmList.getShm(gFileName[0], true)));

        ASSERT_EQ(Object_OK, IMinkdRegister_registerServices(mRegObj, gHLOSTestServiceUid, uid_len,
                                                             gEnv->mIModule));

        // test code
        for (size_t n = 0; n < NUM_THREADS; n++) {
            args[n].index = gHLOSTestServiceUid[n % uid_len];
            args[n].opener = mTestServiceObj[0];

            ASSERT_EQ(0, pthread_create(&client[n], NULL, QRTRConnPreprocess, &args[n]));
        }

        for (size_t n = 0; n < NUM_THREADS; n++) {
            ASSERT_EQ(0, pthread_join(client[n], NULL));
        }
    }
}

/**
 * @brief Stress functional
 *
 * test the maximum untrusted clients supported.
 *
 */
#if !defined(OFFTARGET) && defined(HLOSMINKD_UNIX_TZD_SERVER)
TEST_F(TvmdTest, ONTARGET_TEST(StressQteeMaxUntrustedClientSupportedTest))
{
    if (gMinkConn) {
        uint32_t const wl_list[] = {CDiagnostics_UID, CAppLoader_UID};
        Object clientObj[MAX_QTEE_SYSTEM_CLIENTS_ALLOW + 1];
        int32_t ret = Object_OK;
        size_t i = 0;
        std::string clientId;

        for (; i < MAX_QTEE_SYSTEM_CLIENTS_ALLOW; i++) {
            clientId = "untrusted_client" + std::to_string(i);
            ret = ITzdRegister_getClientEnv(mSsgtzdOpener, (const signed char *)clientId.c_str(),
                                            clientId.length(), std::to_string(i + 1).c_str(),
                                            std::to_string(i + 1).length(), wl_list,
                                            sizeof(wl_list) / sizeof(uint32_t), &clientObj[i]);
            // Any return except belows are considered failures;
            EXPECT_TRUE(ret == Object_OK);
        }

        clientId = "untrusted_client" + std::to_string(i);
        ret = ITzdRegister_getClientEnv(mSsgtzdOpener, (const signed char *)clientId.c_str(),
                                        clientId.length(), std::to_string(i + 1).c_str(),
                                        std::to_string(i + 1).length(), wl_list,
                                        sizeof(wl_list) / sizeof(uint32_t), &clientObj[i]);
        // Any return except belows are considered failures;
        EXPECT_TRUE(ret == Object_ERROR_NOSLOTS);

        for (i = 0; i < MAX_QTEE_SYSTEM_CLIENTS_ALLOW + 1; i++) {
            Object_ASSIGN_NULL(clientObj[i]);
        }
    }
}

/**
 * @brief Stress functional
 *
 * test the maximum qteeObject per untrusted client supported.
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(StressQteeMaxUntrustedObjectperClientSupportedTest))
{
    if (gMinkConn) {
        uint32_t const wl_list[] = {CDiagnostics_UID, CAppLoader_UID};
        Object qteeObject[MAX_QTEEOBJECT_PER_SYSTEM_CLIENT_ALLOWED];
        int32_t ret = Object_OK;
        size_t i = 0;
        std::string clientId = "untrusted_client";

        ret = ITzdRegister_getClientEnv(
            mSsgtzdOpener, (const signed char *)clientId.c_str(), clientId.length(),
            std::to_string(1).c_str(), std::to_string(1).length(), wl_list, 2, &mTestServiceObj[0]);
        ASSERT_TRUE(ret == Object_OK && !Object_isNull(mTestServiceObj[0]));

        // MAX_QTEEOBJECT_PER_SYSTEM_CLIENT_ALLOWED  - 1  because mClientEnv is 1 QteeObject
        for (; i < MAX_QTEEOBJECT_PER_SYSTEM_CLIENT_ALLOWED - 1; i++) {
            ret = IClientEnv_open(mTestServiceObj[0], CDiagnostics_UID, &qteeObject[i]);
            EXPECT_TRUE(ret == Object_OK);
        }

        ret = IClientEnv_open(mTestServiceObj[0], CDiagnostics_UID, &qteeObject[i]);
        EXPECT_TRUE(ret == Object_ERROR_NOSLOTS);

        for (i = 0; i < MAX_QTEEOBJECT_PER_SYSTEM_CLIENT_ALLOWED; i++) {
            Object_ASSIGN_NULL(qteeObject[i]);
        }
    }
}

/**
 * @brief Stress functional
 *
 * test the maximum qteeObject for all untrusted clients supported.
 *
 */
TEST_F(TvmdTest, ONTARGET_TEST(StressQteeMaxUntrustedObjectAllClientSupportedTest))
{
    if (gMinkConn) {
        uint32_t const wl_list[] = {CDiagnostics_UID, CAppLoader_UID};
        Object clientObj[MAX_QTEE_SYSTEM_CLIENTS_ALLOW];
        pthread_t client[MAX_QTEE_SYSTEM_CLIENTS_ALLOW];
        threadData args[MAX_QTEE_SYSTEM_CLIENTS_ALLOW];
        int32_t ret = Object_OK;
        std::string clientId;

        for (size_t i = 0; i < MAX_QTEE_SYSTEM_CLIENTS_ALLOW; i++) {
            clientId = "untrusted_client" + std::to_string(i);
            ret = ITzdRegister_getClientEnv(mSsgtzdOpener, (const signed char *)clientId.c_str(),
                                            clientId.length(), std::to_string(i + 1).c_str(),
                                            std::to_string(i + 1).length(), wl_list,
                                            sizeof(wl_list) / sizeof(uint32_t), &clientObj[i]);
            // Any return except belows are considered failures;
            EXPECT_TRUE(ret == Object_OK && !Object_isNull(clientObj[i]));

            args[i].index = CDiagnostics_UID;
            args[i].opener = clientObj[i];
            EXPECT_EQ(0,
                      pthread_create(&client[i], NULL, qteeObjectAcessCrossPreprocess, &args[i]));
        }

        for (size_t i = 0; i < MAX_QTEE_SYSTEM_CLIENTS_ALLOW; i++) {
            EXPECT_EQ(0, pthread_join(client[i], NULL));
        }

        SLEEP_MS(50 * MAX_QTEE_SYSTEM_CLIENTS_ALLOW);
        for (size_t i = 0; i < MAX_QTEE_SYSTEM_CLIENTS_ALLOW; i++) {
            Object_ASSIGN_NULL(clientObj[i]);
        }
    }
}
#endif

/**
 * @brief Stress functional
 *
 * tprocess1 shares different memObjs with tprocess2 concurrently
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressMultiPoolsSameTAsLocalMemSharing))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[1], true)));
    ASSERT_EQ(Object_OK, ITestService_stressMultiPoolsSameTAsMemSharing(mTestServiceObj[0],
                                                                        CCommonTestService_UID));
}

/**
 * @brief Stress functional
 *
 * tprocess1 shares different memObjs with tprocess2 concurrently
 *
 */
TEST_F(TvmdTest, OFFTARGET_TEST(StressMultiPoolsSameTAsRemoteMemSharing))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_stressMultiPoolsSameTAsMemSharing(
                             mTestServiceObj[0], CEmbeddedCommonTestService_UID));
}

/**
 * @brief Stress functional
 *
 * tprocess1 shares different memObjs with different tprocesses concurrently
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressMultiPoolsDiffTAsLocalMemSharing))
{
    const uint32_t targetUidList[] = {
        CCommonTestService_UID,
        CStressTestService_UID,
    };

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(loadTrustedProcess(CCommonTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[1], &mProcObj[1], &mTestServiceObj[1],
                            mShmList.getShm(gFileName[1], true)));
    isOk(loadTrustedProcess(CStressTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[2], &mProcObj[2], &mTestServiceObj[2],
                            mShmList.getShm(gFileName[2], true)));

    ASSERT_EQ(Object_OK, ITestService_stressMultiPoolsDiffTAsMemSharing(
                             mTestServiceObj[0], targetUidList, sizeof(targetUidList)));
}

/**
 * @brief Stress functional
 *
 * tprocess1 shares different memObjs with different tprocesses concurrently
 *
 */
TEST_F(TvmdTest, OFFTARGET_TEST(StressMultiPoolsDiffTAsRemoteMemSharing))
{
    uint32_t targetUidList[] = {
        CEmbeddedAllPrivilegeTestService_UID,
        CEmbeddedCommonTestService_UID,
    };

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_stressMultiPoolsDiffTAsMemSharing(
                             mTestServiceObj[0], targetUidList, sizeof(targetUidList)));
}

/**
 * @brief Stress functional
 *
 * load one embedded process in an embedded process
 *
 */

// There is a known issue in minksocket. It will be fixed on Pakala, so I need to disable this test
// case.
TEST_F(TvmdTest, UNSTABLE_TEST(StressLocalCallerCallSameService))
{
    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CEmbeddedStressTestService_UID,
                                    mSimulatedHubCred, &mTestServiceObj[0], gMinkConn));
    ASSERT_EQ(Object_OK, ITestService_parallelCallSameService(mTestServiceObj[0]));
}

/**
 * @brief Stress functional
 *
 * load different embedded processes in an downloadable process
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressLocalCallerCallDiffService))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_parallelCallDiffService(mTestServiceObj[0]));
}

/**
 * @brief Stress functional
 *
 * abnormal death of different embedded processes
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressProcessAbnormalDeath))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_stressProcessAbnormalDeath(mTestServiceObj[0]));
}

/**
 * @brief Stress functional
 *
 * load one embedded process
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressLoadSameEmbeddedProc))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_stressLoadSameEmbeddedProc(mTestServiceObj[0]));
}

/**
 * @brief Stress functional
 *
 * load one process from buffer
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressLoadSameTrustedProc))
{
    pthread_t client[NUM_THREADS];
    int maxTestThreadsCount = NUM_THREADS;
    threadData args[NUM_THREADS];

    mIsLoadSame = true;

    for (int n = 0; n < maxTestThreadsCount; n++) {
        args[n].index = CAllPrivilegeTestService_UID;
        args[n].opener = mClientOpener;
        args[n].myProcLoaderObj = &mProcLoaderObj[n];
        args[n].myProcObj = &mProcObj[n];
        args[n].myServiceObj[0] = &mTestServiceObj[n];
        args[n].myMemObj = mShmList.getShm(gFileName[0], true);
        args[n].mySimulatedHubCred = mSimulatedHubCred;
        ASSERT_EQ(0, pthread_create(&client[n], NULL, loadTrustedProcessParallel, &args[n]));
    }

    for (int n = 0; n < maxTestThreadsCount; n++) {
        ASSERT_EQ(0, pthread_join(client[n], NULL));
    }
}

/**
 * @brief Stress functional
 *
 * load different embedded processes
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressLoadDiffEmbeddedProc))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_stressLoadDiffEmbeddedProc(mTestServiceObj[0]));
}

/**
 * @brief Stress functional
 *
 * load different processes from buffer
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressLoadDiffTrustedProc))
{
    pthread_t client[NUM_THREADS];
    int maxTestThreadsCount = NUM_THREADS;
    threadData args[NUM_THREADS];

    for (int n = 0; n < maxTestThreadsCount; n++) {
        args[n].index = gTrustedUid[n];
        args[n].opener = mClientOpener;
        args[n].myProcLoaderObj = &mProcLoaderObj[n];
        args[n].myProcObj = &mProcObj[n];
        args[n].myServiceObj[0] = &mTestServiceObj[n];
        args[n].myMemObj = mShmList.getShm(gFileName[n], true);
        args[n].mySimulatedHubCred = mSimulatedHubCred;
        ASSERT_EQ(0, pthread_create(&client[n], NULL, loadTrustedProcessParallel, &args[n]));
    }

    for (int n = 0; n < maxTestThreadsCount; n++) {
        ASSERT_EQ(0, pthread_join(client[n], NULL));
    }
}

/**
 * @brief Stress functional
 *
 * embedded processes normal death
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressEmbeddedNormalDeath))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_stressEmbeddedNormalDeath(mTestServiceObj[0]));
}

/**
 * @brief Stress functional
 *
 * Multiple processes acquire wake lock
 *
 */
TEST_F(TvmdTest, COMMON_TEST(StressAcquireWakeLock))
{
    // Wakelock tests assume that no one is currently holding the wake lock, so release it.
    Object_ASSIGN_NULL(mWakeLock);

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    ASSERT_EQ(Object_OK, ITestService_stressAcquireWakeLock(mTestServiceObj[0]));
}

/**
 * @brief Stress functional
 *
 * PVM shares large buffer(100MB default) with QTVM.
 *
 */
TEST_F(ProfilingTest, ONTARGET_TEST(StressONTARGETShareLargeDMABufferWithQTVM))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    // 50 iters for both same and different memObj
    isOk(passLargeDMABufferToQTVM(true, true, &mTestServiceObj[0], 50));
    isOk(passLargeDMABufferToQTVM(true, false, &mTestServiceObj[0], 50));
}

/**
 * @brief Stress functional
 *
 * PVM lends large buffer(100MB default) to QTVM.
 *
 */
TEST_F(ProfilingTest, ONTARGET_TEST(StressONTARGETLendLargeDMABufferToQTVM))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    // 50 iters for both same and different memObj
    isOk(passLargeDMABufferToQTVM(false, true, &mTestServiceObj[0], 50));
    isOk(passLargeDMABufferToQTVM(false, false, &mTestServiceObj[0], 50));
}

/**
 * @brief Stress functional
 *
 * QTVM TA1 stressly share memObj with QTVM TA2
 *
 */
TEST_F(ProfilingTest, ONTARGET_TEST(StressONTARGETQTVMalloc))
{
    ASSERT_EQ(Object_OK, ITEST_OPEN(mClientOpener, CEmbeddedAllPrivilegeTestService_UID,
                                    mSimulatedHubCred, &mTestServiceObj[0], gMinkConn));
    ASSERT_EQ(Object_OK, ITestService_QTVMAllocMemory(mTestServiceObj[0], gEnv->memorySizeMB));
}

/**
 * @brief Stress functional
 *
 * PVM stressly send nopayload message to QTVM TA
 *
 */
TEST_F(ProfilingTest, ONTARGET_TEST(StressONTARGETSendNopayload))
{
    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    for (uint32_t it = 0; it < 100; it++) {
        ASSERT_EQ(Object_OK, ITestService_printHello(mTestServiceObj[0]));
        SLEEP_MS(10);
    }
}

/**
 * @brief Stress functional
 *
 * PVM echoly send buffer message to QTVM TA
 *
 */
TEST_F(ProfilingTest, UNSTABLE_TEST(StressONTARGETShareLargeCopyBuffer))
{
    // Temporary solution: convert memory size from MB to KB
    // even qrtr support 64KB payload but over 59KB the process wil hang
    uint32_t bufferSizeKB = gEnv->memorySizeMB / 100;
    ASSERT_TRUE(bufferSizeKB <= 59);

    isOk(loadTrustedProcess(CAllPrivilegeTestService_UID, mClientOpener, mSimulatedHubCred,
                            &mProcLoaderObj[0], &mProcObj[0], &mTestServiceObj[0],
                            mShmList.getShm(gFileName[0], true)));
    isOk(passLargeCopyBufferToQTVM(&mTestServiceObj[0], bufferSizeKB, 10, 100));
}

/**
 * @brief Ensure all SIGCHLD signals are caught by parent
 *
 * TODO: This test can work on-target with the addition of a "controller" TP
 * which sends SIGCONT to the processes queued for shutdown.
 */
TEST_F(TvmdFixture, OFFTARGET_TEST(StressOFFTARGETCatchAllChildExit))
{
    pthread_t client[NUM_THREADS];
    int maxTestThreadsCount = NUM_THREADS;  // -1; set to NUM_THREADS will lead to OOM in LE?
    threadData args[NUM_THREADS];

    // Set up shared memory
    isOk(mShmList.constructor());

    // Spin up TPs
    for (int n = 0; n < maxTestThreadsCount; n++) {
        args[n].index = gTrustedUid[n];
        args[n].opener = mClientOpener;
        args[n].myProcLoaderObj = &mProcLoaderObj[n];
        args[n].myProcObj = &mProcObj[n];
        args[n].myServiceObj[0] = &mTestServiceObj[n];
        args[n].myMemObj = mShmList.getShm(gFileName[n], true);
        args[n].mySimulatedHubCred = mSimulatedHubCred;
        ASSERT_EQ(0, pthread_create(&client[n], NULL, loadTrustedProcessParallel, &args[n]));
    }

    for (int n = 0; n < maxTestThreadsCount; n++) {
        ASSERT_EQ(0, pthread_join(client[n], NULL));
    }

    // Tear down shared memory after all TPs have been launched
    isOk(mShmList.destructor());

    // Kill all children in quick succession
    for (int n = 0; n < maxTestThreadsCount; n++) {
        ASSERT_EQ(Object_OK, ITestService_shutdown(mTestServiceObj[n], true));
    }

    // Send wake-up signal to process group
    killpg(getpgrp(), SIGCONT);

    // Give Mink a chance to clean up children via signal handler
    SLEEP_MS(50 * maxTestThreadsCount);

    // // Check that all signals have been properly caught
    // ASSERT_EQ(Object_OK, ITPPMPriv_getNumProcs(mProcManObj, &mNumProcs));
    // ASSERT_EQ(0U, mNumProcs);

    for (int i = 0; i < NUM_THREADS; i++) {
        // Clean up objects unconditionally
        Object_ASSIGN_NULL(mTestServiceObj[i]);
        Object_ASSIGN_NULL(mProcLoaderObj[i]);
        Object_ASSIGN_NULL(mProcObj[i]);
    }

    SLEEP_MS(100);
}

static void _defaultUsage(void)
{
    printf(
        "Example Usage:\n"
        "1. Connecting to QTVM/OEMVM directly using QRTR socket for testing. Connecting to LAMinkd "
        "using unix socket by default.\n"
        "   ./mink_test or MinkPlatformTest --directconn or ./mink_test or MinkPlatformTest --d\n"
        "2. Connecting to QTVM for testing.\n"
        "   ./mink_test or MinkPlatformTest\n"
        "3. Connecting to OEMVM for testing.\n"
        "   ./mink_test or MinkPlatformTest --oemvm or ./mink_test or MinkPlatformTest --o\n"
        "4. For help.\n"
        "   ./mink_test or MinkPlatformTest --help or ./mink_test or MinkPlatformTest --h\n");
}

/**
 * passing the --oemvm flag will set the oemvmFlag variable to true. This allows us to choose which
 * mOpenerConn to use for testing ex. ./mink_test --oemvm
 */
int32_t main(int32_t argc, char **argv)
{
    int32_t ret = -1;
    int32_t index = 0;
    int32_t flag = 0;
    uint64_t customMemorySizeMB = 100;
    Profiling_configProfile();

    const struct option longopts[] = {{"oemvm", no_argument, NULL, 'a'},
                                      {"o", no_argument, NULL, 'b'},
                                      {"directconn", no_argument, NULL, 'c'},
                                      {"d", no_argument, NULL, 'd'},
                                      {"help", no_argument, NULL, 'e'},
                                      {"h", no_argument, NULL, 'f'},
                                      {"memorySize", required_argument, NULL, 'm'},
                                      {NULL, 0, NULL, 0}};

    ::testing::InitGoogleTest(&argc, argv);

    while ((flag = getopt_long(argc, argv, "abcdefm:", longopts, &index)) != -1) {
        switch (flag) {
            case 'a':
            case 'b':
                // set global flag for mEnvOpener used in TvmdFixture SetUp
                gRemoteConn = OEMVM;
                break;
            case 'c':
            case 'd':
                // set global flag for mEnvOpener used in TvmdFixture SetUp
                gMinkConn = false;
                break;
            case 'e':
            case 'f':
                // print the example usage
                _defaultUsage();
                return 0;
            case 'm':
                // set custom memory size for large memory test
                customMemorySizeMB = atoi(optarg);
                break;
            default:
                break;
        }
    }

    // gtest holds the object, we do not delete it
    gEnv = new TvmdTestEnvironment(customMemorySizeMB);

    ::testing::AddGlobalTestEnvironment(gEnv);
    ret = RUN_ALL_TESTS();

    return ret;
}
