// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <libgen.h>
#include <stdbool.h>

#include "CAutoStartManager.h"
#include "CTPreLauncher.h"
#include "CloseNotifier.h"
#include "ControlSocket.h"
#include "ITEnv.h"
#include "ITPreLauncher.h"
#include "LocalConnectionManager.h"
#include "PlatformConfig.h"
#include "TEnv.h"
#include "TProcessLoader.h"
#include "TRebootVM.h"
#include "TUtils.h"
#include "cdefs.h"
#include "minkipc.h"
#include "minksocket.h"

#define LOCAL_SERVER_RETRY_TIME 2

typedef void *(*ConnectFunc)(void *arg);

typedef struct {
    IPC_TYPE sockType;
    int32_t result;
    pthread_t thread;
    const char *label;
    char *address;
    MinkIPC *minkIpc;
    Object proxy;
    Object closeNotifier;
    ConnectFunc connectFunc;
} MinkConnectionInfo;

static void *_connectPrelauncherDaemon(void *arg);
static void _reconnectServer(void *data, int32_t event);

static MinkConnectionInfo gMinkConnectionInfo = {
    // Connect to prelauncher info
    .sockType = UNIX,
    .result = Object_ERROR,
    .thread = -1,
    .label = PRELAUNCHER_SOCKET_NAME,
    .address = PRELAUNCHER_SOCKET_NAME,
    .minkIpc = NULL,
    .connectFunc = _connectPrelauncherDaemon};

static Object gNotify = Object_NULL;

static void _freeMinkConnect(MinkConnectionInfo *info)
{
    Object_ASSIGN_NULL(info->closeNotifier);
    Object_ASSIGN_NULL(info->proxy);

    if (info->minkIpc) {
        MinkIPC_release(info->minkIpc);
        info->minkIpc = NULL;
    }
}

static int32_t _connectTargetServer(MinkConnectionInfo *info)
{
    int32_t ret = Object_OK;
    int32_t retry = LOCAL_SERVER_RETRY_TIME;
    int32_t notifyFd = -1;
    int32_t watchFd;

    _freeMinkConnect(info);
    T_CALL(ControlSocket_initSocketDirNotify(&notifyFd, &watchFd));
    do {
        LOG_MSG("Trying connect to %s. Remain time = %d", info->label, retry);
        info->minkIpc =
            MinkIPC_connect_common(info->address, &info->proxy, (int32_t)info->sockType);
        if (info->minkIpc != NULL && !Object_isNull(info->proxy)) {
            T_CALL_NO_CHECK(ret, CloseNotifier_new(_reconnectServer, (void *)info, info->proxy,
                                                   &info->closeNotifier));
            if (Object_isOK(ret)) {
                break;
            } else {
                LOG_MSG("Register Conn Event failed.ret = %d\n", ret);
                _freeMinkConnect(info);
            }
        }

        retry--;
        if (retry > 0) {
            /*
             * Because the notification may setup after the socket file has been enable, the
             * socket file enable event won't be detected in this time. We should try to connect
             * to the local server firstly. If the connection failed, the socket file must be not
             * valid and it will be enable later. And in this time, its enable event can be
             * detected by notification mechanism and we won't retry to connect until detecting
             * the event.
             */
            T_CALL(ControlSocket_waitSocketEnable(notifyFd, basename(info->address)));
        }
    } while (info->minkIpc == NULL && retry > 0);

    T_CHECK(info->minkIpc != NULL && !Object_isNull(info->proxy));

    LOG_MSG("Connect to %s successfully.", info->label);

exit:
    if (Object_isERROR(ret)) {
        _freeMinkConnect(info);
    }

    if (notifyFd != -1) {
        ControlSocket_freeNotify(notifyFd, watchFd);
    }

    return ret;
}

static void *_connectPrelauncherDaemon(void *arg)
{
    Object preLauncherObj = Object_NULL;
    int32_t ret = Object_OK;
    bool isReconnect = false;
    MinkConnectionInfo *info = (MinkConnectionInfo *)arg;

    // Generate Core Restart Factory Object that will register to Root Daemon.
    if (Object_isNull(gNotify)) {
        T_GUARD(CTAutoStartManager_open(&gNotify));
    }

    if (info->minkIpc != NULL) {
        isReconnect = true;
    }

    CTProcessLoader_disable();
    CTRebootVM_disable();
    T_CALL(_connectTargetServer(info));
    T_GUARD(ITEnv_open(info->proxy, CTPreLauncher_UID, &preLauncherObj));
    CTProcessLoader_enable(preLauncherObj);
    CTRebootVM_enable(preLauncherObj);

    // Restiter Notify Object to Root daemon
    T_GUARD(ITPreLauncher_registerNotify(preLauncherObj, gNotify));

    if (isReconnect) {
        // If auto-start service exits when prelauncher is restarting, Mink won't get the
        // notification. So, we try to load all the auto-start service again after reconnecting to
        // prelauncher successfully. Root daemon won't load again if the auto-start service is still
        // alive.
        T_TRACE(CTAutoStartManager_startService());
    }

exit:
    Object_ASSIGN_NULL(preLauncherObj);
    info->result = ret;

    return NULL;
}

static void _reconnectServer(void *data, int32_t event)
{
    MinkConnectionInfo *info = (MinkConnectionInfo *)data;

    if (data == NULL) {
        return;
    }

    LOG_MSG("%s Connect status = %x", info->label, event);

    if (info->thread != -1) {
        pthread_join(info->thread, NULL);
    }

    LOG_MSG("Create %s connection", info->label);

    if (pthread_create(&info->thread, NULL, info->connectFunc, info) != 0) {
        LOG_MSG("Failed to create %s connection thread.", info->label);
        info->thread = -1;
    }
}

int32_t LocalConnectionManager_createConnection(void)
{
    int32_t ret = Object_OK;
    MinkConnectionInfo *info = &gMinkConnectionInfo;

    info->connectFunc((void *)info);
    T_CHECK(Object_isOK(info->result));

exit:
    if (Object_isERROR(ret)) {
        LocalConnectionManager_freeAllConnection();
    }

    return ret;
}

void LocalConnectionManager_freeAllConnection(void)
{
    MinkConnectionInfo *info = &gMinkConnectionInfo;
    CTProcessLoader_disable();
    CTRebootVM_disable();
    Object_ASSIGN_NULL(info->closeNotifier);
    _freeMinkConnect(info);
}
