// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include "profiling.h"

#define PVMLOG_BUFFER_SIZE 256
#define VMKMSG_BUFFER_SIZE 4096

bool profiling = false;
uint64_t dataArray[NUMBER_OF_ITERATIONS] = {0};
uint64_t startTime = 0, endTime = 0, costTime = 0;
Stats analysis = {0};

uint64_t BootTimeArray[NUMBER_OF_ITERATIONS] = {0};
uint64_t VmStartTimeArray[NUMBER_OF_ITERATIONS] = {0};
uint64_t BootloaderTimeArray[NUMBER_OF_ITERATIONS] = {0};
uint64_t KernelBootTimeArray[NUMBER_OF_ITERATIONS] = {0};
uint64_t UserBootTimeArray[NUMBER_OF_ITERATIONS] = {0};

int32_t compareUint64(const void *a, const void *b)
{
    const uint64_t *x = (const uint64_t *)a;
    const uint64_t *y = (const uint64_t *)b;
    return (*x > *y) - (*x < *y);
}

int32_t processData(const uint64_t *arr, size_t size, Stats *result)
{
    int32_t ret = -1;

    if (arr == NULL || result == NULL || size < 2) {
        LOG_MSG("Invalid input parameters.");
        return ret;
    }

    double sum = 0.0;
    memset(result, 0, sizeof(*result));
    result->max = arr[0];
    result->min = arr[0];

    for (int i = 0; i < size; i++) {
        if (arr[i] > result->max) result->max = arr[i];

        if (arr[i] < result->min) result->min = arr[i];

        sum += (double)arr[i];
    }

    result->average = sum / size;

    uint64_t *sortedArr = (uint64_t *)malloc(size * sizeof(uint64_t));
    if (sortedArr == NULL) {
        LOG_MSG("Failed to malloc.");
        return ret;
    }

    memcpy(sortedArr, arr, size * sizeof(uint64_t));
    qsort(sortedArr, size, sizeof(uint64_t), compareUint64);
    if (size % 2 == 0) {
        result->median = (sortedArr[size / 2 - 1] + sortedArr[size / 2]) / 2.0;
    } else {
        result->median = sortedArr[size / 2];
    }

    double variance = 0.0;
    for (int i = 0; i < size; i++) {
        double diff = (double)arr[i] - result->average;
        variance += diff * diff;
    }

    variance /= size;
    result->stdDev = sqrt(variance);

    ret = 0;

    if (sortedArr) {
        free(sortedArr);
    }

    return ret;
}

int32_t findAndParseVmlog(const char *search_str, uint64_t *time_in_microseconds)
{
    int fd;
    char buffer[VMKMSG_BUFFER_SIZE];
    char line[VMKMSG_BUFFER_SIZE];
    int lineIndex = 0;
    ssize_t bytesRead;
    char *timestamp_str;
    char *saveptr;
    long seconds, microseconds;

    fd = open("/proc/vmkmsg", O_RDONLY);
    if (fd == -1) {
        LOG_MSG("Open /proc/vmkmsg failed");
        return -1;
    }

    while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        for (int i = 0; i < bytesRead; i++) {
            if (buffer[i] == '\n') {
                line[lineIndex] = '\0';
                lineIndex = 0;

                if (strstr(line, search_str) != NULL) {
                    if (line[0] == '[') {
                        timestamp_str = strtok_r(line, "]", &saveptr);
                        if (timestamp_str != NULL) {
                            sscanf(timestamp_str, "[%ld.%ld]", &seconds, &microseconds);
                            *time_in_microseconds = seconds * 1e6 + microseconds;
                            close(fd);
                            return 0;
                        }
                    }
                }
            } else {
                line[lineIndex++] = buffer[i];
                if (lineIndex >= VMKMSG_BUFFER_SIZE - 1) {
                    line[lineIndex] = '\0';
                    lineIndex = 0;
                }
            }
        }
    }

    close(fd);
    return -1;
}

int32_t findAndParseVmlogMultiTimes(const char *search_str, double *timestamp)
{
    int ret;
    int tryCount = 0;

    ret = findAndParseVmlog(search_str, timestamp);
    while (ret && (tryCount < MAX_RETRY_COUNT)) {
        ret = findAndParseVmlog(search_str, timestamp);
        sleep(1);
        tryCount++;
    }

    if (ret)
        LOG_MSG("Failed to find log %s in vmkmsg\n", search_str);

    return ret;
}

/* get the last timestamp for target log from PVM log */
int32_t findTvmEvent(const char *_cmd, uint64_t *time_in_microseconds)
{
    long seconds, microseconds;
    char *timestamp_str = NULL;
    char *saveptr;
    char line[PVMLOG_BUFFER_SIZE];
    char last_line[PVMLOG_BUFFER_SIZE] = "";

    FILE *fp = popen(_cmd, "r");
    if (!fp) {
        LOG_MSG("Failed to run '%s'", _cmd);
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        strlcpy(last_line, line, sizeof(last_line));
    }
    pclose(fp);

    if (strlen(last_line) > 0) {
        if (last_line[0] == '[')
            timestamp_str = strtok_r(last_line, "]", &saveptr);

        if (timestamp_str) {
            sscanf(timestamp_str, "[%ld.%ld]", &seconds, &microseconds);
            *time_in_microseconds = seconds * 1e6 + microseconds;
        } else {
            LOG_MSG("Failed to parse timestamp from '%s'\n", last_line);
            return -1;
        }
    } else {
        LOG_MSG("No matching log found for '%s'.\n", _cmd);
        return -1;
    }

    return 0;
}

int32_t getBootupTime(int index)
{
    int32_t ret = -1;
    uint64_t bootup_exec_timestamp;
    uint64_t shutdown_exec_timestamp;
    uint64_t timestamp_vm_exit = 0;
    uint64_t timestamp_vm_start;
    uint64_t timestamp_vm_boottokernel;
    uint64_t timestamp_vm_boottouser;
    uint64_t timestamp_mink;
    const char *search_bootupkernel = "Booting Linux";
    const char *search_boottouser = "Run /init as init process";
    const char *search_mink_ready = "Mink Startup Completed";
    int tryCount = 0;
    FILE *fp = NULL;
    struct timespec current_timestamp;

    clock_gettime(CLOCK_MONOTONIC, &current_timestamp);
    shutdown_exec_timestamp = current_timestamp.tv_sec * 1e6 + current_timestamp.tv_nsec / 1e3;

    sleep(2);
    // get the the timestamp for VM exit
    while (findTvmEvent("dmesg | grep 'vm(45) exited'", &timestamp_vm_exit) ||
           timestamp_vm_exit < shutdown_exec_timestamp) {
        if (tryCount > MAX_RETRY_COUNT) {
            LOG_MSG("Failed to get vm exit status, QTVM exec shutdown at %.6f \n",
                    shutdown_exec_timestamp / 1e6);
            goto exit;
        }

        LOG_MSG("Failed to get vm exit status, retry\n");
        sleep(5);
        tryCount++;
    }
    tryCount = 0;

    // TVM bootup
    sleep(3);
    clock_gettime(CLOCK_MONOTONIC, &current_timestamp);
    bootup_exec_timestamp = current_timestamp.tv_sec * 1e6 + current_timestamp.tv_nsec / 1e3;

    fp = popen(BOOTVM_COMMAND, "r");
    if (fp == NULL) {
        LOG_MSG("Failed to bootup tuivm");
        goto exit;
    }

    sleep(1);
    while (findTvmEvent("dmesg | grep 'vm(45) started running'", &timestamp_vm_start) ||
           timestamp_vm_start < bootup_exec_timestamp) {
        if (tryCount > MAX_RETRY_COUNT) {
            LOG_MSG("Failed to get vm start status, QTVM exec bootup at %.6f \n",
                    bootup_exec_timestamp / 1e6);
            goto exit;
        }

        LOG_MSG("Failed to get vm start status, retry\n");
        sleep(5);
        tryCount++;
    }

    if (findAndParseVmlogMultiTimes(search_bootupkernel, &timestamp_vm_boottokernel))
        goto exit;

    if (findAndParseVmlogMultiTimes(search_boottouser, &timestamp_vm_boottouser))
        goto exit;

    if (findAndParseVmlogMultiTimes(search_mink_ready, &timestamp_mink))
        goto exit;

    BootTimeArray[index] = timestamp_mink - bootup_exec_timestamp;
    VmStartTimeArray[index] = timestamp_vm_start - bootup_exec_timestamp;
    BootloaderTimeArray[index] = timestamp_vm_boottokernel - timestamp_vm_start;
    KernelBootTimeArray[index] = timestamp_vm_boottouser - timestamp_vm_boottokernel;
    UserBootTimeArray[index] = timestamp_mink - timestamp_vm_boottouser;

    ret = 0;

exit:
    if (fp != NULL)
        pclose(fp);

    return ret;
}

int32_t showBootupTime()
{
    Stats res = {0};

    if (processData(BootTimeArray, NUMBER_OF_REBOOT_ITERATION, &res))
        return -1;

    memset(BootTimeArray, 0, sizeof(BootTimeArray));
    LOG_MSG("Boot time(ms): average: %.6f, max: %.6f, median: %.6f, min: %.6f, stdev: %.6f",
            res.average / 1e3, res.max / 1e3, res.median / 1e3, res.min / 1e3, res.stdDev / 1e3);

    if (processData(VmStartTimeArray, NUMBER_OF_REBOOT_ITERATION, &res))
        return -1;

    memset(VmStartTimeArray, 0, sizeof(VmStartTimeArray));
    LOG_MSG("VM Start time(ms): average: %.6f, max: %.6f, median: %.6f, min: %.6f, stdev: %.6f",
            res.average / 1e3, res.max / 1e3, res.median / 1e3, res.min / 1e3, res.stdDev / 1e3);

    if (processData(BootloaderTimeArray, NUMBER_OF_REBOOT_ITERATION, &res))
        return -1;

    memset(BootloaderTimeArray, 0, sizeof(BootloaderTimeArray));
    LOG_MSG("Boot loader time(ms): average: %.6f, max: %.6f, median: %.6f, min: %.6f, stdev: %.6f",
            res.average / 1e3, res.max / 1e3, res.median / 1e3, res.min / 1e3, res.stdDev / 1e3);

    if(processData(KernelBootTimeArray, NUMBER_OF_REBOOT_ITERATION, &res))
        return -1;

    memset(KernelBootTimeArray, 0, sizeof(KernelBootTimeArray));
    LOG_MSG("Kernel Boot time(ms): average: %.6f, max: %.6f, median: %.6f, min: %.6f, stdev: %.6f",
            res.average / 1e3, res.max / 1e3, res.median / 1e3, res.min / 1e3, res.stdDev / 1e3);

    if (processData(UserBootTimeArray, NUMBER_OF_REBOOT_ITERATION, &res))
        return -1;

    memset(UserBootTimeArray, 0, sizeof(UserBootTimeArray));
    LOG_MSG("User Boot time(ms): average: %.6f, max: %.6f, median: %.6f, min: %.6f, stdev: %.6f",
            res.average / 1e3, res.max / 1e3, res.median / 1e3, res.min / 1e3, res.stdDev / 1e3);

    return 0;
}
