// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <pthread.h>
#include <stdbool.h>
#include "ITProcess.h"
#include "ITProcessController_invoke.h"
#include "TProcess.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "qlist.h"

typedef struct {
    int32_t refs;
    Object tprocObj;
    bool neverUnload;
} TProcController;

static int32_t TProcController_retain(TProcController *me)
{
    atomicAdd(&me->refs, 1);
    LOG_MSG("refcount now %d", me->refs);

    return Object_OK;
}

static int32_t TProcController_release(TProcController *me)
{
    int32_t ret = Object_OK;
    int32_t refs = me->refs;

    if (refs == 1 && !me->neverUnload) {
        T_CALL(ITProcess_forceClose(me->tprocObj));
    }

    if (atomicAdd(&me->refs, -1) == 0) {
        Object_ASSIGN_NULL(me->tprocObj);
        HEAP_FREE_PTR(me);
    }

    LOG_MSG("refcount now %d", refs - 1);

exit:
    return ret;
}

// Bypass reference counting and immediately begin cleaning up resources
static int32_t TProcController_forceClose(TProcController *me)
{
    int32_t ret = Object_ERROR;
    T_CALL(ITProcess_forceClose(me->tprocObj));

exit:
    return ret;
}

static ITProcessController_DEFINE_INVOKE(CTProcController_invoke, TProcController_,
                                         TProcController *);

// TProcessController retains TProcess Object
int32_t TProcController_new(Object tprocObj, bool neverUnload, Object *objOut)
{
    int32_t ret = Object_OK;
    TProcController *me = HEAP_ZALLOC_TYPE(TProcController);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    me->refs = 1;
    Object_ASSIGN(me->tprocObj, tprocObj);
    me->neverUnload = neverUnload;

    LOG_MSG("Successfully created TProcController");

    *objOut = (Object){CTProcController_invoke, me};

exit:
    return ret;
}
