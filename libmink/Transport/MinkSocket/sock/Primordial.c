// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "VmOsal.h"
#include "cdefs.h"
#include "check.h"
#include "Heap.h"
#include "IPrimordial_invoke.h"
#include "msforwarder.h"
#include "Primordial.h"
#include "Utils.h"

struct Primordial {
  int32_t refs;
  NotifierRegFunc notifierRegFunc;
};

static inline
int32_t Primordial_retain(Primordial *me)
{
  vm_osal_atomic_add(&me->refs, 1);

  return Object_OK;
}

static inline
int32_t Primordial_release(Primordial *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("Released primordial = %p, notifierRegFunc = %p\n", me, me->notifierRegFunc);
    me->notifierRegFunc = NULL;
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static
int32_t Primordial_registerSubNotifier(Primordial *me, Object target,
                                   Object handler, Object *subNotifier)
{
  if (NULL == me->notifierRegFunc) {
    LOG_ERR("There is no closeNotifier register function\n");
    return Object_ERROR_UNAVAIL;
  }

  LOG_TRACE("Register notifier from primordial = %p, notifierRegFunc = %p\n", me,
             me->notifierRegFunc);

  return (*me->notifierRegFunc)(target, handler, subNotifier);
}

static
IPrimordial_DEFINE_INVOKE(Primordial_invoke, Primordial_, Primordial*);

int32_t Primordial_setCloseNotifierReg(Object *pmdObj, NotifierRegFunc func)
{
  Primordial *me = (Primordial *)pmdObj->context;
  if (!me) {
    LOG_ERR("Invalid Primordial Object\n");
    return Object_ERROR_BADOBJ;
  }

  me->notifierRegFunc = func;

  LOG_TRACE("Set handler to primordial = %p, notifierRegFunc = %p\n", me,
             me->notifierRegFunc);

  return Object_OK;
}

bool isPrimordialOrPrimordialFwd(Object obj)
{
  MSForwarder *msf = MSForwarderFromObject(obj);
  if (msf && (PRIMORDIAL_HANDLE == msf->handle)) {
    return true;
  }

  if (Primordial_invoke == obj.invoke) {
    return true;
  }

  return false;
}

int32_t Primordial_new(Object *objOut)
{
  Primordial *me = HEAP_ZALLOC_REC(Primordial);
  if (!me) {
    LOG_ERR("Memory allocation for Primordial failed\n");
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  *objOut = (Object) {Primordial_invoke, me};

  return Object_OK;
}
