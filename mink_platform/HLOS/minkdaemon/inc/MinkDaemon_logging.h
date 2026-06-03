// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __HLOSMINKD_LOGGING_H
#define __HLOSMINKD_LOGGING_H

#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "MinkDaemon"

#if defined(OFFTARGET)
#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif
#define gettid() ((pid_t)syscall(SYS_gettid))
#endif

#ifdef __ANDROID__
#include <android/log.h>
#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#else
#define LOG(xx_fmt, ...)                                 \
    {                                                    \
        printf(LOG_TAG ": " xx_fmt "\n", ##__VA_ARGS__); \
        fflush(stdout);                                  \
    }
#define LOGE LOG
#define LOGD LOG
#endif

#ifdef LOG_MSG
#undef LOG_MSG
#endif

#ifdef LOG_ERR
#undef LOG_ERR
#endif

#define LOG_MSG LOG
#define LOG_ERR LOGE

#define MinkDaemon_ERROR(error_code)                                                       \
    LOGE("(%5u:%-5u) Error %s::%d err=0x%x", getpid(), gettid(), __func__, __LINE__, (error_code));

#define MinkDaemon_GUARD(error_code)     \
    ret = (error_code);                  \
    if (!Object_isOK(ret)) {             \
        MinkDaemon_ERROR(ret);           \
        goto exit;                       \
    }

#define MinkDaemon_CHECK(cond)                                                               \
    if (!(cond)) {                                                                           \
        LOGE("(%5u:%-5u) Error %s::%d (%s)", getpid(), gettid(), __func__, __LINE__, #cond); \
        goto exit;                                                                           \
    }

#define MinkDaemon_CHECK_ERR(cond, errorCode) \
    if (!(cond)) {                            \
        MinkDaemon_ERROR(errorCode);          \
        ret = errorCode;                      \
        goto exit;                            \
    }

static inline int atomic_add(int *pn, int n)
{
    return __sync_add_and_fetch(pn, n);  // GCC builtin
}

#endif  //__HLOSMINKD_LOGGING_H
