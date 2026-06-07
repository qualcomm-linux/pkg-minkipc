// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CTPreLauncher_open.h"
#include "IModule.h"
#include "IModule_invoke.h"
#include "TUtils.h"
#include "heap.h"

/***********************************************************************
 * Env opener
 * ********************************************************************/
typedef struct {
    int32_t refs;
} TEnv;

static int32_t TEnv_retain(TEnv *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t TEnv_release(TEnv *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static int32_t TEnv_shutdown(TEnv *me)
{
    // Don't support remotely shutting down Mink process.
    return Object_ERROR;
}

static int32_t TEnv_open(TEnv *me, uint32_t uid, Object linkCred, Object *objOut)
{
    int32_t ret = Object_OK;

    T_CALL(CTPreLauncher_open(uid, linkCred, objOut));

exit:
    return ret;
}

static IModule_DEFINE_INVOKE(TEnv_invoke, TEnv_, TEnv *);

Object TEnv_new()
{
    TEnv *me = HEAP_ZALLOC_TYPE(TEnv);
    if (!me) {
        return Object_NULL;
    }

    me->refs = 1;
    return (Object){TEnv_invoke, me};
}
