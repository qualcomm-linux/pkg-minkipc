// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _WRAPPEDMEMPARCEL_H_
#define _WRAPPEDMEMPARCEL_H_

#include "VmOsal.h"
#include "minksocket.h"
#include "object.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define WrappedMemparcel_OP_getConfinement 0
#define WrappedMemparcel_OP_getMemparcelHandle 1
#define WrappedMemparcel_OP_getWrappedFdObj 2

typedef struct WrappedMemparcel WrappedMemparcel;

int32_t WrappedMemparcel_new(Object wrappedFdObj, MinkSocket* mSock,
                             uint32_t invId, Object *objOut);

static inline int32_t
IWrappedMemparcel_release(Object self)
{
  return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IWrappedMemparcel_retain(Object self)
{
  return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
IWrappedMemparcel_getConfinement(Object self, ITAccessPermissions_rules *wmpConfRules)
{
  ObjectArg wmpArgs[1];
  wmpArgs[0].b = (ObjectBuf) { wmpConfRules, sizeof(ITAccessPermissions_rules) };

  return Object_invoke(self, WrappedMemparcel_OP_getConfinement, wmpArgs, ObjectCounts_pack(0, 1, 0, 0));
}

static inline int32_t
IWrappedMemparcel_getMemparcelHandle(Object self, int64_t *wmpHdl)
{
  ObjectArg wmpArgs[1];
  wmpArgs[0].b = (ObjectBuf) { wmpHdl, sizeof(int64_t) };

  return Object_invoke(self, WrappedMemparcel_OP_getMemparcelHandle, wmpArgs, ObjectCounts_pack(0, 1, 0, 0));
}

static inline int32_t
IWrappedMemparcel_getWrappedFdObj(Object self, Object *wrappedFdObj)
{
  ObjectArg wmpArgs[1];
  int32_t result = Object_OK;

  result = Object_invoke(self, WrappedMemparcel_OP_getWrappedFdObj, wmpArgs, ObjectCounts_pack(0, 0, 0, 1));
  *wrappedFdObj = wmpArgs[0].o;

  return result;
}

bool isWrappedMemparcel(Object obj);

#if defined(__cplusplus)
}
#endif

#endif
