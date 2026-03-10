// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include "object.h"

#include "cdefs.h"
#include "ITestCBack.h"
#include "ITestCBack_invoke.h"
#include "CTestCallable_open.h"
#include "CTestCallable_priv.h"
#include "heap.h"
#include "tzt.h"

extern int32_t create_and_assign_mem_obj(Object, Object *);

static int32_t CTestCallable_retain(TestCallable *me)
{
	if (!me) {
		return Object_ERROR;
	}

	me->refs++;
	LOGD_PRINT("[%s (%p)] %d -> %d\n", __FUNCTION__, me, me->refs - 1,
		   me->refs);
	return Object_OK;
}

static int32_t CTestCallable_release(TestCallable *me)
{
	if (!me) {
		return Object_ERROR;
	}

	me->refs--;
	LOGD_PRINT("[%s (%p)] %d -> %d\n", __FUNCTION__, me, me->refs + 1,
		   me->refs);

	if (me->refs == 0) {
		LOGD_PRINT("[%s (%p)] delete\n", __FUNCTION__, me);
		Object_RELEASE_IF(me->oArg);
		Object_RELEASE_IF(me->oOArg);
		Object_RELEASE_IF(me->oOOArg);
		Object_RELEASE_IF(me->oOOArg0);
		Object_RELEASE_IF(me->oOOArg1);
		Object_RELEASE_IF(me->oOOArg2);
		HEAP_FREE_PTR(me);
	}
	return Object_OK;
}

static int32_t CTestCallable_call(TestCallable *me)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);

	me->counter++;
	me->op = ITestCallable_OP_call;
	return me->retValue;
}

static int32_t CTestCallable_callWithBuffer(TestCallable *me,
					    const void *arg_ptr, size_t arg_len)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);

	me->counter++;
	me->op = ITestCallable_OP_callWithBuffer;

	if ((arg_len == me->bArg_len) &&
	    (0 == memcmp(arg_ptr, me->bArg_ptr, arg_len))) {
		return me->retValue;
	} else {
		return me->retValueError;
	}
}

static int32_t CTestCallable_callWithBufferOut(TestCallable *me,
					       void *arg1_ptr,
					       size_t arg1_len,
					       size_t *arg1_lenout)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);

	me->counter++;
	me->op = ITestCallable_OP_callWithBufferOut;
	memset(arg1_ptr, 'A', arg1_len);
	*arg1_lenout = arg1_len;

	return me->retValue;
}

static int32_t CTestCallable_callWithObject(TestCallable *me, Object arg_val)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);

	me->counter++;
	me->op = ITestCallable_OP_callWithObject;

	/* The passed object is supposed to be a callback Obj, so we can
	 * compare its members directly
	 */
	if ((arg_val.invoke == me->oArg.invoke) &&
	    (arg_val.context == me->oArg.context)) {

		return me->retValue;
	} else if (arg_val.invoke == me->oOOArg.invoke) {

		/* oOOArg is initialized to a remote object, so we can
		 * use it for this comparison
		 */
		return ITestCallable_ERROR_OBJECT_REMOTE;
	} else {
		return me->retValueError;
	}
}

static int32_t CTestCallable_callGetObject(TestCallable *me, Object *arg_ptr)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_callGetObject;
	Object_INIT(*arg_ptr, me->oOOArg);

	return me->retValue;
}

static int32_t CTestCallable_callGetThreeObjects(TestCallable *me,
						 Object *arg0_ptr,
						 Object *arg1_ptr,
						 Object *arg2_ptr)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_callGetThreeObjects;
	Object_INIT(*arg0_ptr, me->oOOArg0);
	Object_INIT(*arg1_ptr, me->oOOArg1);
	Object_INIT(*arg2_ptr, me->oOOArg2);

	return me->retValue;
}

static int32_t CTestCallable_callAddInt(TestCallable *me, uint32_t inVal1_val,
					uint32_t inVal2_val,
					uint32_t *outVal_ptr)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_callAddInt;
	*outVal_ptr = inVal1_val + inVal2_val;

	return me->retValue;
}

static int32_t CTestCallable_returnError(TestCallable *me)
{
	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_returnError;

	return Object_ERROR;
}

static int32_t CTestCallable_callWithDelay(TestCallable *me,
					   int32_t *outResponseCounter)
{
	VERIFY_MEM(me);

	LOGD_PRINT("%s (%p)]\n", __FUNCTION__, me);
	me->op = ITestCallable_OP_callWithDelay;

	LOGD_PRINT("[%s] Client Sleeping... for %lu s\n", __FUNCTION__,
		  (me->responseCounter * 2));

	sleep(me->responseCounter * 2);
	++me->responseCounter;
	*outResponseCounter = me->responseCounter;

	LOGD_PRINT("[%s] Waking up setting counter to %lu, the output"
		   "counter is %d\n", __FUNCTION__, me->responseCounter,
		   *outResponseCounter);

	return me->retValue;
}

static int32_t CTestCallable_callCopyBuffer(TestCallable *me,
					    const void *inBuf, size_t inBuf_len,
					    void *outBuf, size_t outBuf_len,
					    size_t *outBuf_lenOut)
{
	(void)me;
	(void)inBuf;
	(void)inBuf_len;
	(void)outBuf;
	(void)outBuf_len;
	(void)outBuf_lenOut;

	return Object_OK;
}

static int32_t CTestCallable_callFuncWithBuffer(TestCallable *me,
						const void *arg_ptr,
						size_t arg_len)
{
	(void)me;
	(void)arg_ptr;
	(void)arg_len;

	return Object_OK;
}

static int32_t CTestCallable_callGetMemObject(TestCallable *me, Object *arg_ptr)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_callGetMemObject;

	create_and_assign_mem_obj(me->oOArg, arg_ptr);

	return me->retValue;
}

static int32_t CTestCallable_callGetMemObjectWithBufferIn(TestCallable *me,
							  Object *arg_ptr,
							  const void *arg1_ptr,
							  size_t arg1_len)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_callGetMemObjectWithBufferIn;

	if ((arg1_len != me->bArg_len) ||
		(0 != memcmp(arg1_ptr, me->bArg_ptr, arg1_len)))
		return me->retValueError;

	create_and_assign_mem_obj(me->oOArg, arg_ptr);
	return me->retValue;
}

static int32_t CTestCallable_callGetMemObjectWithBufferOut(TestCallable *me,
							   Object *arg_ptr,
							   void *arg1_ptr,
							   size_t arg1_len,
							   size_t *arg1_lenout)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_callGetMemObjectWithBufferOut;

	memset(arg1_ptr, 'A', arg1_len);
	*arg1_lenout = arg1_len;

	create_and_assign_mem_obj(me->oOArg, arg_ptr);
	return me->retValue;
}

static int32_t CTestCallable_callGetMemObjectWithBufferInAndOut(TestCallable *me,
								Object *arg_ptr,
								const void *arg1_ptr,
								size_t arg1_len,
								void *arg2_ptr,
								size_t arg2_len,
								size_t *arg2_lenout)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_callGetMemObjectWithBufferInAndOut;

	if ((arg1_len != me->bArg_len) ||
		(0 != memcmp(arg1_ptr, me->bArg_ptr, arg1_len)))
		return me->retValueError;

	memset(arg2_ptr, 'A', arg2_len);
	*arg2_lenout = arg2_len;

	create_and_assign_mem_obj(me->oOArg, arg_ptr);
	return me->retValue;
}

static int32_t CTestCallable_callGetTwoMemObjects(TestCallable *me,
						  Object *arg_ptr, Object *arg1_ptr)
{
	if (!me) {
		return Object_ERROR;
	}

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);
	me->counter++;
	me->op = ITestCallable_OP_callGetTwoMemObjects;

	create_and_assign_mem_obj(me->oOArg, arg_ptr);
	create_and_assign_mem_obj(me->oOArg, arg1_ptr);
	return me->retValue;
}

static ITestCallable_DEFINE_INVOKE(CTestCallable_invoke, CTestCallable_,
				   TestCallable *)

int32_t CTestCallable_open(Object remoteObj, Object root, Object *obj)
{
	TestCallable *me = HEAP_ZALLOC_TYPE(TestCallable);
	if (!me) {
		return Object_ERROR_KMEM;
	}

	// Keep a reference to the remote object
	Object_INIT(me->oOOArg, remoteObj);
	Object_INIT(me->oOArg, root);
	me->responseCounter = 0;
	me->refs = 1;
	me->op = -1;
	*obj = (Object){ CTestCallable_invoke, me };

	LOGD_PRINT("[%s (%p)]\n", __FUNCTION__, me);

	return Object_OK;
}
