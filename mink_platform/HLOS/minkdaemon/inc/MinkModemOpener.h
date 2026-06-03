// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __MINKMODEMOPENER_H
#define __MINKMODEMOPENER_H

#include "object.h"

/**
 * New IMinkModemOpener class for Modem.
 *
 * param[in]    tEnvObj            the TEnv Object
 * param[out]   objOut             the MinkModemOpener Object
 *
 * return Object_OK if successful
 */
int32_t MinkModemOpener_new(Object tEnvObj, Object *objOut);

#endif  // __MINKMODEMOPENER_H
