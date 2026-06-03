// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TMODULE_H
#define __TMODULE_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include "MinkHub.h"
#include "object.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Register service into TModule List.
 *
 * param[in]    hub                 the MinkHub instance
 * param[in]    key                 the key value which is used to find TModule instance
 * param[in]    serviceIds          the service ids which are related to the service
 * param[in]    serviceIdsLen       the length about service ids
 * param[in]    privilegeIds        the privilege ids
 * param[in]    privilegeIdsLen     the length about privilege ids
 * param[in]    procCred[optional]  the credentials of TModule
 * param[in]    iMo                 the local service IModule instance
 *
 * param[out]   objOut              the TModule instance
 *
 * return Object_OK if successful
 */
int32_t TModule_registerTModule(MinkHub *hub, uint32_t key, const uint32_t *serviceIds,
                                uint32_t serviceIdsLen, const uint32_t *privilegeIds,
                                uint32_t privilegeIdsLen, Object procCred, Object iMod,
                                Object *objOut);

/**
 * Find the TModule instance
 *
 * param[in]    key                 the key value of TModule instance
 *
 * param[out]   tMod                the TModule instance
 *
 * return Object_OK if successful
 *
 * Note: the caller should call Object_ASSIGN_NULL after finishing the tMod.
 */
int32_t TModule_findTModule(uint32_t key, Object *tMod);

/**
 * create the remote TModule instance
 *
 * param[in]    hub                 the MinkHub instance
 *
 * param[out]   objOut              the TModule instance
 *
 * return Object_OK if successful
 */
int32_t TModule_createRemoteTModule(MinkHub *hub, Object *objOut);

/**
 * Check the service whether it has the privilege to access the target service.
 *
 * param[in]    tMod                the TModule instance
 * param[in]    uid                 the target service's uid
 *
 * return true if it has the privilege
 */
bool TModule_checkPrivilege(Object tMod, uint32_t uid);

/**
 * Open the target service locating in the local MinkHub
 *
 * param[in]    tMod                the TModule instance
 * param[in]    uid                 the target service's uid
 * param[in]    linkCred            the link credential
 *
 * param[out]   objOut              the target service instance
 *
 * return Object_OK if successful
 */
int32_t TModule_localOpen(Object tMod, uint32_t uid, Object linkCred, Object *objOut);

/**
 * Open the target service locating in the remote MinkHub
 *
 * param[in]    tMod                the TModule instance
 * param[in]    uid                 the target service's uid
 *
 * param[out]   objOut              the target service instance
 *
 * return Object_OK if successful
 */
int32_t TModule_remoteOpen(Object tMod, uint32_t uid, Object *objOut);

#if defined(__cplusplus)
}
#endif

#endif  // __TMODULE_H