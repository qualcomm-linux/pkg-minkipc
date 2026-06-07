// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*===============================================================================================
 * FILE:        memscpy.c
 *
 * DESCRIPTION: Implementation of a secure API memscpy - Size bounded memory copy.
 *===============================================================================================*/

#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t memscpy(void *dst, size_t dst_size, const void *src, size_t src_size)
{
    size_t copy_size = (dst_size <= src_size) ? dst_size : src_size;

    if (copy_size && dst && src) {
        memcpy(dst, src, copy_size);
    }

    return copy_size;
}
