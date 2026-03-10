// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#ifdef OE
#include <syslog.h>
#define MSGV(...) syslog(LOG_NOTICE, "INFO:" __VA_ARGS__)
#define MSGD(...) syslog(LOG_DEBUG, "INFO:" __VA_ARGS__)
#define MSGE(...) syslog(LOG_ERR, "ERR:" __VA_ARGS__)
#else
#include <stdio.h>
#define MSGV printf
#define MSGD(...)
#define MSGE printf
#endif

#define LOGV_PRINT(fmt, ...)                                            \
  do {                                                                  \
    MSGV("[%s:%u] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
  } while(0)

#define LOGD_PRINT(fmt, ...)                                            \
  do {                                                                  \
    MSGD("[%s:%u] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
  } while(0)

#define LOGE_PRINT(fmt, ...)                                            \
  do {                                                                  \
    MSGE("[%s:%u] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
  } while(0)

/* Macros for testing */
#define TEST_OK(xx)                                                     \
  do                                                                    \
  {                                                                     \
    if ((xx))                                                           \
    {                                                                   \
      LOGE_PRINT("Failed!\n");                                          \
      exit(-1);                                                         \
    }                                                                   \
    LOGD_PRINT("Passed\n");                                             \
  } while(0)

#define SILENT_OK(xx)                                                   \
  do                                                                    \
  {                                                                     \
    if ((xx))                                                           \
    {                                                                   \
      LOGE_PRINT("Failed!\n");                                          \
      exit(-1);                                                         \
    }                                                                   \
  } while(0)

#define TEST_FAIL(xx)     TEST_OK(!(xx))
#define TEST_FALSE(xx)    TEST_OK(xx)
#define TEST_TRUE(xx)     TEST_FAIL(xx)

#define SILENT_FAIL(xx)   SILENT_OK(!(xx))
#define SILENT_FALSE(xx)  SILENT_OK(xx)
#define SILENT_TRUE(xx)   SILENT_FAIL(xx)
