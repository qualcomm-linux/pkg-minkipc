// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __MINK_HUB_H
#define __MINK_HUB_H

#include <stdbool.h>
#include <stdint.h>
#include "object.h"

#if defined(__cplusplus)
extern "C" {
#endif

// MINKHUB error codes are derived from Object error codes.
#define MINKHUB_OK (Object_OK)
#define MINKHUB_ERROR_NOT_VALID (Object_ERROR_DEFUNCT)
#define MINKHUB_ERROR_INVALID (Object_ERROR_INVALID)
#define MINKHUB_ERROR_MEM (Object_ERROR_MEM)
#define MINKHUB_ERROR_SERVICE_NOT_FOUND (Object_ERROR_USERBASE)
#define MINKHUB_ERROR_SERVICE_ALREADY_REGISTER (Object_ERROR_USERBASE + 1U)

#define REMOTE_SOCKET_ADDR_MAX_LEN 100

typedef enum {
    IOPENER,
    IMODULE,
    MAX_OPENER_TYPE,
} RemoteOpenerType;

typedef enum {
    MINKHUB_QIPCRTR,
    MINKHUB_VSOCK,
    MINKHUB_QTEE,
    MINKHUB_UNIX,
    MINKHUB_SIMULATE,
    MINKHUB_QMSGQ,
    MAX_MINKHUB_IPC_TYPE,
} RemoteIPCType;

typedef struct {
    RemoteIPCType ipcType;
    RemoteOpenerType openerType;
    char addr[REMOTE_SOCKET_ADDR_MAX_LEN];
} RemoteSocketAddrInfo;

typedef struct MinkHub MinkHub;
typedef struct MinkHubSession MinkHubSession;

/**
 * Create MinkHub instance
 *
 * param[in]    info            the remote address information array. When the ipcType is
 *                              MINKHUB_QTEE, openerType and addr are ignored.
 * param[in]    size            the size of array
 * param[in]    envCred         the environment credential
 *
 * param[out]   hub             the MinkHub instance
 *
 * return MINKHUB_OK if successful
 */
int32_t MinkHub_new(const RemoteSocketAddrInfo *info, uint32_t infoLen, Object envCred,
                    MinkHub **hub);

/**
 * Destroy MinkHub instance. The session registered into MinkHub wont be release when it calls
 * MinkHub_destroy.
 *
 * param[in]    hub             the MinkHub instance
 *
 * return MINKHUB_OK if successful
 */
int32_t MinkHub_destroy(MinkHub *hub);

/**
 * Create MinkHubSession instance
 * param[in]    hub             the MinkHub instance associated with session
 *
 * param[out]   session         the MinkHubSession instance
 *
 * return MINKHUB_OK if successful
 */
int32_t MinkHub_createSession(MinkHub *hub, MinkHubSession **session);

/**
 * Destroy MinkHubSession instance and unregister itself from MinkHub instance.
 *
 * param[in]    session         the MinkHubSession instance
 *
 * return MINKHUB_OK if successful
 */
int32_t MinkHub_destroySession(MinkHubSession *session);

/**
 * Register MinkHubSession instance into MinkHub instance.
 *
 * param[in]        session         the MinkHubSession instance
 * param[in]        uidList         the service ids related to module
 * param[in]        uidListLen      the size of uidList
 * param[in]        module          the service instance which should be IModule instance
 *
 * return MINKHUB_OK if successful
 */
int32_t MinkHub_registerServices(MinkHubSession *session, const uint32_t *uidList,
                                 size_t uidListLen, Object module);

/**
 * Open the service which is registered into MinkHub
 *
 * param[in]        hub             the MinkHub instance
 * param[in]        local           whether the caller is under the same VM of MinkHub
 * param[in]        cred            the caller's credential
 * param[in]        uid             the target service's uid
 *
 * param[out]       service         the target service instance
 *
 * return MINKHUB_OK if successful
 */
int32_t MinkHub_localOpen(MinkHub *hub, bool local, Object cred, uint32_t uid, Object *service);

/**
 * Open the remote service which locates on other VM or EE
 *
 * param[in]        hub            the MinkHub instance
 * param[in]        session        the MinkHubSession instance associated with the MinkHub instance.
 *                                 If there is no session instance, it could be NULL.
 * param[in]        procCred       the caller's credential
 * param[in]        uid            the target service's uid
 *
 * param[out]       service        the target service instance
 *
 * return MINKHUB_OK if successful
 *
 */
int32_t MinkHub_remoteOpen(MinkHub *hub, MinkHubSession *session, Object procCred, uint32_t uid,
                           Object *service);

#if defined(__cplusplus)
}
#endif

#endif  // __MINK_HUB_H
