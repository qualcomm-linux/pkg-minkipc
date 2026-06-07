// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "CTestModule.h"
#include "CTestOpener.h"
#include "CTestUtils.h"
#include "IModule.h"
#include "IModule_invoke.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "logging.h"
#include "lxcom_sock.h"
#include "memscpy.h"
#include "minkipc.h"
#include "msforwarder.h"
#include "qtest.h"

static int atomic_add(int *pn, int n)
{
    return __sync_add_and_fetch(pn, n);  // GCC builtin
}

static int32_t CTestOpener_retain(TestModule *me)
{
    int32_t ret = Object_OK;

    atomic_add(&me->refs, 1);

    return ret;
}

static int32_t CTestOpener_release(TestModule *me)
{
    int32_t ret = Object_OK;

    if (atomic_add(&me->refs, -1) == 0) {
        HEAP_FREE_PTR(me);
    }

    return ret;
}

static int32_t CTestOpener_open(TestModule *me, uint32_t id,
                                Object credentialObj, Object *objOut)
{
    int32_t ret = Object_OK;

    if (TEST_MODULE_UID == id || TEST_MODULE_UID + 1 == id) {
        ret = CTestModule_new(objOut);
    } else if (TEST_UTILS_UID == id) {
        ret = CTestUtils_new(objOut);
    } else {
        *objOut = Object_NULL;
    }

    return ret;
}

static int32_t CTestOpener_shutdown(TestModule *me)
{
    return Object_OK;
}

static IModule_DEFINE_INVOKE(CTestOpener_invoke, CTestOpener_, TestModule *)

int32_t CTestOpener_new(Object *objOut)
{
    TestModule *me = HEAP_ZALLOC_TYPE(TestModule);
    if (NULL == me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;

    *objOut = (Object){CTestOpener_invoke, me};

    return Object_OK;
}
