// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _SHARE_MEMORY_H_
#define _SHARE_MEMORY_H_

#include "VmOsal.h"
#include "ITAccessPermissions.h"
#include "object.h"

#if defined(__cplusplus)
extern "C" {
#endif

int32_t ShareMemory_GetMemParcelHandle(int32_t dmaBufFd, Object conf,
                                       char *destVMName, int64_t *outMPHandle);

int32_t ShareMemory_GetMSMem(int64_t memparcelHandle, char *destVMName,
                             ITAccessPermissions_rules *confRules, vm_osal_mutex *lockPtr,
                             Object *objOut);

int32_t ShareMemory_ReclaimMemBuf(int32_t fd, int64_t memparcelHandle);

#if defined(__cplusplus)
}
#endif

#endif
