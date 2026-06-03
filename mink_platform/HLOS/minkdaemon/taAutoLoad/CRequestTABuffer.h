// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CREGISTER_TABUFF_H
#define __CREGISTER_TABUFF_H

#include <list>
#include "TEE_client_api.h"
#include "TaImageReader.h"
#include "json/json.h"

#ifdef __cplusplus
extern "C" {
#endif

// configuration file path
static const char *config_path("/vendor/etc/ssg/ta_config.json");

typedef struct {
    std::list<std::string> searchLocations;
    int32_t refs;
} CRequestTABuffer;

/**
 * Create a Callback Buffer Object for loading TA.
 *
 * param[out]   requestTABufferObj        Buffer returned to Client
 *
 * return Object_OK if successful
 */
int32_t CRequestTABuffer_open(Object *RequestTABufferObj);

/**
 * Get the TA image assoicated with the UUID.
 *
 * param[in]    me            Local Object associated with this interface
 * param[in]    uuid_ptr      UID for requested TA
 * param[in]    uuid_len      This length of UID for requested TA
 * param[out]   appElf        Buffer with TA Image
 *
 * return Object_OK if successful
 */
int32_t CRequestTABuffer_get(CRequestTABuffer *me, const void *uuid_ptr, size_t uuid_len,
                             Object *appElf);

#ifdef __cplusplus
}
#endif

#endif  // __CREGISTER_TABUFF_H
