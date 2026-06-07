// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "VMCredentials.h"
#include "ICredentials.h"
#include "ICredentials_invoke.h"
#include "MemSCpy.h"
#include "TUtils.h"
#include "heap.h"
#include "object.h"
#include "string.h"

typedef struct {
    int32_t refs;
    uint8_t *vmuuid;
    size_t vmuuidLen;
} VMCredentials;

static void _destruct(VMCredentials *me)
{
    if (!me) {
        return;
    }

    HEAP_FREE_PTR(me->vmuuid);
    HEAP_FREE_PTR(me);
}

static int32_t VMCredentials_retain(VMCredentials *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t VMCredentials_release(VMCredentials *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        HEAP_FREE_PTR(me);
    }
    return Object_OK;
}

int32_t VMCredentials_getPropertyByIndex(VMCredentials *me, uint32_t index, void *name,
                                         size_t nameLen, size_t *nameLenOut, void *value,
                                         size_t valueLen, size_t *valueLenOut)
{
    /* Currently unsupported */
    return ICredentials_ERROR_NOT_FOUND;
}

int32_t VMCredentials_getValueByName(VMCredentials *me, const void *name, size_t nameLen,
                                     void *value, size_t valueLen, size_t *valueLenOut)
{
    /* We only support vmuuid now, and we use the same "name" as in QTEE */
    if (nameLen != 2 || strncmp((const char *)name, "vm", nameLen) != 0) {
        return ICredentials_ERROR_NOT_FOUND;
    }

    if (me->vmuuidLen == 0) {
        return ICredentials_ERROR_NOT_FOUND;
    }

    if (valueLen < me->vmuuidLen) {
        return ICredentials_ERROR_VALUE_SIZE;
    }

    *valueLenOut = memscpy(value, valueLen, me->vmuuid, me->vmuuidLen);

    return Object_OK;
}

static ICredentials_DEFINE_INVOKE(VMCredentials_invoke, VMCredentials_, VMCredentials *);

int32_t VMCredentials_open(uint8_t const *vmuuid, size_t vmuuidLen, Object *objOut)
{
    int32_t ret = Object_ERROR;
    VMCredentials *me = HEAP_ZALLOC_TYPE(VMCredentials);

    T_CHECK_ERR(me, Object_ERROR_MEM);
    T_CHECK(vmuuid);
    T_CHECK(vmuuidLen > 0);

    me->refs = 1;

    me->vmuuid = HEAP_ZALLOC_ARRAY(uint8_t, vmuuidLen);
    T_CHECK_ERR(me->vmuuid, Object_ERROR_MEM);

    me->vmuuidLen = memscpy(me->vmuuid, vmuuidLen, vmuuid, vmuuidLen);

    *objOut = (Object){VMCredentials_invoke, me};
    return Object_OK;

exit:
    _destruct(me);
    return ret;
}
