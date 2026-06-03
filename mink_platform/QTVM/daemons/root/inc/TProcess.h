// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TPROCESS_H
#define __TPROCESS_H

#include <stdint.h>
#include <sys/types.h>
#include "MinkTypes.h"
#include "object.h"

typedef struct TProcess TProcess;

int32_t TProcess_new(pid_t pid, pid_t pPid, const DistId *did, Object *objOut);

#endif  // __TPROCESS_H
