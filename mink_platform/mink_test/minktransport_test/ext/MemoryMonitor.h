// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <unistd.h>

/* the info line num in /proc/{pid}/status file */
#define VMRSS_LINE 22

static inline int32_t getCurrentPid()
{
    return getpid();
}

static inline float getMemoryInfoByPid(int32_t pid)
{
    char fileName[64] = { 0 };
    FILE* fd;
    char lineBuff[512] = { 0 };
    snprintf(fileName, sizeof(fileName), "/proc/%d/status", pid);

    fd = fopen(fileName, "r");
    if (NULL == fd) {
        return 0;
    }

    char name[64];
    int vmrss = 0;
    for (int i = 0; i < VMRSS_LINE - 1; i++) {
        fgets(lineBuff, sizeof(lineBuff), fd);
    }

    fgets(lineBuff, sizeof(lineBuff), fd);
    sscanf(lineBuff, "%s %d", name, &vmrss);
    fclose(fd);

    /* cnvert VmRSS from KB to MB */
    return vmrss / 1024.0;
}
