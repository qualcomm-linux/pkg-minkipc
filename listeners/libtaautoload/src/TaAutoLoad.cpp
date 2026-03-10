// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "TaAutoLoad.h"

#include "CRegisterTABufCBO.h"
#include "CRequestTABuffer.h"
#include "IRegisterTABufCBO.h"
#include "IRequestTABuffer.h"
#include "IClientEnv.h"
#include "MinkCom.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

int init(void);
void deinit(void);

static Object requestTABuffer = Object_NULL;
static Object registerTABuffer = Object_NULL;

int init(void) {

  int ret = 0;
  int32_t rv = Object_ERROR;
  Object rootObj = Object_NULL;
  Object clientEnvObj = Object_NULL;

  rv = MinkCom_getRootEnvObject(&rootObj);
  if (Object_isERROR(rv)) {
    rootObj = Object_NULL;
    MSGE("getRootEnvObject failed: 0x%x\n", rv);
    return -1;
  }

  /* Create a Buffer Loader Instance */
  MSGD("Opening CRequestTABuffer_open\n");
  rv = CRequestTABuffer_open(&requestTABuffer, rootObj);
  if (Object_isERROR(rv)) {
    requestTABuffer = Object_NULL;
    ret = -1;
    MSGE("Opening CRequestTABuffer failed: 0x%x\n", rv);
    goto err;
  }

  rv = MinkCom_getClientEnvObject(rootObj, &clientEnvObj);
  if (Object_isERROR(rv)) {
    clientEnvObj = Object_NULL;
    ret = -1;
    MSGE("getClientEnvObject failed: 0x%x\n", rv);
    goto err;
  }

  MSGD("%s ::Opening CRegisterTABufCBO_UID\n", __FUNCTION__);
  rv = IClientEnv_open(clientEnvObj, CRegisterTABufCBO_UID, &registerTABuffer);
  if (Object_isERROR(rv)) {
    registerTABuffer = Object_NULL;
    ret = -1;
    MSGE("Opening CRegisterTABufferCBO_UID failed: 0x%x\n", rv);
    goto err;
  }

  /* Register RequestTABuffer Object with QTEE Service */
  MSGD("Calling TAbufCBO Register\n");
  rv = IRegisterTABufCBO_register(registerTABuffer, requestTABuffer);
  if (Object_isERROR(rv)) {
    ret = -1;
    MSGE("Calling TABufCBO Register failed: 0x%x\n", rv);
    goto err;
  }

  Object_ASSIGN_NULL(rootObj);
  Object_ASSIGN_NULL(clientEnvObj);

  return ret;

err:
  Object_ASSIGN_NULL(registerTABuffer);
  Object_ASSIGN_NULL(clientEnvObj);
  Object_ASSIGN_NULL(requestTABuffer);
  Object_ASSIGN_NULL(rootObj);

  return ret;
}

void deinit(void) {

  /* Required to release memory on QTEE side */
  if(!Object_isNull(registerTABuffer))
  {
    IRegisterTABufCBO_register(registerTABuffer, Object_NULL);
  }

  Object_ASSIGN_NULL(registerTABuffer);
  Object_ASSIGN_NULL(requestTABuffer);
}

#ifdef __cplusplus
}
#endif
