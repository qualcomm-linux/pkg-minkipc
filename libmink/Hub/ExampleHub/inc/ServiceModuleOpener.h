// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __SERVIECEMODULEOPENER_H
#define __SERVIECEMODULEOPENER_H

#include "minksocket.h"
#include "object.h"
#include "ServiceManager.h"
#include "SockAgnostic.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
    char addr[MAX_SOCKADDR_LEN];
    int32_t sockType;
} RemoteAddr;

int32_t ServiceModuleOpenerLocal_new(ServiceManager *mgr, Object envCred, RemoteAddr *remoteHubAddr,
                                     uint32_t remoteHubNum, Object *objOut);
int32_t ServiceModuleOpenerRemote_new(ServiceManager *mgr, Object envCred, Object *objOut);

#if defined(__cplusplus)
}
#endif

#endif  // __SERVIECEMODULEOPENER_H
