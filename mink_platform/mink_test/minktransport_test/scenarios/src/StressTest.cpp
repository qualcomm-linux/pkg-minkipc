// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "BasicTest.h"
#include "CTestModule.h"
#include "CTestOpener.h"
#include "IOpener.h"
#include "IRegisterApp.h"
#include "ITestModule.h"
#include "StressTest.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "qtest.h"

#define MAX_SERVICES 10

typedef struct {
    Object registerServiceObj;
    Object serviceObj;
    Object serviceOpener;
    int32_t registerUid;
} threadInfo;

static void *concurrentAccessBIBO(void *arg)
{
    threadInfo registerInfo = *(threadInfo *)arg;
    Object remoteService = registerInfo.serviceObj;
    uint32_t idToGet[3] = {0};
    uint32_t idToSend[3] = {1000, 2000, 3000};

    ITestModule_setAndGetMultiId(remoteService, idToSend[0], idToSend[1],
                                 idToSend[2], &idToGet[0], &idToGet[1],
                                 &idToGet[2]);

    qt_eqi(0, memcmp(idToGet, idToSend, sizeof(idToGet) / sizeof(idToGet[0])));

    return NULL;
}

static void *concurrentAccessOIOO(void *arg)
{
    int32_t ret = Object_OK;
    threadInfo registerInfo = *(threadInfo *)arg;
    Object remoteService = registerInfo.serviceObj;
    Object localObjToGet = Object_NULL;
    Object localObjToSend = Object_NULL;

    ret = CTestModule_new(&localObjToSend);
    qt_eqi(0, ret);
    qt_local(localObjToSend);

    ITestModule_setAndGetObj(remoteService, localObjToSend, &localObjToGet);
    qt_local(localObjToGet);

    ITestModule_release(localObjToGet);
    ITestModule_release(localObjToSend);

    return NULL;
}

static void *concurrentAccessRemoteOIOO(void *arg)
{
    threadInfo registerInfo = *(threadInfo *)arg;
    Object remoteService = registerInfo.serviceObj;
    Object remoteObjToGet = Object_NULL;

    ITestModule_setMsforwardObjAndGet(remoteService, remoteService,
                                      &remoteObjToGet);
    qt_remote(remoteObjToGet);

    ITestModule_release(remoteObjToGet);

    return NULL;
}

static void *concurrentAccessLocalFdOIOO(void *arg)
{
    int32_t ret = Object_OK;
    int ix = 0, fd = -1;
    char fileName[64] = {0};
    int32_t pathValue = (int32_t)random();
    threadInfo registerInfo = *(threadInfo *)arg;
    Object remoteService = registerInfo.serviceObj;
    Object localFdObjToSend = Object_NULL;
    Object localFdObjToGet = Object_NULL;

    snprintf(fileName, sizeof(fileName), "/tmp/somefile_%d", pathValue);
    fd = open(fileName, O_CREAT | O_RDWR, 0666);
    qt_assert(fd > 0);
    localFdObjToSend = FdWrapper_new(fd);
    qt_fd(localFdObjToSend);

    qt_eqi(Object_OK, Object_unwrapFd(localFdObjToSend, &ix));
    qt_assert(ix != -1);
    ITestModule_setLocalFdObjAndGet(remoteService, localFdObjToSend,
                                    &localFdObjToGet);

    ix = -1;
    qt_eqi(Object_OK, Object_unwrapFd(localFdObjToGet, &ix));
    qt_assert(ix > 0);
    qt_assert(fcntl(ix, F_GETFD) != -1);
    qt_fd(localFdObjToGet);
    ITestModule_release(localFdObjToGet);
    ITestModule_release(localFdObjToSend);

    qt_eqi(Object_OK, ret);

    return NULL;
}

static void *concurrentRegisterService(void *arg)
{
    threadInfo registerInfo = *(threadInfo *)arg;
    int32_t count;
    int32_t registerUid = 0;
    int32_t ret = Object_OK;

    for (count = 0; count < MAX_SERVICES; count++) {
        registerUid = (int32_t)random();

        ret = IRegisterApp_registerApp(registerInfo.registerServiceObj,
                                       registerUid, registerInfo.serviceObj,
                                       NULL, 0);
        if (Object_OK == ret) {
            ret = IRegisterApp_deregisterApp(registerInfo.registerServiceObj,
                                             registerUid, NULL, 0);
            qt_eqi(Object_OK, ret);
            continue;
        }

        if (Object_ERROR_MAXDATA == ret) {
            sleep(1);
            continue;
        }
    }

    qt_eqi(Object_OK, ret);

    return NULL;
}

static void *concurrentServiceOpen(void *arg)
{
    threadInfo registerInfo = *(threadInfo *)arg;
    int32_t count;
    int32_t registerUid = 0;
    int32_t ret = Object_OK;
    Object remoteService = Object_NULL;

    ret = IOpener_open(registerInfo.serviceOpener, registerInfo.registerUid,
                       &remoteService);
    qt_eqi(Object_OK, ret);
    qt_assert(!Object_isNull(remoteService));

    if (!Object_isNull(remoteService)) {
        Object_ASSIGN_NULL(remoteService);
    }

    return NULL;
}

/*
 * @brief Multiple processes access input-buffer & output-buffer
 * Simulate scenarios where multiple trusted app access BI/BO.
 */
int32_t stressMarshallingAccessBIBO(Object obj, int32_t threadCount)
{
    int32_t ret = Object_OK;
    int32_t count;
    pthread_t *threads = NULL;
    threadInfo registerInfo = {0};

    threads = (pthread_t *)malloc(sizeof(pthread_t) * threadCount);
    if (NULL == threads) {
        return Object_ERROR_MEM;
    }

    registerInfo.serviceObj = obj;
    for (count = 0; count < threadCount; count++) {
        pthread_create(&threads[count], NULL, concurrentAccessBIBO,
                       (void *)&registerInfo);
    }

    ret = positiveMarshallingSendMultiBIGetMultiBO(obj);
    if (Object_OK != ret) {
        goto err;
    }

err:
    for (count = 0; count < threadCount; count++) {
        pthread_join(threads[count], NULL);
    }
    free(threads);

    return ret;
}

/*
 * @brief Multiple processes access local input-object & output-object
 * Simulate scenarios where multiple trusted app access IO/OO.
 */
int32_t stressMarshallingAccessOIOO(Object obj, int32_t threadCount)
{
    int32_t ret = Object_OK;
    int32_t count;
    pthread_t *threads = NULL;
    threadInfo registerInfo = {0};

    threads = (pthread_t *)malloc(sizeof(pthread_t) * threadCount);
    if (NULL == threads) {
        return Object_ERROR_MEM;
    }

    registerInfo.serviceObj = obj;
    for (count = 0; count < threadCount; count++) {
        pthread_create(&threads[count], NULL, concurrentAccessOIOO,
                       (void *)&registerInfo);
    }

    for (count = 0; count < threadCount; count++) {
        pthread_join(threads[count], NULL);
    }
    free(threads);

    return ret;
}

/*
 * @brief Multiple processes access remote input-object & output-object
 * Simulate scenarios where multiple trusted app access IO/OO.
 */
int32_t stressMarshallingAccessRemoteOIOO(Object obj, int32_t threadCount)
{
    int32_t ret = Object_OK;
    int32_t count;
    pthread_t *threads = NULL;
    threadInfo registerInfo = {0};

    threads = (pthread_t *)malloc(sizeof(pthread_t) * threadCount);
    if (NULL == threads) {
        return Object_ERROR_MEM;
    }

    registerInfo.serviceObj = obj;
    for (count = 0; count < threadCount; count++) {
        pthread_create(&threads[count], NULL, concurrentAccessRemoteOIOO,
                       (void *)&registerInfo);
    }

    for (count = 0; count < threadCount; count++) {
        pthread_join(threads[count], NULL);
    }
    free(threads);

    return ret;
}

/*
 * @brief Multiple processes access local fd input-object & output-object
 * Simulate scenarios where multiple trusted app access fd IO/OO.
 */
int32_t stressMarshallingAccessLocalFdOIOO(Object obj, int32_t threadCount)
{
    int32_t ret = Object_OK;
    int32_t count;
    pthread_t *threads = NULL;
    threadInfo registerInfo = {0};

    threads = (pthread_t *)malloc(sizeof(pthread_t) * threadCount);
    if (NULL == threads) {
        return Object_ERROR_MEM;
    }

    registerInfo.serviceObj = obj;
    for (count = 0; count < threadCount; count++) {
        pthread_create(&threads[count], NULL, concurrentAccessLocalFdOIOO,
                       (void *)&registerInfo);
    }

    for (count = 0; count < threadCount; count++) {
        pthread_join(threads[count], NULL);
    }
    free(threads);

    return ret;
}

/*
 * @brief Multiple processes register service
 * Simulate scenarios where multiple trusted app register service to tzd.
 */
int32_t stressServiceRegister(Object registerServiceObj, int32_t threadCount)
{
    int32_t ret = Object_OK;
    int32_t count;
    pthread_t *threads = NULL;
    threadInfo registerInfo = {0};
    Object serviceObj = Object_NULL;

    threads = (pthread_t *)malloc(sizeof(pthread_t) * threadCount);
    if (NULL == threads) {
        return Object_ERROR_MEM;
    }

    ret = CTestOpener_new(&serviceObj);
    if (Object_OK != ret) {
        goto err;
    }

    registerInfo.registerServiceObj = registerServiceObj;
    registerInfo.serviceObj = serviceObj;

    for (count = 0; count < threadCount; count++) {
        pthread_create(&threads[count], NULL, concurrentRegisterService,
                       (void *)&registerInfo);
    }

    for (count = 0; count < threadCount; count++) {
        pthread_join(threads[count], NULL);
    }

    if (!Object_isNull(serviceObj)) {
        ITestModule_release(serviceObj);
    }
err:
    free(threads);

    return ret;
}

/*
 * @brief Multiple processes 'IOpener_open' the same service
 * Simulate scenarios where multiple trusted app access service from tzd.
 */
int32_t stressServiceOpen(Object registerServiceObj, Object serviceOpener,
                          int32_t threadCount)
{
    int32_t ret = Object_OK;
    int32_t count;
    pthread_t *threads = NULL;
    threadInfo registerInfo = {0};
    Object serviceObj = Object_NULL;

    threads = (pthread_t *)malloc(sizeof(pthread_t) * threadCount);
    if (NULL == threads) {
        return Object_ERROR_MEM;
    }

    ret = CTestOpener_new(&serviceObj);
    if (Object_OK != ret) {
        goto errOpenerNew;
    }

    registerInfo.registerServiceObj = registerServiceObj;
    registerInfo.serviceOpener = serviceOpener;
    registerInfo.serviceObj = serviceObj;
    registerInfo.registerUid = TEST_MODULE_UID;

    ret =
        IRegisterApp_registerApp(registerInfo.registerServiceObj,
                                 registerInfo.registerUid, serviceObj, NULL, 0);
    if (Object_OK != ret) {
        goto errRegister;
    }

    for (count = 0; count < threadCount; count++) {
        pthread_create(&threads[count], NULL, concurrentServiceOpen,
                       (void *)&registerInfo);
    }

    for (count = 0; count < threadCount; count++) {
        pthread_join(threads[count], NULL);
    }
    IRegisterApp_deregisterApp(registerInfo.registerServiceObj, TEST_MODULE_UID,
                               NULL, 0);
errRegister:
    if (!Object_isNull(serviceObj)) {
        ITestModule_release(serviceObj);
    }
errOpenerNew:
    free(threads);

    return ret;
}
