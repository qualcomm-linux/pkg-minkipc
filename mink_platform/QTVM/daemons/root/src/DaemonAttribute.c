// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "DaemonAttribute.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include "TUtils.h"

void DaemonAttribute_setOOMAttribute(int32_t pid, int32_t oomScoreAbj)
{
    int32_t fd = -1;
    int32_t ret = Object_OK;
    int32_t len = 0;
    char path[PATH_MAX] = {0};
    char value[6] = {0};

    snprintf(path, sizeof(path) - 1, "/proc/%d/oom_score_adj", pid);
    snprintf(value, sizeof(value), "%d", oomScoreAbj);

    T_CALL_CHECK_ERR(fd, open(path, O_WRONLY), fd > 0, Object_ERROR);
    T_CALL_CHECK_ERR(len, write(fd, value, strlen(value)), len == strlen(value), Object_ERROR);

exit:
    if (Object_isOK(ret)) {
        LOG_MSG("Set process(%d) OOM Score priority to %d successfully!", pid, oomScoreAbj);
    } else {
        LOG_MSG("Set process(%d) OOM Score priority failed. %s", pid, strerror(errno));
    }

    if (fd != -1) {
        close(fd);
    }
}