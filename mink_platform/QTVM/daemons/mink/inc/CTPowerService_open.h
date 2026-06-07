// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CTPOWERSERVICE_OPEN_H
#define __CTPOWERSERVICE_OPEN_H

#include <stdint.h>
#include "object.h"

/**
 * Return an IPowerService object
 */
int32_t CTPowerServiceFactory_open(uint32_t uid, Object credentials, Object *objOut);

#endif /* __CTPOWERSERVICE_OPEN_H */
