// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#define _GNU_SOURCE
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "CAppClient.h"
#include "CAppLoader.h"
#include "CDiagnostics.h"
#include "CTMemoryService.h"
#include "CTOEMVMMemoryService.h"
#include "CTOEMVMPowerService.h"
#include "IAppClient.h"
#include "IAppLoader.h"
#include "ICredentials.h"
#include "IDiagnostics.h"
#include "ISMCIExampleApp.h"
#include "ISampleService_invoke.h"
#include "ITEnv.h"
#include "ITMemoryService.h"
#include "TUtils.h"
#include "heap.h"
#include "moduleAPI.h"

#define SIZE_4KB 0x001000
#define SIZE_2MB 0x200000

typedef struct {
    int32_t refs;
    Object credentials;
} SampleService;

// ------------------------------------------------------------------------
// Global variable definitions
// ------------------------------------------------------------------------
static sem_t gShutdownLock;
static Object gAppLoaderObj = Object_NULL;
static Object gAppCtlObj = Object_NULL;
static Object gQteeAppObj = Object_NULL;

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
int main(int argc, char *argv[])
{
    int ret = 0;

    if (sem_init(&gShutdownLock, 0, 0) != 0) {
        LOG_MSG("Failed to initialize semaphore");
        return -1;
    }

    LOG_MSG("%s() ", __func__);
    for (int i = 1; i < argc; i++) {
        LOG_MSG(" %s ", argv[i]);
    }

    // Initialize structures or connections before service becomes available to
    // other processes.

    // Decrement (lock) the semaphore. Put to sleep indefinitely.
    if (sem_wait(&gShutdownLock) != 0) {
        LOG_MSG("Failed to wait on semaphore.");
        return -1;
    }

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

//############### SampleService ###############
static int32_t CSampleService_retain(SampleService *me)
{
    uint32_t r = me->refs;
    LOG_MSG("Svc Obj references incrementing from %d -> %d", r, r + 1);
    me->refs++;
    return Object_OK;
}

static int32_t CSampleService_release(SampleService *me)
{
    uint32_t r = me->refs;
    LOG_MSG("Svc Obj references decrementing from %d -> %d", r, r - 1);
    if (--me->refs == 0) {
        Object_RELEASE_IF(me->credentials);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static int32_t CSampleService_printHello(SampleService *me)
{
    LOG_MSG("Hello world!");
    return Object_OK;
}

static int32_t testMemBuf(Object memBuf)
{
    uint32_t bufSize = 5 * 1024 * 1024;  // 5MB
    int32_t ret = Object_ERROR;
    int32_t bufFd = -1;
    void *ptr = NULL;

    LOG_MSG("Testing memory buffer from DMABUFHEAP!");

    T_GUARD(Object_unwrapFd(memBuf, &bufFd));

    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(ptr != MAP_FAILED);

    memset(ptr, 0, bufSize);
    ret = munmap(ptr, bufSize);

exit:
    return ret;
}

static int32_t CSampleService_getDMABuf(SampleService *me)
{
    Object memPoolFactory = Object_NULL;
    Object memPool = Object_NULL;
    Object memBuf[2] = {Object_NULL};
    uint32_t poolMemSize = 10 * 1024 * 1024;  // 10MB
    uint32_t bufSize = 5 * 1024 * 1024;       // 5MB
    int32_t ret = Object_ERROR, idx;
    ITAccessPermissions_rules defaultRules = {0};
    defaultRules.specialRules = ITAccessPermissions_keepSelfAccess;

    LOG_MSG("Start to get DMA buffers!");

    T_GUARD(ITEnv_open(gTVMEnv, CTMemPoolFactory_UID, &memPoolFactory));
    T_GUARD(ITMemPoolFactory_createPool(memPoolFactory, &defaultRules, poolMemSize, &memPool));

    for (idx = 0; idx < 2; idx++) {
        T_GUARD(ITMemPool_allocateBuffer(memPool, bufSize, &memBuf[idx]));
        T_GUARD(testMemBuf(memBuf[idx]));
    }

exit:
    for (idx = 0; idx < 2; idx++) {
        Object_ASSIGN_NULL(memBuf[idx]);
    }

    Object_ASSIGN_NULL(memPool);
    Object_ASSIGN_NULL(memPoolFactory);

    return ret;
}

static int32_t CSampleService_callQTEETA(SampleService *me)
{
    int32_t ret = Object_ERROR;
    Object appClientObj = Object_NULL;
    Object appObj = Object_NULL;
    const char appName[] = "smcinvoke_example_ta64";

    LOG_MSG("Start to call a QTEE TA!");

    T_GUARD(ITEnv_open(gTVMEnv, CAppClient_UID, &appClientObj));
    T_GUARD(IAppClient_getAppObject(appClientObj, appName, strlen(appName), &appObj));
    T_GUARD(ISMCIExampleApp_logSinkExample(appObj));

exit:
    Object_ASSIGN_NULL(appObj);
    Object_ASSIGN_NULL(appClientObj);

    return ret;
}

static int32_t CSampleService_callQTEEKernelService(SampleService *me)
{
    int32_t ret = Object_ERROR;
    Object appObject = Object_NULL;
    IDiagnostics_HeapInfo heapInfo;
    memset((void *)&heapInfo, 0, sizeof(IDiagnostics_HeapInfo));

    LOG_MSG("Start to call a QTEE kernel service!");

    T_GUARD(ITEnv_open(gTVMEnv, CDiagnostics_UID, &appObject));
    T_GUARD(IDiagnostics_queryHeapInfo(appObject, &heapInfo));

    LOG_MSG("Total bytes as heap: %d", heapInfo.totalSize);

exit:
    Object_ASSIGN_NULL(appObject);

    return ret;
}

static int32_t CSampleService_shareMemory(SampleService *me, IMemObject memObj)
{
    int32_t ret = Object_ERROR;
    int32_t fd = -1;
    uint64_t sz = 0;
    void *ptr = NULL;
    struct stat memObjStat;
    char const message[] = "This string is from the TA";

    T_GUARD(Object_unwrapFd(memObj, &fd));
    T_CHECK(fstat(fd, &memObjStat) == 0);
    T_CHECK(memObjStat.st_size > 0);

    sz = memObjStat.st_size;
    ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    T_CHECK(ptr != MAP_FAILED);

    snprintf((char *)ptr, sz, "%s", message);
    LOG_MSG("The message back to the caller: %s", (char *)ptr);

exit:
    if (ptr != NULL) munmap(ptr, sz);

    return ret;
}

static int32_t CSampleService_loadQTEETA(SampleService *me, IMemObject appFileObj)
{
    int32_t ret = Object_ERROR;

    LOG_MSG("Start to load a QTEE TA!");

    T_GUARD(ITEnv_open(gTVMEnv, CAppLoader_UID, &gAppLoaderObj));
    T_GUARD(IAppLoader_loadFromRegion(gAppLoaderObj, appFileObj, &gAppCtlObj));
    T_GUARD(IAppController_getAppObject(gAppCtlObj, &gQteeAppObj));

exit:
    return ret;
}

static int32_t CSampleService_shareMemWithQTEETA(SampleService *me, uint32_t uid)
{
    int32_t ret = Object_ERROR, bufFd = -1;
    uint32_t bufSize = SIZE_4KB;
    uint32_t poolMemSize = (bufSize + (SIZE_2MB - 1)) & (~(SIZE_2MB - 1));
    Object memPoolFactoryObj = Object_NULL;
    Object memPoolObj = Object_NULL;
    Object memBuf = Object_NULL;
    ITAccessPermissions_rules confRules = {0};
    confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    char const message[] = "This string is from the TVM TA";
    void *ptr = NULL;

    T_GUARD(ITEnv_open(gTVMEnv, uid, &memPoolFactoryObj));
    T_GUARD(ITMemPoolFactory_createPool(memPoolFactoryObj, &confRules, poolMemSize, &memPoolObj));
    T_GUARD(ITMemPool_allocateBuffer(memPoolObj, bufSize, &memBuf));

    T_GUARD(Object_unwrapFd(memBuf, &bufFd));
    ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
    T_CHECK(ptr != MAP_FAILED);

    snprintf((char *)ptr, bufSize, "%s", message);

    T_GUARD(ISMCIExampleApp_sharedMemoryExample(gQteeAppObj, memBuf));

    ((char *)ptr)[bufSize - 1] = '\0';
    LOG_MSG("Modified buffer: %s", (char *)ptr);

exit:
    if (ptr != NULL) munmap(ptr, bufSize);
    Object_ASSIGN_NULL(gQteeAppObj);
    Object_ASSIGN_NULL(gAppCtlObj);
    Object_ASSIGN_NULL(gAppLoaderObj);
    Object_ASSIGN_NULL(memBuf);
    Object_ASSIGN_NULL(memPoolObj);
    Object_ASSIGN_NULL(memPoolFactoryObj);

    return ret;
}

static int32_t CSampleService_dumpMemoryUsage(SampleService *me, void *bufferOut,
                                              size_t bufferOutLen, size_t *bufferOutLenOut)
{
    char line[256];

    int32_t ret = Object_OK;

    unsigned long memtotal = 0;
    unsigned long memfree = 0;
    unsigned long memavail = 0;
    unsigned long membuffers = 0;
    unsigned long memcached = 0;
    unsigned long memslab = 0;

    // get the contents of /proc/meminfo and store them
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL)
        return Object_ERROR;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &memtotal) == 1)
            continue;
        if (sscanf(line, "MemFree: %lu kB", &memfree) == 1)
            continue;
        if (sscanf(line, "MemAvailable: %lu kB", &memavail) == 1)
            continue;
        if (sscanf(line, "Buffers: %lu kB", &membuffers) == 1)
            continue;
        if (sscanf(line, "Cached: %lu kB", &memcached) == 1)
            continue;
        if (sscanf(line, "Slab: %lu kB", &memslab) == 1)
            continue;
    }

    fclose(fp);

    double mem_total = memtotal / 1024.0;
    double mem_free = memfree / 1024.0;
    double mem_avail = memavail / 1024.0;
    double mem_buffers = membuffers / 1024.0;
    double mem_cached = memcached / 1024.0;
    double mem_slab = memslab / 1024.0;

    double mem_used = mem_total - mem_free - mem_buffers - mem_cached - mem_slab;
    double mem_unreclaimable = mem_total - mem_avail;

    int pos = 0;

    pos += snprintf((char *)bufferOut, bufferOutLen, "qtvm: mem_total=%lf\n", mem_total);
    pos +=
        snprintf((char *)bufferOut + pos, bufferOutLen - pos, "qtvm: mem_used_MB=%lf\n", mem_used);
    pos += snprintf((char *)bufferOut + pos, bufferOutLen - pos, "qtvm: mem_buffers_MB=%lf\n",
                    mem_buffers);
    pos += snprintf((char *)bufferOut + pos, bufferOutLen - pos, "qtvm: mem_cached_MB=%lf\n",
                    mem_cached);
    pos +=
        snprintf((char *)bufferOut + pos, bufferOutLen - pos, "qtvm: mem_slab_MB=%lf\n", mem_slab);
    pos += snprintf((char *)bufferOut + pos, bufferOutLen - pos, "qtvm: mem_unreclaimable_MB=%lf",
                    mem_unreclaimable);

    ((char *)bufferOut)[bufferOutLen -1] = '\0';

    LOG_MSG("The message back to the caller: %s", (char *)bufferOut);

    return ret;
}

static ISampleService_DEFINE_INVOKE(CSampleService_invoke, CSampleService_, SampleService *);

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
    SampleService *me = HEAP_ZALLOC_TYPE(SampleService);

    T_CHECK_ERR(me != NULL, Object_ERROR_MEM);

    LOG_MSG("Opening service with UID = %d", uid);

    Object_ASSIGN(me->credentials, cred);
    me->refs = 1;

    *objOut = (Object){CSampleService_invoke, me};

exit:
    if (ret) {
        CSampleService_release(me);
    }

    return ret;
}
