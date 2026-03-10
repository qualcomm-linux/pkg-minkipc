// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __CIO_H
#define __CIO_H

#include <stdint.h>
#include <stddef.h>

#include "object.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t CIO_open(void* cred_buffer,
                 size_t cred_buffer_len,
                 Object* objOut);

#ifdef __cplusplus
}
#endif

#endif // __CIO_H
