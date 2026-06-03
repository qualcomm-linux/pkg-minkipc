// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>
#include "CAppLoader.h"
#include "IAppLoader.h"
#include "IModule.h"
#include "MinkDaemon_logging.h"
#include "heap.h"

#ifndef OFFTARGET
#include "json/json.h"
#endif

/**
 * Converts IAppLoader error to enum string
 */
char const *const app_loader_strerror(int32_t id)
{
    const int32_t error_base = 10;
    static char const *const app_loader_error_codes_types[] = {
        "IAppLoader_ERROR_INVALID_BUFFER INT32_C(10)",
        "IAppLoader_ERROR_PIL_ROLLBACK_FAILURE INT32_C(11)",
        "IAppLoader_ERROR_ELF_SIGNATURE_ERROR INT32_C(12)",
        "IAppLoader_ERROR_METADATA_INVALID INT32_C(13)",
        "IAppLoader_ERROR_MAX_NUM_APPS INT32_C(14)",
        "IAppLoader_ERROR_NO_NAME_IN_METADATA INT32_C(15)",
        "IAppLoader_ERROR_ALREADY_LOADED INT32_C(16)",
        "IAppLoader_ERROR_EMBEDDED_IMAGE_NOT_FOUND INT32_C(17)",
        "IAppLoader_ERROR_TZ_HEAP_MALLOC_FAILURE INT32_C(18)",
        "IAppLoader_ERROR_TA_APP_REGION_MALLOC_FAILURE INT32_C(19)",
        "IAppLoader_ERROR_CLIENT_CRED_PARSING_FAILURE INT32_C(20)",
        "IAppLoader_ERROR_APP_UNTRUSTED_CLIENT INT32_C(21)",
        "Not IAppLoader error code"};
    const int32_t num =
        sizeof(app_loader_error_codes_types) / sizeof(app_loader_error_codes_types[0]);
    int32_t id_fixed = id - error_base;

    // just in case if id is signed type
    if ((id_fixed >= num) || (id_fixed < 0)) {
        return app_loader_error_codes_types[num - 1];
    }

    return app_loader_error_codes_types[id_fixed];
}

static int32_t loadEmbedded(Object const &appLoader, std::string const &name, Object *appController)
{
    int32_t ret = IAppLoader_loadEmbedded(appLoader, name.c_str(), name.size(), appController);

    if (ret) {
        LOG_ERR("Load embedded app `%s` failed: %d (%s)", name.c_str(), ret,
                app_loader_strerror(ret));
    } else {
        LOG_MSG("Load embedded app %s succeeded", name.c_str());
    }

    return ret;
}

static int32_t loadApp(Object const &appLoader, std::string const &path, Object *appController)
{
    size_t size = 0;
    uint8_t retry = 0;
    size_t readBytes = 0;
    int32_t ret = Object_ERROR;
    struct stat st;
    uint8_t *buffer = NULL;
    FILE *file = NULL;

    do {
        file = fopen(path.c_str(), "r");
        if (file == NULL) {
            LOG_ERR("Failed to open file %s: %s (%d)", path.c_str(), strerror(errno), errno);
            break;
        }

        if (fstat(fileno(file), &st)) {
            LOG_ERR("Failed to stat file %s: %s (%d)", path.c_str(), strerror(errno), errno);
            break;
        }

        size = st.st_size;
        buffer = new uint8_t[st.st_size];
        if (!buffer) {
            LOG_ERR("error in buffer allocation");
            break;
        }
        readBytes = fread(buffer, 1, size, file);
        if (readBytes != size) {
            LOG_ERR("Error reading the file %s: %zu vs %zu bytes: %s (%d)", path.c_str(), readBytes,
                    size, strerror(errno), errno);
            break;
        }

        do {
            ret = IAppLoader_loadFromBuffer(appLoader, buffer, size, appController);
            if (ret) {
                LOG_ERR("Retry count %d: Load app %s failed: %d (%s)", retry, path.c_str(), ret,
                        app_loader_strerror(ret));
                retry++;
            } else {
                LOG_MSG("Load app %s succeeded", path.c_str());
                break;
            }
        } while ((ret == Object_ERROR_BUSY) && (retry < 100));

    } while (0);

    if (buffer) delete[] buffer;
    if (file) fclose(file);

    return ret;
}

#ifndef OFFTARGET
void loadApps(Object tEnvObj, std::vector<Object> &loadedApps, std::string const &config_path)
{
    std::ifstream config_file(config_path);
    Object appLoader = Object_NULL;
    bool isParse = false;
    int32_t ret = Object_ERROR;

    if (config_file.is_open()) {
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        std::string errorMessage;
        Json::Value root;
        bool isParse = Json::parseFromStream(builder, config_file, &root, &errorMessage);
        if (!isParse) {
            LOG_ERR("Unable to parse Json config %s, error %s", config_path.c_str(),
                    errorMessage.c_str());
        }

        ret = IModule_open(tEnvObj, CAppLoader_UID, Object_NULL, &appLoader);
        if (ret || Object_isNull(appLoader)) {
            LOG_ERR("Could not get object for UID = %d, ret = %d", CAppLoader_UID, ret);
        } else {
            {
                // load embedded
                const Json::Value &configs = root["embedded_ta_images"];
                for (int32_t i = 0; i < configs.size(); i++) {
                    Object appController = Object_NULL;
                    ret = loadEmbedded(appLoader, configs[i]["name"].asString(), &appController);
                    if (ret || Object_isNull(appController)) {
                        std::string taName = configs[i]["name"].asString();
                        continue;
                    }
                    loadedApps.push_back(appController);
                }
            }

            {
                // load regular TA's
                const Json::Value &configs = root["ta_images"];
                for (int32_t i = 0; i < configs.size(); i++) {
                    Object appController = Object_NULL;
                    ret = loadApp(appLoader, configs[i]["path"].asString(), &appController);
                    if (ret || Object_isNull(appController))
                        continue;
                    loadedApps.push_back(appController);
                }
                Object_ASSIGN_NULL(appLoader);
            }
        }
    } else {
        LOG_ERR("Unable to open file %s\n", config_path.c_str());
    }
}
#endif

void *loadSym(void *handle, const char *sym)
{
    if (!handle) {
        return nullptr;
    }

    void *ret_ptr = dlsym(handle, sym);
    if (!ret_ptr) {
        LOG_ERR("Failed to load sym %s error:%s\n", sym, dlerror());
    }

    return ret_ptr;
}
