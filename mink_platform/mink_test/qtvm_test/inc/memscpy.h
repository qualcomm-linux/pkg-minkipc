// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef __MEMSCPY_H_INTERNAL
#define __MEMSCPY_H_INTERNAL

#include <string.h>

static inline size_t memscpy(void *dst, size_t dst_size,
                             const void  *src, size_t src_size)
{
  size_t res = (dst_size <= src_size)? dst_size : src_size;
  if (dst && src && dst_size > 0 && src_size > 0) {
    memcpy(dst, src, res);
  } else {
    res = 0;
  }
  return res;
}

#endif //__MEMSCPY_H_INTERNAL