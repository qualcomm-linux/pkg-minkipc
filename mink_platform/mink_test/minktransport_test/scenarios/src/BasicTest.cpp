// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "BasicTest.h"
#include "CTestModule.h"
#include "IOpener.h"
#include "IRegisterApp.h"
#include "ITestModule.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "qtest.h"

#define MAX_BUF_SIZE 1024 * 512
#define TEST_BUFFER "TEST"
#define TEST_UID 1357
#define MAX_SERVICES 10

int32_t positiveMarshallingSendNullBIGetSingleBO(Object obj)
{
    int32_t ret = Object_OK;
    uint32_t id = 0;

    ITestModule_getId(obj, &id);
    qt_eqi(INVOKE_MAGIC_ID, id);

    return ret;
}

int32_t positiveMarshallingSendSingleBIGetSingleBO(Object obj)
{
    int32_t ret = Object_OK;
    uint32_t id = 0;

    ITestModule_setId(obj, 314159);
    ITestModule_getId(obj, &id);
    qt_eqi(314159, id);

    return ret;
}

int32_t positiveMarshallingSendSingleBIGetMultiBO(Object obj)
{
    int32_t ret = Object_OK;
    uint32_t id[3] = {0};
    int32_t count, idValue = 0;

    ITestModule_setId(obj, 202301);
    ITestModule_getMultiId(obj, &id[0], &id[1], &id[2]);

    for (count = 0; count < sizeof(id) / sizeof(id[0]); count++) {
        idValue = 202301 + count * 100;
        qt_eqi(idValue, id[count]);
    }

    return ret;
}

int32_t positiveMarshallingSendMultiBIGetMultiBO(Object obj)
{
    int32_t ret = Object_OK;
    uint32_t idToGet[3] = {0};
    uint32_t idToSend[3] = {1000, 2000, 3000};
    int32_t count, idValue = 0;

    ITestModule_setMultiId(obj, idToSend[0], idToSend[1], idToSend[2]);
    ITestModule_getMultiId(obj, &idToGet[0], &idToGet[1], &idToGet[2]);

    for (count = 0; count < sizeof(idToGet) / sizeof(idToGet[0]); count++) {
        idValue = 1000 + 2000 + 3000 + count * 100;
        qt_eqi(idValue, idToGet[count]);
    }

    return ret;
}

int32_t positiveMarshallingSendNullOIGetSingleOO(Object obj)
{
    int32_t ret = Object_OK;
    Object remoteObj = Object_NULL;

    ITestModule_getObject(obj, &remoteObj);
    qt_remote(remoteObj);

    ITestModule_release(remoteObj);

    return ret;
}

int32_t positiveMarshallingSendSingleOIGetSingleOO(Object obj)
{
    int32_t ret = Object_OK;
    Object localObjToGet = Object_NULL;
    Object localObjToSend = Object_NULL;

    ret = CTestModule_new(&localObjToSend);
    qt_eqi(0, ret);
    qt_local(localObjToSend);
    ITestModule_setObject(obj, localObjToSend);
    ITestModule_getObject(obj, &localObjToGet);
    qt_local(localObjToGet);

    ITestModule_release(localObjToGet);
    ITestModule_release(localObjToSend);

    return ret;
}

int32_t positiveMarshallingSendSingleOIGetMultiOO(Object obj)
{
    int32_t ret = Object_OK;
    Object localObjToSend = Object_NULL;
    Object localObjToGet[3] = {0};
    int32_t count;

    ret = CTestModule_new(&localObjToSend);
    qt_eqi(0, ret);
    qt_local(localObjToSend);
    ITestModule_setObject(obj, localObjToSend);
    ITestModule_getMultiObject(obj, &localObjToGet[0], &localObjToGet[1],
                               &localObjToGet[2]);

    for (count = 0; count < sizeof(localObjToGet) / sizeof(localObjToGet[0]);
         count++) {
        qt_local(localObjToGet[count]);
        ITestModule_release(localObjToGet[count]);
    }
    ITestModule_release(localObjToSend);

    return ret;
}

int32_t positiveMarshallingSendMultiOIGetMultiOO(Object obj)
{
    int32_t ret = Object_OK;
    Object localObjToSend[3] = {0};
    Object localObjToGet[3] = {0};
    int32_t count = 0;

    for (count = 0; count < sizeof(localObjToSend) / sizeof(localObjToSend[0]);
         count++) {
        ret = CTestModule_new(&localObjToSend[count]);
        qt_eqi(0, ret);
        qt_local(localObjToSend[count]);
        ITestModule_setObject(obj, localObjToSend[count]);
    }

    ITestModule_getMultiObject(obj, &localObjToGet[0], &localObjToGet[1],
                               &localObjToGet[2]);

    for (count = 0; count < sizeof(localObjToGet) / sizeof(localObjToGet[0]);
         count++) {
        qt_local(localObjToGet[count]);
        ITestModule_release(localObjToGet[count]);
        ITestModule_release(localObjToSend[count]);
    }

    return ret;
}

int32_t positiveMarshallingSendRemoteOIGetOO(Object obj)
{
    int32_t ret = Object_OK;
    Object remoteObjToGet = Object_NULL;
    uint32_t id = 0;

    ITestModule_setObject(obj, obj);
    ITestModule_getObject(obj, &remoteObjToGet);
    qt_remote(remoteObjToGet);

    ITestModule_setId(remoteObjToGet, 1024);
    ITestModule_getId(remoteObjToGet, &id);
    qt_eqi(1024, id);

    ITestModule_release(remoteObjToGet);

    return ret;
}

int32_t positiveMarshallingSendLocalFdOIGetOO(Object obj)
{
    int32_t ret = Object_OK;
    int ix = 0, fd = -1;
    Object localFdObjToSend = Object_NULL;
    Object localFdObjToGet = Object_NULL;

    fd = open("/tmp/somefile", O_CREAT | O_RDWR, 0666);
    qt_assert(fd > 0);
    localFdObjToSend = FdWrapper_new(fd);
    qt_fd(localFdObjToSend);

    ITestModule_setObject(obj, localFdObjToSend);
    qt_eqi(Object_OK, Object_unwrapFd(localFdObjToSend, &ix));
    qt_assert(ix != -1);
    ITestModule_release(localFdObjToSend);
    qt_assert(fcntl(ix, F_GETFD) == -1);

    ix = -1;
    ITestModule_getObject(obj, &localFdObjToGet);
    qt_eqi(Object_OK, Object_unwrapFd(localFdObjToGet, &ix));
    qt_assert(ix > 0);
    qt_assert(fcntl(ix, F_GETFD) != -1);
    qt_fd(localFdObjToGet);
    ITestModule_release(localFdObjToGet);

    return ret;
}

int32_t positiveMarshallingMaxArgs(Object obj)
{
    int32_t ret = Object_OK;
    Object testObj = Object_NULL;
    Object objOut[15] = {Object_NULL};
    char outbuf[15][4];
    size_t lenOut = 0;
    int32_t count;

    ret = CTestModule_new(&testObj);
    qt_eqi(0, ret);

    memset(outbuf, 0, sizeof(outbuf));

    ITestModule_maxArgs(
        obj, TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4,
        TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4,
        TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4,
        TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4, outbuf[0], 4, &lenOut,
        outbuf[1], 4, &lenOut, outbuf[2], 4, &lenOut, outbuf[3], 4, &lenOut,
        outbuf[4], 4, &lenOut, outbuf[5], 4, &lenOut, outbuf[6], 4, &lenOut,
        outbuf[7], 4, &lenOut, outbuf[8], 4, &lenOut, outbuf[9], 4, &lenOut,
        outbuf[10], 4, &lenOut, outbuf[11], 4, &lenOut, outbuf[12], 4, &lenOut,
        outbuf[13], 4, &lenOut, outbuf[14], 4, &lenOut, testObj, testObj,
        testObj, testObj, testObj, testObj, testObj, testObj, testObj, testObj,
        testObj, testObj, testObj, testObj, testObj, &objOut[0], &objOut[1],
        &objOut[2], &objOut[3], &objOut[4], &objOut[5], &objOut[6], &objOut[7],
        &objOut[8], &objOut[9], &objOut[10], &objOut[11], &objOut[12],
        &objOut[13], &objOut[14]);

    for (count = 0; count < 15; count++) {
        qt_eqi(0, memcmp(outbuf[count], TEST_BUFFER, 4));
    }

    for (count = 0; count < 15; count++) {
        qt_assert(!Object_isNull(objOut[count]));
        ITestModule_release(objOut[count]);
    }

    ITestModule_release(testObj);

    return ret;
}

int32_t positiveMarshallingDataAligned(Object obj)
{
    int32_t ret = Object_OK;
    uint32_t pval = 0;
    uint32_t id = INVOKE_MAGIC_ID;

    ITestModule_unalignedSet(obj, &id, 1, sizeof(id), &pval);
    qt_eqi(0, pval % 4);

    return ret;
}

int32_t positiveMarshallingBigBufferBIBO(Object obj)
{
    int32_t ret = Object_OK;
    int32_t count;
    char echoIn[MAX_BUF_SIZE];
    char echoOut[MAX_BUF_SIZE];
    int32_t echoInLen = 0;
    size_t echoOutLen = 0;

    for (count = 1; count <= 16; count++) {
        echoInLen = 1 << count;
        echoOutLen = 1 << count;
        memset(echoIn, count, echoInLen);
        memset(echoOut, 0, echoOutLen);
        ITestModule_echo(obj, echoIn, echoInLen, echoOut, echoOutLen,
                         &echoOutLen);

        qt_eqi(0, memcmp(echoIn, echoOut, echoOutLen));
    }

    return ret;
}

int32_t positiveServiceRegisterDeregisterLocal(Object serviceRegisterObj)
{
    int32_t ret = Object_OK;
    Object serviceObj = Object_NULL;

    ret = CTestModule_new(&serviceObj);
    qt_eqi(0, ret);

    ret = IRegisterApp_registerApp(serviceRegisterObj, TEST_UID, serviceObj,
                                   NULL, 0);
    qt_eqi(Object_OK, ret);

    ret = IRegisterApp_deregisterApp(serviceRegisterObj, TEST_UID, NULL, 0);
    qt_eqi(Object_OK, ret);

    ret = IRegisterApp_registerApp(serviceRegisterObj, TEST_UID, serviceObj,
                                   NULL, 0);
    qt_eqi(Object_OK, ret);

    ret = IRegisterApp_deregisterApp(serviceRegisterObj, TEST_UID, NULL, 0);
    qt_eqi(Object_OK, ret);

    if (!Object_isNull(serviceObj)) {
        ITestModule_release(serviceObj);
    }

    return ret;
}

int32_t positiveServiceRegisterDeregisterRemote(Object serviceRegisterObj)
{
    int32_t ret = Object_OK;

    ret = IRegisterApp_registerApp(serviceRegisterObj, TEST_UID,
                                   serviceRegisterObj, NULL, 0);
    qt_eqi(Object_OK, ret);

    ret = IRegisterApp_deregisterApp(serviceRegisterObj, TEST_UID, NULL, 0);
    qt_eqi(Object_OK, ret);

    ret = IRegisterApp_registerApp(serviceRegisterObj, TEST_UID,
                                   serviceRegisterObj, NULL, 0);
    qt_eqi(Object_OK, ret);

    ret = IRegisterApp_deregisterApp(serviceRegisterObj, TEST_UID, NULL, 0);
    qt_eqi(Object_OK, ret);

    return ret;
}

int32_t negativeMarshallingNullBuffNonzeroLenBO(Object obj)
{
    int32_t ret = Object_OK;
    size_t lenOut = 0;

    qt_eqi(Object_ERROR_INVALID,
           ITestModule_bufferOutNull(obj, NULL, 4, &lenOut));
    qt_eqi(0, ITestModule_bufferOutNull(obj, NULL, 0, &lenOut));

    return ret;
}

int32_t negativeMarshallingOverMaxArgs(Object obj)
{
    int32_t ret = Object_OK;
    // TODO: it will casue stack-overflow issue, need add validation in
    // minksocket code
    /*ret = ITestModule_overMaxArgs(obj,
                                TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4,
                                TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4,
                                TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4,
                                TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4,
                                TEST_BUFFER, 4, TEST_BUFFER, 4, TEST_BUFFER, 4,
                                TEST_BUFFER, 4);*/

    qt_eqi(0, ret);

    return ret;
}

int32_t negativeServiceRegisterInvalidUID(Object serviceRegisterObj)
{
    int32_t ret = Object_OK;
    Object serviceObj = Object_NULL;

    ret = CTestModule_new(&serviceObj);
    qt_eqi(0, ret);

    ret = IRegisterApp_registerApp(serviceRegisterObj, 0, serviceObj, NULL, 0);
    qt_eqi(Object_ERROR, ret);

    if (!Object_isNull(serviceObj)) {
        ITestModule_release(serviceObj);
    }

    return ret;
}

int32_t negativeServiceRegisterOverMax(Object serviceRegisterObj)
{
    int32_t ret = Object_OK, res = Object_OK;
    int32_t registerUid = 1000;
    int32_t count;
    Object serviceObj = Object_NULL;

    ret = CTestModule_new(&serviceObj);
    qt_eqi(0, ret);

    for (count = 0; count < MAX_SERVICES; count++) {
        ret = IRegisterApp_registerApp(serviceRegisterObj, registerUid + count,
                                       serviceObj, NULL, 0);
        qt_eqi(Object_OK, ret);
    }

    res = IRegisterApp_registerApp(serviceRegisterObj, registerUid + count + 1,
                                   serviceObj, NULL, 0);
    qt_eqi(Object_ERROR_MAXDATA, res);

    for (count = 0; count < MAX_SERVICES; count++) {
        ret = IRegisterApp_deregisterApp(serviceRegisterObj,
                                         registerUid + count, NULL, 0);
        qt_eqi(Object_OK, ret);
    }

    if (!Object_isNull(serviceObj)) {
        ITestModule_release(serviceObj);
    }

    return res;
}

int32_t negativeServiceDoubleRegister(Object serviceRegisterObj)
{
    int32_t ret = Object_OK, res = Object_OK;
    Object serviceObj = Object_NULL;

    ret = CTestModule_new(&serviceObj);
    qt_eqi(0, ret);

    ret = IRegisterApp_registerApp(serviceRegisterObj, TEST_UID, serviceObj,
                                   NULL, 0);
    qt_eqi(Object_OK, ret);

    res = IRegisterApp_registerApp(serviceRegisterObj, TEST_UID, serviceObj,
                                   NULL, 0);
    qt_eqi(Object_ERROR, res);

    ret = IRegisterApp_deregisterApp(serviceRegisterObj, TEST_UID, NULL, 0);
    qt_eqi(Object_OK, ret);

    if (!Object_isNull(serviceObj)) {
        ITestModule_release(serviceObj);
    }

    return res;
}
