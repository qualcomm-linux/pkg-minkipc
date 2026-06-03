// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef _COMMON_COMPLEX_TEST_H_
#define _COMMON_COMPLEX_TEST_H_

#include "object.h"
#include "minkipc.h"

#if defined (__cplusplus)
extern "C" {
#endif /* __cplusplus */

int32_t complexSituationSingleServiceMultiClient(Object moduleRegister, Object clientOpenerA,
                                                 Object clientOpenerB, Object clientOpenerC);
int32_t complexSituationMultiService(Object serverOpenerA, Object serverOpenerB,
                                     Object serverOpenerC);
int32_t complexSituationMultiServiceMultiClient(Object serverOpenerA, Object serverOpenerB,
                                                Object clientOpenerA, Object clientOpenerB);

#if defined (__cplusplus)
}
#endif /* __cplusplus */

#endif /* _COMMON_COMPLEX_TEST_H_ */
