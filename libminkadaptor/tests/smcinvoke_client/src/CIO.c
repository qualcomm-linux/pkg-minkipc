// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#include "CIO.h"
#include "IIO_invoke.h"
#include "memscpy.h"

typedef struct {
	atomic_int refs;
	void *cred_buffer;
	size_t cred_buffer_len;
} CIO;

static int32_t CIO_release(CIO *me)
{
	if (atomic_fetch_sub(&me->refs, 1) == 1) {
		free(me->cred_buffer);
		free(me);
	}
	return Object_OK;
}

static int32_t CIO_retain(CIO *me)
{
	atomic_fetch_add(&me->refs, 1);
	return Object_OK;
}

static int32_t CIO_getLength(CIO *me, uint64_t *len_ptr)
{
	*len_ptr = (uint64_t)me->cred_buffer_len;
	return Object_OK;
}

static int32_t CIO_readAtOffset(CIO *me, uint64_t offset_val, void *data_ptr,
				size_t data_len, size_t *data_lenout)
{
	if ((size_t)offset_val >= me->cred_buffer_len) {
		return IIO_ERROR_OFFSET_OUT_OF_BOUNDS;
	}

	size_t needed_len = me->cred_buffer_len - (size_t)offset_val;
	*data_lenout = memscpy(data_ptr, data_len,
			       (char *)me->cred_buffer + offset_val,
			       needed_len);
	return Object_OK;
}

static int32_t CIO_writeAtOffset(CIO *me, uint64_t offset_val,
				 const void *data_ptr, size_t data_len)
{
        (void)me;
        (void)offset_val;
        (void)data_ptr;
        (void)data_len;

	return Object_ERROR; //Cannot write to credential buffers
}

static IIO_DEFINE_INVOKE(IIO_invoke, CIO_, CIO *)

int32_t CIO_open(void *cred_buffer, size_t cred_buffer_len,
                 Object *objOut)
{
	CIO *me = (CIO *)malloc(sizeof(CIO));
	if (!me) {
		return Object_ERROR;
	}

	me->refs = 1;
	me->cred_buffer_len = cred_buffer_len;
	me->cred_buffer = cred_buffer;

	*objOut = (Object){ IIO_invoke, me };
	return Object_OK;
}
