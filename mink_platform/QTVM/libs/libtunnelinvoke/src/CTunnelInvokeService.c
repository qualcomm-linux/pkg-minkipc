// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CTunnelInvokeService.h"
#include "ITunnelInvokeService_invoke.h"
#include "heap.h"
#include "ssgtzd_logging.h"

typedef struct CTunnelInvokeService {
    int refs;
    Object env;
} CTunnelInvokeService;

static int32_t CTunnelInvokeService_retain(CTunnelInvokeService* me)
{
    atomic_add(&me->refs, 1);
    return Object_OK;
}

static int32_t CTunnelInvokeService_release(CTunnelInvokeService* me)
{
    if (atomic_add(&me->refs, -1) == 0) {
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static inline int32_t CTunnelInvokeService_getClientEnv(CTunnelInvokeService* me,
                                                        Object* clientEnv_ptr)
{
    Object_retain(me->env);
    *clientEnv_ptr = me->env;
    return Object_OK;
}

static ITunnelInvokeService_DEFINE_INVOKE(ITunnelInvokeService_invoke, CTunnelInvokeService_,
                                          CTunnelInvokeService*)

//----------------------------------------------------------------
// Exported functions
//----------------------------------------------------------------
Object CTunnelInvokeService_new(Object env)
{
    CTunnelInvokeService* me = HEAP_ZALLOC_TYPE(CTunnelInvokeService);
    if (!me) {
        return Object_NULL;
    }

    me->refs = 1;
    Object_INIT(me->env, env);
    return (Object){ITunnelInvokeService_invoke, me};
}
