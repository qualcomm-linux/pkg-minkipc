// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "rpmb_logging.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

static rpmb_log_level_t rpmb_log_level = RPMB_LOG_LEVEL_INFO;
static int rpmb_log_to_console;

void rpmb_log_init(void)
{
	const char *env;

	env = getenv("RPMB_LOG_CONSOLE");
	if (env && strcmp(env, "1") == 0)
		rpmb_log_to_console = 1;

	env = getenv("RPMB_LOG_LEVEL");
	if (env) {
		int level = atoi(env);

		if (level >= RPMB_LOG_LEVEL_ERROR &&
		    level <= RPMB_LOG_LEVEL_DEBUG)
			rpmb_log_level = level;
	}
}

void rpmb_log(rpmb_log_level_t level, const char *format, ...)
{
	char msg[512];
	char full_msg[600];
	int syslog_priority;
	va_list args;

	if (level > rpmb_log_level)
		return;

	va_start(args, format);
	vsnprintf(msg, sizeof(msg), format, args);
	va_end(args);

	snprintf(full_msg, sizeof(full_msg), "[RPMB] %s", msg);

	if (rpmb_log_to_console)
		fprintf(stderr, "%s\n", full_msg);

	switch (level) {
	case RPMB_LOG_LEVEL_ERROR:
		syslog_priority = LOG_ERR;
		break;
	case RPMB_LOG_LEVEL_WARN:
		syslog_priority = LOG_WARNING;
		break;
	case RPMB_LOG_LEVEL_DEBUG:
		syslog_priority = LOG_DEBUG;
		break;
	default:
		syslog_priority = LOG_INFO;
		break;
	}

	syslog(syslog_priority, "%s", full_msg);
}

void rpmb_set_log_level(rpmb_log_level_t level)
{
	rpmb_log_level = level;
}

rpmb_log_level_t rpmb_get_log_level(void)
{
	return rpmb_log_level;
}

void rpmb_log_cleanup(void)
{
	/* nothing to clean up when not calling openlog() */
}
