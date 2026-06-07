// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __METADATAINFO_H
#define __METADATAINFO_H

#include <stdint.h>
#include <sys/types.h>
#include "MinkTypes.h"
#include "object.h"

typedef struct MetadataInfo MetadataInfo;

/**
 * Free this object and each element within it.
 * */
void MetadataInfo_destruct(MetadataInfo *me);

/**
 * ICredentials interface: given an index, return the name/value pair.
 * */
int32_t MetadataInfo_getPropertyByIndex(MetadataInfo *me, uint32_t index, void *name,
                                        size_t nameLen, size_t *nameLenOut, void *value,
                                        size_t valueLen, size_t *valueLenOut);

/**
 * ICredentials interface: given a name, return the corresponding value.
 * */
int32_t MetadataInfo_getValueByName(MetadataInfo *me, const void *name, size_t nameLen, void *value,
                                    size_t valueLen, size_t *valueLenOut);

int32_t MetadataInfo_getPrivileges(MetadataInfo *me, const uint32_t **privileges,
                                   uint32_t *privilegeLen);

int32_t MetadataInfo_getServices(MetadataInfo *me, const uint32_t **services, uint32_t *serviceLen);

int32_t MetadataInfo_setDomain(MetadataInfo *me, char const *domain);

const char *MetadataInfo_getDomain(MetadataInfo *me);

int32_t MetadataInfo_setRot(MetadataInfo *me, uint32_t taRot);

uint32_t MetadataInfo_getRot(MetadataInfo *me);

int32_t MetadataInfo_setPid(MetadataInfo *me, pid_t procId);

pid_t MetadataInfo_getPid(MetadataInfo *me);

const char *MetadataInfo_getName(MetadataInfo *me);

const char *MetadataInfo_getDistName(MetadataInfo *me);

int32_t MetadataInfo_getDistId(MetadataInfo *me, DistId *distId);

int32_t MetadataInfo_new(MetadataInfo **meOut, char const *metaData, size_t metaDataSize);

#endif  // __METADATAINFO_H
