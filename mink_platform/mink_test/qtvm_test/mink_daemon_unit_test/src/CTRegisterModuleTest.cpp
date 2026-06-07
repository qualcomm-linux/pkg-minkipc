// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
extern "C" {
#include "ICredentials.h"
#include "ITRegisterModule.h"
#include "CTRegisterModule.h"
#include "Credentials.h"
#include "CTRegisterModule_open.h"
#include "CTRegisterModule_priv.h"
}
#include <gtest/gtest.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "MyTestIModule.h"
#include "heap.h"
#include "MinkTypes.h"
#include "IModuleHelper.h"

#include <cstdio>
#include <cstdarg>
#include <string>
using namespace std;

/*
 *  ----------------------- GLOBAL BACKGROUND --------------------------
 */

#define MAX_TEST_THREADS 5

// The above value should consist with MAX_WAIT_TIME_IN_SECONDS in CTRegisterModule.c
#define REGISTER_IMODULE_MAX_TIMEOUT 1

/*
 *  ----------------------- HELPER FUNCTION --------------------------
 */

#define CMD_STR_BUFFER_MAX_SIZE 100

static string gCmdStrBuffer;

const char* cmdStr(const char *__format, ...)
{
    char __buffer[CMD_STR_BUFFER_MAX_SIZE];
    va_list __vargs;

    memset(__buffer, 0, sizeof(__buffer));
    va_start(__vargs, __format);
    vsnprintf(__buffer, sizeof(__buffer), __format, __vargs);
    va_end(__vargs);

    gCmdStrBuffer = string(__buffer);

    return gCmdStrBuffer.c_str();
}

/*
 * ------------------- TEST SUITE DEFINITION ------------------------------
 */

#define isOk(__subroutine__) ASSERT_NO_FATAL_FAILURE(__subroutine__)

typedef struct {
    int32_t errCode;
    int pid;
    Object *myModObj;
} threadData;

class CTRegisterModuleTest : public testing::Test
{
   protected:
    // global setup, only called once (for this test suite).
    static void SetUpTestCase()
    {
        srand(time(NULL));
    }

    // global teardown, only called once (for this test suite).
    static void TearDownTestCase()
    {
    }

    // local setup, called each time for each instance of test fixture.
    void SetUp() override
    {
        memset(mPid, 0, sizeof(mPid));

        for (int i = 0; i < MAX_TEST_THREADS; i++) {
            mMyModObj[i] = (Object){MyTestIModule_invoke, &(mMyMod[i])};
            mPid[i] = GeneratePid();
            mSetterData[i].pid = mPid[i];
            mSetterData[i].myModObj = &mMyModObj[i];
            mGetterData[i].pid = mPid[i];
        }

        printf("setup completed.\n");
    }

    // local teardown, called each time for each instance of test fixture.
    void TearDown() override
    {

    }

    bool InvalidPid(int tmpPid)
    {
        for (int i = 0; i < MAX_TEST_THREADS; i++) {
            if (mPid[i] == tmpPid) {
                return true;
            }
        }

        return false;
    }

    int GeneratePid()
    {
        int tmpPid = rand() % 100 + 1;
        while (InvalidPid(tmpPid)) {
            tmpPid = rand() % 100 + 1;
        }

        return tmpPid;
    }

    MyTestIModule mMyMod[MAX_TEST_THREADS] = {{}};
    Object mMyModObj[MAX_TEST_THREADS] = {Object_NULL};
    int mPid[MAX_TEST_THREADS];
    pthread_t mSetterThread[MAX_TEST_THREADS], mGetterThread[MAX_TEST_THREADS];
    threadData mSetterData[MAX_TEST_THREADS], mGetterData[MAX_TEST_THREADS];
};

/*
 *  --------------------- START_ROUTINE DEFINITION ------------------------
 */

static void _getRegisteredIModule(int32_t errCode, int pid)
{
    Object iMod = Object_NULL;
    ASSERT_EQ(errCode, CTRegisterModule_getIModuleFromPendingList(pid, &iMod));
}

void *getRegisteredIModule(void *args)
{
    threadData *data = (threadData *)args;
    _getRegisteredIModule(data->errCode, data->pid);
    return NULL;
}

void *registerIModule(void *args)
{
    threadData *data = (threadData *)args;
    __registerIModule(data->pid, data->myModObj);
    return NULL;
}

/*
 * --------------- TEST CASES DEFINITION ---------------
 */
TEST_F(CTRegisterModuleTest, PositiveRegisterIModuleFirst)
{
    pthread_create(&mSetterThread[0], NULL, registerIModule, &mSetterData[0]);
    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT / 2.0));

    mGetterData[0].errCode = Object_OK;
    pthread_create(&mGetterThread[0], NULL, getRegisteredIModule, &mGetterData[0]);

    pthread_join(mSetterThread[0], NULL);
    pthread_join(mGetterThread[0], NULL);
}

TEST_F(CTRegisterModuleTest, PositiveGetIModuleFirst)
{
    mGetterData[0].errCode = Object_OK;
    pthread_create(&mGetterThread[0], NULL, getRegisteredIModule, &mGetterData[0]);

    pthread_create(&mSetterThread[0], NULL, registerIModule, &mSetterData[0]);

    pthread_join(mSetterThread[0], NULL);
    pthread_join(mGetterThread[0], NULL);
}

/**
  This test works for both registering too early or too late. Importantly, there
  are no registered IModules in the queue at the end of the test.
 */
TEST_F(CTRegisterModuleTest, NegativeGetUnregisteredIModule)
{
    mGetterData[0].errCode = ITRegisterModule_ERROR_TIMEDOUT;
    pthread_create(&mGetterThread[0], NULL, getRegisteredIModule, &mGetterData[0]);

    pthread_join(mGetterThread[0], NULL);
}

/**
  Due to limitation of platform, it is not possible to have multiple IModules
  waiting to be registered.
 */
TEST_F(CTRegisterModuleTest, PositiveInstantlyAfterFailedGetIModule)
{
    // Fail first
    mGetterData[0].errCode = ITRegisterModule_ERROR_TIMEDOUT;
    pthread_create(&mGetterThread[0], NULL, getRegisteredIModule, &mGetterData[0]);
    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT / 8.0));
    pthread_join(mGetterThread[0], NULL);

    // then pass
    pthread_create(&mSetterThread[1], NULL, registerIModule, &mSetterData[1]);

    mGetterData[1].errCode = Object_OK;
    pthread_create(&mGetterThread[1], NULL, getRegisteredIModule, &mGetterData[1]);

    pthread_join(mSetterThread[1], NULL);
    pthread_join(mGetterThread[1], NULL);
}

/**
  Due to limitation of platform, it is not possible to have multiple IModules
  waiting to be registered. Test is disabled until that is possible.
 */
TEST_F(CTRegisterModuleTest, DISABLED_PositiveOneNestAnotherGetSetPair)
{
    mGetterData[0].errCode = Object_OK;
    pthread_create(&mGetterThread[0], NULL, getRegisteredIModule, &mGetterData[0]);
    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.075));

    mGetterData[1].errCode = Object_OK;
    pthread_create(&mGetterThread[1], NULL, getRegisteredIModule, &mGetterData[1]);
    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.075));

    pthread_create(&mSetterThread[1], NULL, registerIModule, &mSetterData[1]);
    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.475));
    pthread_create(&mSetterThread[0], NULL, registerIModule, &mSetterData[0]);

    pthread_join(mSetterThread[1], NULL);
    pthread_join(mSetterThread[0], NULL);
    pthread_join(mGetterThread[1], NULL);
    pthread_join(mGetterThread[0], NULL);
}

/**
  Due to limitation of platform, it is not possible to have multiple IModules
  waiting to be registered. Test is disabled until that is possible.
 */
TEST_F(CTRegisterModuleTest, DISABLED_PositiveInterleaveOneWithAnotherGetSetPair)
{
    mGetterData[0].errCode = Object_OK;
    pthread_create(&mGetterThread[0], NULL, getRegisteredIModule, &mGetterData[0]);
    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.075));

    mGetterData[1].errCode = Object_OK;
    pthread_create(&mGetterThread[1], NULL, getRegisteredIModule, &mGetterData[1]);
    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.075));

    pthread_create(&mSetterThread[0], NULL, registerIModule, &mSetterData[0]);
    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.225));
    pthread_create(&mSetterThread[1], NULL, registerIModule, &mSetterData[1]);

    pthread_join(mSetterThread[0], NULL);
    pthread_join(mSetterThread[1], NULL);
    pthread_join(mGetterThread[0], NULL);
    pthread_join(mGetterThread[1], NULL);
}

/**
  Due to limitation of platform, it is not possible to have multiple IModules
  waiting to be registered. Test is disabled until that is possible.
 */
TEST_F(CTRegisterModuleTest, DISABLED_PositiveStressNormalGetSetPair)
{
    for (int i = 0; i < MAX_TEST_THREADS; i++) {
        mGetterData[i].errCode = Object_OK;
        pthread_create(&mGetterThread[i], NULL, getRegisteredIModule, &mGetterData[i]);
        system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.75));
        pthread_create(&mSetterThread[i], NULL, registerIModule, &mSetterData[i]);
    }

    for (int i = 0; i < MAX_TEST_THREADS; i++) {
        pthread_join(mSetterThread[i], NULL);
        pthread_join(mGetterThread[i], NULL);
    }
}

/**
  Due to limitation of platform, it is not possible to have multiple IModules
  waiting to be registered. Test is disabled until that is possible.
 */
TEST_F(CTRegisterModuleTest, DISABLED_PositiveStressRecursiveNestsWithGetSetPair)
{
    for (int i = 0; i < MAX_TEST_THREADS; i++) {
        mGetterData[i].errCode = Object_OK;
        pthread_create(&mGetterThread[i], NULL, getRegisteredIModule, &mGetterData[i]);
        system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.15));
    }

    for (int i = MAX_TEST_THREADS - 1; i >= 0; i--) {
        pthread_create(&mSetterThread[i], NULL, registerIModule, &mSetterData[i]);
    }

    for (int i = MAX_TEST_THREADS - 1; i >= 0; i--) {
        pthread_join(mSetterThread[i], NULL);
    }

    for (int i = 0; i < MAX_TEST_THREADS; i++) {
        pthread_join(mGetterThread[i], NULL);
    }
}

/**
  Due to limitation of platform, it is not possible to have multiple IModules
  waiting to be registered. Test is disabled until that is possible.
 */
// critical timing condition test, might fail with repetitions
TEST_F(CTRegisterModuleTest, DISABLED_PositiveStressRecursiveNestsWithGetSetPairTimeout)
{
    mGetterData[0].errCode = ITRegisterModule_ERROR_TIMEDOUT;
    pthread_create(&mGetterThread[0], NULL, getRegisteredIModule, &mGetterData[0]);

    for (int i = 1; i < MAX_TEST_THREADS; i++) {
        mGetterData[i].errCode = Object_OK;
        pthread_create(&mGetterThread[i], NULL, getRegisteredIModule, &mGetterData[i]);
        system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.24));
    }

    for (int i = MAX_TEST_THREADS - 1; i > 0; i--) {
        pthread_create(&mSetterThread[i], NULL, registerIModule, &mSetterData[i]);
    }

    for (int i = MAX_TEST_THREADS - 1; i > 0; i--) {
        pthread_join(mSetterThread[i], NULL);
    }

    for (int i = 1; i < MAX_TEST_THREADS; i++) {
        pthread_join(mGetterThread[i], NULL);
    }

    system(cmdStr("sleep %lf", REGISTER_IMODULE_MAX_TIMEOUT * 0.25));
    pthread_create(&mSetterThread[0], NULL, registerIModule, &mSetterData[0]);
    pthread_join(mSetterThread[0], NULL);
    pthread_join(mGetterThread[0], NULL);
}
