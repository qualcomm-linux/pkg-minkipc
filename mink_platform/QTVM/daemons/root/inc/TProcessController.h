// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TPROCCONTROLLER_H
#define __TPROCCONTROLLER_H

#include <stdint.h>
#include <sys/types.h>
#include "object.h"

typedef struct TProcController TProcController;

int32_t TProcController_getPID(TProcController *me, uint32_t *pid_ptr);

int32_t TProcController_new(Object tprocObj, bool neverUnload, Object *objOut);

#endif  // __TPROCCONTROLLER_H
