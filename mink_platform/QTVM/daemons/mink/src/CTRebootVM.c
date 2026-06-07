// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CTRebootVM.h"
#include "CTPowerService_open.h"
#include "ITPowerService.h"
#include "ITPreLauncher.h"
#include "ITRebootVM.h"
#include "ITRebootVM_invoke.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "object.h"

typedef struct {
    int32_t refs;
    Object credentials;
} TRebootVM;

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;

static Object gPreLauncherObj = Object_NULL;

static int32_t CTRebootVM_shutdown(TRebootVM *me, uint32_t restart, uint32_t force)
{
    int32_t ret = Object_ERROR;
    Object powerService = Object_NULL;
    Object wakeLock = Object_NULL;

    T_CALL(CTPowerServiceFactory_open(0, Object_NULL, &powerService));
    T_CALL(ITPowerService_acquireWakeLock(powerService, &wakeLock));
    Object_ASSIGN_NULL(powerService);

    pthread_mutex_lock(&gLock);
    T_CHECK(!Object_isNull(gPreLauncherObj));
    if (force) {
        LOG_MSG("force shutdown is disabled. Changing to unforced.");
        force = 0;
    }
    T_CALL(ITPreLauncher_shutdown(gPreLauncherObj, restart, force));

exit:
    if (ret == Object_ERROR_BUSY) {
        LOG_ERR("A process is still running!")
        ret = ITRebootVM_ERROR_CANT_SHUTDOWN_GRACEFUL;
    }
    pthread_mutex_unlock(&gLock);
    // Hold wakelock to prevent suspension during shutdown
    if (Object_isERROR(ret)) {
        Object_ASSIGN_NULL(wakeLock);
    }

    return ret;
}

static int32_t CTRebootVM_retain(TRebootVM *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t CTRebootVM_release(TRebootVM *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        Object_RELEASE_IF(me->credentials);
        HEAP_FREE_PTR(me);
    }
    return Object_OK;
}

static ITRebootVM_DEFINE_INVOKE(CTRebootVM_invoke, CTRebootVM_, TRebootVM *);

int32_t CTRebootVM_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    int32_t ret = Object_OK;
    TRebootVM *me = HEAP_ZALLOC_TYPE(TRebootVM);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    me->refs = 1;
    Object_INIT(me->credentials, credentials);

    *objOut = (Object){CTRebootVM_invoke, me};

exit:
    return ret;
}

void CTRebootVM_enable(Object prelauncherObj)
{
    pthread_mutex_lock(&gLock);
    Object_ASSIGN(gPreLauncherObj, prelauncherObj);
    pthread_mutex_unlock(&gLock);
}

void CTRebootVM_disable(void)
{
    pthread_mutex_lock(&gLock);
    Object_ASSIGN_NULL(gPreLauncherObj);
    pthread_mutex_unlock(&gLock);
}
