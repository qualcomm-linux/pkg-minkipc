// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __BACKTRACE_H
#define __BACKTRACE_H

#include <stdint.h>
#include <sys/types.h>

/** Currently backtrace is implemented for all signals,
 * no signal choice selection possible */
#define BACKTRACE_ALL_SIGNALS 0xFFFF
#define DEMANGLE_SUCCESS 0
#define DEMANGLE_FAILURE -1

int32_t register_backtrace_handlers(uint32_t signals);

#endif  // __BACKTRACE_H
