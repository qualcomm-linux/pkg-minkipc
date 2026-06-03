// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TVMENV_H
#define __TVMENV_H

#include "object.h"

typedef struct TEnv TEnv;

// indicate MinkHub serves for interaction within-VM or across-VM
typedef enum {
    LOCAL,
    REMOTE,
} CallerType;

/**
 * New IEnv class on top of our registration framework.
 *
 * IEnv extends IModule.
 *
 * param[in]    remoteEnvCred   the remote server's credential which includes EEUID and so on.
 * param[in]    callerType      the type of caller
 * param[in]    remoteTMod      the remote TModule
 *
 * return TEnvObj if successful
 * */
Object TEnv_new(Object remoteEnvCred, CallerType callerType, Object remoteTMod);

#endif  // __TVMENV_H
