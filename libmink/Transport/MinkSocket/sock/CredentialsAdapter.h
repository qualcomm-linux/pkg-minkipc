// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CREDENTIALSADAPTER_H
#define __CREDENTIALSADAPTER_H

#include "object.h"

#if defined (__cplusplus)
extern "C" {
#endif

int32_t LocalCredAdapter_new(Object endpoint, Object credentials, Object *objOut);

int32_t RemoteCredAdapter_new(Object endpoint, Object credentials, Object *objOut);

#if defined (__cplusplus)
}
#endif

#endif //__CREDENTIALSADAPTER_H
