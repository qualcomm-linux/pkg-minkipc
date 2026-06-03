// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <pthread.h>
#include "IMinkdRegister_invoke.h"
#include "MinkDaemon_logging.h"
#include "MinkHub.h"
#include "VmOsal.h"
#include "heap.h"
#include "object.h"

typedef struct {
    int32_t refs;
    MinkHubSession *session;
} MinkdReg;

static int32_t MinkdRegister_retain(MinkdReg *me)
{
    vm_osal_atomic_add(&me->refs, 1);

    return Object_OK;
}

static int32_t MinkdRegister_release(MinkdReg *me)
{
    if (vm_osal_atomic_add(&me->refs, -1) == 0) {
        LOG_MSG("Released MinkdReg = %p\n", me);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

/**
 * Register service to Mink Hub.
 *
 * param[in] uidList               Array of Mink services/UIDs hosted within corresponding
 *                                  iModule.
 *
 * param[in] iModule               Mink module used to open hosted services. Must implement
 *                                  \link iModule \endlink interface.
 *
 * return Object_OK if successful
 */
static int32_t MinkdRegister_registerServices(MinkdReg *me, const uint32_t *uidList,
                                              size_t uidListLen, Object iModule)
{
    int32_t ret = Object_OK;

    if (NULL == me || NULL == uidList || 0 == uidListLen || Object_isNull(iModule)) {
        LOG_ERR("Invalid service to register");
        return Object_ERROR_INVALID;
    }

    ret = MinkHub_registerServices(me->session, uidList, uidListLen, iModule);

    return ret;
}

static IMinkdRegister_DEFINE_INVOKE(MinkdRegister_invoke, MinkdRegister_, MinkdReg *);

int32_t MinkdRegister_new(MinkHubSession *session, Object *objOut)
{
    MinkdReg *me = HEAP_ZALLOC_TYPE(MinkdReg);
    if (!me) {
        LOG_ERR("MinkdReg memory allocation failed");
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    me->session = session;
    *objOut = (Object){MinkdRegister_invoke, me};

    return Object_OK;
}
