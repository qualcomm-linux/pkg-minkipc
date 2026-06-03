// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "MinkHubUtils.h"
#include <pthread.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "libMinkHub"
#define LOG(buf) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf)
#else
#include <stdio.h>
#include <unistd.h>
#define LOG(buf) (void)write(STDOUT_FILENO, buf, strlen(buf))
#endif

#define MAX_FUNCNAME_LEN 100
#define MAX_COMBINED_LOG_MSG_LEN 1024
static char gFuncName[MAX_FUNCNAME_LEN] = {0};
static char gLogBuf[MAX_COMBINED_LOG_MSG_LEN];
static pthread_mutex_t gPrintLock = PTHREAD_MUTEX_INITIALIZER;

void MINKHUB_printLogWithTail(uint32_t pid, uint32_t tid, const char *fulFunc, bool addFail,
                              int32_t rv)
{
    const char *fulFuncPos = fulFunc;
    const char *printFmt = "(%5u:%-5u) Trace %s %s ---> %s\n";
    int32_t index = 0;

    pthread_mutex_lock(&gPrintLock);
    while (index < (MAX_FUNCNAME_LEN - 1) && *fulFuncPos != '\0' && *fulFuncPos != '(') {
        gFuncName[index++] = *fulFuncPos;
        ++fulFuncPos;
    }

    gFuncName[index] = '\0';

    if (addFail) {
        /* (proc_id:thread_id) func_name ---> "failed : rv" */
        printFmt = "(%5u:%-5u) Trace %s ---> failed : %d\n";
        snprintf(gLogBuf, MAX_COMBINED_LOG_MSG_LEN - 1, printFmt, pid, tid, gFuncName, rv);
    } else {
        /* (proc_id:thread_id) func_name  */
        printFmt = "(%5u:%-5u) Trace %s\n";
        snprintf(gLogBuf, MAX_COMBINED_LOG_MSG_LEN - 1, printFmt, pid, tid, gFuncName);
    }

    LOG(gLogBuf);
    pthread_mutex_unlock(&gPrintLock);
}

void MINKHUB_printLog(const char *fmt, ...)
{
    va_list args;
    pthread_mutex_lock(&gPrintLock);
    va_start(args, fmt);
    vsnprintf(gLogBuf, sizeof(gLogBuf) - 1, fmt, args);
    va_end(args);
    LOG(gLogBuf);
    pthread_mutex_unlock(&gPrintLock);
}
