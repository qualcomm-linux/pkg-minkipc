// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef CTEST_UTILS_H_
#define CTEST_UTILS_H_

#include "object.h"

#define CTESTUTILS_ID 2

typedef struct {
    int32_t refs;
} TestUtils;

#if defined (__cplusplus)
extern "C" {
#endif /* __cplusplus */

TestUtils *CTestUtils_fromObject(Object obj);
int32_t CTestUtils_new(Object *objOut);

#if defined (__cplusplus)
}
#endif /* __cplusplus */

#endif /* CTEST_UTILS_H_ */
