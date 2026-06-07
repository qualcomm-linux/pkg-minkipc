// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __HEAP_H
#define __HEAP_H

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
static inline void *zalloc(size_t size) {
  void *ptr = malloc(size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static inline void *zcalloc(size_t size, size_t num) {
  void *ptr = calloc(size, num);
  if (ptr) {
    memset(ptr, 0, size * num);
  }
  return ptr;
}
#ifdef __cplusplus
}
#endif


#define HEAP_ZALLOC(size)         (zalloc((size)))

#define HEAP_ZALLOC_TYPE(type)      ((type *) zalloc(sizeof(type)))

#define HEAP_ZALLOC_ARRAY(type, k)  ((type *) zcalloc((k), sizeof(type)))

#define HEAP_FREE_PTR(var)        ((void) (free(var), (var) = NULL))

// Older convention:
#define HEAP_ZALLOC_REC(type)       HEAP_ZALLOC_TYPE(type)

#define HEAP_FREE_PTR_IF(var)                                        \
  do { if ((var) != NULL) { HEAP_FREE_PTR(var); } } while(0)

#endif // __HEAP_H
