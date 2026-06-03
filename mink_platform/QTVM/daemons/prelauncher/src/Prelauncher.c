// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "CTPrivilegedProcessManager.h"
#include "ControlSocket.h"
#include "ITEnv.h"
#include "PlatformConfig.h"
#include "TEnv.h"
#include "TUtils.h"
#include "backtrace.h"
#include "minkipc.h"
#include "object.h"

extern Object gRootObj;

int32_t main(int32_t argc, char *argv[])

{
    MinkIPC *rootOpener = NULL;
    Object opener = Object_NULL;
    Object module = Object_NULL;
    MinkIPC *service = NULL;
    int32_t ret = Object_OK;
    int32_t retry = 5;

    LOG_MSG("Process name = %s", argv[0]);

    T_TRACE(register_backtrace_handlers(BACKTRACE_ALL_SIGNALS));

    // 1. Connect to the root to get ITEnv object
    do {
        rootOpener = MinkIPC_connect(ROOT_SOCKET_NAME, &opener);
        if (rootOpener == NULL || Object_isNull(opener)) {
            retry--;
            usleep(5000);
            LOG_MSG("retrying.. ");
        }
    } while (rootOpener == NULL && retry >= 0);
    T_CHECK(rootOpener != NULL && !Object_isNull(opener));

    // 2. Open TPrivilegedProcessManager_UID service to receive service object instance
    T_CALL(ITEnv_open(opener, CTPrivilegedProcessManager_UID, &gRootObj));

    module = TEnv_new();
    if (Object_isNull(module)) {
        LOG_MSG("Error in CTPreLauncher_new\n");
        goto exit;
    }

    service = MinkIPC_startServiceModule(PRELAUNCHER_SOCKET_NAME, module);
    T_CHECK(service != NULL);
    T_CALL(ControlSocket_enableSocketAttribute(PRELAUNCHER_SOCKET_NAME));

    LOG_MSG("Prelanucher Serving TZD interface\n");

    MinkIPC_join(service);

exit:
    if (service) {
        MinkIPC_release(service);
    }

    if (rootOpener) {
        MinkIPC_release(rootOpener);
    }

    Object_ASSIGN_NULL(opener);
    Object_ASSIGN_NULL(gRootObj);

    return ret;
}
