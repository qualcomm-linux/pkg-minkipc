// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CAutoStartManager.h"
#include "EmbeddedProcessIDs.h"
#include "EmbeddedProcessLoader.h"
#include "ErrorMap.h"
#include "ITAutoStartManager_invoke.h"
#include "ITProcessLoader.h"
#include "ServiceCenter.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "object.h"

#define MAX_RETRY_ATTEMPTS 3

typedef struct {
    int32_t refs;
} TAutoStartManager;

static TAutoStartManager gAutoStartManager;
static uint32_t *gAutoStartList;
static uint32_t gAutoStartListCount = 0;

static int32_t _remapLoaderError(int32_t error)
{
    return (error == ITProcessLoader_ERROR_PROC_ALREADY_LOADED) ? Object_OK : error;
}

static int32_t _startCoreService(uint32_t uid)
{
    int32_t ret = Object_OK;
    int32_t retry = MAX_RETRY_ATTEMPTS;

    while (retry > 0) {
        T_CALL_NO_CHECK(ret, EmbeddedProcessLoader_load(uid, Object_NULL));
        if (Object_isOK(ret = _remapLoaderError(ret))) {
            break;
        }

        --retry;
        LOG_MSG("Service Loading Failed(%d): retries remaining = %d", ret, retry);
    }

    return ret;
}

static void *_reStartCoreService(void *arg)
{
    uint32_t *uid = (uint32_t *)arg;
    int32_t ret;

    ret = _startCoreService(*uid);

    LOG_MSG("Restart AutoService(%x) %s(%d)!", *uid, Object_isOK(ret) ? "Success" : "Fail", ret);
    return NULL;
}

static int32_t CTAutoStartManager_notify(TAutoStartManager *me, uint32_t cid)
{
    uint32_t uid;
    int32_t ret = Object_OK;
    int32_t i;

    for (i = 0; i < embeddedProcessIDCount; ++i) {
        if (embeddedProcessIDList[i].cid == cid) {
            uid = embeddedProcessIDList[i].uid;
            break;
        }
    }

    if (i != embeddedProcessIDCount) {
        for (i = 0; i < gAutoStartListCount; ++i) {
            if (uid == gAutoStartList[i]) {
                pthread_t thread;
                LOG_MSG("AutoStart Service(%x) has died. Begin to restart it.", gAutoStartList[i]);
                T_GUARD(pthread_create(&thread, NULL, _reStartCoreService, &gAutoStartList[i]));
                pthread_detach(thread);
                break;
            }
        }
    }

exit:
    return Object_OK;
}

static int32_t CTAutoStartManager_release(TAutoStartManager *me)
{
    atomicAdd(&me->refs, -1);
    return Object_OK;
}

static int32_t CTAutoStartManager_retain(TAutoStartManager *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static ITAutoStartManager_DEFINE_INVOKE(CTAutoStartManager_invoke, CTAutoStartManager_,
                                        TAutoStartManager *);

int32_t CTAutoStartManager_open(Object *objOut)
{
    CTAutoStartManager_retain(&gAutoStartManager);
    *objOut = (Object){CTAutoStartManager_invoke, &gAutoStartManager};

    return Object_OK;
}

void CTAutoStartManager_startService(void)
{
    int32_t ret;

    gAutoStartListCount = ServiceCenter_getAutoStartServiceCount();
    T_CHECK_ERR(gAutoStartListCount >= 0, Object_ERROR_INVALID);

    HEAP_FREE_PTR_IF(gAutoStartList);
    gAutoStartList = HEAP_ZALLOC_ARRAY(uint32_t, gAutoStartListCount);
    T_GUARD(ServiceCenter_getAutoStartServiceList(gAutoStartList, &gAutoStartListCount));

    for (int32_t i = 0; i < gAutoStartListCount; ++i) {
        T_CALL_NO_CHECK(ret, _startCoreService(gAutoStartList[i]));
        LOG_MSG("Start AutoService(%x) %s(%d)!", gAutoStartList[i],
                Object_isOK(ret) ? "Success" : "Fail", ret);
    }

exit:
    return;
}
