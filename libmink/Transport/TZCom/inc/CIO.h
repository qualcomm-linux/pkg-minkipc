// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CIO_H
#define __CIO_H

#include <stdint.h>
#include <stddef.h>

#include "object.h"

int32_t CIO_open(const void* cred_buffer,
                  size_t cred_buffer_len,
                  Object* objOut);

#endif // __CIO_H
