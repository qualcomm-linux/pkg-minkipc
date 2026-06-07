// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TIMUTEX_PORT_H__
#define __TIMUTEX_PORT_H__

#include <stdint.h>
#include "TIErrno.h"
#include "object.h"
#include "pthread.h"

typedef pthread_mutex_t TIMutex;

static inline int32_t TIMutex_init(TIMutex* me)
{
    int rv = pthread_mutex_init((pthread_mutex_t*)me, NULL);
    if (rv) {
        TI_LOG_ERR(TI_MUTEX_ERR_INIT, rv);
        return Object_ERROR;
    }

    return Object_OK;
}

static inline int32_t TIMutex_lock(TIMutex* me)
{
    int rv = pthread_mutex_lock((pthread_mutex_t*)me);
    if (rv) {
        TI_LOG_ERR(TI_MUTEX_ERR_LOCK, rv);
        return Object_ERROR;
    }

    return Object_OK;
}

static inline int32_t TIMutex_unlock(TIMutex* me)
{
    int rv = pthread_mutex_unlock((pthread_mutex_t*)me);
    if (rv) {
        TI_LOG_ERR(TI_MUTEX_ERR_UNLOCK, rv);
        return Object_ERROR;
    }

    return Object_OK;
}

#endif  // __TIMUTEX_PORT_H__
