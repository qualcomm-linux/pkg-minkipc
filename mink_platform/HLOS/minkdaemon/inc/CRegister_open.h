// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CREGISTER_OPEN_H
#define __CREGISTER_OPEN_H

#include <fstream>
#include <mutex>
#include "CredentialsManager.h"
#include "MinkHub.h"
#include "object.h"

#ifndef OFFTARGET
#include "json/json.h"
#endif

/**
 * Containing Index for configuration
 *
 * CONFIG_MAX_SYSTEM_CLIENTS_IDX:      Config index for Maximum configured allowed System Clients.
 * CONFIG_MAX_OBJS_PER_CLIENT_IDX:     Config index for Maximum configured objects per System Client.
 */
typedef enum {
    CONFIG_MAX_SYSTEM_CLIENTS_IDX = 0,
    CONFIG_MAX_OBJS_PER_CLIENT_IDX,
    CONFIG_MAX_SYS_OBJ_QUOTA_IDX,
} configIndex;

// Config For TZ config
#define CONFIG_FILE "/vendor/etc/ssg/ta_config.json"

struct CRegister {
    int32_t refs;
    CredentialsManager *creds;
    sysObjQuota *objQuota;
    MinkHub *hub;
    CRegister()
    {
        objQuota = (sysObjQuota::getInstance()).get();
        creds = new CredentialsManager(DEFAULT_OBJS_PER_CLIENT, DEFAULT_SYSTEM_CLIENTS,
                                       DEFAULT_SYSTEM_QUOTA);
    }

    // override
    CRegister(char *configFilePath)
    {
        std::ifstream configFile(configFilePath);
        objQuota = (sysObjQuota::getInstance()).get();
        creds = new CredentialsManager(DEFAULT_OBJS_PER_CLIENT, DEFAULT_SYSTEM_CLIENTS,
                                       DEFAULT_SYSTEM_QUOTA);
#ifndef OFFTARGET
        if (!configFile.is_open()) {
            LOG_ERR("Error:Unable to Open File %s", configFilePath);
        } else {
            Json::Value root;
            Json::CharReaderBuilder builder;
            builder["collectComments"] = false;
            std::string errorMessage;
            bool isParse = Json::parseFromStream(builder, configFile, &root, &errorMessage);
            if (!isParse) {
                LOG_ERR("Unable to parse Json config %s\n", configFilePath);
                LOG_ERR("%s\n", errorMessage.c_str());
                return;
            }
            const Json::Value& configs = root["tz_configuration"];
            if (0 == configs.size()) {
                LOG_MSG("No Configuration Object Capping ! Working with defaults");
            } else {
                for (int32_t index = 0; index < configs.size(); index++) {
                    switch (index) {
                        case CONFIG_MAX_SYSTEM_CLIENTS_IDX: {
                            creds->updateMaxSystemClients(
                                configs[CONFIG_MAX_SYSTEM_CLIENTS_IDX]["MaxSystemClients"].asInt());
                            break;
                        }
                        case CONFIG_MAX_OBJS_PER_CLIENT_IDX: {
                            creds->updateMaxObjectPerClient(
                                configs[CONFIG_MAX_OBJS_PER_CLIENT_IDX]["MaxObjectsPerClient"]
                                    .asInt());
                            break;
                        }
                        case CONFIG_MAX_SYS_OBJ_QUOTA_IDX: {
                            creds->updateMaxSystemObjQuota(
                                configs[CONFIG_MAX_SYS_OBJ_QUOTA_IDX]["MaxObjectCap"].asInt());
                            break;
                        }
                        default: {
                            LOG_MSG("Invalid Config Index");
                            break;
                        }
                    }
                }
            }
        }
#endif
    }

    ~CRegister()
    {
        LOG_MSG("Releasing Credentials");
        delete (creds);
    }
};

int32_t CRegister_open(MinkHub *hub, Object *objOut);

#endif  // __CREGISTER_OPEN_H
