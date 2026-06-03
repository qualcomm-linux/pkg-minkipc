// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CTREBOOTVM_OPEN_H
#define __CTREBOOTVM_OPEN_H

#include <stdint.h>
#include "object.h"

/**
 * Return an ITRebootVM object
 */

int32_t CTRebootVM_open(uint32_t uid, Object credentials, Object *objOut);

#endif /* __CTREBOOTVM_OPEN_H */
