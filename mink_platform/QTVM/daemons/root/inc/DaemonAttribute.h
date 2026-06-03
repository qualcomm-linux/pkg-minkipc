// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __DAEMONATTRIBUTE_H
#define __DAEMONATTRIBUTE_H

#include <stdint.h>

#define CORE_DAEMON_OOM_SCORE_ADJ -900
#define SERVICE_DAEMON_OOM_SCORE_ADJ 50

void DaemonAttribute_setOOMAttribute(int32_t pid, int32_t oomScoreAbj);

#endif  // __DAEMONATTRIBUTE_H
