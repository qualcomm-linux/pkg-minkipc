// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TUNNEL_INVOKE_H__
#define __TUNNEL_INVOKE_H__

#include <stdbool.h>
#include <stdint.h>
#include "IMinkPortal.h"

enum TI_VERSIONS {
    TI_VERSION_0 = 0,  // Initial version
    TI_VERSION_1 = 1,  // Introduce crypto
    TI_VERSION_2 = 2,  // Remove root Object
    /* ... add more here */
    TI_VERSIONS_COUNT
};

#define TI_LATEST_VERSION (TI_VERSIONS_COUNT - 1)

// The root object will occupy index 0
#define TI_CENV_OBJ_INDEX 0

typedef struct TI TI;

int32_t TI_new(uint32_t version, bool isHost, uint8_t* key, size_t keyLen, size_t objTableLen,
               TI** meOut);
int32_t TI_retain(TI* me);
int32_t TI_release(TI* me);

// client functions
int32_t TI_setPeer(TI* me, Object peer);
int32_t TI_releasePeer(TI* me);
int32_t TI_registerForCallbacks(TI* me);
int32_t TI_newRemoteObject(TI* me, size_t index, Object* objOut);

// host functions
Object TI_getIMinkPortal(TI* me);
int32_t TI_addLocalObject(TI* me, Object obj, size_t* index);

bool TI_isRemoteObj(Object obj, size_t* index);

#endif  // __TUNNEL_INVOKE_H__
