// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _SAMPLECA_PROFILING_H_
#define _SAMPLECA_PROFILING_H_

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "TUtils.h"

#define BOOTVM_COMMAND "/vendor/bin/test_vm_client_cpp --oneshot &"
#define MAX_RETRY_COUNT 10
#define NUMBER_OF_ITERATIONS 10
#define NUMBER_OF_REBOOT_ITERATION 5

typedef struct {
    uint64_t max;
    uint64_t min;
    double average;
    double median;
    double stdDev;
} Stats;

extern bool profiling;
extern uint64_t dataArray[NUMBER_OF_ITERATIONS];
extern uint64_t startTime, endTime, costTime;
extern Stats analysis;

int32_t showBootupTime();
int32_t getBootupTime(int index);
int32_t compareUint64(const void *a, const void *b);
int32_t processData(const uint64_t *arr, size_t size, Stats *result);

#endif /* _SAMPLECA_PROFILING_H_ */
