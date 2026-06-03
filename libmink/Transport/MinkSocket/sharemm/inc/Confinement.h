// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _CONFINEMENT_H_
#define _CONFINEMENT_H_

#include "VmOsal.h"
#include "ITAccessPermissions.h"
#include "object.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
  int32_t refs;
  ITAccessPermissions_rules confRules;
} Confinement;

int32_t CConfinement_new(const ITAccessPermissions_rules *userRules,
                         Object *objOut);

Confinement *ConfinementFromObject(Object obj);

#if defined(__cplusplus)
}
#endif

#endif
