// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "CTestUtils.h"
#include "ITestModule.h"
#include "ITestUtils.h"
#include "ITestUtils_invoke.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "logging.h"
#include "lxcom_sock.h"
#include "memscpy.h"
#include "minkipc.h"
#include "msforwarder.h"
#include "qtest.h"

static void writeNum(void *p, uint32_t i)
{
    *((uint32_t *)p) = i;
}

static int atomic_add(int *pn, int n)
{
    return __sync_add_and_fetch(pn, n);  // GCC builtin
}

static int32_t CTestUtils_getTypeId(TestUtils *me, uint32_t *id)
{
    int32_t ret = Object_OK;

    writeNum(id, CTESTUTILS_ID);

    return ret;
}

static int32_t CTestUtils_release(TestUtils *me)
{
    int32_t ret = Object_OK;

    if (atomic_add(&me->refs, -1) == 0) {
        HEAP_FREE_PTR(me);
    }

    return ret;
}

static int32_t CTestUtils_retain(TestUtils *me)
{
    int32_t ret = Object_OK;

    atomic_add(&me->refs, 1);

    return ret;
}

static int32_t CTestUtils_bufferEcho(TestUtils *me, const void *echo_in_ptr,
                                     size_t echo_in_len, void *echo_out_ptr,
                                     size_t echo_out_len,
                                     size_t *echo_out_lenOut)
{
    int32_t ret = Object_OK;

    memscpy(echo_out_ptr, echo_out_len, echo_in_ptr, echo_in_len);

    return ret;
}

static int32_t CTestUtils_bufferPlus(TestUtils *me, uint32_t value_a,
                                     uint32_t value_b, uint32_t *value_sum_ptr)
{
    int32_t ret = Object_OK;

    writeNum(value_sum_ptr, value_a + value_b);

    return ret;
}

static int32_t CTestUtils_invocationTransfer(
    TestUtils *me, const void *invocation_in, size_t invocation_in_len,
    Object invocation_obj_in, void *invocation_out, size_t invocation_out_len,
    size_t *invocation_out_lenOut, Object *invocation_obj_out)
{
    return ITestModule_invocationTransferOut(
        invocation_obj_in, invocation_in, invocation_in_len, invocation_out,
        invocation_out_len, invocation_out_lenOut, invocation_obj_out);
}

static ITestUtils_DEFINE_INVOKE(CTestUtils_invoke, CTestUtils_, TestUtils *)

TestUtils *CTestUtils_fromObject(Object obj)
{
    return (obj.invoke == CTestUtils_invoke) ? (TestUtils *)obj.context : NULL;
}

int32_t CTestUtils_new(Object *objOut)
{
    TestUtils *me = HEAP_ZALLOC_TYPE(TestUtils);
    if (NULL == me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;

    *objOut = (Object){CTestUtils_invoke, me};

    return Object_OK;
}
