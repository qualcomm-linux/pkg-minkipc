// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _REMOTE_SHARE_MEMORY_H_
#define _REMOTE_SHARE_MEMORY_H_

#include "VmOsal.h"
#include "ITAccessPermissions.h"
#include "object.h"

#if defined(__cplusplus)
extern "C" {
#endif

/////////////////////////////////////////////
//      RemoteShareMemory definition     ////
/////////////////////////////////////////////

int32_t RemoteShareMemory_attachConfinement(
  const ITAccessPermissions_rules *userRules,
  Object *memObj);

#if defined(__cplusplus)
}
#endif

#endif
