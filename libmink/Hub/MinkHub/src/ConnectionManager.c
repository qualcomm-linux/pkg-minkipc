// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "CloseNotifier.h"
#include "ConnectionManager.h"
#include "IModule.h"
#include "IOpener.h"
#include "MinkHub.h"
#include "MinkHubUtils.h"
#include "minkipc.h"
#include "minksocket.h"

#ifndef OFFTARGET
#include "IClientEnv.h"
#include "TZCom.h"
#endif

#define MAX_RETRY_TIME 2

static void _notifyDisconnection(void *data, int32_t event);

struct RemoteConnection {
    int32_t refs;
    bool isDone;
    pthread_t thread;
    pthread_mutex_t mutex;
    RemoteSocketAddrInfo info;
    Object closeNotifier;
    Object remoteOpener;
    MinkIPC *remoteConn;
};

static IPC_TYPE _getIPCType(RemoteIPCType ipcType)
{
    switch (ipcType) {
        case MINKHUB_QIPCRTR:
            return QIPCRTR;
        case MINKHUB_VSOCK:
            return VSOCK;
        case MINKHUB_UNIX:
            return UNIX;
        case MINKHUB_QMSGQ:
            return QMSGQ;
        default:
            return SIMULATED;
    }
}

static void _releaseRemoteConnection(RemoteConnection *me)
{
    Object_ASSIGN_NULL(me->closeNotifier);
    Object_ASSIGN_NULL(me->remoteOpener);
    if (me->remoteConn != NULL) {
        MinkIPC_release(me->remoteConn);
        me->remoteConn = NULL;
    }
}

static int32_t _refreshRemoteConn(RemoteConnection *me)
{
    int32_t ret = Object_OK;

    if (me->remoteConn == NULL) {
        MINKHUB_LOG_MSG("Trying to connect %d server %s.\n", me->info.ipcType, me->info.addr);
        me->remoteConn =
            MinkIPC_connect_common(me->info.addr, &me->remoteOpener, _getIPCType(me->info.ipcType));
        if (me->remoteConn != NULL && !Object_isNull(me->remoteOpener)) {
            MINKHUB_CALL(CloseNotifier_new(_notifyDisconnection, (void *)me, me->remoteOpener,
                                           &me->closeNotifier));
        } else {
            ret = Object_ERROR_UNAVAIL;
        }
    }

exit:
    if (Object_isERROR(ret)) {
        MINKHUB_LOG_MSG("Connect to %d server %s failed!", me->info.ipcType, me->info.addr);
        _releaseRemoteConnection(me);
    }

    return ret;
}

static void _checkConnection(void *data, int32_t event)
{
    (void)data;
    (void)event;
    return;
}

static void *_reconnectRemoteConn(void *data)
{
    RemoteConnection *info = (RemoteConnection *)data;
    Object notifyObj = Object_NULL;
    int32_t ret = Object_OK;

    pthread_mutex_lock(&info->mutex);
    MINKHUB_CHECK(!info->isDone);
    if (info->remoteConn != NULL && !Object_isNull(info->remoteOpener)) {
        // When notify event and opening request happen in the meanwhile, the remote connection may
        // have been reconnected by opening request before handling notify event. So here uses
        // notify mechanism to check whether the connection is available. When it set notifierObj
        // successfully, the connection is available.
        MINKHUB_CALL_CHECK(
            CloseNotifier_new(_checkConnection, data, info->remoteOpener, &notifyObj),
            Object_isERROR(ret));
    }

    _releaseRemoteConnection(info);
    (void)_refreshRemoteConn(info);

exit:
    pthread_mutex_unlock(&info->mutex);
    Object_ASSIGN_NULL(notifyObj);
    return NULL;
}

static void _notifyDisconnection(void *data, int32_t event)
{
    RemoteConnection *info = (RemoteConnection *)data;

    if (data == NULL) {
        return;
    }

    MINKHUB_LOG_MSG("%s Connection status = %x", info->info.addr, event);

    if (info->thread != -1) {
        pthread_join(info->thread, NULL);
    }

    MINKHUB_LOG_MSG("Create %s connection", info->info.addr);

    if (pthread_create(&info->thread, NULL, _reconnectRemoteConn, data) != 0) {
        MINKHUB_LOG_MSG("Failed to create %s connection thread.", info->info.addr);
        info->thread = -1;
    }
}

static int32_t _openVMService(RemoteConnection *me, uint32_t uid, Object linkCred, Object *objOut)
{
    int32_t ret = Object_OK;
    Object opener = Object_NULL;
    uint32_t retry = MAX_RETRY_TIME;

    pthread_mutex_lock(&me->mutex);

    do {
        MINKHUB_GUARD_ERR(_refreshRemoteConn(me), IModule_ERROR_NOT_FOUND);
        if (me->info.openerType == IMODULE) {
            ret = IModule_open(me->remoteOpener, uid, linkCred, objOut);
        } else {
            ret = IOpener_open(me->remoteOpener, uid, objOut);
        }
        if (ret != Object_ERROR_UNAVAIL && ret != Object_ERROR_DEFUNCT) {
            MINKHUB_LOG_RESULT("open VM service", uid, ret);
            break;
        } else {
            // Since remote server is down, it will return IModule_ERROR_NOT_FOUND which means the
            // service can't be found through remote server.
            ret = IModule_ERROR_NOT_FOUND;
        }

        MINKHUB_LOG_MSG("Remote server[%d:%s] is down.\n", me->info.ipcType, me->info.addr);
        _releaseRemoteConnection(me);
        --retry;
    } while (retry > 0);

exit:
    pthread_mutex_unlock(&me->mutex);
    return ret;
}

static int32_t _openQTEEService(RemoteConnection *connectionInfo, uint32_t uid, Object cred,
                                Object *clientEnv, Object *objOut)
{
    int32_t ret = IModule_ERROR_NOT_FOUND;
    Object tmpClientEnv = Object_NULL;
    Object *opener = clientEnv != NULL ? clientEnv : &tmpClientEnv;

    pthread_mutex_lock(&connectionInfo->mutex);
#ifndef OFFTARGET
    if (Object_isNull(*opener)) {
        // it will pass cred when it try to open clientEnv instance.
        MINKHUB_CALL_ERR(TZCom_getClientEnvObjectWithCreds(opener, cred), IModule_ERROR_NOT_FOUND);
    }

    ret = IClientEnv_open(*opener, uid, objOut);
    MINKHUB_LOG_RESULT("open SMC service", uid, ret);
    ret = Object_isOK(ret) ? ret : IModule_ERROR_NOT_FOUND;
#endif

exit:
    pthread_mutex_unlock(&connectionInfo->mutex);
    Object_ASSIGN_NULL(tmpClientEnv);
    return ret;
}

int32_t ConnectionManager_createConnection(const RemoteSocketAddrInfo *addrInfo,
                                           RemoteConnection **connectionInfo)
{
    int32_t ret = Object_OK;
    RemoteConnection *me = NULL;

    MINKHUB_CHECK_ERR(addrInfo->ipcType < MAX_MINKHUB_IPC_TYPE, Object_ERROR_INVALID);
    MINKHUB_CHECK_ERR(strlen(addrInfo->addr) < REMOTE_SOCKET_ADDR_MAX_LEN, Object_ERROR_INVALID);

    me = (RemoteConnection *)calloc(1, sizeof(RemoteConnection));
    MINKHUB_CHECK_ERR(me != NULL, Object_ERROR_MEM);

    me->thread = -1;
    me->isDone = false;

    memcpy(&me->info, addrInfo, sizeof(RemoteSocketAddrInfo));
    pthread_mutex_init(&me->mutex, NULL);
    *connectionInfo = me;

    MINKHUB_LOG_MSG("Create remote server[%d] connection.", me->info.ipcType);

exit:
    return ret;
}

void ConnectionManager_destroyConnection(RemoteConnection *connectionInfo)
{
    pthread_mutex_lock(&connectionInfo->mutex);
    connectionInfo->isDone = true;
    _releaseRemoteConnection(connectionInfo);
    pthread_mutex_unlock(&connectionInfo->mutex);

    if (connectionInfo->thread != -1) {
        pthread_join(connectionInfo->thread, NULL);
    }
    pthread_mutex_destroy(&connectionInfo->mutex);
    free(connectionInfo);
}

int32_t ConnectionManager_open(RemoteConnection *connectionInfo, uint32_t uid, Object cred,
                               Object *clientEnv, Object *objOut)
{
    if (connectionInfo->info.ipcType == MINKHUB_QTEE) {
        return _openQTEEService(connectionInfo, uid, cred, clientEnv, objOut);
    } else {
        return _openVMService(connectionInfo, uid, cred, objOut);
    }
}
