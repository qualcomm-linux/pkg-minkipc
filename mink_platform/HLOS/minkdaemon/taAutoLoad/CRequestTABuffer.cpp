// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear+

#include "CRequestTABuffer.h"
#include "IRequestTABuffer.h"
#include "IRequestTABuffer_invoke.h"
#include "MinkDaemon_logging.h"
#include "TaImageReader.h"
#include "TZCom.h"

using namespace std;

// TEEC UUID STRING
static const size_t HYPHEN_LEN(4);
static const size_t NULLCHAR_LEN(1);
static const size_t STRING_MULTIPLIER(2);

/**
 * Create CRequestTABBuffer and parse JSON config file for ta store paths.
 *
 * param[in]    configFilePath       JSON config file path
 *
 * return ptr to CRequestTABuffer
 */
static CRequestTABuffer *getCRequestTABuffer(const char *configFilePath)
{
    CRequestTABuffer *requestTABuff = nullptr;
    bool isParse = false;
    ifstream configFile(configFilePath);

    // check if stream was opened
    if (!configFile.is_open()) {
        LOG_ERR("Error:Unable to Open File");
    } else {
        // parsing JSON File
        Json::Value root;
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        string errorMessage;
        isParse = Json::parseFromStream(builder, configFile, &root, &errorMessage);
        if (!isParse) {
            LOG_ERR("Unable to parse Json config ");
            LOG_ERR("Error %s", errorMessage.c_str());
        } else {
            if (!(root.isMember("ta_paths") && root["ta_paths"].isArray())) {
                LOG_ERR("Error:Can't find ta_paths");
                return nullptr;
            }

            requestTABuff = new CRequestTABuffer();
            if (requestTABuff == nullptr) {
                LOG_ERR("Error:Unable to alloc memory");
                return nullptr;
            }

            // parse config File
            const Json::Value &configs = root["ta_paths"];
            for (int32_t i = 0; i < configs.size(); i++) {
                string taPaths = configs[i]["path"].asString();
                // append / in case not present at the end
                if ('/' != taPaths.at(taPaths.length() - 1)) {
                    taPaths = taPaths + string("/");
                }
                requestTABuff->searchLocations.push_back(taPaths);
                LOG_MSG("Path %s", taPaths.c_str());
            }
        }
    }

    return requestTABuff;
}

static int32_t CRequestTABuffer_retain(CRequestTABuffer *me)
{
    atomic_add(&me->refs, 1);
    return Object_OK;
}

static int32_t CRequestTABuffer_release(CRequestTABuffer *me)
{
    if (atomic_add(&me->refs, -1) == 0) {
        delete (me);
    }
    return Object_OK;
}

/**
 * Get the TA image assoicated with the UUID.
 *
 * param[in]    me            Local Object associated with this interface
 * param[in]    uuid_ptr      UID for requested TA
 * param[in]    uuid_len      This length of UID for requested TA
 * param[out]   appElf        Buffer with TA Image
 *
 * return Object_OK if successful
 */
int32_t CRequestTABuffer_get(CRequestTABuffer *me, const void *uuid_ptr, size_t uuid_len,
                             Object *appElf)
{
    int32_t ret = Object_ERROR;
    char *distName = NULL;
    size_t distNameLen = 0;
    TAImageReader *TAImage = nullptr;
    TEEC_UUID *pTargetUUID = NULL;

    if (NULL == uuid_ptr || uuid_len != sizeof(TEEC_UUID)) {
        LOG_ERR("Invalid UUID Len");
        goto exit;
    }

    distNameLen = sizeof(TEEC_UUID) * STRING_MULTIPLIER + HYPHEN_LEN + NULLCHAR_LEN;
    pTargetUUID = (TEEC_UUID *)uuid_ptr;
    distName = new (std::nothrow) char[distNameLen];
    if (distName == nullptr) {
        LOG_ERR("Memory allocation failed for distName");
        goto exit;
    }

    snprintf(distName, distNameLen,
             "%08X-%04X-%04X-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
             pTargetUUID->timeLow, pTargetUUID->timeMid, pTargetUUID->timeHiAndVersion,
             pTargetUUID->clockSeqAndNode[0], pTargetUUID->clockSeqAndNode[1],
             pTargetUUID->clockSeqAndNode[2], pTargetUUID->clockSeqAndNode[3],
             pTargetUUID->clockSeqAndNode[4], pTargetUUID->clockSeqAndNode[5],
             pTargetUUID->clockSeqAndNode[6], pTargetUUID->clockSeqAndNode[7]);

    LOG_MSG("UUID Name %s", distName);
    // Create a New Image Object
    if (taImageStatus::kErrOk !=
        TAImageReader::createTAImageReader(me->searchLocations, string(distName), &TAImage)) {
        goto exit;
    }

#ifndef ENABLE_UNIT_TEST
    if (TZCom_getFdObject(dup(TAImage->getFileDescriptor()), appElf)) {
        // Failed to get FD Object
        LOG_ERR("Cannot Get FD Object");
        *appElf = Object_NULL;
        goto exit;
    }
#endif
    ret = Object_OK;

exit:
    if (NULL != distName) {
        delete[](distName);
    }
    // Ummap the Image after handling the MemObj in case of success
    if (nullptr != TAImage) {
        delete (TAImage);
    }

    return ret;
}

static IRequestTABuffer_DEFINE_INVOKE(IRequestTABuffer_invoke, CRequestTABuffer_,
                                      CRequestTABuffer *)

/**
 * Create a Callback Buffer Object for loading TA.
 *
 * param[out]   requestTABufferObj        Buffer returned to Client
 *
 * return Object_OK if successful
 */
int32_t CRequestTABuffer_open(Object *requestTABufferObj)
{
    CRequestTABuffer *me = getCRequestTABuffer(config_path);
    if (me == nullptr) {
        LOG_ERR("Failed to parse Configuration");
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    *requestTABufferObj = (Object){IRequestTABuffer_invoke, me};

    return Object_OK;
}
