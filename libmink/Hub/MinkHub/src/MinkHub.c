// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "MinkHub.h"
#include "ConnectionManager.h"
#include "ICredentials.h"
#include "IDSet.h"
#include "IModule.h"
#include "MinkHubUtils.h"
#include "osIndCredentials.h"
#include "pthread.h"
#include "qlist.h"

struct MinkHub {
    int32_t refs;
    bool isValid;
    int32_t remoteServerSize;
    RemoteConnection **remoteServer;
    Object hubEnvCred;
    QList sessionList;
    pthread_rwlock_t rwMutex;
};

struct MinkHubSession {
    MinkHub *hub;
    QNode node;
    IDSet services;
    Object clientEnv;
    Object modObj;
};

static void _MinkHub_release(MinkHub *minkhub)
{
    if (atomicAdd(&minkhub->refs, -1) == 0) {
        for (int32_t index = 0; index < minkhub->remoteServerSize; ++index) {
            ConnectionManager_destroyConnection(minkhub->remoteServer[index]);
        }

        pthread_rwlock_destroy(&minkhub->rwMutex);
        if (minkhub->remoteServer != NULL) {
            free(minkhub->remoteServer);
            minkhub->remoteServer = NULL;
        }
        free(minkhub);
        minkhub = NULL;
    }
}

static void _deregisterSession(MinkHubSession *session)
{
    MinkHub *hub = session->hub;

    pthread_rwlock_wrlock(&hub->rwMutex);
    if (QNode_isQueued(&session->node)) {
        MINKHUB_LOG_MSG("Deregister the Session\n");
        QNode_dequeue(&session->node);
    }
    pthread_rwlock_unlock(&hub->rwMutex);
}

int32_t MinkHub_destroySession(MinkHubSession *session)
{
    int32_t ret = MINKHUB_OK;

    MINKHUB_CHECK_ERR(session != NULL, MINKHUB_ERROR_INVALID);

    _deregisterSession(session);
    if (!Object_isNull(session->modObj)) {
        MINKHUB_TRACE(IModule_shutdown(session->modObj));
        Object_ASSIGN_NULL(session->modObj);
    }

    IDSet_destruct(&session->services);
    Object_ASSIGN_NULL(session->clientEnv);
    _MinkHub_release(session->hub);
    free(session);
    session = NULL;

exit:
    return ret;
}

int32_t MinkHub_createSession(MinkHub *hub, MinkHubSession **session)
{
    int32_t ret = MINKHUB_OK;
    MinkHubSession *tmp = NULL;
    bool isValid = false;

    MINKHUB_CHECK_ERR(hub != NULL && session != NULL, MINKHUB_ERROR_INVALID);

    pthread_rwlock_rdlock(&hub->rwMutex);
    isValid = hub->isValid;
    pthread_rwlock_unlock(&hub->rwMutex);

    MINKHUB_CHECK_ERR(isValid, MINKHUB_ERROR_NOT_VALID);

    tmp = (MinkHubSession *)calloc(1, sizeof(MinkHubSession));
    MINKHUB_CHECK_ERR(tmp != NULL, MINKHUB_ERROR_MEM);

    QNode_construct(&tmp->node);

    // Every session takes one ref of Minkhub in order that MinkHub won't be released before
    // releasing all the sessions.
    atomicAdd(&hub->refs, 1);
    tmp->hub = hub;
    tmp->clientEnv = Object_NULL;
    *session = tmp;

exit:
    return ret;
}

int32_t MinkHub_registerServices(MinkHubSession *session, const uint32_t *uidList,
                                 size_t uidListLen, Object module)
{
    int32_t ret = MINKHUB_OK;
    bool isValid = false;
    MinkHub *hub = NULL;

    MINKHUB_CHECK_ERR(session != NULL, MINKHUB_ERROR_INVALID);
    MINKHUB_CHECK_ERR(uidList != NULL && uidListLen != 0 && !Object_isNull(module),
                      MINKHUB_ERROR_INVALID);

    hub = session->hub;
    pthread_rwlock_rdlock(&hub->rwMutex);
    isValid = hub->isValid;
    pthread_rwlock_unlock(&hub->rwMutex);

    MINKHUB_CHECK_ERR(isValid, MINKHUB_ERROR_NOT_VALID);

    // If session has been registered, it will be deregistered firstly.
    _deregisterSession(session);
    IDSet_destruct(&session->services);

    for (size_t index = 0; index < uidListLen; ++index) {
        MINKHUB_GUARD(IDSet_set(&session->services, uidList[index]));
    }

    Object_ASSIGN(session->modObj, module);

    pthread_rwlock_wrlock(&hub->rwMutex);
    QList_appendNode(&hub->sessionList, &session->node);
    pthread_rwlock_unlock(&hub->rwMutex);

exit:
    return ret;
}

int32_t MinkHub_new(const RemoteSocketAddrInfo *info, uint32_t infoLen, Object envCred,
                    MinkHub **hub)
{
    int32_t ret = MINKHUB_OK;
    MinkHub *minkHub = NULL;

    MINKHUB_CHECK_ERR(hub != NULL, MINKHUB_ERROR_INVALID);
    MINKHUB_CHECK_ERR((info != NULL && infoLen != 0) || (info == NULL && infoLen == 0),
                      MINKHUB_ERROR_INVALID);
    MINKHUB_CHECK_ERR(!Object_isNull(envCred), MINKHUB_ERROR_INVALID);

    minkHub = (MinkHub *)calloc(1, sizeof(MinkHub));
    MINKHUB_CHECK_ERR(minkHub != NULL, MINKHUB_ERROR_MEM);

    minkHub->refs = 1;
    minkHub->remoteServerSize = 0;

    minkHub->remoteServer = (RemoteConnection **)calloc(infoLen, sizeof(RemoteConnection *));
    MINKHUB_CHECK_ERR(minkHub->remoteServer != NULL, MINKHUB_ERROR_MEM);

    for (int32_t index = 0; index < infoLen; ++index) {
        MINKHUB_CHECK_ERR(info[index].openerType < MAX_OPENER_TYPE, MINKHUB_ERROR_INVALID);
        MINKHUB_CHECK_ERR(info[index].ipcType < MAX_MINKHUB_IPC_TYPE, MINKHUB_ERROR_INVALID);
        MINKHUB_CALL(
            ConnectionManager_createConnection(&info[index], &minkHub->remoteServer[index]));
        ++(minkHub->remoteServerSize);
    }

    pthread_rwlock_init(&minkHub->rwMutex, NULL);
    QList_construct(&minkHub->sessionList);
    minkHub->isValid = true;

    Object_INIT(minkHub->hubEnvCred, envCred);

    *hub = minkHub;
    ret = MINKHUB_OK;

exit:
    if (Object_isERROR(ret) && minkHub != NULL) {
        _MinkHub_release(minkHub);
    }

    return ret;
}

int32_t MinkHub_destroy(MinkHub *hub)
{
    int32_t ret = MINKHUB_OK;

    MINKHUB_CHECK_ERR(hub != NULL, MINKHUB_ERROR_INVALID);

    pthread_rwlock_wrlock(&hub->rwMutex);
    hub->isValid = false;
    pthread_rwlock_unlock(&hub->rwMutex);

    _MinkHub_release(hub);

exit:
    return ret;
}

int32_t MinkHub_localOpen(MinkHub *hub, bool local, Object cred, uint32_t uid, Object *service)
{
    int32_t ret = MINKHUB_OK;
    Object localService = Object_NULL;
    Object wrapCred = Object_NULL;
    QNode *pNode = NULL;

    if (hub == NULL || Object_isNull(cred) || service == NULL) {
        return MINKHUB_ERROR_INVALID;
    }

    pthread_rwlock_rdlock(&hub->rwMutex);

    MINKHUB_CHECK_ERR(hub->isValid, MINKHUB_ERROR_NOT_VALID);

    if (local) {
        MINKHUB_CALL(OSIndCredentials_WrapCredentials(&cred, &hub->hubEnvCred, &wrapCred));
    } else {
        Object_INIT(wrapCred, cred);
    }

    QLIST_FOR_ALL(&hub->sessionList, pNode)
    {
        MinkHubSession *session = C_CONTAINEROF(pNode, MinkHubSession, node);
        if (IDSet_test(&session->services, uid)) {
            ret = Object_OK;
            MINKHUB_LOG_RESULT("find the service", uid, ret);
            MINKHUB_CALL(IModule_open(session->modObj, uid, wrapCred, &localService));
            *service = localService;
            goto exit;
        }
    }

    ret = MINKHUB_ERROR_SERVICE_NOT_FOUND;

exit:
    pthread_rwlock_unlock(&hub->rwMutex);
    Object_ASSIGN_NULL(wrapCred);
    return ret;
}

int32_t MinkHub_remoteOpen(MinkHub *hub, MinkHubSession *session, Object procCred, uint32_t uid,
                           Object *service)
{
    int32_t ret = MINKHUB_OK;
    Object remoteService = Object_NULL;
    Object wrapCred = Object_NULL;
    Object *clientEnv = session != NULL ? &session->clientEnv : NULL;

    if (hub == NULL || (session != NULL && session->hub != hub) || service == NULL ||
        Object_isNull(procCred)) {
        return MINKHUB_ERROR_INVALID;
    }

    pthread_rwlock_rdlock(&hub->rwMutex);
    MINKHUB_CHECK_ERR(hub->isValid, MINKHUB_ERROR_NOT_VALID);

    MINKHUB_GUARD(OSIndCredentials_WrapCredentials(&procCred, &hub->hubEnvCred, &wrapCred));

    for (int32_t index = 0; index < hub->remoteServerSize; ++index) {
        ret = ConnectionManager_open(hub->remoteServer[index], uid, wrapCred, clientEnv,
                                     &remoteService);
        MINKHUB_LOG_RESULT("open Remote Service", uid, ret);
        if (ret != IModule_ERROR_NOT_FOUND) {
            goto exit;
        }
    }

    ret = MINKHUB_ERROR_SERVICE_NOT_FOUND;

exit:
    pthread_rwlock_unlock(&hub->rwMutex);
    Object_ASSIGN_NULL(wrapCred);

    *service = Object_isOK(ret) ? remoteService : Object_NULL;
    return (ret == IModule_ERROR_NOT_FOUND) ? MINKHUB_ERROR_SERVICE_NOT_FOUND : ret;
}
