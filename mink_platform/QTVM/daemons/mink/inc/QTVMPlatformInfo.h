// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __QTVMPLATFORMINFO_H
#define __QTVMPLATFORMINFO_H

#include <stdint.h>
#include "object.h"

int32_t CQTVMPlatformInfo_setEnvCred(Object envCred);

int32_t CQTVMPlatformInfo_open(uint32_t uid, Object credentials, Object *objOut);

#endif /* __QTVMPLATFORMINFO_H */
