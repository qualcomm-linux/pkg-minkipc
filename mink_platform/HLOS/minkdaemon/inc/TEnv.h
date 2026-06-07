// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __HLOSMINKD_TENV_H
#define __HLOSMINKD_TENV_H

#include "MinkHub.h"
#include "object.h"

// indicate MinkHub serves for interaction within-VM or across-VM typedef enum {
typedef enum {
    LOCAL,
    REMOTE,
} CallerType;

typedef enum {
    TENV_BASIC = 1,
    TENV_LOCAL_REMOTE,
    TENV_UID_LIST,
} TEnvType;

/**
 * New IEnv class for HLOS Platform Service.
 *
 * param[in]    hub            the MinkHub instance
 * param[in]    callerType     the caller type
 * param[out]   objOut         the TEnv Object
 *
 * return Object_OK if successful
 */
int32_t TEnv_new(MinkHub *hub, CallerType callerType, Object *objOut);

/**
 * New Custom class for registering service.
 *
 * param[in]    hub            the MinkHub instance
 * param[in]    cred           the caller's credential
 * param[in]    session        the MinkHubSession instance
 * param[in]    bVendor        Determine whether this service is from the system or the vendor
 * param[in]    uidList        Registered service list
 * param[in]    uidListLen     The length of the list of registered services
 * param[out]   objOut         the TEnv Object
 *
 * return Object_OK if successful
 */
int32_t Custom_newClient(MinkHub *hub, Object cred, MinkHubSession *session, bool bVendor,
                         const uint32_t *uidList, size_t uidListLen, Object *objOut);

#endif  // __HLOSMINKD_TENV_H
