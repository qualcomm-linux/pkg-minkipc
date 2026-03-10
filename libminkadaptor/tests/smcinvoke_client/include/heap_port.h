// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __HEAP_PORT_H
#define __HEAP_PORT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static inline void *heap_zalloc(size_t size)
{
	void *p = malloc(size);
	if (p != NULL) {
		memset(p, 0, size);
	}
	return p;
}

static inline void *heap_calloc(size_t num, size_t size)
{
	if (num > SIZE_MAX / size) {
		return NULL;
	}
	return heap_zalloc(num * size);
}

static inline void *heap_memdup(const void *ptr, size_t size)
{
	void *p = malloc(size);
	if (p != NULL) {
		memcpy(p, ptr, size);
	}
	return p;
}

#define heap_free(x) free(x)

#endif // __HEAP_PORT_H
