// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __PROCESSMANAGER_PRIV_H
#define __PROCESSMANAGER_PRIV_H

#include <stddef.h>
#include <stdint.h>
#include "TProcess.h"

/**
 * Remove TProcess from the set of loaded processes
 */
void PPM_remove(TProcess* proc);

#endif  // __PROCESSMANAGER_PRIV_H
