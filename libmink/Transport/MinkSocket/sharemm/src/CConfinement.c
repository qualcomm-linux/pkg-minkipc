// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "cdefs.h"
#include "Confinement.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "IConfinement.h"
#include "IConfinement_invoke.h"
#include "ITAccessPermissions.h"
#include "memscpy.h"
#include "object.h"
#include "Utils.h"

/////////////////////////////////////////////
//        Confinement definition         ////
/////////////////////////////////////////////

static int32_t CConfinement_retain(Confinement *me)
{
  vm_osal_atomic_add(&me->refs, 1);

  return Object_OK;
}

static int32_t CConfinement_release(Confinement *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("released confinement = %p\n", me);
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static int32_t CConfinement_getSpecialRules(Confinement *me,
                                            uint64_t *specialRules)
{
  const ITAccessPermissions_rules *confRules = &(me->confRules);

  *specialRules = confRules->specialRules;

  LOG_TRACE("get specialRules = %u%09u from confinement = %p \n",
             UINT64_HIGH(*specialRules), UINT64_LOW(*specialRules), me);

  return Object_OK;
}

static int32_t CConfinement_getConfinementRules(
  Confinement *me, ITAccessPermissions_rules *outConfRules)
{
  memscpy(outConfRules, sizeof(ITAccessPermissions_rules), &(me->confRules),
          sizeof(ITAccessPermissions_rules));

  LOG_TRACE("get onfRules = %p from confinement = %p\n", outConfRules, me);
  return Object_OK;
}

static IConfinement_DEFINE_INVOKE(CConfinement_invoke, CConfinement_,
                                  Confinement *);

int32_t CConfinement_new(const ITAccessPermissions_rules *userRules,
                         Object *objOut)
{
  int32_t ret = Object_OK;

  if (!userRules) {
    return Object_ERROR;
  }

  Confinement *me = HEAP_ZALLOC_TYPE(Confinement);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  memscpy(&(me->confRules), sizeof(ITAccessPermissions_rules), userRules,
          sizeof(ITAccessPermissions_rules));

  *objOut = (Object){CConfinement_invoke, me};

  return ret;
}

Confinement *ConfinementFromObject(Object obj)
{
  return (obj.invoke == CConfinement_invoke ? (Confinement *)obj.context
                                            : NULL);
}
