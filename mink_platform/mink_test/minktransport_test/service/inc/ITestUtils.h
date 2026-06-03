// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
/** @cond */
#pragma once

#include <stdint.h>
#include "object.h"

#define ITestUtils_ERROR_NOT_FOUND INT32_C(10)
#define ITestUtils_ERROR_NAME_SIZE INT32_C(11)
#define ITestUtils_ERROR_VALUE_SIZE INT32_C(12)

#define ITestUtils_OP_getTypeId           0
#define ITestUtils_OP_bufferEcho          1
#define ITestUtils_OP_bufferPlus          2
#define ITestUtils_OP_invocationTransfer  3

static inline int32_t
ITestUtils_release(Object self)
{
  return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
ITestUtils_retain(Object self)
{
  return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
ITestUtils_getTypeId(Object self, uint32_t *id_ptr)
{
  ObjectArg a[1]={{{0,0}}};
  a[0].b = (ObjectBuf) { id_ptr, sizeof(uint32_t) };

  return Object_invoke(self, ITestUtils_OP_getTypeId, a, ObjectCounts_pack(0, 1, 0, 0));
}

static inline int32_t
ITestUtils_bufferEcho(Object self, const void *echo_in_ptr, size_t echo_in_len, void *echo_out_ptr, size_t echo_out_len, size_t *echo_out_lenout)
{
  ObjectArg a[2]={{{0,0}}};
  a[0].bi = (ObjectBufIn) { echo_in_ptr, echo_in_len * 1 };
  a[1].b = (ObjectBuf) { echo_out_ptr, echo_out_len * 1 };

  int32_t result = Object_invoke(self, ITestUtils_OP_bufferEcho, a, ObjectCounts_pack(1, 1, 0, 0));

  *echo_out_lenout = a[1].b.size / 1;

  return result;
}

static inline int32_t
ITestUtils_bufferPlus(Object self, uint32_t value_a_val, uint32_t value_b_val, uint32_t *value_sum_ptr)
{
  ObjectArg a[2]={{{0,0}}};
  struct {
    uint32_t m_value_a;
    uint32_t m_value_b;
  } i;
  a[0].b = (ObjectBuf) { &i, 8 };
  i.m_value_a = value_a_val;
  i.m_value_b = value_b_val;
  a[1].b = (ObjectBuf) { value_sum_ptr, sizeof(uint32_t) };

  return Object_invoke(self, ITestUtils_OP_bufferPlus, a, ObjectCounts_pack(1, 1, 0, 0));
}

static inline int32_t
ITestUtils_invocationTransfer(Object self, const void *invocation_in_ptr, size_t invocation_in_len, Object invocetion_obj_in_val, void *invocation_out_ptr, size_t invocation_out_len, size_t *invocation_out_lenout, Object *invocation_obj_out_ptr)
{
  ObjectArg a[4]={{{0,0}}};
  a[0].bi = (ObjectBufIn) { invocation_in_ptr, invocation_in_len * 1 };
  a[2].o = invocetion_obj_in_val;
  a[1].b = (ObjectBuf) { invocation_out_ptr, invocation_out_len * 1 };

  int32_t result = Object_invoke(self, ITestUtils_OP_invocationTransfer, a, ObjectCounts_pack(1, 1, 1, 1));

  *invocation_out_lenout = a[1].b.size / 1;
  *invocation_obj_out_ptr = a[3].o;

  return result;
}
