// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TREBOOTVM_H
#define __TREBOOTVM_H

#include <stdint.h>
#include "object.h"

void CTRebootVM_enable(Object prelauncherObj);

void CTRebootVM_disable(void);

#endif // __TREBOOTVM_H
