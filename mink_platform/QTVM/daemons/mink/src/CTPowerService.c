// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "ITPowerService.h"
#include "ITPowerService_invoke.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "TUtils.h"
#include "object.h"

typedef struct {
    int32_t refs;
} TPowerService;

typedef struct {
    int32_t refs;
} TWakeLock;

#define TVMD_STR "tvmd"
#define TIME_OUT "500000000"  // 500ms
#define ACQUIRE_WAKE_LOCK (TVMD_STR)
// Keep the VM wake up for 500ms after releasing wake lock to make sure minksocket can reply the
// message before VM suspend.
#define RELEASE_WAKE_LOCK (TVMD_STR " " TIME_OUT)

#ifndef OFFTARGET
#define WAKE_LOCK_FILE "/sys/power/wake_lock"
#else
#define WAKE_LOCK_FILE "wake_lock"
#endif

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;
static TPowerService gPowerServiceFactory;
static TWakeLock gWakeLock;

static int32_t _wakeLockOperation(const char *content)
{
    int32_t ret = Object_OK;
#ifndef OFFTARGET
    int32_t fd = open(WAKE_LOCK_FILE, O_WRONLY | O_APPEND);
#else
    int32_t fd = open(WAKE_LOCK_FILE, O_RDWR | O_CREAT, 0777);
#endif
    T_CHECK(fd >= 0);

    T_CHECK(write(fd, content, strlen(content) + 1) == (strlen(content) + 1));

exit:
    if (fd >= 0) {
        close(fd);
    }

    if (Object_isERROR(ret)) {
        LOG_MSG("Operate file %s, Error : %s ", WAKE_LOCK_FILE, strerror(errno));
    }

    return ret;
}

static int32_t _acquireWakeLock(void)
{
    return _wakeLockOperation(ACQUIRE_WAKE_LOCK);
}

static int32_t _releaseWakeLock(void)
{
    return _wakeLockOperation(RELEASE_WAKE_LOCK);
}

static int32_t CTWakeLock_retain(TWakeLock *me)
{
    int32_t ret = Object_OK;

    // Avoid the second service B get Object_OK before the first service A finished call
    // ACQUIRE_WAKE_LOCK. We add mutex to guarantee the order of execution.
    pthread_mutex_lock(&gLock);
    if (me->refs == 0) {
        T_CALL_ERR(_acquireWakeLock(), ITPowerService_ERROR_GET_WAKELOCK);
    }

    ++me->refs;

exit:
    pthread_mutex_unlock(&gLock);
    return ret;
}

static int32_t CTWakeLock_release(TWakeLock *me)
{
    // Avoid releasing and acquiring wakelock happen in the meanwhile, we add mutex lock to
    // guarantee these two operations are mutually exclusive.
    pthread_mutex_lock(&gLock);
    if (atomicAdd(&me->refs, -1) == 0) {
        T_TRACE(_releaseWakeLock());
    }

    pthread_mutex_unlock(&gLock);
    return Object_OK;
}

static ITWakeLock_DEFINE_INVOKE(CTWakeLock_invoke, CTWakeLock_, TWakeLock *);

static int32_t CTPowerService_acquireWakeLock(TPowerService *me, Object *wakeLockObj)
{
    int32_t ret = Object_OK;

    T_GUARD(CTWakeLock_retain(&gWakeLock));

    *wakeLockObj = (Object){CTWakeLock_invoke, &gWakeLock};

exit:
    return ret;
}

static int32_t CTPowerService_retain(TPowerService *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t CTPowerService_release(TPowerService *me)
{
    atomicAdd(&me->refs, -1);
    return Object_OK;
}

static ITPowerService_DEFINE_INVOKE(CTPowerService_invoke, CTPowerService_, TPowerService *);

int32_t CTPowerServiceFactory_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    (void)credentials;

    CTPowerService_retain(&gPowerServiceFactory);
    *objOut = (Object){CTPowerService_invoke, &gPowerServiceFactory};
    return Object_OK;
}