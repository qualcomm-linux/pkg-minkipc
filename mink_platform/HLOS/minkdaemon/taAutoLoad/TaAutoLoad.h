// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TAAUTOLOAD_H
#define __TAAUTOLOAD_H

#include <object.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Init and register for TA load
 *
 * param[in]        hlosminkdEnvObj                TEnv Object from HLOS Mink Daemon
 *
 * return Object_OK if successful
 */
int32_t registerService(Object hlosminkdEnvObj);

void deregisterService();

#ifdef __cplusplus
}
#endif

#endif // __TAAUTOLOAD_H
