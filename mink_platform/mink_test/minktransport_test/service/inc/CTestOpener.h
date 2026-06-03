// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef _CTEST_OPENER_H_
#define _CTEST_OPENER_H_

#include "object.h"

#define TEST_MODULE_UID (0x00000010u)
#define TEST_UTILS_UID  (0x00000020u)

#if defined (__cplusplus)
extern "C" {
#endif /* __cplusplus */

int32_t CTestOpener_new(Object *objOut);

#if defined (__cplusplus)
}
#endif /* __cplusplus */

#endif /* _CTEST_OPENER_H_ */
