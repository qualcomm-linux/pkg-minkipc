// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CAutoStartManager.h"
#include "CTPowerService_open.h"
#include "ControlSocket.h"
#include "ITEnv.h"
#include "ITPowerService.h"
#include "LocalConnectionManager.h"
#include "MinkHub.h"
#include "PlatformConfig.h"
#include "PlatformServices.h"
#include "QTVMPlatformInfo.h"
#include "ServiceCenter.h"
#include "SockVSOCK.h"
#include "TEnv.h"
#include "TModule.h"
#include "TProcessLoader.h"
#include "TUtils.h"
#include "backtrace.h"
#include "cdefs.h"
#include "heap.h"
#include "minkipc.h"
#include "osIndCredentials.h"
#include "version.h"

#ifndef OFFTARGET
#if ENABLE_TUNNEL_INVOKE
#include "TunnelInvokeService.h"
#endif

#if ENABLE_QRTR_SERVER
#include "libqrtr.h"
#endif

// Enable VSOCK service
#if ENABLE_VSOCK_SERVER
#include <linux/vm_sockets.h>
#include "VMCredentials.h"
#include "vmuuid.h"
#endif

// Enable QMSGQ service
#if ENABLE_QMSGQ_SERVER
#include "SockQMSGQ.h"
#endif
#endif

typedef MinkIPC *(*StartServiceFunc)(Object module);

typedef struct {
    CallerType type;
    MinkIPC *service;
    char *osID;
    uint8_t uuid[VMUUID_MAX_SIZE];
    Object cred;
    Object module;
    StartServiceFunc startService;
} ServerInfo;

MinkIPC *_startUnixService(Object module);

#if ENABLE_QRTR_SERVER
MinkIPC *_startQrtrService(Object module);
#endif

// Enable VSOCK service
#if ENABLE_VSOCK_SERVER
MinkIPC *_startVsockService(Object module);
#endif

// Enable QMSGQ service
#if ENABLE_QMSGQ_SERVER
MinkIPC *_startQMSGQService(Object module);
#endif

/* clang-format off */
static ServerInfo gLocalServiceInfo = {
    LOCAL, NULL, LOCAL_OSID, {VM_UUID}, Object_NULL, Object_NULL, _startUnixService
};

static ServerInfo gRemoteServiceInfo[] = {
#if ENABLE_QRTR_SERVER
    {REMOTE, NULL, "avm", {CLIENT_VMUID_HLOS}, Object_NULL, Object_NULL, _startQrtrService},
#endif
// Enable VSOCK service
#if ENABLE_VSOCK_SERVER
    {REMOTE, NULL, "oemvm", {CLIENT_VMUID_OEM}, Object_NULL, Object_NULL, _startVsockService},
#endif
// Enable QMSGQ service
#if ENABLE_QMSGQ_SERVER
    {REMOTE, NULL, "oemvm", {CLIENT_VMUID_OEM}, Object_NULL, Object_NULL, _startQMSGQService},
#endif
};
/* clang-format on */

/***********************************************************************
 * Thread functions to start the services
 * ********************************************************************/
#ifdef OFFTARGET
MinkIPC *_startSimulatedService(Object opener, char const *type, const char *sock, const char *name)
{
    MinkIPC *service = NULL;
    int32_t ret = Object_OK;

    service = MinkIPC_startServiceModule_simulated(name, opener);
    T_CHECK(service != NULL);

    T_CALL(ControlSocket_enableSocketAttribute(name));
    LOG_MSG("Serving simulated %s interface", type);

exit:
    if (Object_isERROR(ret) && service != NULL) {
        MinkIPC_release(service);
        service = NULL;
    }

    return service;
}
#endif

#if ENABLE_QRTR_SERVER
#ifndef OFFTARGET
MinkIPC *_startQrtrService(Object module)
{
    int32_t ret = Object_OK;
    int32_t fd = -1;
    MinkIPC *server = NULL;

    fd = qrtr_open(0);
    if (fd < 0) {
        LOG_MSG("qrtr_open failed %d for MINK_QRTR_SERVICE_TO_LA\n", fd);
        goto exit;
    } else {
        LOG_MSG("Opened QRTR socket %d\n", fd);
    }

    T_GUARD(qrtr_publish(fd, MINK_QRTR_SERVICE_TO_LA, MINK_QRTR_SERVICE_TO_LA_VERSION,
                         MINK_QRTR_SERVICE_TO_LA_INSTANCE));

    server = MinkIPC_startServiceModuleOnSocket_QRTR(fd, module);
    if (server) {
        LOG_MSG("starting QRTR server on %d", MINK_QRTR_SERVICE_TO_LA);
        fd = -1;
    } else {
        LOG_MSG("Failed to create QRTR service on %d", MINK_QRTR_SERVICE_TO_LA);
    }

exit:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    return server;
}
#else
MinkIPC *_startQrtrService(Object module)
{
    return _startSimulatedService(module, "QRTR", MINK_QRTR_SIMULATED_FD_ENV_VAR,
                                  MINK_QRTR_SIMULATED_SOCKET_NAME);
}
#endif
#endif

// Enable VSOCK service
#if ENABLE_VSOCK_SERVER
#ifndef OFFTARGET
MinkIPC *_startVsockService(Object module)
{
    int32_t ret = Object_OK;
    MinkIPC *server = NULL;
    int32_t fd = -1;
    struct sockaddr_vm svm = {
        .svm_family = AF_VSOCK,
        .svm_port = MINK_VSOCK_SERVICE_TO_XVM,
        .svm_cid = VMADDR_CID_ANY,
    };

    fd = SockVSOCK_constructFd();
    if (fd < 0) {
        LOG_MSG("socket failed %d for MINK_VSOCK_SERVICE_TO_XVM\n", fd);
        goto exit;
    } else {
        LOG_MSG("Opened VSOCK socket %d\n", fd);
    }

    T_GUARD(bind(fd, (struct sockaddr *)&svm, sizeof(svm)));

    server = MinkIPC_startServiceModuleOnSocket_vsock(fd, module);
    if (server) {
        LOG_MSG("starting VSOCK server on %d", MINK_VSOCK_SERVICE_TO_XVM);
        fd = -1;
    } else {
        LOG_MSG("Failed to create VSOCK service on %d", MINK_VSOCK_SERVICE_TO_XVM);
    }

exit:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    return server;
}
#else
MinkIPC *_startVsockService(Object module)
{
    return _startSimulatedService(module, "VSOCK", MINK_VSOCK_SIMULATED_FD_ENV_VAR,
                                  MINK_VSOCK_SIMULATED_SOCKET_NAME);
}
#endif
#endif

// Enable QMSGQ service
#if ENABLE_QMSGQ_SERVER
#ifndef OFFTARGET
MinkIPC *_startQMSGQService(Object module)
{
    int32_t ret = Object_OK;
    MinkIPC *server = NULL;
    int32_t fd = -1;
    struct sockaddr_vm svm = {
        .svm_family = AF_VSOCK,
        .svm_port = MINK_QMSGQ_SERVICE_TO_XVM,
        .svm_cid = VMADDR_CID_ANY,
    };

    fd = SockQMSGQ_constructFd();
    if (fd <= 0) {
        LOG_MSG("socket failed %d for MINK_QMSGQ_SERVICE_TO_XVM\n", fd);
        goto exit;
    } else {
        LOG_MSG("Opened QMSGQ socket %d\n", fd);
    }

    T_GUARD(bind(fd, (struct sockaddr *)&svm, sizeof(svm)));

    server = MinkIPC_startServiceModuleOnSocket_QMSGQ(fd, module);
    if (server) {
        LOG_MSG("starting QMSGQ server on %d", MINK_QMSGQ_SERVICE_TO_XVM);
        fd = -1;
    } else {
        LOG_MSG("Failed to create QMSGQ service on %d", MINK_QMSGQ_SERVICE_TO_XVM);
    }

exit:
    if (fd > 0) {
        close(fd);
        fd = -1;
    }

    return server;
}
#else
MinkIPC *_startQMSGQService(Object module)
{
    return _startSimulatedService(module, "QMSGQ", MINK_QMSGQ_SIMULATED_FD_ENV_VAR,
                                  MINK_QMSGQ_SIMULATED_SOCKET_NAME);
}
#endif
#endif

MinkIPC *_startUnixService(Object module)
{
    int32_t ret;
    MinkIPC *service = NULL;

    service = MinkIPC_startServiceModule(MINKD_SOCKET_NAME, module);
    T_CHECK(service != NULL);

    // Set the attribute of Mink socket file which will restrict the users and access permissions.
    T_CALL(ControlSocket_enableSocketAttribute(MINKD_SOCKET_NAME));

exit:
    if (Object_isERROR(ret) && service != NULL) {
        MinkIPC_release(service);
        service = NULL;
    }

    return service;
}

/***********************************************************************
 * You guessed it, main
 * ********************************************************************/
int32_t main(int32_t argc, char *argv[])
{
    Object powerService = Object_NULL;
    Object wakeLock = Object_NULL;
    int32_t ret = Object_OK;
    Object minkTMod = Object_NULL;
    Object remoteTMod = Object_NULL;
    MinkHub *minkhub = NULL;
    RemoteSocketAddrInfo info[] = {
        {MINKHUB_QTEE},
// Open VSOCK service
#if OPEN_VSOCK_SERVICE
        {MINK_XVM_VSOCK_TYPE, IMODULE, MINK_XVM_VSOCK_NAME},
#endif
// Open QMSGQ service
#if OPEN_QMSGQ_SERVICE
        {MINK_XVM_QMSGQ_TYPE, IMODULE, MINK_XVM_QMSGQ_NAME},
#endif
        {MINK_AVM_SOCKET_TYPE, IOPENER, MINK_AVM_SOCKET_NAME},
        {MINK_AVM_SOCKET_TYPE, IMODULE, MINK_HLOS_SOCKET_NAME},
    };

    LOG_MSG("Process name = %s", argv[0]);

    T_CALL(CTPowerServiceFactory_open(0, Object_NULL, &powerService));
    T_CALL(ITPowerService_acquireWakeLock(powerService, &wakeLock));
    Object_ASSIGN_NULL(powerService);

    T_GUARD(ServiceCenter_loadServiceProfiles(CONFIGURE_DIR, EMBEDDED_SERVICE_TYPE));

    // 1. Set backtrace handler
    T_TRACE(register_backtrace_handlers(BACKTRACE_ALL_SIGNALS));

    // 2. Connect to prelauncher daemons
    T_CALL(LocalConnectionManager_createConnection());

    // 3. Create MinkHub
    T_GUARD(OSIndCredentials_newEnvCred(gLocalServiceInfo.osID, gLocalServiceInfo.uuid, NULL,
                                        "vmDomain", QTVM_PLATFORM_VERSION,
                                        &gLocalServiceInfo.cred));
    T_CALL(MinkHub_new(info, C_LENGTHOF(info), gLocalServiceInfo.cred, &minkhub));

    // 4. Register Mink Platform Serivce into libminkhub
    T_GUARD(PlatformService_registerServices(minkhub, &minkTMod));

    CTProcessLoader_setMinkHub(minkhub);

    T_GUARD(TModule_createRemoteTModule(minkhub, &remoteTMod));

    // 5. Set local envCred to PlatformInfo service
    T_GUARD(CQTVMPlatformInfo_setEnvCred(gLocalServiceInfo.cred));

    // 6. Startup Unix server
    gLocalServiceInfo.module = TEnv_new(gLocalServiceInfo.cred, gLocalServiceInfo.type, remoteTMod);
    T_CHECK(!Object_isNull(gLocalServiceInfo.module));
    gLocalServiceInfo.service = gLocalServiceInfo.startService(gLocalServiceInfo.module);
    T_CHECK(gLocalServiceInfo.service != NULL);

    // 7. Startup AVM/XVM server.
    for (int32_t index = 0; index < C_LENGTHOF(gRemoteServiceInfo); ++index) {
        T_GUARD(OSIndCredentials_newEnvCred(gRemoteServiceInfo[index].osID,
                                            gRemoteServiceInfo[index].uuid, NULL, "vmDomain", 0,
                                            &gRemoteServiceInfo[index].cred));
        gRemoteServiceInfo[index].module =
            TEnv_new(gRemoteServiceInfo[index].cred, gRemoteServiceInfo[index].type, remoteTMod);
        T_CHECK(!Object_isNull(gRemoteServiceInfo[index].module));
        gRemoteServiceInfo[index].service =
            gRemoteServiceInfo[index].startService(gRemoteServiceInfo[index].module);
        T_CHECK(gRemoteServiceInfo[index].service != NULL);
    }

#ifndef OEM_VM
    // 8. Load auto start services
    T_TRACE(CTAutoStartManager_startService());
#endif

    LOG_MSG("Mink Startup Completed\n");

    Object_ASSIGN_NULL(wakeLock);
    MinkIPC_join(gLocalServiceInfo.service);

exit:
    LocalConnectionManager_freeAllConnection();

    Object_ASSIGN_NULL(gLocalServiceInfo.cred);
    Object_ASSIGN_NULL(gLocalServiceInfo.module);
    if (gLocalServiceInfo.service != NULL) {
        MinkIPC_release(gLocalServiceInfo.service);
    }

    for (int32_t index = 0; index < C_LENGTHOF(gRemoteServiceInfo); ++index) {
        Object_ASSIGN_NULL(gRemoteServiceInfo[index].cred);
        Object_ASSIGN_NULL(gRemoteServiceInfo[index].module);
        if (gRemoteServiceInfo[index].service != NULL) {
            MinkIPC_release(gRemoteServiceInfo[index].service);
        }
    }

    Object_ASSIGN_NULL(wakeLock);
    Object_ASSIGN_NULL(remoteTMod);
    Object_ASSIGN_NULL(minkTMod);

    MinkHub_destroy(minkhub);

    ServiceCenter_destory();

    return ret;
}
