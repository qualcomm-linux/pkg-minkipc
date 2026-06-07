// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _TMEMLOCK_H_
#define _TMEMLOCK_H_

#include <stdint.h>
#include "object.h"

#if defined(__cplusplus)
extern "C" {
#endif

int32_t TMemLock_new(Object memObj, uint64_t uid, Object *objOut);

#if defined(__cplusplus)
}
#endif

#endif
