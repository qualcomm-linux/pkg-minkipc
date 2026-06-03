// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CRegisterTABufCBO.h"
#include "CRequestTABuffer.h"
#include "IRegisterTABufCBO.h"
#include "IRequestTABuffer.h"
#include "IModule.h"
#include "MinkDaemon_logging.h"
#include "TaAutoLoad.h"

#ifdef __cplusplus
extern "C" {
#endif

static Object requestTABuffer = Object_NULL;
static Object registerTABuffer = Object_NULL;

int32_t registerService(Object hlosminkdEnvObj)
{
    int32_t ret = Object_ERROR;

    // Create a Buffer Loader Instance
    LOG_MSG("Opening CRequestTABuffer_open");
    ret = CRequestTABuffer_open(&requestTABuffer);
    if (Object_isERROR(ret) || Object_isNull(requestTABuffer)) {
        LOG_ERR("Opening CRequestTABuffer failed ret = %d", ret);
        ret = Object_ERROR;
        goto exit;
    }

    LOG_MSG("Opening CRegisterTABufCBO_UID");
    ret = IModule_open(hlosminkdEnvObj, CRegisterTABufCBO_UID, Object_NULL, &registerTABuffer);
    if (Object_isERROR(ret) || Object_isNull(registerTABuffer)) {
        LOG_ERR("Opening CRegisterTABufferCBO_UID failed ret = %d", ret);
        ret = Object_ERROR;
        goto exit;
    }

    // Register RequestTABuffer Object with QTEE Service
    LOG_MSG("Calling TAbufCBO Register");
    ret = IRegisterTABufCBO_register(registerTABuffer, requestTABuffer);
    if (Object_isERROR(ret)) {
        LOG_ERR("Calling TABufCBO Register failed ret = %d", ret);
        goto exit;
    }

    return ret;

exit:
    Object_ASSIGN_NULL(registerTABuffer);
    Object_ASSIGN_NULL(requestTABuffer);

    return ret;
}

void deregisterService()
{
    // required to release memory in QTEE side
    if (!Object_isNull(registerTABuffer)) {
        IRegisterTABufCBO_register(registerTABuffer, Object_NULL);
    }

    Object_ASSIGN_NULL(registerTABuffer);
    // Release all mmaped memory
    Object_ASSIGN_NULL(requestTABuffer);
}

#ifdef __cplusplus
}
#endif
