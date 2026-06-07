// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TPRIVILEGEDPROCESSMANAGER_H
#define __TPRIVILEGEDPROCESSMANAGER_H

#include <stdbool.h>
#include "libcontainer.h"

int32_t TPPM_processDied(uint32_t pPid);

int32_t TPPM_launch(int32_t fd, cid_t cid, const ITPPM_programData *programData, pid_t *pidOut,
                    pid_t *pPidOut);

#endif  // __TPRIVILEGEDPROCESSMANAGER_H
