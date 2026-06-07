// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include "object.h"

/** Return an object implementing the ICredentials interface, populated
 * with the passed VMUUID.
 * */
int32_t VMCredentials_open(uint8_t const *vmuuid, size_t vmuuidLen, Object *objOut);
