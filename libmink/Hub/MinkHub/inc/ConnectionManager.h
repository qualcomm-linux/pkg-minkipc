// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CONNECTION_MANAGER_H
#define __CONNECTION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "MinkHub.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct RemoteConnection RemoteConnection;

/**
 * Create the remote Connection based on remote address information
 *
 * param[in]    addrInfo        the remote address information instance
 *
 * param[out]   connectionInfo  the remote connection server instance
 *
 * return Object_OK if successful
 */
int32_t ConnectionManager_createConnection(const RemoteSocketAddrInfo *addrInfo,
                                           RemoteConnection **connectionInfo);

/**
 * Destroy the remote Connection
 *
 * param[in]    connectionInfo  the remote connection server instance
 */
void ConnectionManager_destroyConnection(RemoteConnection *connectionInfo);

/**
 * Open remote Service through connectionInfo or clientEnv
 *
 * param[in]        connectionInfo  the remote connection server instance
 * param[in]        uid             the target service uid
 * param[in]        cred            the credentials of caller
 *
 * param[in/out]    clientEnv       the instance that is used to call QTEE service. When it is not
 *                                  NULL, it will return the QTEE opener instance which could use to
 *                                  open QTEE service next time.
 * param[out]       objOut          the remote service instance
 */
int32_t ConnectionManager_open(RemoteConnection *connectionInfo, uint32_t uid, Object cred,
                               Object *clientEnv, Object *objOut);

#if defined(__cplusplus)
}
#endif

#endif  // __CONNECTION_MANAGER_H
