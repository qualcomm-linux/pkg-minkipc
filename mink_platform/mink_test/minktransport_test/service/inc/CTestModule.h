// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef _CTEST_MODULE_H_
#define _CTEST_MODULE_H_

#include "object.h"

#define INVOKE_MAGIC_ID 55555
#define CTESTMODULE_ID 1

typedef struct {
    int32_t refs;
    int32_t id;
    Object obj;
} TestModule;

#if defined (__cplusplus)
extern "C" {
#endif /* __cplusplus */

TestModule *CTestModule_fromObject(Object obj);
int32_t CTestModule_new(Object *objOut);

#if defined (__cplusplus)
}
#endif /* __cplusplus */

#endif /* _CTEST_MODULE_H_ */
