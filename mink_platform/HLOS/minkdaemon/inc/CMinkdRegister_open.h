// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CMINKDREGISTER_OPEN_H
#define __CMINKDREGISTER_OPEN_H

#include <stdint.h>
#include "MinkHub.h"
#include "object.h"

/**
 * New Minkd Register class for Client.
 *
 * param[in]    session            the MinkHubSession instance
 * param[out]   objOut             the MinkdReg Object
 *
 * return Object_OK if successful
 */
int32_t MinkdRegister_new(MinkHubSession *session, Object *objOut);

#endif  // __CMINKDREGISTER_OPEN_H
