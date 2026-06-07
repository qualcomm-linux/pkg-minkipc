// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <dlfcn.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <climits>
#include "CRegister_open.h"
#include "ITzdRegister_invoke.h"
#include "MinkDaemon_logging.h"
#include "MinkHub.h"
#include "TEnv.h"
#include "object.h"
#include "osIndCredentials.h"

static int32_t CRegister_getClientEnv(CRegister *me, const void *client_id, size_t client_id_len,
                                      const void *credential_buf, size_t cred_buf_len,
                                      const void *whitelist_buf, size_t whitelist_buf_len,
                                      Object *clientEnvObj)
{
    int32_t ret = Object_ERROR;
    Object realEnv = Object_NULL;
    Object procCred = Object_NULL;
    MinkHubSession *clientSession = NULL;

#ifndef OFFTARGET
    if (NULL == credential_buf || 0 == cred_buf_len) {
        LOG_ERR("No Credential buffer supplied");
        goto exit;
    }

    if (me->creds->get(client_id, client_id_len, clientEnvObj)) {
        LOG_MSG("served cached env");
        return Object_OK;
    } else if (false == me->creds->maxSystemClientConnected()) {
        MinkDaemon_GUARD(OSIndCredentials_newLASystem("system", NULL, 0, 0,
                                                         (uint8_t *)credential_buf, cred_buf_len,
                                                         "unk", 0, 0, 0, &procCred));
        MinkDaemon_GUARD(MinkHub_createSession(me->hub, &clientSession));
        MinkDaemon_CHECK(NULL != clientSession);
        MinkDaemon_GUARD(Custom_newClient(me->hub, procCred, clientSession, false,
                                             (const uint32_t *)whitelist_buf, whitelist_buf_len,
                                             &realEnv));

        if (!me->creds->add(client_id, client_id_len, credential_buf, cred_buf_len, realEnv,
                            procCred, clientEnvObj)) {
            LOG_ERR("failed to store credentials");
            goto exit;
        } else {
            LOG_MSG("served new env");
        }
        ret = Object_OK;
    } else {
        LOG_ERR("Error Max Clients Limit %zu Reached, please reconfigure configuration",
                me->creds->getNumSystemClients());
        ret = Object_ERROR_NOSLOTS;
    }

exit:
#endif
    Object_ASSIGN_NULL(procCred);
    Object_ASSIGN_NULL(realEnv);
    if (ret && clientSession) {
        MinkHub_destroySession(clientSession);
        clientSession = NULL;
    }

    return ret;
}

static int32_t CRegister_getTrustedClientEnv(CRegister *me, const void *client_id,
                                             size_t client_id_len, const void *credential_buf,
                                             size_t cred_buf_len, Object *clientEnvObj)
{
    int32_t ret = Object_ERROR;
    Object realEnv = Object_NULL;
    Object procCred = Object_NULL;
    MinkHubSession *clientSession = NULL;

#ifndef OFFTARGET
    if (NULL == credential_buf || 0 == cred_buf_len) {
        LOG_ERR("No Credential buffer supplied");
        goto exit;
    }

    MinkDaemon_GUARD(OSIndCredentials_newLAVendor("vendor", 0, 0, &procCred));
    MinkDaemon_GUARD(MinkHub_createSession(me->hub, &clientSession));
    MinkDaemon_CHECK(NULL != clientSession);
    MinkDaemon_GUARD(Custom_newClient(me->hub, procCred, clientSession, true, NULL, 0, &realEnv));

    Object_ASSIGN(*clientEnvObj, realEnv);

exit:
#endif
    Object_ASSIGN_NULL(procCred);
    Object_ASSIGN_NULL(realEnv);
    if (ret && clientSession) {
        MinkHub_destroySession(clientSession);
        clientSession = NULL;
    }

    return ret;
}

static int32_t CRegister_retain(CRegister *me)
{
    atomic_add(&me->refs, 1);
    return Object_OK;
}

static int32_t CRegister_release(CRegister *me)
{
    if (atomic_add(&me->refs, -1) == 0) {
        delete (me);
    }
    return Object_OK;
}

static ITzdRegister_DEFINE_INVOKE(ITzdRegister_invoke, CRegister_, CRegister *)

int32_t CRegister_open(MinkHub *hub, Object *objOut)
{
    CRegister *me = new CRegister((char *)CONFIG_FILE);
    if (!me) {
        LOG_ERR("CRegister memory allocation failed");
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    me->hub = hub;
    *objOut = (Object){ITzdRegister_invoke, me};

    return Object_OK;
}
