// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CRYPTSETUP_H
#define __CRYPTSETUP_H

#include <stdint.h>
#include "object.h"

int32_t cryptsetup_smcinvoke_setup(Object *appObject, uint32_t UID);
int32_t cryptsetup_fetch_key(uint8_t **derived_key);
int32_t run_cryptsetup(const char* command, uint8_t *derived_key);
int32_t do_cryptsetup();

#endif  // __CRYPTSETUP_H
