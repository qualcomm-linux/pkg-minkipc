// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __LOCAL_CONNECTION_MANAGER_H
#define __LOCAL_CONNECTION_MANAGER_H

#include <stdint.h>
#include "object.h"

int32_t LocalConnectionManager_createConnection(void);

void LocalConnectionManager_freeAllConnection(void);

#endif  // __LOCAL_CONNECTION_MANAGER_H
