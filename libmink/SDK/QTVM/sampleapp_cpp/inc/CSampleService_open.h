// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

/** @file  CSampleService_open.h */

#ifndef __CSAMPLESERVICE_OPEN_H
#define __CSAMPLESERVICE_OPEN_H

#include <stdint.h>
#include "object.h"

int32_t CSampleService_open(uint32_t uid, Object credentials, Object *objOut);

#endif  // __CSAMPLESERVICE_OPEN_H
