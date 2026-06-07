// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "MyTestIModule.h"
#include "IModule_invoke.h"
#include "IObject_invoke.h"
#include "object.h"

/***********************************************************************
 * A simple module object, for testing
 * ********************************************************************/
static int32_t MyTestIModule_retain(MyTestIModule *me)
{
    me->refs++;
    return Object_OK;
}

static int32_t MyTestIModule_release(MyTestIModule *me)
{
    me->refs--;
    return Object_OK;
}

static int32_t MyTestIModule_open(MyTestIModule *me, uint32_t id, Object credentials,
                             Object *objOut)
{
    me->count++;
    me->id = id;
    Object_ASSIGN(me->lastRemoteCredentials, credentials);
    Object_INIT(*objOut, me->toBeReturned);
    return Object_OK;
}

static int32_t MyTestIModule_shutdown(MyTestIModule *me)
{
    return Object_OK;
}

IModule_DEFINE_INVOKE(MyTestIModule_invoke, MyTestIModule_, MyTestIModule *);

/***********************************************************************
 * A simple service object, for testing
 * ********************************************************************/
static int32_t MyTestService_retain(MyTestService *me)
{
    me->refs++;
    return Object_OK;
}

static int32_t MyTestService_release(MyTestService *me)
{
    me->refs--;
    return Object_OK;
}

IObject_DEFINE_INVOKE(MyTestService_invoke, MyTestService_, MyTestService *);
