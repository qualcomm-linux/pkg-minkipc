// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <stdint.h>
#include <stddef.h>
#include "object.h"

typedef struct {
	int refs;
	size_t counter;
	int32_t op;
	int32_t retValue;
	int32_t retValueError;
	Object oArg;
	Object oOArg;
	Object oOOArg;
	Object oOOArg0;
	Object oOOArg1;
	Object oOOArg2;
	void *bArg_ptr;
	size_t bArg_len;
	size_t responseCounter;
} TestCallable;

/* Macro for validating memory */
#define VERIFY_MEM(x)                                                   \
  do {                                                                  \
    if (x == NULL) {                                                    \
      LOGE_PRINT("Invalid memory pointer 0x%p",&x);                     \
      return Object_ERROR;                                              \
    }                                                                   \
  } while(0)
