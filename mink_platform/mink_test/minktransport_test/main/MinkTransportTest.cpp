// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include "BasicTest.h"
#include "ComplexTest.h"
#include "CTestModule.h"
#include "CTestUtils.h"
#include "CTestOpener.h"
#include "CRegisterApp.h"
#include "StressTest.h"
#include "gtest/gtest.h"
#include "logging.h"
#include "IOpener.h"
#include "IRegisterApp.h"

using namespace std;

#define CHECK(cond, ...) { if (!(cond)) { FATAL(__VA_ARGS__); } }
#define ASSERT_EQUAL(A, B) {  \
    if (((A) != (B))) {    \
        LOGE("%s:%d failed, A=%lld, B=%lld\n", __func__, __LINE__, (long long int)A, (long long int)B); \
        exit(-1);\
  } \
}

#define ASSERT_N_EQUAL(A, B) {  \
    if (((A) == (B))) {    \
        LOGE("%s:%d failed, A=%lld, B=%lld\n", __func__, __LINE__, (long long int)A, (long long int)B); \
        exit(-1);\
  } \
}

#ifdef OFFTARGET
#define EXAMPLEHUB_PVM_SOCKADDR_UNIX      "exampleHub_PVM_sockUnix"
#define EXAMPLEHUB_PVM_SOCKADDR_QRTR      "exampleHub_PVM_sockQRTR"
#define EXAMPLEHUB_QTVM_SOCKADDR_UNIX     "exampleHub_QTVM_sockUnix"
#define EXAMPLEHUB_QTVM_SOCKADDR_QRTR     "exampleHub_QTVM_sockQRTR"
#define EXAMPLEHUB_QTVM_SOCKARRD_VSOCK    "exampleHub_QTVM_sockVSOCK"
#define EXAMPLEHUB_OEMVM_SOCKADDR_UNIX    "exampleHub_OEMVM_sockUnix"
#define EXAMPLEHUB_OEMVM_SOCKADDR_VSOCK   "exampleHub_OEMVM_sockVsock"
#else
#define EXAMPLEHUB_PVM_SOCKADDR_UNIX      "/dev/socket/exampleHub_PVM"
#define EXAMPLEHUB_PVM_SOCKADDR_QRTR      "5100"
#define EXAMPLEHUB_QTVM_SOCKADDR_UNIX     "/dev/socket/exampleHub_QTVM"
#define EXAMPLEHUB_QTVM_SOCKADDR_QRTR     "5102"
#define EXAMPLEHUB_QTVM_SOCKARRD_VSOCK    "21850"
#define EXAMPLEHUB_OEMVM_SOCKADDR_UNIX    "/dev/socket/exampleHub_OEMVM"
#define EXAMPLEHUB_OEMVM_SOCKADDR_VSOCK   "21860"
#endif

#define G_TEST_THREAD_COUNT 5

class MinkTransportFixture : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
#ifdef OFFTARGET
        system("pkill exampleHub");
        usleep(10000);
        system("./exampleHub -qtvm -offtarget &");
        usleep(10000);
        printf("launched exampleHub of qtvm\n");
#endif
    }

    static void TearDownTestCase()
    {
#ifdef OFFTARGET
        system("pkill exampleHub");
        printf("exited exampleHub of qtvm\n");
#endif
    }

    void SetUp() override
    {
        int ret = 0;

        mLEUnixMinkConn = MinkIPC_connect(EXAMPLEHUB_QTVM_SOCKADDR_UNIX,
                                          &mLEHubUnixOpener);
        CHECK(mLEUnixMinkConn != NULL && !Object_isNull(mLEHubUnixOpener),
              "Connect mLEHubUnixOpener failed");

        mLEUnixDupMinkConn = MinkIPC_connect(EXAMPLEHUB_QTVM_SOCKADDR_UNIX,
                                             &mLEHubUnixDupOpener);
        CHECK(mLEUnixDupMinkConn != NULL && !Object_isNull(mLEHubUnixDupOpener),
              "Connect mLEHubUnixOpener failed");

        ret = IOpener_open(mLEHubUnixOpener, CRegisterApp_UID, &mLEHubUnixRegister);
        CHECK(Object_isOK(ret), "Open mLEHubUnixRegister failed: %d", ret);
    }

    void TearDown() override
    {
        Object_ASSIGN_NULL(mLEHubUnixRegister);
        Object_ASSIGN_NULL(mLEHubUnixDupOpener);
        Object_ASSIGN_NULL(mLEHubUnixOpener);

        if (mLEUnixDupMinkConn) {
            MinkIPC_release(mLEUnixDupMinkConn);
            mLEUnixDupMinkConn = NULL;
        }

        if (mLEUnixMinkConn) {
            MinkIPC_release(mLEUnixMinkConn);
            mLEUnixMinkConn = NULL;
        }
    }

    MinkIPC *mLEUnixMinkConn = NULL;
    MinkIPC *mLEUnixDupMinkConn = NULL;
    Object mLEHubUnixOpener = Object_NULL;
    Object mLEHubUnixDupOpener = Object_NULL;
    Object mLEHubUnixRegister = Object_NULL;
};

class MinkTransport : public MinkTransportFixture
{
    void SetUp() override
    {
        MinkTransportFixture::SetUp();
    }

    void TearDown() override
    {
        MinkTransportFixture::TearDown();
    }
};

TEST_F(MinkTransport, PostiveRegisterServiceTest)
{
    ASSERT_EQ(positiveServiceRegisterDeregisterLocal(mLEHubUnixRegister),
              Object_OK);
    ASSERT_EQ(positiveServiceRegisterDeregisterRemote(mLEHubUnixRegister),
              Object_OK);
}

TEST_F(MinkTransport, NegativeRegisterServiceTest)
{
    ASSERT_EQ(negativeServiceRegisterInvalidUID(mLEHubUnixRegister),
              Object_ERROR);
    ASSERT_EQ(negativeServiceRegisterOverMax(mLEHubUnixRegister),
              Object_ERROR_MAXDATA);
    ASSERT_EQ(negativeServiceDoubleRegister(mLEHubUnixRegister),
              Object_ERROR);
}

TEST_F(MinkTransport, StressServiceTest)
{
    ASSERT_EQ(stressServiceRegister(mLEHubUnixRegister, G_TEST_THREAD_COUNT),
              Object_OK);
    ASSERT_EQ(stressServiceOpen(mLEHubUnixRegister, mLEHubUnixDupOpener,
              G_TEST_THREAD_COUNT), Object_OK);
}

TEST_F(MinkTransport, PostiveMarshalTest)
{
    Object serviceObj = Object_NULL;
    Object remoteService = Object_NULL;

    ASSERT_EQ(CTestOpener_new(&serviceObj), Object_OK);

    ASSERT_EQ(IRegisterApp_registerApp(mLEHubUnixRegister, TEST_MODULE_UID,
              serviceObj, NULL, 0), Object_OK);
    ASSERT_EQ(IOpener_open(mLEHubUnixDupOpener, TEST_MODULE_UID,
              &remoteService), Object_OK);
    CHECK(!Object_isNull(remoteService), "failed at line %u", __LINE__);

    ASSERT_EQ(positiveMarshallingSendNullBIGetSingleBO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendSingleBIGetSingleBO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendSingleBIGetMultiBO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendMultiBIGetMultiBO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendNullOIGetSingleOO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendSingleOIGetSingleOO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendSingleOIGetMultiOO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendMultiOIGetMultiOO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendRemoteOIGetOO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingSendLocalFdOIGetOO(remoteService),
              Object_OK);
    ASSERT_EQ(positiveMarshallingMaxArgs(remoteService), Object_OK);
    ASSERT_EQ(positiveMarshallingDataAligned(remoteService), Object_OK);
    ASSERT_EQ(positiveMarshallingBigBufferBIBO(remoteService), Object_OK);
    ASSERT_EQ(IRegisterApp_deregisterApp(mLEHubUnixRegister, TEST_MODULE_UID,
              NULL, 0), Object_OK);
    if (!Object_isNull(remoteService)) {
        Object_ASSIGN_NULL(remoteService);
    }
    if (!Object_isNull(serviceObj)) {
        Object_ASSIGN_NULL(serviceObj);
    }
}

TEST_F(MinkTransport, NegativeMarshalTest)
{
    Object serviceObj = Object_NULL;
    Object remoteService = Object_NULL;

    ASSERT_EQ(CTestOpener_new(&serviceObj), Object_OK);

    ASSERT_EQ(IRegisterApp_registerApp(mLEHubUnixRegister, TEST_MODULE_UID,
              serviceObj, NULL, 0), Object_OK);
    ASSERT_EQ(IOpener_open(mLEHubUnixDupOpener, TEST_MODULE_UID,
              &remoteService), Object_OK);
    CHECK(!Object_isNull(remoteService), "failed at line %u", __LINE__);

    ASSERT_EQ(negativeMarshallingNullBuffNonzeroLenBO(remoteService),
              Object_OK);
    ASSERT_EQ(negativeMarshallingOverMaxArgs(remoteService), Object_OK);

    ASSERT_EQ(IRegisterApp_deregisterApp(mLEHubUnixRegister, TEST_MODULE_UID,
              NULL, 0), Object_OK);
    if (!Object_isNull(remoteService)) {
        Object_ASSIGN_NULL(remoteService);
    }
    if (!Object_isNull(serviceObj)) {
        Object_ASSIGN_NULL(serviceObj);
    }
}

TEST_F(MinkTransport, StressMarshalTest)
{
    Object serviceObj = Object_NULL;
    Object remoteService = Object_NULL;

    ASSERT_EQ(CTestOpener_new(&serviceObj), Object_OK);

    ASSERT_EQ(IRegisterApp_registerApp(mLEHubUnixRegister, TEST_MODULE_UID,
              serviceObj, NULL, 0), Object_OK);
    ASSERT_EQ(IOpener_open(mLEHubUnixDupOpener, TEST_MODULE_UID,
              &remoteService), Object_OK);
    CHECK(!Object_isNull(remoteService), "failed at line %u", __LINE__);

    ASSERT_EQ(stressMarshallingAccessBIBO(remoteService, G_TEST_THREAD_COUNT),
              Object_OK);
    ASSERT_EQ(stressMarshallingAccessOIOO(remoteService, G_TEST_THREAD_COUNT),
              Object_OK);
    ASSERT_EQ(stressMarshallingAccessRemoteOIOO(remoteService, G_TEST_THREAD_COUNT),
              Object_OK);
    //ASSERT_EQ(stressMarshallingAccessLocalFdOIOO(remoteService, G_TEST_THREAD_COUNT),
    //            Object_OK);

    ASSERT_EQ(IRegisterApp_deregisterApp(mLEHubUnixRegister, TEST_MODULE_UID,
              NULL, 0), Object_OK);
    if (!Object_isNull(remoteService)) {
        Object_ASSIGN_NULL(remoteService);
    }

    if (!Object_isNull(serviceObj)) {
        Object_ASSIGN_NULL(serviceObj);
    }
}

TEST_F(MinkTransport, PositiveMemparcelTest)
{
    //TODO: add test case after finish memparcel implement
}

TEST_F(MinkTransport, NegativeMemparcelTest)
{
    //TODO: add test case after finish memparcel implement
}

TEST_F(MinkTransport, StressMemparcelTest)
{
    //TODO: add test case after finish memparcel implement
}

TEST_F(MinkTransport, ComplexSituationTest)
{
    int32_t count;
    MinkIPC *clientMinkConn[3] = {0};
    Object clientOpener[3] = {0};
    MinkIPC *serverMinkConn[3] = {0};
    Object serverOpener[3] = {0};

    for (count = 0; count < 3; count++) {
        clientMinkConn[count] = MinkIPC_connect(EXAMPLEHUB_QTVM_SOCKADDR_UNIX,
                                                &clientOpener[count]);
        CHECK(clientMinkConn[count] != NULL &&
              !Object_isNull(clientOpener[count]),
              "Connect mLEHubUnixOpener failed");
    }

    for (count = 0; count < 3; count++) {
        serverMinkConn[count] = MinkIPC_connect(EXAMPLEHUB_QTVM_SOCKADDR_UNIX,
                                                &serverOpener[count]);
        CHECK(serverMinkConn[count] != NULL &&
              !Object_isNull(serverOpener[count]),
              "Connect mLEHubUnixOpener failed");
    }

    ASSERT_EQ(complexSituationSingleServiceMultiClient(mLEHubUnixRegister,
                                                       clientOpener[0],
                                                       clientOpener[1],
                                                       clientOpener[2]),
                                                       Object_OK);
    ASSERT_EQ(complexSituationMultiService(serverOpener[0], serverOpener[1],
                                           serverOpener[2]), Object_OK);
    ASSERT_EQ(complexSituationMultiServiceMultiClient(serverOpener[0],
                                                      serverOpener[1],
                                                      serverOpener[0],
                                                      serverOpener[1]),
                                                      Object_OK);

    for (count = 0; count < 3; count++) {
        Object_ASSIGN_NULL(serverOpener[count]);
        Object_ASSIGN_NULL(clientOpener[count]);
    }

    for (count = 0; count < 3; count++) {
        MinkIPC_release(serverMinkConn[count]);
        MinkIPC_release(clientMinkConn[count]);
    }
}
