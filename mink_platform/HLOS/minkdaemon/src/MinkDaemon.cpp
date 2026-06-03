// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include "CRegister_open.h"
#include "MinkDaemonConfig.h"
#include "MinkDaemon_logging.h"
#include "MinkdVendorClients.h"
#include "MinkHub.h"
#include "MinkModemOpener.h"
#include "TaLoader.h"
#include "TEnv.h"
#include "libqrtr.h"
#include "minkipc.h"
#include "osIndCredentials.h"
#include "vmuuid.h"
extern "C" {
#include "cdefs.h"
#include "strlcpy.h"
}

#ifdef HLOSMINKD_TAAUTO_LOAD
#define TALOAD_REGISTER_SYM "registerService"
#define MAX_RETRY_ATTEMPTS 20
void *ta_autoload_handle = NULL;
typedef int32_t (*registerService)(Object);
registerService start_ta_load_func;
#endif  // HLOSMINKD_TAAUTO_LOAD

#ifdef HLOSMINKD_MODEM_SERVICE
static MinkIPC *start_modem_opener_service(Object hlosminkdTEnv)
{
    struct timeval tv;
    Object minkModemOpener = Object_NULL;
    MinkIPC *service = NULL;
    int32_t fd = -1;
    int32_t ret = Object_ERROR;

    LOG_MSG("calling MinkModemOpener_new");
    ret = MinkModemOpener_new(hlosminkdTEnv, &minkModemOpener);
    if (ret || Object_isNull(minkModemOpener)) {
        LOG_ERR("Error in MinkModemOpener_new");
        goto exit;
    }

    fd = qrtr_open(0);
    if (fd < 0) {
        LOG_ERR("qrtr_open failed %d\n", fd);
        goto exit;
    } else {
        LOG_MSG("Opened QRTR socket %d\n", fd);
    }

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (ret) {
        LOG_ERR("Error %d setting blocking option for QRTR socket %d: errnor %d\n", ret, fd, errno);
        goto exit;
    }

    ret = qrtr_publish(fd, MINK_IPCR_MODEM_SERVICE, MINK_IPCR_MODEM_SERVICE_VERSION,
                       MINK_IPCR_MODEM_SERVICE_INSTANCE);
    if (ret) {
        LOG_ERR("qrtr_publish returned %d\n", ret);
        goto exit;
    }

    service = MinkIPC_startServiceOnSocket_QRTR(fd, minkModemOpener);
    if (service) {
        // MinkIPC takes care of closing it
        fd = -1;
        LOG_MSG("started server on %d", MINK_IPCR_MODEM_SERVICE);
    } else {
        LOG_ERR("Failed to create service on %d", MINK_IPCR_MODEM_SERVICE);
    }

exit:
    Object_ASSIGN_NULL(minkModemOpener);
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    return service;
}
#endif // HLOSMINKD_MODEM_SERVICE

#if ENABLE_QRTR_SERVER
#ifndef OFFTARGET
static MinkIPC *start_la_qrtr_service(Object *hlosTenv)
{
    MinkIPC *server = NULL;
    int32_t ret = Object_OK;
    int32_t fd = -1;

    fd = qrtr_open(0);
    if (fd < 0) {
        LOG_MSG("qrtr_open failed %d for MINK_QRTR_SERVICE_FROM_LA\n", fd);
        goto exit;
    } else {
        LOG_MSG("Opened QRTR socket %d\n", fd);
    }

    MinkDaemon_GUARD(qrtr_publish(fd, MINK_QRTR_SERVICE_FROM_LA,
                                      MINK_QRTR_SERVICE_FROM_LA_VERSION,
                                      MINK_QRTR_SERVICE_FROM_LA_INSTANCE));

    server = MinkIPC_startServiceModuleOnSocket_QRTR(fd, *hlosTenv);
    if (server) {
        LOG_MSG("starting QRTR server on %d", MINK_QRTR_SERVICE_FROM_LA);
        fd = -1;
    } else {
        LOG_MSG("Failed to create QRTR service on %d", MINK_QRTR_SERVICE_FROM_LA);
    }

exit:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    return server;
}
#else
static MinkIPC *start_la_qrtr_service(Object *hlosTenv)
{
    MinkIPC *server = NULL;

    server = MinkIPC_startServiceModule_simulated(MINK_AVM_SOCKET_NAME, *hlosTenv);
    if (server) {
        LOG_MSG("starting QRTR server on %s", MINK_AVM_SOCKET_NAME);
    } else {
        LOG_MSG("Failed to create QRTR server on %s", MINK_AVM_SOCKET_NAME);
    }

exit:
    return server;
}
#endif
#endif  // ENABLE_QRTR_SERVER

#ifdef HLOSMINKD_TAAUTO_LOAD
static void registerAutoTALoadService(Object hlosminkdEnvObj)
{
    int32_t ret = 0;
    const char *LIBTAAUTOLOAD = "libtaautoload.so";

    ta_autoload_handle = dlopen(LIBTAAUTOLOAD, RTLD_LAZY);
    if (ta_autoload_handle) {
        start_ta_load_func = (registerService)loadSym(ta_autoload_handle, TALOAD_REGISTER_SYM);
        if (start_ta_load_func == NULL) {
            LOG_ERR("Failed to enable Autoload %s", dlerror());
            dlclose(ta_autoload_handle);
        } else {
            ret = start_ta_load_func(hlosminkdEnvObj);
            if (Object_OK != ret) {
                LOG_ERR("Failed to enable Autoload, ret = %d", ret);
                dlclose(ta_autoload_handle);
            }
            LOG_MSG("Autoload is active");
        }
    } else {
        LOG_ERR("Autoload feature not supported error %s", dlerror());
    }
}
#endif // HLOSMINKD_TAAUTO_LOAD

static int android_get_control_socket(const char *sockName, const char *fallbackName)
{
    const char *env_sock = getenv(sockName);
    if (!env_sock) {
        LOG_MSG("Did not find env var with sock fd for %s\n", sockName);
        struct sockaddr_un file;

        memset(&file, 0, sizeof(file));
        file.sun_family = AF_UNIX;
        strlcpy(file.sun_path, fallbackName, sizeof(file.sun_path));

        LOG_MSG("Using file %s", file.sun_path);

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return fd;
        unlink(fallbackName);

        int bindret = bind(fd, (struct sockaddr *)&file, sizeof(file));
        if (bindret) {
            LOG_ERR("bind failed.  errno %d\n", errno);
            close(fd);
            fd = -1;
            return fd;
        }

        return fd;
    }

    errno = 0;
    int fd = (int)strtol(env_sock, NULL, 10);
    if (errno) {
        LOG_MSG("strtol for sock fd returned error %d\n", errno);
        return -1;
    }

    return fd;
}

int32_t main()
{
    uint8_t laVmUuid[] = {CLIENT_VMUID_HLOS};
    MinkIPC *unixhlosService = NULL;
    MinkIPC *qrtrhlosService = NULL;
    MinkIPC *unixtzdService = NULL;
    MinkIPC *modemService = NULL;
    MinkHub *hlosHub = NULL;
    Object hlosminkdEnvCred = Object_NULL;
    Object TEnvObj = Object_NULL;
    Object TEnvQRTRObj = Object_NULL;
    Object cRegObj = Object_NULL;
    int sock = -1;
    int32_t ret = 0;
    RemoteSocketAddrInfo info[] = {
#ifdef OFFTARGET
        {MINKHUB_SIMULATE, IMODULE, MINK_QRTR_SIMULATED_QTVM_NAME},
        {MINKHUB_SIMULATE, IMODULE, MINK_QRTR_SIMULATED_OEMVM_NAME},
#else
        {MINKHUB_QIPCRTR, IMODULE, MINK_QTVM_SOCKET_PORT},
        {MINKHUB_QIPCRTR, IMODULE, MINK_OEMVM_SOCKET_PORT},
        {MINKHUB_QTEE},
#endif
    };

    LOG_MSG("HLOSMinkDaemon initializing ...");
    MinkDaemon_GUARD(
        OSIndCredentials_newEnvCred("hlos", laVmUuid, NULL, "oem", 0, &hlosminkdEnvCred));
    MinkDaemon_GUARD(
        MinkHub_new(info, sizeof(info) / sizeof(RemoteSocketAddrInfo), hlosminkdEnvCred, &hlosHub));
    MinkDaemon_GUARD(TEnv_new(hlosHub, LOCAL, &TEnvObj));

#if ENABLE_QRTR_SERVER
    MinkDaemon_GUARD(TEnv_new(hlosHub, REMOTE, &TEnvQRTRObj));
    // start a server on QRTR for QTVM to connect
    qrtrhlosService = start_la_qrtr_service(&TEnvQRTRObj);
    MinkDaemon_CHECK(NULL != qrtrhlosService);
#endif  // ENABLE_QRTR_SERVER

#ifdef HLOSMINKD_TAAUTO_LOAD
    registerAutoTALoadService(TEnvObj);
#endif // HLOSMINKD_TAAUTO_LOAD

#ifdef HLOSMINKD_CLIENTS
    loadHLOSMinkdVendorClients(TEnvObj);
#endif  // HLOSMINKD_CLIENTS

#ifdef HLOSMINKD_MODEM_SERVICE
    /* Start a modem opener service on port 5013 */
    LOG_MSG("Starting modem_opener service");
    modemService = start_modem_opener_service(TEnvObj);
    MinkDaemon_CHECK(NULL != modemService);
    LOG_MSG("start_modem_opener_service is completed successfully");
#endif // HLOSMINKD_MODEM_SERVICE

#ifdef HLOSMINKD_UNIX_TZD_SERVER
    // Create CRegister Object
    MinkDaemon_GUARD(CRegister_open(hlosHub, &cRegObj));

    // Start a service on /dev/socket/ssgtzd
    sock = android_get_control_socket(SSGTZD_SOCKET_NAME, SSGTZD_FALLBACK_SOCKET_LOCATION);
    if (sock >= 0) {
        unixtzdService = MinkIPC_startServiceOnSocket(sock, cRegObj);
        if (unixtzdService) {
            // MinkIPC takes care of closing it
            sock = -1;
            LOG_MSG("Started service for connecting QTEE");
        } else {
            close(sock);
            sock = -1;
            LOG_ERR("Failed to create service");
        }
    } else {
        close(sock);
        sock = -1;
        LOG_ERR("No service socket provided");
    }
#endif // HLOSMINKD_UNIX_TZD_SERVER

    sock = android_get_control_socket(HLOSMINKD_OPENER_SOCKET_NAME,
                                      HLOSMINKD_OPENER_FALLBACK_SOCKET_NAME);
    if (sock >= 0) {
        unixhlosService = MinkIPC_startServiceModuleOnSocket(sock, TEnvObj);
        if (unixhlosService) {
            // MinkIPC takes care of closing it
            sock = -1;
            LOG_MSG("Serving HLOSMINKD interface");
            // Block waiting for service to stop
            MinkIPC_join(unixhlosService);
        } else {
            LOG_ERR("Failed to create service");
        }
    } else {
        LOG_ERR("No service socket provided");
    }

exit:
    LOG_MSG("HLOSMinkDaemon EXIT.");
    if (hlosHub) {
        MinkHub_destroy(hlosHub);
    }
    if (unixhlosService) {
        MinkIPC_release(unixhlosService);
    }
    if (qrtrhlosService) {
        MinkIPC_release(qrtrhlosService);
    }
    if (unixtzdService) {
        MinkIPC_release(unixtzdService);
    }
    if (modemService) {
        MinkIPC_release(modemService);
    }
#ifdef HLOSMINKD_CLIENTS
    unloadHLOSMinkdVendorClients();
#endif  // HLOSMINKD_CLIENTS

#ifdef HLOSMINKD_TAAUTO_LOAD
    if (ta_autoload_handle) {
        dlclose(ta_autoload_handle);
    }
#endif // HLOSMINKD_TAAUTO_LOAD

    Object_ASSIGN_NULL(TEnvObj);
    Object_ASSIGN_NULL(hlosminkdEnvCred);
    Object_ASSIGN_NULL(TEnvQRTRObj);
    Object_ASSIGN_NULL(cRegObj);

    return 0;
}
