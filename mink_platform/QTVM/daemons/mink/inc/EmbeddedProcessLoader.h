// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __EMBEDDEDPROCESSLOADER_H
#define __EMBEDDEDPROCESSLOADER_H

#include <stdbool.h>
#include <stdint.h>
#include "object.h"

bool EmbeddedProcessLoader_isUIDSupported(uint32_t id);

int32_t EmbeddedProcessLoader_load(uint32_t id, Object credentials);

#endif  // __EMBEDDEDPROCESSLOADER_H
