// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _Primordial_H
#define _Primordial_H

#include "object.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef int32_t (*NotifierRegFunc)(Object target, Object handler, Object *subNotifier);

typedef struct Primordial Primordial;

int32_t Primordial_setCloseNotifierReg(Object *pmdObj, NotifierRegFunc func);
bool isPrimordialOrPrimordialFwd(Object obj);
int32_t Primordial_new(Object *objOut);

#if defined (__cplusplus)
}
#endif

#endif //_Primordial_H
