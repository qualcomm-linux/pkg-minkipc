// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

#ifdef STUB
#include "BufferAllocatorWrapper.h"
#else
#include <BufferAllocator/BufferAllocatorWrapper.h>
#include "IClientEnv.h"
#include "TZCom.h"
#endif
#include "CAppLoader.h"
#include "CHLOSPlatformEnv.h"
#include "CSampleService.h"
#include "CTMemoryService.h"
#include "CTOEMVMMemoryService.h"
#include "CTOEMVMPowerService.h"
#include "CTOEMVMProcessLoader.h"
#include "CTPowerService.h"
#include "CTProcessLoader.h"
#include "CTRebootVM.h"
#include "IAppLoader.h"
#include "IOpener.h"
#include "ISampleService.h"
#include "ITPowerService.h"
#include "ITProcessLoader.h"
#include "ITRebootVM.h"
#include "RemoteShareMemory.h"
#include "fdwrapper.h"
#include "memscpy.h"
#include "minkipc.h"
#include "profiling.h"

#define ALLOCATED_BUFFER_SIZE_MB 64 // 64MB
#define FILE_PATH_MAX_LENGTH 60
#ifdef STUB
#define MINK_UNIX_LA_SERVICE "simulated_hlos_mink_opener"
#define TVM_TA_PATH ""
#else
#define MINK_UNIX_LA_SERVICE "/dev/socket/hlos_mink_opener"
#define TVM_TA_PATH "/vendor/bin/"
#endif
#define QTEE_TA_PATH "/vendor/bin/smcinvoke_example_ta64.mbn"
#define SIZE_2MB 0x200000
#define TVM_TA_NAME_C "sampleapp"
#define TVM_TA_NAME_CPP "sampleapp_cpp"

bool isOEMVM = false;
static MinkIPC *gVMIntf = NULL;
static Object gProcObj = Object_NULL;
static Object gRemoteAppObject = Object_NULL;
static Object gTvmAppOpener = Object_NULL;
static Object gAppController = Object_NULL;
static Object gUniqueOpener = Object_NULL;
static Object shutdownObj = Object_NULL;

int32_t __readMemObjFromFile(char *fileName, Object *memObj)
{
    int32_t ret = Object_ERROR, actualRead = -1, bufferFd = -1, appFd = -1;
    struct stat appStat = {};
    BufferAllocator *bufferAllocator = NULL;
    unsigned char *vAddr = NULL;
    char appPath[FILE_PATH_MAX_LENGTH] = {0};
    size_t appSize = 0;
    ITAccessPermissions_rules confRules = {};

    snprintf(appPath, FILE_PATH_MAX_LENGTH, "%s%s", TVM_TA_PATH, fileName);
    LOG_MSG("Opening file at '%s'", appPath);
    appFd = open(appPath, O_RDONLY);
    T_CHECK_ERR(appFd >= 0, appFd);

    T_CHECK(fstat(appFd, &appStat) == 0);
    T_CHECK((appStat.st_size) > 0);
    appSize = appStat.st_size;

    bufferAllocator = CreateDmabufHeapBufferAllocator();
    T_CHECK(bufferAllocator != NULL);

    bufferFd = DmabufHeapAlloc(bufferAllocator, "qcom,display",
                               (appSize + (SIZE_2MB - 1)) & (~(SIZE_2MB - 1)), 0, 0);
    T_CHECK(bufferFd >= 0);

    vAddr = (unsigned char *)mmap(NULL, appSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufferFd, 0);
    T_CHECK(vAddr != MAP_FAILED);

    actualRead = read(appFd, vAddr, appSize);
    T_CHECK(actualRead == appSize);

    T_CHECK(munmap(vAddr, appSize) == 0);

    *memObj = FdWrapper_new(bufferFd);
    T_CHECK(!Object_isNull(*memObj));
    confRules.specialRules = ITAccessPermissions_removeSelfAccess;
    T_GUARD(RemoteShareMemory_attachConfinement(&confRules, memObj));

    ret = Object_OK;

exit:
    if (bufferAllocator) {
        FreeDmabufHeapBufferAllocator(bufferAllocator);
    }

    if (appFd >= 0) {
        close(appFd);
    }

    return ret;
}

int32_t shareMemory(bool isShared)
{
    int32_t ret = Object_ERROR;
    int fd = -1;
    Object sharedMemObj = Object_NULL;
    uint64_t allocSize = ALLOCATED_BUFFER_SIZE_MB * 1024 * 1024;
    BufferAllocator *bufferAllocator = NULL;
    unsigned char *vAddr = NULL;
    uint8_t *buffer = NULL;
    ITAccessPermissions_rules confRules = {};

    bufferAllocator = CreateDmabufHeapBufferAllocator();
    T_CHECK(bufferAllocator != NULL);

    fd = DmabufHeapAlloc(bufferAllocator, "qcom,system",
                         (allocSize + (SIZE_2MB - 1)) & (~(SIZE_2MB - 1)), 0, 0);
    T_CHECK(fd >= 0);

    vAddr = (unsigned char *)mmap(NULL, allocSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    T_CHECK(vAddr != MAP_FAILED);

    buffer = (uint8_t *)malloc(sizeof(uint8_t[allocSize]));
    T_CHECK(buffer != NULL);

    snprintf((char *)buffer, allocSize, "A message from the CA");
    memscpy(vAddr, allocSize, buffer, allocSize);
    LOG_MSG("The string to be sent to TA: %s", (char *)vAddr);

    sharedMemObj = FdWrapper_new(fd);
    T_CHECK(!Object_isNull(sharedMemObj));
    fd = -1;

    if (isShared) {
        confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    } else {
        confRules.specialRules = ITAccessPermissions_removeSelfAccess;
        munmap(vAddr, allocSize);
        vAddr = NULL;
    }

    T_GUARD(RemoteShareMemory_attachConfinement(&confRules, &sharedMemObj));

    if (!profiling) {
        T_GUARD(ISampleService_shareMemory(gRemoteAppObject, sharedMemObj));

        // sleep for 1 second
        sleep(1);
    } else {
        for(int32_t i = 0; i < NUMBER_OF_ITERATIONS; i++) {
            startTime = vm_osal_getCurrentTimeUs();
            T_GUARD(ISampleService_shareMemory(gRemoteAppObject, sharedMemObj));
            endTime = vm_osal_getCurrentTimeUs();
            costTime = endTime - startTime;
            dataArray[i] = ALLOCATED_BUFFER_SIZE_MB * 1e6 / costTime; // data unit: MB/s

            // sleep for 1 second
            sleep(1);
        }

        T_GUARD(processData(dataArray, NUMBER_OF_ITERATIONS, &analysis));
        memset(dataArray, 0, sizeof(dataArray));

        LOG_MSG("Transfer_host_to_vm(MB/s): average: %.2f, max: %llu, median: %.2f, min: %llu, stdev: %.2f",
                analysis.average, analysis.max, analysis.median, analysis.min, analysis.stdDev);
    }

    if (!isShared) {
        T_GUARD(Object_unwrapFd(sharedMemObj, &fd));
        vAddr = (unsigned char *)mmap(NULL, allocSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        fd = -1;
        T_CHECK(vAddr != MAP_FAILED);
    }

    // ensure that the last character of what we're about to print is \0.
    ((char *)vAddr)[allocSize - 1] = '\0';
    // print the buffer to show that the TA has modified it.
    LOG_MSG("The string returned by TA to CA: %s", (char *)vAddr);

exit:
    if (fd > 0) {
        close(fd);
    }

    if (vAddr != NULL) {
        munmap(vAddr, allocSize);
    }

    if (bufferAllocator) {
        FreeDmabufHeapBufferAllocator(bufferAllocator);
    }

    if (buffer) {
        free(buffer);
    }

    Object_ASSIGN_NULL(sharedMemObj);

    return ret;
}

#ifndef STUB
int32_t loadQTEETA()
{
    Object appLoader = Object_NULL;
    int32_t ret = Object_ERROR;
    size_t readBytes = 0;
    struct stat st = {0};
    char *taBufferPtr = NULL;

    FILE *pfile = fopen(QTEE_TA_PATH, "rb");
    T_CHECK(pfile != NULL);
    T_CHECK(stat(QTEE_TA_PATH, &st) == 0);
    taBufferPtr = (char *)malloc(st.st_size);
    T_CHECK(taBufferPtr != NULL);
    memset(taBufferPtr, 0, st.st_size);
    T_CHECK(fread(taBufferPtr, 1, st.st_size, pfile) == st.st_size);

    T_GUARD(IOpener_open(gUniqueOpener, CAppLoader_UID, &appLoader));
    T_GUARD(IAppLoader_loadFromBuffer(appLoader, taBufferPtr, st.st_size, &gAppController));

exit:
    if (pfile) {
        fclose(pfile);
    }

    if (taBufferPtr) {
        free(taBufferPtr);
    }

    Object_ASSIGN_NULL(appLoader);

    return ret;
}

int32_t unloadQTEETA()
{
    int32_t ret = Object_ERROR;

    T_GUARD(IAppController_unload(gAppController));
    Object_ASSIGN_NULL(gAppController);

exit:
    return ret;
}

int32_t loadQTEETAInTVMTA()
{
    int32_t ret = Object_ERROR;
    Object memObj = Object_NULL;

    T_GUARD(__readMemObjFromFile("smcinvoke_example_ta64.mbn", &memObj));
    T_GUARD(ISampleService_loadQTEETA(gRemoteAppObject, memObj));

exit:
    Object_ASSIGN_NULL(memObj);
    return ret;
}

int32_t shareTVMMemWithQTEETA()
{
    int32_t ret = Object_ERROR, uid = 0;

    if (isOEMVM) {
        uid = CTOEMVMMemPoolFactory_UID;
    } else {
        uid = CTMemPoolFactory_UID;
    }

    T_GUARD(ISampleService_shareMemWithQTEETA(gRemoteAppObject, uid));

exit:
    return ret;
}
#endif

void unLoadTVMTA()
{
    int32_t ret = Object_ERROR;

    Object_ASSIGN_NULL(gRemoteAppObject);
    Object_ASSIGN_NULL(gProcObj);
    Object_ASSIGN_NULL(gUniqueOpener);
    Object_ASSIGN_NULL(gTvmAppOpener);

    if (gVMIntf != NULL) {
        MinkIPC_release(gVMIntf);
        gVMIntf = NULL;
    }

    LOG_MSG("Cleanup complete!");

exit:
    return;
}

int32_t loadTVMTA(char *appName, int32_t processLoaderUid)
{
    int32_t ret = Object_ERROR;
    Object nsMemoryObj = Object_NULL;
    Object procLoaderObj = Object_NULL;

    T_GUARD(__readMemObjFromFile(appName, &nsMemoryObj));

    T_GUARD(IOpener_open(gUniqueOpener, processLoaderUid, &procLoaderObj));
    ret = ITProcessLoader_loadFromBuffer(procLoaderObj, nsMemoryObj, &gProcObj);
    T_CHECK(ret == 0 || ret == ITProcessLoader_ERROR_PROC_ALREADY_LOADED);

    T_GUARD(IOpener_open(gUniqueOpener, CSampleService_UID, &gRemoteAppObject));

    if (!profiling) {
        T_GUARD(ISampleService_printHello(gRemoteAppObject));
    } else {
        for(int32_t i = 0; i < NUMBER_OF_ITERATIONS; i++) {
            startTime = vm_osal_getCurrentTimeUs();
            T_GUARD(ISampleService_printHello(gRemoteAppObject));
            endTime = vm_osal_getCurrentTimeUs();
            costTime = endTime - startTime;
            dataArray[i] = costTime;
        }

        T_GUARD(processData(dataArray, NUMBER_OF_ITERATIONS, &analysis));
        memset(dataArray, 0, sizeof(dataArray));

        LOG_MSG("MinkIPC latency(us): average: %.2f, max: %llu, median: %.2f, min: %llu, stdev: %.2f",
                analysis.average, analysis.max, analysis.median, analysis.min, analysis.stdDev);
    }

exit:
    Object_ASSIGN_NULL(procLoaderObj);
    Object_ASSIGN_NULL(nsMemoryObj);

    return ret;
}

int32_t passMemUasgeBufferToTVM()
{
    int32_t ret = Object_ERROR;
    const uint32_t buffersize = 2 * 1024; //2KB
    char *bufferOut = NULL;
    size_t bufferOutLen = 0, LenOut = 0;

    bufferOut =  (char*)malloc(buffersize * sizeof(char));
    T_CHECK(bufferOut != NULL);
    memset(bufferOut, 0, buffersize * sizeof(char));

    bufferOutLen = buffersize * sizeof(char);

    T_GUARD(ISampleService_dumpMemoryUsage(gRemoteAppObject, bufferOut, bufferOutLen, &LenOut));

    LOG_MSG("The meminfo by qtvm to pvm:\n%s", (char *)bufferOut);

exit:
    free(bufferOut);

    return ret;
}

int32_t runBootuptimeTest()
{
    int32_t ret = Object_ERROR;

    for (int32_t i = 0; i < NUMBER_OF_REBOOT_ITERATION; i++) {
        // TVM shutdown
        LOG_MSG("TVM reboot iteration %d\n", i + 1);

        /*
         * DON'T USE ITREBOOTVM SERVICE IN PRODUCTION, YOU MAY ENCOUNTER
         * DENIAL OF SERVICE OR OTHER UNINTENDED CONSEQUENCES
         */
        IOpener_open(gTvmAppOpener, CTRebootVM_UID, &shutdownObj);
        T_GUARD(ITRebootVM_shutdown(shutdownObj, 1, 1));
        ret = Object_ERROR;

        if (getBootupTime(i))
            goto exit;

        Object_ASSIGN_NULL(shutdownObj);
        Object_ASSIGN_NULL(gTvmAppOpener);
        if (gVMIntf != NULL) {
            MinkIPC_release(gVMIntf);
            gVMIntf = NULL;
        }

        gVMIntf = MinkIPC_connect(MINK_UNIX_LA_SERVICE, &gTvmAppOpener);
        T_CHECK(gVMIntf != NULL && !Object_isNull(gTvmAppOpener));
    }

    if (showBootupTime())
        goto exit;

    return 0;

exit:
    Object_ASSIGN_NULL(shutdownObj);
    Object_ASSIGN_NULL(gTvmAppOpener);
    if (gVMIntf != NULL) {
        MinkIPC_release(gVMIntf);
        gVMIntf = NULL;
    }

    return ret;
}

int32_t runTest(char *appName, int32_t processLoaderUid)
{
    int32_t ret = Object_ERROR;

    // connect to Mink LA service
    gVMIntf = MinkIPC_connect(MINK_UNIX_LA_SERVICE, &gTvmAppOpener);
    T_CHECK(gVMIntf != NULL && !Object_isNull(gTvmAppOpener));
    T_GUARD(IOpener_open(gTvmAppOpener, CHLOSPlatformEnv_UID, &gUniqueOpener));

    // acquire wake lock
    Object qtvmPowerService = Object_NULL;
    Object oemvmPowerService = Object_NULL;
    Object qtvmWakeLock = Object_NULL;
    Object oemvmWakeLock = Object_NULL;
    T_GUARD(IOpener_open(gUniqueOpener, CTPowerService_UID, &qtvmPowerService));
    T_GUARD(ITPowerService_acquireWakeLock(qtvmPowerService, &qtvmWakeLock));
    if (isOEMVM) {
        T_GUARD(IOpener_open(gUniqueOpener, CTOEMVMPowerService_UID, &oemvmPowerService));
        T_GUARD(ITPowerService_acquireWakeLock(oemvmPowerService, &oemvmWakeLock));
    }
    LOG_MSG("Acquire wake lock successfully.");

    if (profiling) {
        ret = runBootuptimeTest();

        if (ret) {
            // try to connect to Mink LA service again
            gVMIntf = MinkIPC_connect(MINK_UNIX_LA_SERVICE, &gTvmAppOpener);
            T_CHECK(gVMIntf != NULL && !Object_isNull(gTvmAppOpener));
        } else {
            LOG_MSG("Run TVM bootup time testcase successfully.");
        }

        T_GUARD(IOpener_open(gTvmAppOpener, CHLOSPlatformEnv_UID, &gUniqueOpener));
        T_GUARD(IOpener_open(gUniqueOpener, CTPowerService_UID, &qtvmPowerService));
        T_GUARD(ITPowerService_acquireWakeLock(qtvmPowerService, &qtvmWakeLock));
        LOG_MSG("Acquire wake lock (after TVM reboot) successfully.");
    }

    // load TVM TA and get TA object (QTVM/OEMVM)
    T_GUARD(loadTVMTA(appName, processLoaderUid));
    LOG_MSG("Load TVM TA successfully.");

#ifndef STUB
    // call into callQTEEKernelService interface of the TVM TA
    T_GUARD(ISampleService_callQTEEKernelService(gRemoteAppObject));
    LOG_MSG("Call into openQTEEKernelService interface of the TVM TA successfully.");

    // load QTEE TA
    T_GUARD(loadQTEETA());
    LOG_MSG("Load QTEE TA successfully.");

    // call into callQTEETA interface of the TVM TA
    T_GUARD(ISampleService_callQTEETA(gRemoteAppObject));
    LOG_MSG("Call into callQTEETA interface of the TVM TA successfully.");

    // unload QTEE TA
    T_GUARD(unloadQTEETA());
    LOG_MSG("Unload QTEE TA successfully.");
#endif

    // call into shareMemory interface of the TVM TA, share memory
    T_GUARD(shareMemory(true));
    LOG_MSG("Call into shareMemory(share) interface of the TVM TA successfully.");

    // call into shareMemory interface of the TVM TA, lend memory
    T_GUARD(shareMemory(false));
    LOG_MSG("Call into shareMemory(lend) interface of the TVM TA successfully.");

#ifndef STUB
    // call into loadQTEETA interface of the TVM TA
    T_GUARD(loadQTEETAInTVMTA());
    LOG_MSG("Call into loadQTEETA interface of the TVM TA successfully.");

    // call into shareMemWithQTEETA interface of the TVM TA
    T_GUARD(shareTVMMemWithQTEETA());
    LOG_MSG("Call into shareMemWithQTEETA interface of the TVM TA successfully.");

    //call dumpMemoryUsage
    if (profiling) {
        T_GUARD(passMemUasgeBufferToTVM());
        LOG_MSG("Call into dumpMemoryUsage interface of the TVM TA successfully.");
    }

#endif

exit:
    // release resources
    unLoadTVMTA();

    // release wake lock
    Object_ASSIGN_NULL(oemvmWakeLock);
    Object_ASSIGN_NULL(oemvmPowerService);
    Object_ASSIGN_NULL(qtvmWakeLock);
    Object_ASSIGN_NULL(qtvmPowerService);
    LOG_MSG("Release wake lock successfully.");

    return ret;
}

void __usage(void)
{
    printf("*************************************************************\n");
    printf("**************        QTVM SAMPLE CA HELP      **************\n");
    printf("*************************************************************\n");
    printf(
        "\n"
        " QTVMSampleCA <appname> <destination>\n"
        " <appname>      : Name of the tvm app: sampleapp or sampleapp_cpp\n"
        " <destination>  : Destination name: qtvm or oemvm\n"
        " -p             : Profiling: disabled by default,\n"
        "                             enabled by entering '-p'\n"
        "-----------------------------------------------------------------\n\n");
}

int32_t __parseinputs(int argc, char *argv[])
{
    int32_t ret = -1;
    int32_t i = 0;
    char *appName = TVM_TA_NAME_C;
    int32_t processLoaderUid = CTProcessLoader_UID;

    if (argv == NULL) {
        LOG_MSG("No arguments to process, exiting!");
        __usage();
        return ret;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "sampleapp_cpp") == 0) {
            appName = argv[i];
        } else if (strcmp(argv[i], "sampleapp") == 0) {
            appName = argv[i];
        } else if (strcmp(argv[i], "qtvm") == 0) {
            processLoaderUid = CTProcessLoader_UID;
        } else if (strcmp(argv[i], "oemvm") == 0) {
            processLoaderUid = CTOEMVMProcessLoader_UID;
            isOEMVM = true;
        } else if (strcmp(argv[i], "-p") == 0) {
            profiling = true;
        } else {
#ifdef STUB
            appName = argv[i];
#else
            LOG_MSG("Unsupported input arguments!");
#endif
        }
    }

    ret = runTest(appName, processLoaderUid);

    return ret;
}

int main(int argc, char *argv[])
{
    int ret = -1;

    if (argc < 3) {
        LOG_MSG("Incorrect usage.");
        __usage();
        return ret;
    }

    ret = __parseinputs(argc, argv);
    if (ret) {
        LOG_MSG("Test cases FAILED.");
    } else {
        LOG_MSG("Test cases PASSED.");
    }

    return ret;
}
