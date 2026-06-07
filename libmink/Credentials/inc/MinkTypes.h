// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _MINK_TYPES_H_
#define _MINK_TYPES_H_

/**
@file mink_types.h
@brief Provides mink common type definitions
*/

#include <stdint.h>

typedef struct {
    uint8_t val[32];
} DistId;  // Distinguished Identifier

typedef struct {
    uint8_t val[32];
} SHA256Hash;  // SHA256 hash

#endif /* _MINK_TYPES_H_ */
