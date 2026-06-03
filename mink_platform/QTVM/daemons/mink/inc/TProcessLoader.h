// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TPROCESSLOADER_H
#define __TPROCESSLOADER_H

#include <stdint.h>
#include "MinkHub.h"
#include "object.h"

void CTProcessLoader_setMinkHub(MinkHub *hub);

void CTProcessLoader_enable(Object prelauncherObj);

void CTProcessLoader_disable(void);

#endif  // _TPROCESSLOADER_H_
