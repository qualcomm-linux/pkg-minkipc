// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CAUTOSTARTMANAGER_H
#define __CAUTOSTARTMANAGER_H

#include <stdint.h>
#include "object.h"

int32_t CTAutoStartManager_open(Object *objOut);

void CTAutoStartManager_startService(void);

#endif  //  __CAUTOSTARTMANAGER_H
