// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <pthread.h>
#include <stdint.h>

#include "CTAccessControl.h"
#include "CTMemoryService.h"
#include "CTPowerService.h"
#include "CTPowerService_open.h"
#include "CTRegisterModule.h"
#include "EmbeddedProcessLoader.h"
#include "ErrorMap.h"
#include "IModule_invoke.h"
#include "ITEnv.h"
#include "ITPowerService.h"
#include "ITProcessLoader.h"
#include "MinkHub.h"
#include "PlatformConfig.h"
#include "PlatformServices.h"
#include "TEnv.h"
#include "TModule.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "osIndCredentials.h"

#ifndef OFFTARGET
#include "IClientEnv.h"
#include "TZCom.h"
#include "TunnelInvokeService.h"
#else
#include "ICredentials.h"
#endif

#define MAX_RELOAD_ATTEMPTS 10

/* clang-format off */
/**
 * Error conversion from ITProcessLoaderEmbedded to ITEnv
 * */
static const ErrorPair gITProcessLoaderToITEnvErrors[] = {
    {ITProcessLoader_ERROR_INVALID_BUFFER, Object_ERROR},
    {ITProcessLoader_ERROR_PROC_NOT_LOADED, Object_ERROR},
    {ITProcessLoader_ERROR_PROC_ALREADY_LOADED, Object_OK},
    {ITProcessLoader_ERROR_EMBED_MISSING_PROPERTY, ITEnv_ERROR_EMBED_MISSING_PROPERTY},
    {ITProcessLoader_ERROR_IMODULE_REGISTRATION, Object_ERROR},
};

static const ErrorMap gITProcessLoaderToITEnvErrorMap = {
    .errors = gITProcessLoaderToITEnvErrors,
    .length = C_LENGTHOF(gITProcessLoaderToITEnvErrors),
    .genericError = Object_ERROR,
    .startConversionAt = Object_ERROR_USERBASE
};
/* clang-format on */

static int32_t ITProcessLoaderToITEnvError(int32_t error)
{
    return ErrorMap_convert(&gITProcessLoaderToITEnvErrorMap, error);
}

/***********************************************************************
 * Env opener
 * ********************************************************************/

struct TEnv {
    int32_t refs;
    Object remoteEnvCred;
    Object remoteTMod;
    CallerType callerType;
};

static int32_t TEnv_open(TEnv *me, uint32_t uid, Object receivedCred, Object *objOut)
{
    int32_t ret = Object_OK;
    uint32_t key = 0;
    size_t lenOut = 0;
    int32_t reloadCount = 1;
    Object cred = Object_NULL;
    Object powerService = Object_NULL;
    Object wakeLock = Object_NULL;
    Object tMod = Object_NULL;
    bool isEmbeddedService = false;

    // Acquire wakeLock for duration of this call
    T_GUARD(CTPowerServiceFactory_open(0, Object_NULL, &powerService));
    T_GUARD(ITPowerService_acquireWakeLock(powerService, &wakeLock));

    // Check the privilege whether the caller can access the target service.
    if (me->callerType == LOCAL) {
        if (uid != CTRegisterModule_UID) {
            // Get the unique identifier of caller and use it to find the local client which has
            // been registered into MinkHub.
            T_GUARD(ICredentials_getValueByName(receivedCred, "lpid", strlen("lpid"), &key,
                                                sizeof(uint32_t), &lenOut));
            T_GUARD(TModule_findTModule(key, &tMod));
            // Bypass the privilege checking when the local client try to open default platform
            // service.
            if (!PlatformServices_isUIDSupported(uid)) {
                T_CHECK_ERR(TModule_checkPrivilege(tMod, uid), ITEnv_ERROR_PRIVILEGE);
            }
        } else {
            Object_INIT(tMod, me->remoteTMod);
        }

        Object_INIT(cred, receivedCred);
    } else {
        T_CHECK_ERR(PlatformServices_isRemoteAllowed(uid), ITEnv_ERROR_LOCAL_EXCLUSIVE);
        T_GUARD(OSIndCredentials_WrapCredentials(&receivedCred, &me->remoteEnvCred, &cred));
        Object_INIT(tMod, me->remoteTMod);
    }

    /* To handle a race between cleaning up the TModule + TProcess of a launched
     * embedded process and opening a service from it, we have devised a retry
     * mechanism. For non-embedded processes, the retry mechanism will not be
     * engaged. */
    isEmbeddedService = EmbeddedProcessLoader_isUIDSupported(uid);
    if (isEmbeddedService) {
        reloadCount = MAX_RELOAD_ATTEMPTS;
    }

    do {
        T_CALL_NO_CHECK(ret, TModule_localOpen(tMod, uid, cred, objOut), "for UID = %x", uid);
        if (Object_isOK(ret)) {
            goto exit;
        }
        if (reloadCount-- == 0) {
            break;
        }

        if (isEmbeddedService) {
            // Launch embedded app, if it hosts service and is not yet loaded
            T_CALL_REMAP(EmbeddedProcessLoader_load(uid, cred), ITProcessLoaderToITEnvError);
            LOG_MSG("Embedded Service discovery: retries remaining = %d", reloadCount);
        }
    } while (true);

    // Services hosted in Embedded Processes should have been opened by now.
    T_CHECK_ERR(!isEmbeddedService, ITEnv_ERROR_EMBED_RETRY);

    if (me->callerType == LOCAL) {
        T_CALL_NO_CHECK(ret, TModule_remoteOpen(tMod, uid, objOut), "for UID = %x", uid);
        if (ret != IModule_ERROR_NOT_FOUND) {
            goto exit;
        }
    }

    ret = ITEnv_ERROR_NOT_FOUND;

exit:
    Object_ASSIGN_NULL(wakeLock);
    Object_ASSIGN_NULL(powerService);
    Object_ASSIGN_NULL(tMod);
    Object_ASSIGN_NULL(cred);

    return ret;
}

static int32_t TEnv_retain(TEnv *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t TEnv_release(TEnv *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        Object_ASSIGN_NULL(me->remoteEnvCred);
        Object_ASSIGN_NULL(me->remoteTMod);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static int32_t TEnv_shutdown(TEnv *me)
{
    // Don't support remotely shutting down Mink process.
    return Object_ERROR;
}

static IModule_DEFINE_INVOKE(TEnv_invoke, TEnv_, TEnv *);

Object TEnv_new(Object remoteEnvCred, CallerType callerType, Object remoteTMod)
{
    TEnv *me = HEAP_ZALLOC_TYPE(TEnv);
    if (!me) {
        return Object_NULL;
    }

    me->refs = 1;
    me->callerType = callerType;
    // It is a temporary fix until MinkIPC transport protocols can all support direct queries remote
    // env credentials.
    Object_INIT(me->remoteEnvCred, remoteEnvCred);
    Object_INIT(me->remoteTMod, remoteTMod);

    return (Object){TEnv_invoke, me};
}
