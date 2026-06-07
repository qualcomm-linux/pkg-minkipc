// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <stddef.h>
#include <algorithm>
#include "EnvWrapper.h"

#ifndef OFFTARGET
#include "TZCom.h"
#endif

// In order to determine whether the Object comes from QTEE.
static ObjectInvoke qteeInvoke = NULL;
int32_t ObjWrapper_new(ObjectCxt h, OBJECT_CONSUMED Object in, Object *objOut);

/**
 * Get Pointer to EnvWrapper containing Meta Information
 *
 * param[in]        envObj                pointer to EnvObjWrapper
 * param[in]        envWrapper            pointer to pointer of EnvWrapper containing EnvObj
 *
 * return Object_OK if successful
 */
static int32_t getMeta(EnvObjWrapper *envObj, EnvWrapper **envWrapper)
{
    int32_t ret = Object_ERROR;

    if (envObj->parentWrapperType == OBJTYPE_ENVWRAPPER) {
        *envWrapper = (struct EnvWrapper *)(envObj);
        LOG_MSG("OP on EnvWrapper %p Obj Wrapper %p", *envWrapper,
                &(*envWrapper)->mClientEnvWrapper);
        ret = Object_OK;
    } else if (envObj->parentWrapperType == OBJTYPE_OBJWRAPPER) {
        *envWrapper = (struct EnvWrapper *)(envObj->parentWrapper);
        LOG_MSG("Op on Wrapped OO  EnvWrapper %p EnvObjWrapper %p %d", *envWrapper,
                &(*envWrapper)->mClientEnvWrapper, (*envWrapper)->qteeObjectCount);
        ret = Object_OK;
    } else {
        LOG_ERR("Error : Invalid Object Invoked");
    }

    return ret;
}

/**
 * Generic Release for all Wrapped Objects
 *
 * param[in]        me            Pointer to Wrapped Env or OO Object
 *
 * return Object_OK if successful
 */
static int32_t EnvObjWrapper_release(EnvObjWrapper *me)
{
    struct EnvWrapper *envWrapper = nullptr;
    int32_t ret = Object_ERROR;

    // Get the Hold of the Orginal EnvWrapper of EnvObjWrapper
    if (Object_OK == getMeta(me, &envWrapper)) {
        LOG_MSG(
            "Release Op for EnvObjWrapper %p,decrement refs count %d for Parent EnvWrapper %p with "
            "qteeObjCounts %d",
            me, me->refs, envWrapper, envWrapper->qteeObjectCount);
        if (atomic_add(&me->refs, -1) == 0) {
            envWrapper->decrementObjCount();
            Object_ASSIGN_NULL(me->obj);
            LOG_MSG("Release EnvWrapper %p with EnvObjWrapper %p with qteeObjectCount %d",
                    envWrapper, &envWrapper->mClientEnvWrapper, envWrapper->qteeObjectCount);
            // This Check is necessary but qteeObjectCount may not be zero when
            // envObjWrapper refs reaches zero, release EnvWrapper is only allowed when
            // the underlying mClientEnvWrapper is finally released.
            if (me->parentWrapperType == OBJTYPE_ENVWRAPPER) {
                if (envWrapper->qteeObjectCount == 0) {
                    LOG_MSG("Releasing EnvWrapper");
                    delete (envWrapper);
                }
            } else {
                // Can be deleted as EnvObjWrapper was a wrapped OO
                LOG_MSG("Releasing OO Wrapper");
                delete (me);
            }
        }
        ret = Object_OK;
    } else {
        LOG_ERR("Error : Release Failed Invalid Wrapper type");
    }
    return ret;
}

static int32_t EnvObjWrapper_retain(EnvObjWrapper *me)
{
    atomic_add(&me->refs, 1);
    return Object_OK;
}

/**
 * Generic Invoke for all Wrapped Objects
 *
 * param[in]        h                Object Context
 * param[in]        op               operation to perform on object
 * param[in]        a                Invoke ObjectArgs
 * param[in]        k                Object Count
 *
 * return Object_OK if successful
 */
static int32_t ObjWrapper_Invoke(ObjectCxt h, ObjectOp op, ObjectArg *a, ObjectCounts k)
{
    int32_t ret = Object_ERROR;
    EnvObjWrapper *ObjWrapper = (EnvObjWrapper *)h;
    EnvWrapper *me = nullptr;

    // Make Sure pointer type is either ObjWrapper or EnvWrapper
    if (Object_OK == getMeta((EnvObjWrapper *)h, &me)) {
        // Check if Total Objects Exceeded or Per Client Exceeded, if OO requested return error
        if (((true == me->systemObjQuotaReached()) || (true == me->objLimitReached())) &&
            (0 < ObjectCounts_numOO(k))) {
            LOG_ERR("Error : Object Limit Exceeded");
            ret = Object_ERROR_NOSLOTS;
            goto exit;
        }
        LOG_MSG("Invoked Wrapped Object, requesting QTEE Total QTEE Objects %d,"
                 "with EnvObjWrapper %p and %p", me->qteeObjectCount, &me->mClientEnvWrapper,
                 ObjWrapper);
        ret = Object_invoke(ObjWrapper->obj, op, a, k);
        if (Object_isOK(ret)) {
            // OP requested is to open a QTEE Object associated with a Service
            // Hence we increment the number of qteeObjectCount associated with EnvWrapper
            FOR_ARGS(i, k, OO)
            {
                LOG_MSG("Wraping Out Objects index %d", i);
                // Remote Object from QTEE
                if (a[i].o.invoke == qteeInvoke) {
                    Object forwarder = Object_NULL;
                    ret = ObjWrapper_new(h, a[i].o, &forwarder);
                    if (Object_isERROR(ret)) {
                        LOG_ERR("Error %s: failed wrap forwarder  ret:%d.", __func__, ret);
                        for (size_t ii = ObjectCounts_indexOO(k); ii < i; ++ii) {
                            Object_ASSIGN_NULL(a[i].o);
                        }
                        ret = (ret == Object_ERROR_KMEM) ? ret : Object_ERROR_UNAVAIL;
                        break;
                    } else {
                        // replace in-place in the array of returned arguments
                        a[i].o = forwarder;
                    }
                }
            }
        }
    } else {
        LOG_ERR("Error : Invoke Failed Invalid Wrapper type");
    }

exit:
    return ret;
}

/**
 * Wrapper for invoke
 *
 * param[in]        h            Object Context
 * param[in]        op           operation to perform on object
 * param[in]        k            Object Counts
 *
 * return Object_OK if successful
 */
static int32_t EnvObjWrapper_invoke(ObjectCxt h, ObjectOp op, ObjectArg *a, ObjectCounts k)
{
    int32_t ret = Object_ERROR_INVALID;

    switch (ObjectOp_methodID(op)) {
        case Object_OP_release: {
            ret = EnvObjWrapper_release((EnvObjWrapper *)h);
            goto bail;
        }
        case Object_OP_retain: {
            ret = EnvObjWrapper_retain((EnvObjWrapper *)h);
            goto bail;
        }
        default: {
            if (!Object_isNull(((EnvObjWrapper *)h)->obj)) {
                ret = ObjWrapper_Invoke(h, op, a, k);
            } else {
                LOG_ERR("Error : Invalid Buffer Pointer %p", h);
            }
        }
    }

bail:
    return ret;
}

/**
 * Create A Wrapper for OO Object or Env
 *
 * param[in]        h                         Parent EnvObjWrapper
 * param[in]        in                        Input Object
 * param[out]       objOut                    Output Object
 *
 * return Object_OK if successful
 */
int32_t ObjWrapper_new(ObjectCxt h, OBJECT_CONSUMED Object in, Object *objOut)
{
    int32_t ret = Object_ERROR;
    EnvWrapper *me = nullptr;

    LOG_MSG("Wrapping OO into ObjWrapper/Forwarder");
    if (Object_OK == getMeta((EnvObjWrapper *)h, &me)) {
        EnvObjWrapper *ObjWrapper = new EnvObjWrapper();
        if (!ObjWrapper) {
            LOG_ERR("Error : Failed to allocate Object Wrapper Not Found");
            ret = Object_ERROR_KMEM;
            goto exit;
        }

        me->incrementObjCount();
        LOG_MSG("Incrementing Object Count for %p Count %d", &me->mClientEnvWrapper,
                me->qteeObjectCount);
        ObjWrapper->refs = 1;
        ObjWrapper->obj = in;
        ObjWrapper->parentWrapper = (size_t)me;
        ObjWrapper->parentWrapperType = (size_t)OBJTYPE_OBJWRAPPER;

        if (me->mClientEnvWrapper.parentWrapperType == OBJTYPE_ENVWRAPPER) {
            LOG_MSG("Adding OO with Parent Link %p", ObjWrapper);
        } else {
            LOG_MSG("Wrapping Local Object %p", ObjWrapper);
        }

        *objOut = (Object){EnvObjWrapper_invoke, ObjWrapper};
        ret = Object_OK;
    } else {
        LOG_ERR("Error : Failed to get Meta Invalid Object Type");
    }

exit:
    return ret;
}

/**
 * Create EnvWrapper for ClientEnv Object
 *
 * param[in]        env              ClientEnv Object
 * param[out]       objOut           Output Object
 * param[in]        qteeObjectLimit  qteeObjectLimit Default qtee Limit to be imposed
 *                                   by EnvWrapper on OO that can be requested using EnvObjWrapper
 * param[in]        quotaLimit
 *
 * return Object_OK if successful
 */
int32_t EnvWrapper_open(Object env, Object *objOut, int32_t qteeObjectLimit, int32_t quotaLimit)
{
    int32_t ret = Object_ERROR;
    Object qteeObj = Object_NULL;

    LOG_MSG("Constructing EnvWrapper for Client with Max qteeObjLimit %u", qteeObjectLimit);
    EnvWrapper *me = new EnvWrapper(qteeObjectLimit, quotaLimit);
    if (!me) {
        LOG_ERR("EnvWrapper allocation failed");
        return Object_ERROR_MEM;
    }

#ifndef OFFTARGET
    if (NULL == qteeInvoke) {
        ret = TZCom_getRootEnvObject(&qteeObj);
        if (Object_OK == ret && !Object_isNull(qteeObj)) {
            qteeInvoke = qteeObj.invoke;
        } else {
            LOG_ERR("TZCom_getRootEnvObject failed, ret = %d", ret);
        }
        Object_ASSIGN_NULL(qteeObj);
    }
#endif

    LOG_MSG("EnvWrapper allocated %p", me);
    Object_INIT(me->mClientEnvWrapper.obj, env);
    *objOut = (Object){EnvObjWrapper_invoke, &me->mClientEnvWrapper};

    return Object_OK;
}

int32_t EnvWrapper_getQteeObjCount(const Object *me)
{
    return atomic_add(&static_cast<EnvWrapper *>(me->context)->qteeObjectCount, 0);
}

int32_t EnvObjWrapper_getRefs(const Object *me)
{
    return atomic_add(&static_cast<EnvObjWrapper *>(me->context)->refs, 0);
}
