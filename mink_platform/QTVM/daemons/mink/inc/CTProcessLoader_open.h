// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CTPROCESSLOADER_OPEN_H
#define __CTPROCESSLOADER_OPEN_H

#include <stdint.h>
#include "object.h"

/**
 * Return an ITProcessLoader object
 */
int32_t CTProcessLoader_open(uint32_t uid, Object credentials, Object *objOut);

int32_t CTProcessLoader_openEmbedded(uint32_t cid, Object credentials, Object *objOut);

#endif /* __CTPROCESSLOADER_OPEN_H */
