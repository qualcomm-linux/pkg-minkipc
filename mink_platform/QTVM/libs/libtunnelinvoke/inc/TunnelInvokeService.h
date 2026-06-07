// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TUNNEL_INVOKE_SERVICE_H__
#define __TUNNEL_INVOKE_SERVICE_H__

#include <stddef.h>
#include <stdint.h>
#include "object.h"

int32_t TunnelInvokeService_init(Object opener);

int32_t TunnelInvokeService_deinit(void);

Object TunnelInvokeService_getClientEnv(void);

int32_t TunnelInvokeService_open(uint32_t uid, Object *objOut);

#endif  // __TUNNEL_INVOKE_SERVICE_H__
