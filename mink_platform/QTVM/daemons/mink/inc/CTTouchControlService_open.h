// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CTTOUCHCONTROLSERVICE_OPEN_H
#define __CTTOUCHCONTROLSERVICE_OPEN_H

#include <stdint.h>
#include "object.h"

/**
 * Return an ITTouchControlService object
 */
int32_t CTTouchControlService_open(uint32_t uid, Object credentials, Object *objOut);

#endif /* __CTTOUCHCONTROLSERVICE_OPEN_H */
