// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
extern "C" {
#include "ICredentials.h"
#include "ITRegisterModule.h"
#include "CTRegisterModule.h"
#include "TModule.h"
#include "ITModule.h"
#include "Credentials.h"
#include "CTRegisterModule_open.h"
#include "CTRegisterModule_priv.h"
}
#include <gtest/gtest.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "IModuleHelper.h"
#include "MinkTypes.h"
#include "MyTestIModule.h"
#include "heap.h"

/*
 *  ----------------------- GLOBAL BACKGROUND --------------------------
 */

#define MAX_TEST_THREADS 5

const char *gMdStr[] = {
    "n=appName_0;p=1:f;s=7;x=extra",
    "n=appName_1;p=5:f;s=3",
    "p=5;s=3"
};

const uint32_t gRot = 42;
char const *gDomainStr = "foo";

/*
 * ------------------- TEST SUITE DEFINITION ------------------------------
 */

#define isOk(__subroutine__) ASSERT_NO_FATAL_FAILURE(__subroutine__)

class TModuleTest : public testing::Test
{
   protected:
    // global setup, only called once (for this test suite).
    static void SetUpTestCase()
    {
    }

    // global teardown, only called once (for this test suite).
    static void TearDownTestCase()
    {
    }

    // local setup, called each time for each instance of test fixture.
    void SetUp() override
    {
        mDid = (DistId *)heap_zalloc(sizeof(DistId));
        mPName = (void *)malloc(2 * sizeof(char));
        mPValue = (void *)malloc(10 * sizeof(char));

        for (int i = 0; i < MAX_TEST_THREADS; i++) {
            mMyModObj[i] = (Object){MyTestIModule_invoke, &(mMyMod[i])};
        }

        printf("setup completed.\n");
    }

    // local teardown, called each time for each instance of test fixture.
    void TearDown() override
    {
        for (int i = 0; i < MAX_TEST_THREADS; i++) {
            Object_ASSIGN_NULL(tModObj[i]);
        }

        HEAP_FREE_PTR(mDid);
        HEAP_FREE_PTR(mPName);
        HEAP_FREE_PTR(mPValue);
    }

    Object tModObj[MAX_TEST_THREADS] = {Object_NULL};

    // need not to be assign_null, cuz imodule(mymodule) is not allocated from heap.
    Object mService = Object_NULL;

    MyTestIModule mMyMod[MAX_TEST_THREADS] = {{}};
    Object mMyModObj[MAX_TEST_THREADS] = {Object_NULL};
    DistId *mDid = nullptr;
    size_t mPNameLenOut, mPValLenOut;
    void *mPName = nullptr;
    void *mPValue = nullptr;
};

/*
 *  --------------------- START_ROUTINE DEFINITION ------------------------
 */

typedef struct {
    int idx;
    Object *myModObj;
    Object *tModObj;
    int mdIdx;
} threadData;

#define _TModuleTest_releaseTModule(obj) do {                           \
        ASSERT_EQ(Object_OK, TModule_release((TModule *)((Object *)obj)->context)); \
        *obj = Object_NULL;                                             \
    } while (0)

static void _createTModule(int idx, Object *tModObj, int mdIdx)
{
    if (mdIdx >= 0) {
        ASSERT_EQ(Object_OK, TModule_new(idx, gMdStr[mdIdx], gRot, gDomainStr, tModObj));
    } else {
        ASSERT_EQ(Object_ERROR, TModule_new(idx, NULL, 0, NULL, tModObj));
    }
}

static void registerCreateTModule(int idx, Object *myModObj, Object *tModObj, int mdIdx)
{
    isOk(__registerIModule(idx, myModObj));
    isOk(_createTModule(idx, tModObj, mdIdx));
}

static void *createTModule(void *args)
{
    threadData *data = (threadData *)args;
    _createTModule(data->idx, data->tModObj, data->mdIdx);
    return NULL;
}

static void testCreateAndReleaseTModule(int idx, Object *myModObj, Object *tModObj, int mdIdx)
{
    isOk(registerCreateTModule(idx, myModObj, tModObj, mdIdx));
    if (tModObj->context) {
        _TModuleTest_releaseTModule(tModObj);
    }


}

static void *testCreateAndReleaseTModuleParallel(void *args)
{
    threadData *data = (threadData *)args;
    testCreateAndReleaseTModule(data->idx, data->myModObj, data->tModObj, data->mdIdx);
    return NULL;
}

/*
 * --------------- TEST CASES DEFINITION ---------------
 */
TEST_F(TModuleTest, NegativeCreateTModuleWithNoMD)
{
    isOk(testCreateAndReleaseTModule(0, &mMyModObj[0], &tModObj[0], -1));
}

TEST_F(TModuleTest, PositiveCreateTModuleWithMD)
{
    isOk(testCreateAndReleaseTModule(0, &mMyModObj[0], &tModObj[0], 0));
}

TEST_F(TModuleTest, PositiveCreateTModuleEarlierThanRegisterIModule)
{
    pthread_t mthread;
    threadData mdata;

    mdata.idx = 0;
    mdata.myModObj = &mMyModObj[0];
    mdata.tModObj = &tModObj[0];
    mdata.mdIdx = 0;

    ASSERT_EQ(0, pthread_create(&mthread, NULL, createTModule, &mdata));

    isOk(__registerIModule(0, &mMyModObj[0]));

    ASSERT_EQ(0, pthread_join(mthread, NULL));

    _TModuleTest_releaseTModule(&tModObj[0]);
}

TEST_F(TModuleTest, PositiveTestTModuleWithDiffPriv)
{
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 1));
    ASSERT_EQ(Object_ERROR, TModule_testPrivilege((TModule *)tModObj[0].context, 0x4));
    ASSERT_EQ(Object_OK, TModule_testPrivilege((TModule *)tModObj[0].context, 0x5));
    ASSERT_EQ(Object_ERROR, TModule_testPrivilege((TModule *)tModObj[0].context, 0xa));
    _TModuleTest_releaseTModule(&tModObj[0]);
}

TEST_F(TModuleTest, PositiveOperationOnTModuleRefCount)
{
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 0));
    ASSERT_EQ(Object_OK, TModule_retain((TModule *)tModObj[0].context));
    ASSERT_EQ(Object_OK, TModule_release((TModule *)tModObj[0].context));
    _TModuleTest_releaseTModule(&tModObj[0]);
}

TEST_F(TModuleTest, PositiveMultipleTModulesExplicitFinalRelease)
{
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 0));
    isOk(registerCreateTModule(1, &mMyModObj[1], &tModObj[1], 1));
    _TModuleTest_releaseTModule(&tModObj[0]);
    _TModuleTest_releaseTModule(&tModObj[1]);
}

TEST_F(TModuleTest, PositiveOperationOnIModule)
{
    // Setup tmodule
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 0));
    isOk(registerCreateTModule(1, &mMyModObj[1], &tModObj[1], 1));

    // Setup imodule
    ITModule_setIModule(tModObj[0], mMyModObj[0]);
    ITModule_setIModule(tModObj[1], mMyModObj[1]);

    // Not Found TModule
    ASSERT_EQ(Object_ERROR, TModule_open((TModule *)tModObj[0].context, 0x3, &mService));

    ITModule_enable(tModObj[0]);

    // Not Found TModule
    ASSERT_EQ(Object_ERROR, TModule_open((TModule *)tModObj[0].context, 0x3, &mService));

    ITModule_enable(tModObj[1]);

    // Discover caller-allowed services
    ASSERT_EQ(Object_OK, TModule_open((TModule *)tModObj[0].context, 0x3, &mService));
    Object_ASSIGN_NULL(mService);
    ASSERT_EQ(Object_OK, TModule_open((TModule *)tModObj[1].context, 0x7, &mService));
    Object_ASSIGN_NULL(mService);

    // Discover a service for which caller has no privilege
    ASSERT_EQ(Object_ERROR, TModule_open((TModule *)tModObj[0].context, 0xC, &mService));

    // Discover a unregistered service
    ASSERT_EQ(Object_ERROR, TModule_open((TModule *)tModObj[0].context, 0x1, &mService));

    _TModuleTest_releaseTModule(&tModObj[0]);
    _TModuleTest_releaseTModule(&tModObj[1]);
}

TEST_F(TModuleTest, PositiveTModuleGetName)
{
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 0));
    ASSERT_EQ(0, strcmp(TModule_getName((TModule *)tModObj[0].context), "appName_0"));
    isOk(registerCreateTModule(1, &mMyModObj[1], &tModObj[1], 1));
    ASSERT_EQ(0, strcmp(TModule_getName((TModule *)tModObj[1].context), "appName_1"));
    _TModuleTest_releaseTModule(&tModObj[0]);
    _TModuleTest_releaseTModule(&tModObj[1]);
}

TEST_F(TModuleTest, PositiveTModuleGetDistId)
{
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 1));

    /* If we'd like to pass below, we need to set domain in md?
     * But TModule isnt exposed outwards.
     * Furthermore, domain could not be set by parseMetadata.
     */
    ASSERT_EQ(Object_OK, TModule_getDistId((TModule *)tModObj[0].context, mDid));
    _TModuleTest_releaseTModule(&tModObj[0]);
}

TEST_F(TModuleTest, NegativeTModuleGetDistId)
{
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 2));

    /* If we'd like to pass below, we need to set domain in md?
     * But TModule isnt exposed outwards.
     * Furthermore, domain could not be set by parseMetadata.
     */
    ASSERT_EQ(Object_ERROR, TModule_getDistId((TModule *)tModObj[0].context, mDid));
    _TModuleTest_releaseTModule(&tModObj[0]);
}

TEST_F(TModuleTest, PositiveStressCreateTModule)
{
    pthread_t mthread[MAX_TEST_THREADS];
    threadData mdata[MAX_TEST_THREADS];
    for (int i = 0; i < MAX_TEST_THREADS; i++) {
        mdata[i].idx = i;
        mdata[i].myModObj = &mMyModObj[i];
        mdata[i].tModObj = &tModObj[i];
        mdata[i].mdIdx = 0;
        ASSERT_EQ(0, pthread_create(&mthread[i], NULL, testCreateAndReleaseTModuleParallel,
                  &mdata[i]));
    }

    for (int i = 0; i < MAX_TEST_THREADS; i++) {
        ASSERT_EQ(0, pthread_join(mthread[i], NULL));
    }
}

TEST_F(TModuleTest, PositiveTModuleGetPropertyByIndex)
{
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 0));
    Object cred = TModule_getCredentials((TModule *)tModObj[0].context);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(Object_OK, ICredentials_getPropertyByIndex(cred, i, mPName, 2, &mPNameLenOut,
                                                             mPValue, 10, &mPValLenOut));
    }


    // Should fail to get property out of bounds
    ASSERT_EQ(ICredentials_ERROR_NOT_FOUND,
              ICredentials_getPropertyByIndex(cred, -2, mPName, 2, &mPNameLenOut, mPValue, 10,
                                              &mPValLenOut));
    ASSERT_EQ(ICredentials_ERROR_NOT_FOUND,
              ICredentials_getPropertyByIndex(cred, 4, mPName, 2, &mPNameLenOut, mPValue, 10,
                                              &mPValLenOut));
    ASSERT_EQ(ICredentials_ERROR_NAME_SIZE,
              ICredentials_getPropertyByIndex(cred, 1, mPName, 0, &mPNameLenOut, mPValue, 10,
                                              &mPValLenOut));
    ASSERT_EQ(ICredentials_ERROR_VALUE_SIZE,
              ICredentials_getPropertyByIndex(cred, 1, mPName, 2, &mPNameLenOut, mPValue, 2,
                                              &mPValLenOut));

    _TModuleTest_releaseTModule(&tModObj[0]);
}

TEST_F(TModuleTest, PositiveTModuleGetValueByName)
{
    isOk(registerCreateTModule(0, &mMyModObj[0], &tModObj[0], 0));
    Object cred = TModule_getCredentials((TModule *)tModObj[0].context);

    snprintf((char *)mPName, 2, "%s", "n");
    ASSERT_EQ(Object_OK, ICredentials_getValueByName(cred, mPName, 2, mPValue, 10, &mPValLenOut));

    snprintf((char *)mPName, 2, "%s", "p");
    ASSERT_EQ(Object_OK, ICredentials_getValueByName(cred, mPName, 2, mPValue, 10, &mPValLenOut));

    snprintf((char *)mPName, 2, "%s", "s");
    ASSERT_EQ(Object_OK, ICredentials_getValueByName(cred, mPName, 2, mPValue, 10, &mPValLenOut));

    snprintf((char *)mPName, 2, "%s", "x");
    ASSERT_EQ(Object_OK, ICredentials_getValueByName(cred, mPName, 2, mPValue, 10, &mPValLenOut));

    // Fail to get TModule credentials by name
    snprintf((char *)mPName, 2, "%s", "e");
    ASSERT_EQ(ICredentials_ERROR_NOT_FOUND,
              ICredentials_getValueByName(cred, mPName, 2, mPValue, 10, &mPValLenOut));

    // Fail to get TModule credentials because of size
    snprintf((char *)mPName, 2, "%s", "n");
    ASSERT_EQ(ICredentials_ERROR_VALUE_SIZE,
              ICredentials_getValueByName(cred, mPName, 2, mPValue, 2, &mPValLenOut));

    // Fail to get TModule credentials because of size
    snprintf((char *)mPName, 2, "%s", "x");
    ASSERT_EQ(ICredentials_ERROR_VALUE_SIZE,
              ICredentials_getValueByName(cred, mPName, 2, mPValue, 2, &mPValLenOut));

    _TModuleTest_releaseTModule(&tModObj[0]);
}
