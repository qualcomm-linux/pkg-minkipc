// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef __SERVICECENTER_H
#define __SERVICECENTER_H

#include <stdint.h>
#include "object.h"

typedef enum {
    TYPE = 0,
    AUTO_START,
    PATH,
    SERVICE_ATTRIBUTE_MAX,
} SERVICE_ATTRIBUTE;

typedef enum {
    EMBEDDED_DAEMON,
    EMBEDDED_SERVICE,
    DOWNLOADABLE_SERVICE,
    SERVICE_TYPE_MAX,
} SERVICE_TYPE;

// Define the target service type that can be stored into Service Center.
#define EMBEDDED_DAEMON_TYPE        (1 << (EMBEDDED_DAEMON))
#define EMBEDDED_SERVICE_TYPE       (1 << (EMBEDDED_SERVICE))
#define DOWNLOADABLE_SERVICE_TYPE   (1 << (DOWNLOADABLE_SERVICE))

/**
 * Parse the yaml file that is stored in path and store target service information whose service
 * type match targetType.
 *
 * param[in]    path                the directory where the yaml file stored in
 * param[in]    serviceTypeList     the target service type set
 *
 * return Object_OK if successful
 */
int32_t ServiceCenter_loadServiceProfiles(const char *path, int32_t serviceTypeList);

/**
 * Destroy service center and release all the information that is stored in service center.
 */
void ServiceCenter_destory(void);

/**
 * Get the count of Auto-Start service
 *
 * return the count if successful.
 */
int32_t ServiceCenter_getAutoStartServiceCount(void);

/**
 * Get the Auto-Start service list from service center
 *
 * param[out]   uidList             the Auto-Start list
 * param[out]   sizeLen             the length of the Auto-Start list.
 */
int32_t ServiceCenter_getAutoStartServiceList(uint32_t *uidList, uint32_t *sizeLen);

/**
 * Find the target service
 *
 * param[in]    uid                 the unique identifier of the target service
 *
 * param[out]   info                the pointer of the target service info
 *
 * return Object_OK if successful
 */
int32_t ServiceCenter_findService(uint32_t uid, void **info);

/**
 * Get the service's information
 *
 * param[in]    attr                the service's attribute
 *
 * param[out]   data                the information data
 * param[out]   dataLen             the length of data
 *
 * return Object_OK if successful
 */
int32_t ServiceCenter_getServiceAttribute(void *info, SERVICE_ATTRIBUTE attr, uint8_t *data,
                                          uint32_t *dataLen);

#endif /* __SERVICECENTER_H */