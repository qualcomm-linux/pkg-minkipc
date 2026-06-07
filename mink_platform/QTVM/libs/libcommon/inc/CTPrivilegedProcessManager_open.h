// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CTPRIVILEGEDPROCESSMANAGER_OPEN_H
#define __CTPRIVILEGEDPROCESSMANAGER_OPEN_H

#include <stdint.h>
#include "object.h"

int32_t CTPPM_open(uint32_t uid, Object credentials, Object *objOut);

#endif /* __CTPRIVILEGEDPROCESSMANAGER_OPEN_H */
