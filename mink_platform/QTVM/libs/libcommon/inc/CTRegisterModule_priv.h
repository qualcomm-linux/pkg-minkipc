// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CTREGISTERMODULE_PRIV_H
#define __CTREGISTERMODULE_PRIV_H

#include <stdint.h>
#include "object.h"

/**
 * Get the target IModule instance from pending list
 *
 * param[in]    pid       the pid of target service
 *
 * param[out]   imodule   the target service instance
 *
 * return Object_isOK if successful
 */
int32_t CTRegisterModule_getIModuleFromPendingList(uint32_t pid, Object *imodule);

#endif  // __CTREGISTERMODULE_PRIV_H
