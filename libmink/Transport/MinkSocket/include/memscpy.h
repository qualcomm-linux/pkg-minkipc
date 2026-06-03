// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __MEMSCPY_H
#define __MEMSCPY_H

#include <string.h>

static inline size_t memscpy(void *dst, size_t dst_size,
                             const void  *src, size_t src_size)
{
  size_t  copy_size = (dst_size <= src_size)? dst_size : src_size;
  memcpy(dst, src, copy_size);
  return copy_size;
}

#endif
