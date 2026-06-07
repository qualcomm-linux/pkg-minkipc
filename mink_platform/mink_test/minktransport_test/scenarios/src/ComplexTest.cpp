// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "BasicTest.h"
#include "CRegisterApp.h"
#include "CTestModule.h"
#include "CTestOpener.h"
#include "CTestUtils.h"
#include "ComplexTest.h"
#include "IModule.h"
#include "IOpener.h"
#include "IRegisterApp.h"
#include "ITestModule.h"
#include "ITestUtils.h"
#include "Heap.h"
#include "qtest.h"

static bool isTestModuleObj(Object obj)
{
    uint32_t id = 0;

    ITestModule_getTypeId(obj, &id);

    return (CTESTMODULE_ID == id) ? true : false;
}

static bool isTestUtilsObj(Object obj)
{
    uint32_t id = 0;

    ITestUtils_getTypeId(obj, &id);

    return (CTESTUTILS_ID == id) ? true : false;
}

static int32_t testUtilsBasicTest(Object obj)
{
    int32_t ret = Object_OK;
    uint32_t valueA = 0;
    uint32_t valueB = 0;
    uint32_t valueSum = 0;
    char *echoIn = "wonderful tonight - Eric Clapton";
    char echoOut[100] = {0};
    size_t echoOutLen = 0;

    valueA = valueB = 100;
    ITestUtils_bufferPlus(obj, valueA, valueB, &valueSum);
    qt_eqi(valueSum, valueA + valueB);

    ITestUtils_bufferEcho(obj, echoIn, strlen(echoIn), echoOut, strlen(echoIn),
                          &echoOutLen);
    qt_eqi(0, memcmp(echoOut, echoIn, strlen(echoIn)));

    return ret;
}

static int32_t multiServiceBasicTest(Object obj)
{
    int32_t ret = Object_OK;

    if (isTestModuleObj(obj)) {
        ret = positiveMarshallingSendMultiBIGetMultiBO(obj);
        qt_eqi(Object_OK, ret);
        ret = positiveMarshallingSendMultiOIGetMultiOO(obj);
        qt_eqi(Object_OK, ret);
    } else if (isTestUtilsObj(obj)) {
        testUtilsBasicTest(obj);
    } else {
        printf("%s error: wrong obj \n", __func__);
        ret = Object_ERROR;
    }

    return ret;
}

static void *accessThread(void *arg)
{
    int32_t ret = Object_OK;
    Object obj = *(Object *)arg;
    uint32_t idToGet[3] = {0};
    uint32_t idToSend[3] = {1000, 2000, 3000};

    if (isTestModuleObj(obj)) {
        ITestModule_setAndGetMultiId(obj, idToSend[0], idToSend[1], idToSend[2],
                                     &idToGet[0], &idToGet[1], &idToGet[2]);
        qt_eqi(0,
               memcmp(idToGet, idToSend, sizeof(idToGet) / sizeof(idToGet[0])));
    } else if (isTestUtilsObj(obj)) {
        testUtilsBasicTest(obj);
    } else {
        printf("error: wrong obj \n");
    }

    return NULL;
}

/*
 * @brief multiple client access one service, include basic test & stress test.
 * Simulate scenarios where multiple trusted app access the same service
 * which register to tzd.
 */
int32_t complexSituationSingleServiceMultiClient(Object moduleRegister,
                                                 Object clientOpenerA,
                                                 Object clientOpenerB,
                                                 Object clientOpenerC)
{
    int32_t ret = Object_OK;
    Object serviceObj = Object_NULL;
    int32_t count;
    Object remoteService[3];
    pthread_t thread_1, thread_2, thread_3;

    ret = CTestOpener_new(&serviceObj);
    qt_eqi(0, ret);

    ret = IRegisterApp_registerApp(moduleRegister, TEST_MODULE_UID, serviceObj,
                                   NULL, 0);
    qt_eqi(Object_OK, ret);

    /* basic test */
    ret = IOpener_open(clientOpenerA, TEST_MODULE_UID, &remoteService[0]);
    qt_eqi(Object_OK, ret);
    qt_assert(!Object_isNull(remoteService[0]));
    ret = positiveMarshallingSendMultiBIGetMultiBO(remoteService[0]);
    qt_eqi(Object_OK, ret);
    ret = positiveMarshallingSendMultiOIGetMultiOO(remoteService[0]);
    qt_eqi(Object_OK, ret);

    ret = IOpener_open(clientOpenerB, TEST_MODULE_UID, &remoteService[1]);
    qt_eqi(Object_OK, ret);
    qt_assert(!Object_isNull(remoteService[1]));
    ret = positiveMarshallingSendMultiBIGetMultiBO(remoteService[1]);
    qt_eqi(Object_OK, ret);
    ret = positiveMarshallingSendMultiOIGetMultiOO(remoteService[1]);
    qt_eqi(Object_OK, ret);

    ret = IOpener_open(clientOpenerC, TEST_MODULE_UID, &remoteService[2]);
    qt_eqi(Object_OK, ret);
    qt_assert(!Object_isNull(remoteService[2]));
    ret = positiveMarshallingSendMultiBIGetMultiBO(remoteService[2]);
    qt_eqi(Object_OK, ret);
    ret = positiveMarshallingSendMultiOIGetMultiOO(remoteService[2]);
    qt_eqi(Object_OK, ret);

    /* multi-thread test */
    pthread_create(&thread_1, NULL, accessThread, (void *)&remoteService[0]);
    pthread_create(&thread_2, NULL, accessThread, (void *)&remoteService[1]);
    pthread_create(&thread_3, NULL, accessThread, (void *)&remoteService[2]);

    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);
    pthread_join(thread_3, NULL);
    for (count = 0; count < 3; count++) {
        if (!Object_isNull(remoteService[count])) {
            Object_ASSIGN_NULL(remoteService[count]);
        }
    }
    if (!Object_isNull(serviceObj)) {
        Object_ASSIGN_NULL(serviceObj);
    }
    ret = IRegisterApp_deregisterApp(moduleRegister, TEST_MODULE_UID, NULL, 0);
    qt_eqi(Object_OK, ret);

    return ret;
}

/*
 * @brief multiple service register to tzd and aceess each other.
 * Simulate scenarios where multiple trusted app register service
 * to tzd and acess service each other.
 */
int32_t complexSituationMultiService(Object serverOpenerA, Object serverOpenerB,
                                     Object serverOpenerC)
{
    int32_t ret = Object_OK;
    int32_t count;
    Object moduleRegister[3] = {0};
    Object remoteServiceAB, remoteServiceAC, remoteServiceBA, remoteServiceBC,
        remoteServiceCA, remoteServiceCB;
    Object serviceObjA = Object_NULL;
    Object serviceObjB = Object_NULL;
    Object serviceObjC = Object_NULL;
    pthread_t thread_1, thread_2, thread_3, thread_4, thread_5, thread_6;

    ret = CTestOpener_new(&serviceObjA);
    qt_eqi(0, ret);
    ret = CTestOpener_new(&serviceObjB);
    qt_eqi(0, ret);
    ret = CTestOpener_new(&serviceObjC);
    qt_eqi(0, ret);

    ret = IOpener_open(serverOpenerA, CRegisterApp_UID, &moduleRegister[0]);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerB, CRegisterApp_UID, &moduleRegister[1]);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerC, CRegisterApp_UID, &moduleRegister[2]);
    qt_eqi(Object_OK, ret);

    ret = IRegisterApp_registerApp(moduleRegister[0], TEST_MODULE_UID,
                                   serviceObjA, NULL, 0);
    qt_eqi(Object_OK, ret);
    ret = IRegisterApp_registerApp(moduleRegister[1], TEST_UTILS_UID,
                                   serviceObjB, NULL, 0);
    qt_eqi(Object_OK, ret);
    ret = IRegisterApp_registerApp(moduleRegister[2], TEST_MODULE_UID + 1,
                                   serviceObjC, NULL, 0);
    qt_eqi(Object_OK, ret);

    ret = IOpener_open(serverOpenerA, TEST_UTILS_UID, &remoteServiceAB);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerA, TEST_MODULE_UID + 1, &remoteServiceAC);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerB, TEST_MODULE_UID, &remoteServiceBA);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerB, TEST_MODULE_UID + 1, &remoteServiceBC);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerC, TEST_MODULE_UID, &remoteServiceCA);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerC, TEST_UTILS_UID, &remoteServiceCB);
    qt_eqi(Object_OK, ret);

    /* basic test */
    ret = multiServiceBasicTest(remoteServiceAB);
    qt_eqi(Object_OK, ret);
    ret = multiServiceBasicTest(remoteServiceAC);
    qt_eqi(Object_OK, ret);
    ret = multiServiceBasicTest(remoteServiceBA);
    qt_eqi(Object_OK, ret);
    ret = multiServiceBasicTest(remoteServiceBC);
    qt_eqi(Object_OK, ret);
    ret = multiServiceBasicTest(remoteServiceCA);
    qt_eqi(Object_OK, ret);
    ret = multiServiceBasicTest(remoteServiceCB);
    qt_eqi(Object_OK, ret);

    /* multi-thread test */
    pthread_create(&thread_1, NULL, accessThread, (void *)&remoteServiceAB);
    pthread_create(&thread_2, NULL, accessThread, (void *)&remoteServiceAC);
    pthread_create(&thread_3, NULL, accessThread, (void *)&remoteServiceBA);
    pthread_create(&thread_4, NULL, accessThread, (void *)&remoteServiceBC);
    pthread_create(&thread_5, NULL, accessThread, (void *)&remoteServiceCA);
    pthread_create(&thread_6, NULL, accessThread, (void *)&remoteServiceCB);

    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);
    pthread_join(thread_3, NULL);
    pthread_join(thread_4, NULL);
    pthread_join(thread_5, NULL);
    pthread_join(thread_6, NULL);

    if (!Object_isNull(remoteServiceAB) && !Object_isNull(remoteServiceAC) &&
        !Object_isNull(remoteServiceBA) && !Object_isNull(remoteServiceBC) &&
        !Object_isNull(remoteServiceCA) && !Object_isNull(remoteServiceCB)) {
        Object_ASSIGN_NULL(remoteServiceCB);
        Object_ASSIGN_NULL(remoteServiceCA);
        Object_ASSIGN_NULL(remoteServiceBC);
        Object_ASSIGN_NULL(remoteServiceBA);
        Object_ASSIGN_NULL(remoteServiceAC);
        Object_ASSIGN_NULL(remoteServiceAB);
    }
    ret =
        IRegisterApp_deregisterApp(moduleRegister[0], TEST_MODULE_UID, NULL, 0);
    qt_eqi(Object_OK, ret);
    ret =
        IRegisterApp_deregisterApp(moduleRegister[1], TEST_UTILS_UID, NULL, 0);
    qt_eqi(Object_OK, ret);
    ret = IRegisterApp_deregisterApp(moduleRegister[2], TEST_MODULE_UID + 1,
                                     NULL, 0);
    qt_eqi(Object_OK, ret);

    for (count = 0; count < 3; count++) {
        if (!Object_isNull(moduleRegister[count])) {
            Object_ASSIGN_NULL(moduleRegister[count]);
        }
    }
    if (!Object_isNull(serviceObjA) && !Object_isNull(serviceObjB) &&
        !Object_isNull(serviceObjC)) {
        Object_ASSIGN_NULL(serviceObjC);
        Object_ASSIGN_NULL(serviceObjB);
        Object_ASSIGN_NULL(serviceObjA);
    }

    return ret;
}

/*
 * @brief client access service B through service A.
 * Simulate scenarios: client -> server A -> server B
 */
int32_t complexSituationMultiServiceMultiClient(Object serverOpenerA,
                                                Object serverOpenerB,
                                                Object clientOpenerA,
                                                Object clientOpenerB)
{
    int32_t ret = Object_OK;
    int32_t count;
    Object moduleRegister[2] = {0};
    Object serviceObjA = Object_NULL;
    Object serviceObjB = Object_NULL;
    Object remoteServiceA, remoteServiceB, remoteServiceCS, remoteServiceCSOut;
    char *echoIn = "wonderful tonight - Eric Clapton";
    char echoOut[100] = {0};
    size_t echoOutLen = 0;

    ret = CTestOpener_new(&serviceObjA);
    qt_eqi(0, ret);
    ret = CTestOpener_new(&serviceObjB);
    qt_eqi(0, ret);

    ret = IOpener_open(serverOpenerA, CRegisterApp_UID, &moduleRegister[0]);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerB, CRegisterApp_UID, &moduleRegister[1]);
    qt_eqi(Object_OK, ret);

    ret = IRegisterApp_registerApp(moduleRegister[0], TEST_UTILS_UID,
                                   serviceObjA, NULL, 0);
    qt_eqi(Object_OK, ret);
    ret = IRegisterApp_registerApp(moduleRegister[1], TEST_MODULE_UID,
                                   serviceObjB, NULL, 0);
    qt_eqi(Object_OK, ret);

    ret = IOpener_open(serverOpenerA, TEST_MODULE_UID, &remoteServiceA);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(serverOpenerB, TEST_UTILS_UID, &remoteServiceB);
    qt_eqi(Object_OK, ret);
    ret = IOpener_open(clientOpenerA, TEST_UTILS_UID, &remoteServiceCS);
    qt_eqi(Object_OK, ret);

    ret = ITestUtils_invocationTransfer(
        remoteServiceCS, (const void *)echoIn, strlen(echoIn), remoteServiceA,
        echoOut, strlen(echoIn), &echoOutLen, &remoteServiceCSOut);
    qt_eqi(Object_OK, ret);
    qt_eqi(0, memcmp(echoOut, echoIn, strlen(echoIn)));
    uint32_t id = 0;
    ITestModule_getId(remoteServiceCSOut, &id);
    qt_eqi(INVOKE_MAGIC_ID, id);

    if (!Object_isNull(remoteServiceCSOut)) {
        Object_ASSIGN_NULL(remoteServiceCSOut);
    }
    if (!Object_isNull(remoteServiceA) && !Object_isNull(remoteServiceB) &&
        !Object_isNull(remoteServiceCS)) {
        Object_ASSIGN_NULL(remoteServiceCS);
        Object_ASSIGN_NULL(remoteServiceB);
        Object_ASSIGN_NULL(remoteServiceA);
    }
    ret =
        IRegisterApp_deregisterApp(moduleRegister[1], TEST_MODULE_UID, NULL, 0);
    qt_eqi(Object_OK, ret);
    ret =
        IRegisterApp_deregisterApp(moduleRegister[0], TEST_UTILS_UID, NULL, 0);
    qt_eqi(Object_OK, ret);
    if (!Object_isNull(moduleRegister[0]) &&
        !Object_isNull(moduleRegister[1])) {
        Object_ASSIGN_NULL(moduleRegister[1]);
        Object_ASSIGN_NULL(moduleRegister[0]);
    }
    if (!Object_isNull(serviceObjA) && !Object_isNull(serviceObjB)) {
        Object_ASSIGN_NULL(serviceObjB);
        Object_ASSIGN_NULL(serviceObjA);
    }

    return ret;
}
