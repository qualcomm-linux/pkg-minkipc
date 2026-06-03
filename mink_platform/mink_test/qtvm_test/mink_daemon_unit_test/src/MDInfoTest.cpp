// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <gtest/gtest.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

extern "C" {
#include "ICredentials.h"
#include "MetadataInfo.h"
#include "heap.h"
}

/*
 *  ----------------------- GLOBAL BACKGROUND --------------------------
 */

#define NUM_PRIVS 20
#define NUM_SERVS 4
#define DIGITS (3 + 1)  // digits plus delimiter
#define UPPER_BOUND 1000
#define P_STR_LEN (NUM_PRIVS * DIGITS)
#define S_STR_LEN (NUM_SERVS * DIGITS)
#define MD_STR_LEN (20 + P_STR_LEN + 2 + S_STR_LEN + 10)

#define NAME_CHAR 0
#define PRIV_CHAR 1
#define SERV_CHAR 2
#define UNLD_CHAR 3
#define XTRA_CHAR 4
constexpr char gMetaChar[] = "npsUx";

typedef int32_t (*__candidateFunc)(MetadataInfo *, uint32_t);

/*
 *  ----------------------- HELPER FUNCTION --------------------------
 */

static bool isIn(int32_t num, int32_t *arr, int32_t cnt)
{
    for (int32_t i = 0; i < cnt; i++) {
        if (num == arr[i]) {
            return true;
        }
    }

    return false;
}

static inline void populateStringRandom(int32_t numElements, int32_t *arr, char **pString,
                                        int32_t strLen)
{
    /* Print numElements random numbers from 0 to UPPER_BOUND-1 */
    for (int32_t i = 0; i < numElements; i++) {
        arr[i] = rand() % UPPER_BOUND;
        while (isIn(arr[i], arr, i)) {
            arr[i] = rand() % UPPER_BOUND;
        }
    }

    /* Print to string */
    char delim[] = ",";
    int32_t pos = 0;

    /* Iniitialize string */
    *pString = (char *)malloc(strLen);
    ASSERT_NE(nullptr, *pString);

    for (int32_t i = 0; i < numElements; i++) {
        pos += snprintf((*pString) + pos, strLen - pos, "%x", arr[i]);
        if (i < numElements - 1) {
            pos += snprintf((*pString) + pos, strLen - pos, "%s", delim);// insert delimiter
        }
    }
}

static void testMethod(MetadataInfo *me, int32_t *arr, int32_t cnt, __candidateFunc func)
{
    for (int32_t i = 0; i < cnt; i++) {
        ASSERT_EQ(Object_OK, func(me, arr[i]));
    }
}

static int excludedNum(int32_t *arr1, int32_t cnt1, int32_t *arr2, int32_t cnt2)
{
    int32_t num;
    do {
        num = rand() % UPPER_BOUND;
    } while (isIn(num, arr1, cnt1) || isIn(num, arr2, cnt2));

    return num;
}

/*
 * ------------------- TEST SUITE DEFINITION ------------------------------
 */

#define isOk(__subroutine__) ASSERT_NO_FATAL_FAILURE(__subroutine__)

class MetadataInfoTests : public ::testing::Test
{
   protected:

    void SetUp() override
    {
        /* Establish expected Distinguished Name */
        int nameLen = strlen(mDomain) + 1 + strlen(mName) + 1;  // '.' and '\0'
        mExpectName = (char *)malloc(nameLen);
        snprintf(mExpectName, nameLen, "%s.%s", mDomain, mName);

        /* Allocate memory for other scratch buffers */
        mPName = (void *)malloc(2 * sizeof(char));
        mPValue = (void *)malloc(P_STR_LEN * sizeof(char));

        /* Print NUM_PRIVS random numbers from 0 to UPPER_BOUND-1 */
        populateStringRandom(NUM_PRIVS, mPrivileges, &mPrivStr, P_STR_LEN);

        /* Print NUM_SERVS random numbers from 0 to UPPER_BOUND-1 */
        populateStringRandom(NUM_SERVS, mServices, &mServStr, S_STR_LEN);

        mExpectNeverUnload = ((rand() % 10) & 1) ? 1 : 0;

        snprintf(mMdStr, MD_STR_LEN, "%c=%s;%c=%s;%c=%s;%c=%s;%c=extra",
                 gMetaChar[NAME_CHAR], mName, gMetaChar[PRIV_CHAR], mPrivStr,
                 gMetaChar[SERV_CHAR], mServStr, gMetaChar[UNLD_CHAR],
                 mNeverUnload[mExpectNeverUnload], gMetaChar[XTRA_CHAR]);

        ASSERT_EQ(Object_OK, MetadataInfo_new(&mInfo, mMdStr, strlen(mMdStr)));
    }

    void TearDown() override
    {
        MetadataInfo_destruct(mInfo);
        HEAP_FREE_PTR(mPrivStr);
        HEAP_FREE_PTR(mServStr);
        HEAP_FREE_PTR(mExpectName);
        HEAP_FREE_PTR(mPName);
        HEAP_FREE_PTR(mPValue);
    }

    // global setup
    static void SetUpTestCase()
    {
        srand(time(NULL));
    }

    // global teardown
    static void TearDownTestCase()
    {
    }

    MetadataInfo *mInfo = nullptr;
    char mMdStr[MD_STR_LEN];
    uint32_t mRot = 123;
    const char *mDomain = "foo";
    const char *mName = "appName_0";
    const char *mNeverUnload[2] = { "false", "true" };
    int32_t mExpectNeverUnload;
    char *mExpectName = nullptr;
    void *mPName = nullptr;
    void *mPValue = nullptr;
    size_t mPNameLenOut, mPValLenOut;
    int32_t mPrivileges[NUM_PRIVS];
    char *mPrivStr = nullptr;
    int32_t mServices[NUM_SERVS];
    char *mServStr = nullptr;
};

/*
 * --------------- TEST CASES DEFINITION ---------------
 */

TEST_F(MetadataInfoTests, PositiveCheckPrivilege)
{
    isOk(testMethod(mInfo, mPrivileges, NUM_PRIVS, MetadataInfo_testPrivilege));
}

TEST_F(MetadataInfoTests, PositiveCheckService)
{
    isOk(testMethod(mInfo, mServices, NUM_SERVS, MetadataInfo_testService));
}

TEST_F(MetadataInfoTests, PositiveSetGetDomain)
{
    ASSERT_EQ(Object_OK, MetadataInfo_setDomain(mInfo, mDomain));
    ASSERT_STREQ(mDomain, MetadataInfo_getDomain(mInfo));
}

TEST_F(MetadataInfoTests, PositiveSetGetRot)
{
    ASSERT_EQ(Object_OK, MetadataInfo_setRot(mInfo, mRot));
    uint32_t rot = MetadataInfo_getRot(mInfo);
    ASSERT_EQ(rot, mRot);
}

TEST_F(MetadataInfoTests, PositiveGetName)
{
    ASSERT_STREQ(mName, MetadataInfo_getName(mInfo));
}

TEST_F(MetadataInfoTests, PositiveGetDistinguishedName)
{
    ASSERT_EQ(Object_OK, MetadataInfo_setDomain(mInfo, mDomain));
    ASSERT_STREQ(mExpectName, MetadataInfo_getDistName(mInfo));
}

TEST_F(MetadataInfoTests, PositiveGetDistinguishedId)
{
    ASSERT_EQ(Object_OK, MetadataInfo_setDomain(mInfo, mDomain));
    DistId *did = (DistId *)heap_zalloc(sizeof(DistId));
    ASSERT_EQ(Object_OK, MetadataInfo_getDistId(mInfo, did));
    HEAP_FREE_PTR(did);
}

TEST_F(MetadataInfoTests, PositiveGetPropertyByIndex)
{
    for (int32_t i = 0; i < 5; i++) {
        ASSERT_EQ(Object_OK,
                  MetadataInfo_getPropertyByIndex(mInfo, i, mPName, 2, &mPNameLenOut, mPValue,
                                                  P_STR_LEN, &mPValLenOut));
    }
}

// Positive Parameterized Tests
class MDInfoParameterized
    : public MetadataInfoTests,
      public ::testing::WithParamInterface<std::tuple<const char, int>>
{
};

TEST_P(MDInfoParameterized, PositiveGetCredentialsByName)
{
    const char key = std::get<0>(GetParam());
    int valueSize = std::get<1>(GetParam());
    if (!valueSize) {
        valueSize = P_STR_LEN;
    }
    // Retrieve the value
    ASSERT_EQ(Object_OK,
              MetadataInfo_getValueByName(mInfo, &key, 2, mPValue, valueSize, &mPValLenOut));
    // Check the value against the original strings
    switch (key) {
        case gMetaChar[NAME_CHAR]:
            ASSERT_STREQ(mName, (char *)mPValue);
            break;
        case gMetaChar[PRIV_CHAR]:
            ASSERT_STREQ(mPrivStr, (char *)mPValue);
            break;
        case gMetaChar[SERV_CHAR]:
            ASSERT_STREQ(mServStr, (char *)mPValue);
            break;
        case gMetaChar[UNLD_CHAR]:
            if (mExpectNeverUnload)
                ASSERT_TRUE(*(bool *)mPValue);
            else
                ASSERT_FALSE(*(bool *)mPValue);
            break;
        case gMetaChar[XTRA_CHAR]:
            ASSERT_STREQ("extra", (char *)mPValue);
            break;
        default:
            FAIL();
    }
}

INSTANTIATE_TEST_SUITE_P(MetadataInfoTests, MDInfoParameterized,
                         ::testing::Values(std::make_tuple('n', 0),
                                           std::make_tuple('p', 0),
                                           std::make_tuple('s', 0),
                                           std::make_tuple('U', 0),
                                           std::make_tuple('x', 0)));

TEST_F(MetadataInfoTests, NegativeCreateInstanceNoMetadata)
{
    MetadataInfo *info = nullptr;
    ASSERT_EQ(Object_ERROR, MetadataInfo_new(&info, NULL, 0));
    MetadataInfo_destruct(info);
}

TEST_F(MetadataInfoTests, NegativeCheckNonPrivilege)
{
    int32_t maxTrys = 20;
    for (int32_t i = 0; i < maxTrys; i++)
        ASSERT_EQ(Object_ERROR,
                  MetadataInfo_testPrivilege(mInfo, excludedNum(mPrivileges, NUM_PRIVS, mServices,
                                                                NUM_SERVS)));
}

TEST_F(MetadataInfoTests, NegativeCheckNonService)
{
    int32_t maxTrys = 20;
    for (int32_t i = 0; i < maxTrys; i++) {
        ASSERT_EQ(Object_ERROR,
                  MetadataInfo_testService(mInfo, excludedNum(mServices, NUM_SERVS, NULL, 0)));
    }

}

TEST_F(MetadataInfoTests, NegativeCannotSetDomainAgain)
{
    ASSERT_EQ(Object_OK, MetadataInfo_setDomain(mInfo, mDomain));
    ASSERT_STREQ(mDomain, MetadataInfo_getDomain(mInfo));
    ASSERT_EQ(Object_ERROR, MetadataInfo_setDomain(mInfo, mDomain));
}

TEST_F(MetadataInfoTests, NegativeCannotGetDomainBeforeSet)
{
    ASSERT_EQ(nullptr, MetadataInfo_getDomain(mInfo));
    ASSERT_EQ(Object_OK, MetadataInfo_setDomain(mInfo, mDomain));
    ASSERT_NE(nullptr, MetadataInfo_getDomain(mInfo));
}

TEST_F(MetadataInfoTests, NegativeDoesNotPopulateWithoutDomain)
{
    ASSERT_EQ(nullptr, MetadataInfo_getDistName(mInfo));
}

TEST_F(MetadataInfoTests, NegativeGetPropertyByIndexNegative)
{
    ASSERT_EQ(ICredentials_ERROR_NOT_FOUND,
              MetadataInfo_getPropertyByIndex(mInfo, -2, mPName, 2, &mPNameLenOut, mPValue,
                                              P_STR_LEN, &mPValLenOut));
}

TEST_F(MetadataInfoTests, NegativeGetPropertyByIndexOutOfBound)
{
    ASSERT_EQ(ICredentials_ERROR_NOT_FOUND,
              MetadataInfo_getPropertyByIndex(mInfo, 6, mPName, 2, &mPNameLenOut, mPValue,
                                              P_STR_LEN, &mPValLenOut));
}

TEST_F(MetadataInfoTests, NegativeGetPropertyByIndexNameBufTooSmall)
{
    ASSERT_EQ(ICredentials_ERROR_NAME_SIZE,
              MetadataInfo_getPropertyByIndex(mInfo, 1, mPName, 0, &mPNameLenOut, mPValue,
                                              P_STR_LEN, &mPValLenOut));
}

TEST_F(MetadataInfoTests, NegativeGetPropertyByIndexValueBufTooSmall)
{
    ASSERT_EQ(ICredentials_ERROR_VALUE_SIZE,
              MetadataInfo_getPropertyByIndex(mInfo, 1, mPName, 2, &mPNameLenOut, mPValue, 2,
                                              &mPValLenOut));
}

TEST_F(MetadataInfoTests, NegativeGetCredentialsByNameInvalidName)
{
    snprintf((char *)mPName, 2, "%s", "e");
    ASSERT_EQ(ICredentials_ERROR_NOT_FOUND,
              MetadataInfo_getValueByName(mInfo, mPName, 2, mPValue, P_STR_LEN, &mPValLenOut));
}

TEST_F(MetadataInfoTests, NegativeGetCredentialsByNameInvalidBufSize)
{
    snprintf((char *)mPName, 2, "%s", "n");
    ASSERT_EQ(ICredentials_ERROR_VALUE_SIZE,
              MetadataInfo_getValueByName(mInfo, mPName, 2, mPValue, 2, &mPValLenOut));
    snprintf((char *)mPName, 2, "%s", "x");
    ASSERT_EQ(ICredentials_ERROR_VALUE_SIZE,
              MetadataInfo_getValueByName(mInfo, mPName, 2, mPValue, 2, &mPValLenOut));
}
