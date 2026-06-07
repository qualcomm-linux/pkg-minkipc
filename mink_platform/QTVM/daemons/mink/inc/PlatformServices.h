// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __PLATFORMSERVICES_H
#define __PLATFORMSERVICES_H

#include <stdbool.h>
#include <stdint.h>
#include "MinkHub.h"
#include "object.h"

/**
 * Check if the UID refers to a QTVM platform service.
 * */
bool PlatformServices_isUIDSupported(uint32_t id);

/**
 * Check if the UID can be opened remotely.
 * */
bool PlatformServices_isRemoteAllowed(uint32_t id);

/**
 * Register QTVM platform service into minkhub
 *
 * param[in]        minkhub     the minkhub instance
 *
 * param[out]       tMod        the TModule instance registered into minkhub
 *
 * return Object_OK if successful
 */
int32_t PlatformService_registerServices(MinkHub *minkhub, Object *tMod);

#endif  // __PLATFORMSERVICES_H
