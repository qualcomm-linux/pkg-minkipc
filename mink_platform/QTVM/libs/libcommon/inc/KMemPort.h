// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __KMEM_PORT_H
#define __KMEM_PORT_H

#include <stddef.h>
#include <string.h>

static inline int tmemscmp(const void *ptrA, size_t sizeA, const void *ptrB, size_t sizeB)
{
    return memcmp(ptrA, ptrB, sizeA < sizeB ? sizeA : sizeB);
}

#endif  // __KMEM_PORT_H
