// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * RPMB Logging Interface - Production-Ready Syslog-based Logging
 */

#ifndef __RPMB_LOGGING_H__
#define __RPMB_LOGGING_H__

#include <syslog.h>

/* Log levels mapped to syslog priorities */
typedef enum {
	RPMB_LOG_LEVEL_ERROR = LOG_ERR, /* 3 - Error conditions */
	RPMB_LOG_LEVEL_WARN = LOG_WARNING, /* 4 - Warning conditions */
	RPMB_LOG_LEVEL_INFO = LOG_INFO, /* 6 - Informational messages */
	RPMB_LOG_LEVEL_DEBUG = LOG_DEBUG /* 7 - Debug-level messages */
} rpmb_log_level_t;

/*
 * Define RPMB_DEBUG_CONSOLE_OUTPUT at compile time to mirror log output
 * to stderr in addition to syslog.  Production builds must not define it.
 */
#ifdef RPMB_DEBUG_CONSOLE_OUTPUT
#define RPMB_LOG_ERROR(fmt, ...) printf("RPMB_ERROR: " fmt, ##__VA_ARGS__)
#define RPMB_LOG_WARN(fmt, ...) printf("RPMB_WARN: " fmt, ##__VA_ARGS__)
#define RPMB_LOG_INFO(fmt, ...) printf("RPMB_INFO: " fmt, ##__VA_ARGS__)
#define RPMB_LOG_DEBUG(fmt, ...) printf("RPMB_DEBUG: " fmt, ##__VA_ARGS__)
#else
#define RPMB_LOG_ERROR(fmt, ...) \
	rpmb_log(RPMB_LOG_LEVEL_ERROR, "RPMB_ERROR: " fmt, ##__VA_ARGS__)
#define RPMB_LOG_WARN(fmt, ...) \
	rpmb_log(RPMB_LOG_LEVEL_WARN, "RPMB_WARN: " fmt, ##__VA_ARGS__)
#define RPMB_LOG_INFO(fmt, ...) \
	rpmb_log(RPMB_LOG_LEVEL_INFO, "RPMB_INFO: " fmt, ##__VA_ARGS__)
#define RPMB_LOG_DEBUG(fmt, ...) \
	rpmb_log(RPMB_LOG_LEVEL_DEBUG, "RPMB_DEBUG: " fmt, ##__VA_ARGS__)
#endif /* RPMB_DEBUG_CONSOLE_OUTPUT */

/* rpmb_log_init - initialise the logging subsystem */
void rpmb_log_init(void);

/* rpmb_log_cleanup - clean up the logging subsystem */
void rpmb_log_cleanup(void);

/* rpmb_log - emit a log message at the given level */
void rpmb_log(rpmb_log_level_t level, const char *format, ...);

/* rpmb_set_log_level - change the active log level */
void rpmb_set_log_level(rpmb_log_level_t level);

/* rpmb_get_log_level - return the active log level */
rpmb_log_level_t rpmb_get_log_level(void);

#endif /* __RPMB_LOGGING_H__ */
