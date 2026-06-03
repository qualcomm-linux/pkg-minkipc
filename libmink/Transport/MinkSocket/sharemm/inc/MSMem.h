// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _MSMEM_H_
#define _MSMEM_H_

#include "VmOsal.h"
#include "cdefs.h"
#include "ITAccessPermissions.h"
#include "object.h"
#include "qlist.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
  QNode node;
  int32_t refs;
  int32_t dmaBufFd;
  int64_t memParcelHandle;
  Object dependency;
  bool isLocal;
  // A callback object to the MemoryService which holds a ref to the pool
  Object bufCallBack;
  ITAccessPermissions_rules confRules;
  vm_osal_mutex *gMarshalMemLock;
} MSMem;

int32_t MSMem_new(int32_t fd, Object bufCallBack, Object *objOut);
int32_t MSMem_new_remote(int32_t fd, ITAccessPermissions_rules *confRules,
                         int64_t mpH, vm_osal_mutex *lockPtr, Object *objOut);

bool isMSMem(Object obj, int32_t *fd);
MSMem *MSMemFromObject(Object obj);

#ifdef STUB
int32_t MSChangeToROFd(Object obj, int32_t fd);
#endif

#if defined(__cplusplus)
}
#endif

#endif
