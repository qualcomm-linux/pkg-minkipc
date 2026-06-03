// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "CTestModule.h"
#include "IModule.h"
#include "IModule_invoke.h"
#include "ITestModule.h"
#include "ITestModule_invoke.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "logging.h"
#include "lxcom_sock.h"
#include "memscpy.h"
#include "minkipc.h"
#include "msforwarder.h"
#include "qtest.h"

#define TEST_BUFFER "TEST"
#define MAX_BUF_SIZE 1024 * 512

#define FOR_ARGS(ndxvar, counts, section)                     \
    for (size_t ndxvar = ObjectCounts_index##section(counts); \
         ndxvar < (ObjectCounts_index##section(counts) +      \
                   ObjectCounts_num##section(counts));        \
         ++ndxvar)

static char req[MAX_BUF_SIZE];
static char resp[MAX_BUF_SIZE];

static uint32_t readNum(const void *p)
{
    return *((const uint32_t *)p);
}

static void writeNum(void *p, uint32_t i)
{
    *((uint32_t *)p) = i;
}

static int atomic_add(int *pn, int n)
{
    return __sync_add_and_fetch(pn, n);  // GCC builtin
}

static int32_t CTestModule_retain(TestModule *me)
{
    int32_t ret = Object_OK;

    atomic_add(&me->refs, 1);
    return ret;
}

static int32_t CTestModule_release(TestModule *me)
{
    int32_t ret = Object_OK;

    if (atomic_add(&me->refs, -1) == 0) {
        Object_ASSIGN_NULL(me->obj);
        HEAP_FREE_PTR(me);
    }

    return ret;
}

static int32_t CTestModule_getTypeId(TestModule *me, uint32_t *id)
{
    int32_t ret = Object_OK;

    writeNum(id, CTESTMODULE_ID);

    return ret;
}

static int32_t CTestModule_getId(TestModule *me, uint32_t *id)
{
    int32_t ret = Object_OK;

    writeNum(id, me->id);
    return ret;
}

static int32_t CTestModule_getMultiId(TestModule *me, uint32_t *id_1,
                                      uint32_t *id_2, uint32_t *id_3)
{
    int32_t ret = Object_OK;
    int32_t offset = 0;

    writeNum(id_1, me->id + offset);
    offset += 100;
    writeNum(id_2, me->id + offset);
    offset += 100;
    writeNum(id_3, me->id + offset);

    return ret;
}

static int32_t CTestModule_setId(TestModule *me, uint32_t id)
{
    int32_t ret = Object_OK;

    me->id = id;

    return ret;
}

static int32_t CTestModule_setMultiId(TestModule *me, uint32_t id_1,
                                      uint32_t id_2, uint32_t id_3)
{
    int32_t ret = Object_OK;

    me->id = 0;
    me->id += id_1;
    me->id += id_2;
    me->id += id_3;

    return ret;
}

static int32_t CTestModule_setAndGetMultiId(TestModule *me, uint32_t id_1,
                                            uint32_t id_2, uint32_t id_3,
                                            uint32_t *id_1_toGet,
                                            uint32_t *id_2_toGet,
                                            uint32_t *id_3_toGet)
{
    int32_t ret = Object_OK;

    writeNum(id_1_toGet, id_1);
    writeNum(id_2_toGet, id_2);
    writeNum(id_3_toGet, id_3);

    return ret;
}

static int32_t CTestModule_getObject(TestModule *me, Object *objOut)
{
    int32_t ret = Object_OK;

    if (Object_isNull(me->obj)) {
        ret = CTestModule_new(objOut);
    } else {
        *objOut = me->obj;
        Object_retain(me->obj);
    }

    return ret;
}

static int32_t CTestModule_getMultiObject(TestModule *me, Object *objOut_1,
                                          Object *objOut_2, Object *objOut_3)
{
    int32_t ret = Object_OK;

    *objOut_1 = me->obj;
    Object_retain(me->obj);
    *objOut_2 = me->obj;
    Object_retain(me->obj);
    *objOut_3 = me->obj;
    Object_retain(me->obj);

    return ret;
}

static int32_t CTestModule_setObject(TestModule *me, Object obj)
{
    int32_t ret = Object_OK;

    Object_replace(&me->obj, obj);

    return ret;
}

static int32_t CTestModule_setMultiObject(TestModule *me, Object obj_1,
                                          Object obj_2, Object obj_3)
{
    int32_t ret = Object_OK;

    qt_assert(Object_isNull(obj_1));
    Object_replace(&me->obj, obj_1);
    qt_assert(Object_isNull(obj_2));
    Object_replace(&me->obj, obj_2);
    qt_assert(Object_isNull(obj_3));
    Object_replace(&me->obj, obj_3);

    return ret;
}

static int32_t CTestModule_setAndGetObj(TestModule *me, Object objToSet,
                                        Object *objToGet)
{
    int32_t ret = Object_OK;

    Object_INIT(*objToGet, objToSet);

    return ret;
}

static int32_t CTestModule_setLocalFdObjAndGet(TestModule *me, Object fdObj,
                                               Object *fdObjToGet)
{
    int32_t ret = Object_OK;

    Object_INIT(*fdObjToGet, fdObj);

    return ret;
}

static int32_t CTestModule_setMsforwardObjAndGet(TestModule *me, Object mfObj,
                                                 Object *mfObjToGet)
{
    int32_t ret = Object_OK;

    Object_INIT(*mfObjToGet, mfObj);

    return ret;
}

static int32_t CTestModule_bufferOutNull(TestModule *me, void *bufferOut,
                                         size_t bufferLen, size_t *bufferLenOut)
{
    int32_t ret = Object_OK;

    memscpy(bufferOut, bufferLen, TEST_BUFFER, sizeof(TEST_BUFFER));
    *bufferLenOut = bufferLen;

    return ret;
}

static int32_t CTestModule_objectOutNull(TestModule *me, Object *objOutNull,
                                         Object *objOut)
{
    int32_t ret = Object_OK;

    *objOutNull = Object_NULL;
    ret = CTestModule_new(objOut);

    return ret;
}

static int32_t CTestModule_unalignedSet(
    TestModule *me, const void *magic_id_withWrongSize_ptr,
    size_t magic_id_withWrongSize_len,
    const uint32_t *magic_id_withRightSize_ptr, uint32_t *pid)
{
    int32_t ret = Object_OK;

    writeNum(pid, (uint32_t)(uintptr_t)magic_id_withRightSize_ptr);

    return ret;
}

static int32_t CTestModule_echo(TestModule *me, void *echo_in,
                                size_t echo_in_len, void *echo_out,
                                size_t echo_out_len, size_t *echo_out_lenOut)
{
    int32_t ret = Object_OK;

    memscpy(echo_out, echo_out_len, echo_in, echo_in_len);

    return ret;
}

static int32_t CTestModule_maxArgs(
    TestModule *me, const void *bi_1_ptr, size_t bi_1_len, const void *bi_2_ptr,
    size_t bi_2_len, const void *bi_3_ptr, size_t bi_3_len,
    const void *bi_4_ptr, size_t bi_4_len, const void *bi_5_ptr,
    size_t bi_5_len, const void *bi_6_ptr, size_t bi_6_len,
    const void *bi_7_ptr, size_t bi_7_len, const void *bi_8_ptr,
    size_t bi_8_len, const void *bi_9_ptr, size_t bi_9_len,
    const void *bi_10_ptr, size_t bi_10_len, const void *bi_11_ptr,
    size_t bi_11_len, const void *bi_12_ptr, size_t bi_12_len,
    const void *bi_13_ptr, size_t bi_13_len, const void *bi_14_ptr,
    size_t bi_14_len, const void *bi_15_ptr, size_t bi_15_len, void *bo_1_ptr,
    size_t bo_1_len, size_t *bo_1_lenOut, void *bo_2_ptr, size_t bo_2_len,
    size_t *bo_2_lenOut, void *bo_3_ptr, size_t bo_3_len, size_t *bo_3_lenOut,
    void *bo_4_ptr, size_t bo_4_len, size_t *bo_4_lenOut, void *bo_5_ptr,
    size_t bo_5_len, size_t *bo_5_lenOut, void *bo_6_ptr, size_t bo_6_len,
    size_t *bo_6_lenOut, void *bo_7_ptr, size_t bo_7_len, size_t *bo_7_lenOut,
    void *bo_8_ptr, size_t bo_8_len, size_t *bo_8_lenOut, void *bo_9_ptr,
    size_t bo_9_len, size_t *bo_9_lenOut, void *bo_10_ptr, size_t bo_10_len,
    size_t *bo_10_lenOut, void *bo_11_ptr, size_t bo_11_len,
    size_t *bo_11_lenOut, void *bo_12_ptr, size_t bo_12_len,
    size_t *bo_12_lenOut, void *bo_13_ptr, size_t bo_13_len,
    size_t *bo_13_lenOut, void *bo_14_ptr, size_t bo_14_len,
    size_t *bo_14_lenOut, void *bo_15_ptr, size_t bo_15_len,
    size_t *bo_15_lenOut, Object oi_1, Object oi_2, Object oi_3, Object oi_4,
    Object oi_5, Object oi_6, Object oi_7, Object oi_8, Object oi_9,
    Object oi_10, Object oi_11, Object oi_12, Object oi_13, Object oi_14,
    Object oi_15, Object *oo_1, Object *oo_2, Object *oo_3, Object *oo_4,
    Object *oo_5, Object *oo_6, Object *oo_7, Object *oo_8, Object *oo_9,
    Object *oo_10, Object *oo_11, Object *oo_12, Object *oo_13, Object *oo_14,
    Object *oo_15)
{
    int32_t ret = Object_OK;
    int32_t i;

    memscpy(bo_1_ptr, bo_1_len, bi_1_ptr, bi_1_len);
    memscpy(bo_2_ptr, bo_2_len, bi_2_ptr, bi_2_len);
    memscpy(bo_3_ptr, bo_3_len, bi_3_ptr, bi_3_len);
    memscpy(bo_4_ptr, bo_4_len, bi_4_ptr, bi_4_len);
    memscpy(bo_5_ptr, bo_5_len, bi_5_ptr, bi_5_len);
    memscpy(bo_6_ptr, bo_6_len, bi_6_ptr, bi_6_len);
    memscpy(bo_7_ptr, bo_7_len, bi_7_ptr, bi_7_len);
    memscpy(bo_8_ptr, bo_8_len, bi_8_ptr, bi_8_len);
    memscpy(bo_9_ptr, bo_9_len, bi_9_ptr, bi_9_len);
    memscpy(bo_10_ptr, bo_10_len, bi_10_ptr, bi_10_len);
    memscpy(bo_11_ptr, bo_11_len, bi_11_ptr, bi_11_len);
    memscpy(bo_12_ptr, bo_12_len, bi_12_ptr, bi_12_len);
    memscpy(bo_13_ptr, bo_13_len, bi_13_ptr, bi_13_len);
    memscpy(bo_14_ptr, bo_14_len, bi_14_ptr, bi_14_len);
    memscpy(bo_15_ptr, bo_15_len, bi_15_ptr, bi_15_len);

    Object_INIT(*oo_1, oi_1);
    Object_INIT(*oo_2, oi_2);
    Object_INIT(*oo_3, oi_3);
    Object_INIT(*oo_4, oi_4);
    Object_INIT(*oo_5, oi_5);
    Object_INIT(*oo_6, oi_6);
    Object_INIT(*oo_7, oi_7);
    Object_INIT(*oo_8, oi_8);
    Object_INIT(*oo_9, oi_9);
    Object_INIT(*oo_10, oi_10);
    Object_INIT(*oo_11, oi_11);
    Object_INIT(*oo_12, oi_12);
    Object_INIT(*oo_13, oi_13);
    Object_INIT(*oo_14, oi_14);
    Object_INIT(*oo_15, oi_15);

    return ret;
}

static int32_t CTestModule_overMaxArgs(
    TestModule *me, const void *bi_1_ptr, size_t bi_1_len, const void *bi_2_ptr,
    size_t bi_2_len, const void *bi_3_ptr, size_t bi_3_len,
    const void *bi_4_ptr, size_t bi_4_len, const void *bi_5_ptr,
    size_t bi_5_len, const void *bi_6_ptr, size_t bi_6_len,
    const void *bi_7_ptr, size_t bi_7_len, const void *bi_8_ptr,
    size_t bi_8_len, const void *bi_9_ptr, size_t bi_9_len,
    const void *bi_10_ptr, size_t bi_10_len, const void *bi_11_ptr,
    size_t bi_11_len, const void *bi_12_ptr, size_t bi_12_len,
    const void *bi_13_ptr, size_t bi_13_len, const void *bi_14_ptr,
    size_t bi_14_len, const void *bi_15_ptr, size_t bi_15_len,
    const void *bi_16_ptr, size_t bi_16_len)
{
    int32_t ret = Object_OK;

    return ret;
}

static int32_t CTestModule_invocationTransferOut(
    TestModule *me, const void *invocation_in_ptr, size_t invocation_in_len,
    void *invocation_out_ptr, size_t invocation_out_len,
    size_t *invocation_out_lenOut, Object *invocation_obj_out)
{
    int32_t ret = Object_OK;

    memscpy(invocation_out_ptr, invocation_out_len, invocation_in_ptr,
            invocation_in_len);

    ret = CTestModule_new(invocation_obj_out);

    return ret;
}

static ITestModule_DEFINE_INVOKE(CTestModule_invoke, CTestModule_, TestModule *)

TestModule *CTestModule_fromObject(Object obj)
{
    return (obj.invoke == CTestModule_invoke) ? (TestModule *)obj.context
                                              : NULL;
}

int32_t CTestModule_new(Object *objOut)
{
    TestModule *me = HEAP_ZALLOC_TYPE(TestModule);
    if (NULL == me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    me->id = INVOKE_MAGIC_ID;

    *objOut = (Object){CTestModule_invoke, me};

    return Object_OK;
}
