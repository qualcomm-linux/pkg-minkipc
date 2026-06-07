// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef _COMMON_MULTI_THREAD_TEST_H_
#define _COMMON_MULTI_THREAD_TEST_H_

#include "object.h"

#if defined (__cplusplus)
extern "C" {
#endif /* __cplusplus */

int32_t stressMarshallingAccessBIBO(Object obj, int32_t threadCount);
int32_t stressMarshallingAccessOIOO(Object obj, int32_t threadCount);
int32_t stressMarshallingAccessRemoteOIOO(Object obj, int32_t threadCount);
int32_t stressMarshallingAccessLocalFdOIOO(Object obj, int32_t threadCount);
int32_t stressServiceRegister(Object registerServiceObj, int32_t threadCount);
int32_t stressServiceOpen(Object registerServiceObj, Object serviceOpener, int32_t threadCount);

#if defined (__cplusplus)
}
#endif /* __cplusplus */

#endif /* _COMMON_MULTI_THREAD_TEST_H_ */
