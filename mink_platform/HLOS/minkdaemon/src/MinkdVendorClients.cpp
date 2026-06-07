// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <vector>
#include "MinkDaemon_logging.h"
#include "TaLoader.h"

#ifdef __cplusplus
extern "C" {
#endif
// Noted: Function definition originates from SecureChannelMain.h and qwesd_lib.h
extern int Init_qwes_common(void);
extern int DeInit_qwes_common(void);
extern int SecureChannelInit(void);
extern int SecureChannelDeInit(void);
extern int Init_qwes(void);
extern int DeInit_qwes(void);
extern int Init_setflag(void);
extern  int DeInit_setflag(void);
#ifdef __cplusplus
}
#endif


#ifndef OFFTARGET
typedef struct ssgtzd_client_ops {
    int32_t (*pfun_init)(void);
    int32_t (*pfun_deinit)(void);
    bool init_success;
    const char *pClientName;
} ssgtzd_client_ops_t;

static ssgtzd_client_ops_t static_client_ops[] = {
    {&Init_qwes_common, &DeInit_qwes_common, true, "QWES_COMMON"},
    {&SecureChannelInit, &SecureChannelDeInit, true, "SECURE_CHANNEL"},
    {&Init_qwes, &DeInit_qwes, true, "QWES"},
    {&Init_setflag, &DeInit_setflag, true, "QWES_COMMON"}};

static std::vector<Object> loadedApps;

void unloadHLOSMinkdVendorClients()
{
    int32_t ret = 0;
    uint16_t num_static_clients = 0;

    for (Object object : loadedApps) {
        Object_ASSIGN_NULL(object);
    }

    num_static_clients = sizeof(static_client_ops) / sizeof(ssgtzd_client_ops_t);
    while (num_static_clients) {
        // To use as index for static_client_ops
        num_static_clients--;
        if (static_client_ops[num_static_clients].init_success) {
            ret = static_client_ops[num_static_clients].pfun_deinit();
            if (0 != ret) {
                // In error case we simply log it and continue
                LOG_ERR("DeInit failed for client:%s ret:%d\n",
                        static_client_ops[num_static_clients].pClientName, ret);
            }
        }
    }
}

void loadHLOSMinkdVendorClients(Object tEnvObj)
{
    int32_t ret = 0;
    uint16_t num_static_clients = 0;
    uint16_t client_ops_index = 0;

    loadApps(tEnvObj, loadedApps, "/vendor/etc/ssg/ta_config.json");
    num_static_clients = sizeof(static_client_ops) / sizeof(ssgtzd_client_ops_t);
    for (client_ops_index = 0; client_ops_index < num_static_clients; client_ops_index++) {
        ret = static_client_ops[client_ops_index].pfun_init();
        if (0 != ret) {
            // In error case we simply log it and continue
            LOG_ERR("Init failed for client:%s ret:%d\n",
                    static_client_ops[client_ops_index].pClientName, ret);
            static_client_ops[client_ops_index].init_success = false;
        }
    }
}
#endif
