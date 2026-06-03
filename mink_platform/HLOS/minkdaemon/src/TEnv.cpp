// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CHLOSPlatformEnv.h"
#include "CMinkdRegister.h"
#include "CMinkdRegister_open.h"
#include "ICredentials.h"
#include "IDSet.h"
#include "IMinkdRegister.h"
#include "IModule_invoke.h"
#include "IOpener_invoke.h"
#include "MinkDaemonConfig.h"
#include "MinkDaemon_logging.h"
#include "MinkHub.h"
#include "TEnv.h"
#include "VmOsal.h"
#include "heap.h"
#include "object.h"
#include "osIndCredentials.h"

typedef struct {
    int32_t refs;
    MinkHub *hub;
    CallerType callerType;
} TEnv;

typedef struct {
    int32_t refs;
    MinkHub *hub;
    MinkHubSession *session;
    Object cred;
    IDSet services;
    bool bVendor;
    TEnvType type;
} Custom;

static inline int32_t Custom_retain(Custom *me)
{
    vm_osal_atomic_add(&me->refs, 1);

    return Object_OK;
}

static inline int32_t Custom_release(Custom *me)
{
    if (vm_osal_atomic_add(&me->refs, -1) == 0) {
        LOG_MSG("Released Custom = %p\n", me);
        if (me->session) {
            MinkHub_destroySession(me->session);
            me->session = NULL;
        }
        Object_ASSIGN_NULL(me->cred);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

int32_t Custom_open(Custom *me, uint32_t uid, Object *objOut)
{
    int32_t ret = Object_OK;

    LOG_MSG("open called with id = %d", uid);
    if (uid == CMinkdRegister_UID) {
        LOG_MSG("Creating and registering LAMinkRegister service.");
        MinkDaemon_GUARD(MinkdRegister_new(me->session, objOut));
        goto exit;
    }

    if (me->type == TENV_UID_LIST && !me->bVendor) {
        MinkDaemon_CHECK_ERR(IDSet_test(&me->services, uid), IOpener_ERROR_PRIVILEGE);
    }

    ret = MinkHub_localOpen(me->hub, true, me->cred, uid, objOut);
    // The user of HLOS Platform must be a local client.
    if (ret != MINKHUB_OK) {
        MinkDaemon_GUARD(MinkHub_remoteOpen(me->hub, me->session, me->cred, uid, objOut));
    }

exit:
    return ret;
}

static IOpener_DEFINE_INVOKE(Custom_invoke, Custom_, Custom *);

/**
 * New Custom class for HLOS Platform Service.
 *
 * param[in]    hub            the MinkHub instance
 * param[in]    cred           the caller's credential
 * param[in]    session        the MinkHubSession instance
 * param[out]   objOut         the Custom Object
 *
 * return Object_OK if successful
 */
int32_t Custom_new(MinkHub *hub, Object cred, MinkHubSession *session, Object *objOut)
{
    Custom *me = HEAP_ZALLOC_TYPE(Custom);
    if (!me) {
        LOG_ERR("Custom memory allocation failed");
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    me->hub = hub;
    Object_INIT(me->cred, cred);
    me->session = session;
    *objOut = (Object){Custom_invoke, me};

    return Object_OK;
}

static inline int32_t TEnv_retain(TEnv *me)
{
    vm_osal_atomic_add(&me->refs, 1);

    return Object_OK;
}

static inline int32_t TEnv_release(TEnv *me)
{
    if (vm_osal_atomic_add(&me->refs, -1) == 0) {
        LOG_MSG("Released TEnv = %p\n", me);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static int32_t TEnv_shutdown(TEnv *me)
{
    LOG_MSG("TEnv_shutdown.");

    return Object_OK;
}

static int32_t TEnv_open(TEnv *me, uint32_t uid, Object linkCred, Object *objOut)
{
    int32_t ret = Object_OK;
    Object procCred = Object_NULL;
    uint32_t callerPid = 0;
    uint32_t callerUid = 0;
    size_t lenPidOut = 0;
    size_t lenUidOut = 0;
    MinkHubSession *newClientSession = NULL;

    LOG_MSG("Open called with id = %d", uid);
    if (LOCAL == me->callerType) {
        if (Object_isNull(linkCred)) {
            callerPid = getpid();
            callerUid = getuid();
        } else {
            MinkDaemon_GUARD(ICredentials_getValueByName(
                linkCred, "lpid", strlen("lpid"), &callerPid, sizeof(callerPid), &lenPidOut));
            MinkDaemon_GUARD(ICredentials_getValueByName(
                linkCred, "luid", strlen("luid"), &callerUid, sizeof(callerUid), &lenUidOut));
        }
        MinkDaemon_GUARD(OSIndCredentials_newProcessCred(
            0, "hlosminkd", NULL, 1, 2, NULL, 0, "oem", callerPid, callerUid, &procCred));
    } else {
        if (Object_isNull(linkCred)) {
            LOG_MSG("Remote connection is untrusted.");
            ret = Object_ERROR;
            goto exit;
        }
    }

    if (uid == CHLOSPlatformEnv_UID) {
        if (LOCAL == me->callerType) {
            MinkDaemon_GUARD(MinkHub_createSession(me->hub, &newClientSession));
            MinkDaemon_CHECK(NULL != newClientSession);
            MinkDaemon_GUARD(Custom_new(me->hub, procCred, newClientSession, objOut));
        } else {
            LOG_MSG("HLOS Platform does not support remote use.");
            ret = Object_ERROR;
        }
        goto exit;
    }

    if (LOCAL == me->callerType) {
        ret = MinkHub_localOpen(me->hub, true, procCred, uid, objOut);
        if (ret == Object_OK) {
            goto exit;
        }
        MinkDaemon_GUARD(MinkHub_remoteOpen(me->hub, NULL, procCred, uid, objOut));
    } else {
        MinkDaemon_GUARD(MinkHub_localOpen(me->hub, false, linkCred, uid, objOut));
    }

exit:
    Object_ASSIGN_NULL(procCred);
    if (ret && newClientSession) {
        MinkHub_destroySession(newClientSession);
        newClientSession = NULL;
    }

    return ret;
}

static IModule_DEFINE_INVOKE(TEnv_invoke, TEnv_, TEnv *);

int32_t TEnv_new(MinkHub *hub, CallerType callerType, Object *objOut)
{
    TEnv *me = HEAP_ZALLOC_TYPE(TEnv);
    if (!me) {
        LOG_ERR("TEnv memory allocation failed");
        return Object_ERROR_MEM;
    }
    me->refs = 1;
    me->callerType = callerType;
    me->hub = hub;
    *objOut = (Object){TEnv_invoke, me};

    return Object_OK;
}

int32_t Custom_newClient(MinkHub *hub, Object cred, MinkHubSession *session, bool bVendor,
                         const uint32_t *uidList, size_t uidListLen, Object *objOut)
{
    int32_t ret = Object_ERROR;

    Custom *me = HEAP_ZALLOC_TYPE(Custom);
    if (!me) {
        LOG_ERR("Custom memory allocation failed");
        ret = Object_ERROR_MEM;
        goto exit;
    }

    me->refs = 1;
    me->hub = hub;
    Object_INIT(me->cred, cred);
    me->session = session;
    me->bVendor = bVendor;
    IDSet_destruct(&me->services);

    if (!bVendor) {
        MinkDaemon_CHECK(NULL != uidList && 0 != uidListLen);
        for (size_t index = 0; index < uidListLen; ++index) {
            MinkDaemon_GUARD(IDSet_set(&me->services, uidList[index]));
        }
    }

    me->type = TENV_UID_LIST;
    *objOut = (Object){Custom_invoke, me};
    ret = Object_OK;

exit:
    return ret;
}
