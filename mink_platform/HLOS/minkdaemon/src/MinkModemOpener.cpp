// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <stdint.h>
#include "CTunnelInvokeMgr.h"
#include "IMinkModemOpener_invoke.h"
#include "IModule.h"
#include "MinkModemOpener.h"
#include "ObjectTableMT.h"
#include "heap.h"
#include "object.h"

// maxium 10 modem connections
static const int32_t OBJTABLELEN = 1;

typedef struct {
    ObjectTableMT objTable;
    int32_t refs;
    Object tEnvObj;
} MinkModemOpener;

static int32_t getTiMgr(Object tEnvObj, Object *objOut)
{
    int32_t ret = Object_OK;
    ret = IModule_open(tEnvObj, CTunnelInvokeMgr_UID, Object_NULL, objOut);
    if (Object_isERROR(ret)) {
        LOG_ERR("Opening CRegisterTABufferCBO_UID failed ret = %d", ret);
    }

    return ret;
}

static int32_t MinkModemOpener_retain(MinkModemOpener *me)
{
    vm_osal_atomic_add(&me->refs, 1);

    return Object_OK;
}

static int32_t MinkModemOpener_release(MinkModemOpener *me)
{
    if (vm_osal_atomic_add(&me->refs, -1) == 0) {
        LOG_MSG("MinkModemOpener_release, ObjectTableMT_destruct");
        ObjectTableMT_destruct(&me->objTable);
        Object_ASSIGN_NULL(me->tEnvObj);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

int32_t MinkModemOpener_open(MinkModemOpener *me, uint32_t id_val, Object *obj_ptr)
{
    switch (id_val) {
        case CTunnelInvokeMgr_UID:
            return getTiMgr(me->tEnvObj, obj_ptr);
        default:
            break;
    }

    return Object_ERROR;
}

int32_t MinkModemOpener_register(MinkModemOpener *me, Object obj, int32_t *id)
{
    int32_t ret = Object_OK;
    int32_t handle = 0;

    if (Object_isNull(obj)) {
        LOG_ERR("Invalid object");
        ret = Object_ERROR;
        goto exit;
    }

    /*modem opener is exclusively for the modem side of clients.
      With the introduction of the hub in the modem, number of client is limited to 1
      Durning Modem SSR there are chances that modem minkhub did not get enough time
      to release the associated objects allocated in HLOS
      Explicitly calling close/release to ensure its always fresh start
    */

    ret = ObjectTableMT_close(&me->objTable, 0);
    /*Skipping the check of ret as ret could be OBJECT_ERROR in case of first call or OBJECT_OKAY when register is called next
      causing the actual cleanup and both are allowed */

    /*Now add the new request*/
    handle = ObjectTableMT_addObject(&me->objTable, obj);
    if (handle == 0) {
        *id = handle;
        LOG_MSG("MinkModemOpener_register, return index = %d", handle);
        ret = Object_OK;
    } else { /*This should not happen*/
        LOG_ERR("MinkModemOpener_register, failed to add object");
        ret = Object_ERROR_NOSLOTS;
    }

exit:
    return ret;
}

int32_t MinkModemOpener_unregister(MinkModemOpener *me, int32_t id)
{
    int32_t ret = Object_OK;
    /*As there can only be one object with index 0*/
    if (id == 0) {
        LOG_MSG("MinkModemOpener_unregister, receive index = %d", id);
        ret = ObjectTableMT_releaseHandle(&me->objTable, id);
    } else {
        LOG_ERR("skip ObjectTableMT_releaseHandle, receive invalid index = %d", id);
        ret = Object_ERROR;
    }

    return ret;
}

IMinkModemOpener_DEFINE_INVOKE(MinkModemOpener_invoke, MinkModemOpener_, MinkModemOpener *);

int32_t MinkModemOpener_new(Object tEnvObj, Object *objOut)
{
    MinkModemOpener *me = HEAP_ZALLOC_TYPE(MinkModemOpener);
    if (!me) {
        LOG_ERR("Memory allocation for mink modem opener failed");
        return Object_ERROR_MEM;
    }
    /*With mink hub in modem there can only be one modem client hence the size is set to 1*/
    if (Object_isERROR(ObjectTableMT_construct(&me->objTable, OBJTABLELEN))) {
        LOG_ERR("ObjectTableMT_construct failed");
        HEAP_FREE_PTR(me);
        return Object_ERROR;
    }

    me->refs = 1;
    Object_INIT(me->tEnvObj, tEnvObj);
    *objOut = (Object){MinkModemOpener_invoke, me};

    return Object_OK;
}
