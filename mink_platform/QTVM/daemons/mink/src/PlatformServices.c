// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "PlatformServices.h"
#include "CTAccessControl.h"
#include "CTAccessControl_open.h"
#include "CTMemoryService_open.h"
#include "CTPowerService_open.h"
#include "CTProcessLoader_open.h"
#include "CTRebootVM.h"
#include "CTRebootVM_open.h"
#include "CTRegisterModule.h"
#include "CTRegisterModule_open.h"
#include "CTTouchControlService.h"
#include "CTTouchControlService_open.h"
#include "IModule_invoke.h"
#include "PlatformConfig.h"
#include "QTVMPlatformInfo.h"
#include "TModule.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"

typedef struct {
    uint32_t uid;
    int32_t (*func)(uint32_t, Object, Object *);
    bool isRemoteAllowed;
} PlatformService;

/* clang-format off */
static PlatformService gServices[] = {
    {PROCESS_LOADER_UID, CTProcessLoader_open, true},
    {MEMPOOL_FACTORY_UID, CTMemPoolFactory_open, true},
    {POWER_SERVICE_UID, CTPowerServiceFactory_open, true},
    {CTRegisterModule_UID, CTRegisterModule_open, false},
    {PLATFORM_INFO_SERVICE_UID, CQTVMPlatformInfo_open, true},
    {REBOOT_SERVICE_UID, CTRebootVM_open, true},
    {TACCESS_CONTROL_UID, CTAccessControl_open, true},
#ifndef OEM_VM
    {CTTouchControlService_UID, CTTouchControlService_open, false},
#endif
};
/* clang-format on */

/**
 * Description: Open service hosted by Mink daemon.
 *
 * In:          id: Unique ID of requested service.
 *              credentials: Credentials of caller.
 *
 * Out:         obj: Service object.
 * Return:      true on success.
 *              false on failure.
 */
int32_t PlatformServices_open(PlatformService *me, uint32_t id, Object credentials, Object *obj)
{
    uint32_t i;

    C_FOR_ARRAY(i, gServices)
    {
        if (id == gServices[i].uid) {
            return gServices[i].func(id, credentials, obj);
        }
    }

    return Object_ERROR;
}

static int32_t PlatformServices_shutdown(PlatformService *me)
{
    return Object_OK;
}

static int32_t PlatformServices_release(PlatformService *me)
{
    (void)me;
    return Object_OK;
}

static int32_t PlatformServices_retain(PlatformService *me)
{
    (void)me;
    return Object_OK;
}

static IModule_DEFINE_INVOKE(PlatformServices_invoke, PlatformServices_, PlatformService *);

/**
 * Description: Test if service is hosted by Mink daemon.
 *
 * In:          uid: Unique ID of requested service.
 *
 * Return:      true on success.
 *              false on failure.
 */
bool PlatformServices_isUIDSupported(uint32_t id)
{
    uint32_t i;

    for (i = 0; i < C_LENGTHOF(gServices); i++) {
        if (id == gServices[i].uid) {
            return true;
        }
    }

    return false;
}

/**
 * Description: Test if service can be opened by a remote client.
 *
 * In:          uid: Unique ID of requested service.
 *
 * Return:      true on success.
 *              false on failure.
 */
bool PlatformServices_isRemoteAllowed(uint32_t id)
{
    uint32_t i;

    for (i = 0; i < C_LENGTHOF(gServices); i++) {
        if (id == gServices[i].uid) {
            return gServices[i].isRemoteAllowed;
        }
    }

    // If it's not in the list, then it is not a PlatformService and should be checked elsewhere.
    return true;
}

int32_t PlatformService_registerServices(MinkHub *minkhub, Object *tMod)
{
    int32_t ret = Object_OK;
    uint32_t uidsCount = C_LENGTHOF(gServices);
    uint32_t *uids = NULL;
    Object platformServiceMod = (Object){PlatformServices_invoke, &gServices};

    uids = HEAP_ZALLOC_ARRAY(uint32_t, uidsCount);
    T_CHECK_ERR(uids != NULL, Object_ERROR_MEM);

    for (uint32_t i = 0; i < uidsCount; ++i) {
        uids[i] = gServices[i].uid;
    }

    T_GUARD(TModule_registerTModule(minkhub, getpid(), uids, uidsCount, NULL, 0, Object_NULL,
                                    platformServiceMod, tMod));

exit:
    HEAP_FREE_PTR_IF(uids);
    return ret;
}
