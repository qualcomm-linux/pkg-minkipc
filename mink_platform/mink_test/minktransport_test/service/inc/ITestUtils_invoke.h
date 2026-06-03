// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
/** @cond */
#pragma once

#include <stdint.h>
#include "object.h"
#include "ITestUtils.h"

#define ITestUtils_DEFINE_INVOKE(func, prefix, type) \
  int32_t func(ObjectCxt h, ObjectOp op, ObjectArg *a, ObjectCounts k) \
  { \
    type me = (type) h; \
    switch (ObjectOp_methodID(op)) { \
      case Object_OP_release: { \
        if (k != ObjectCounts_pack(0, 0, 0, 0)) { \
          break; \
        } \
        return prefix##release(me); \
      } \
      case Object_OP_retain: { \
        if (k != ObjectCounts_pack(0, 0, 0, 0)) { \
          break; \
        } \
        return prefix##retain(me); \
      } \
      case ITestUtils_OP_getTypeId: { \
        if (k != ObjectCounts_pack(0, 1, 0, 0) || \
          a[0].b.size != 4) { \
          break; \
        } \
        uint32_t *id_ptr = (uint32_t*)a[0].b.ptr; \
        return prefix##getTypeId(me, id_ptr); \
      } \
      case ITestUtils_OP_bufferEcho: { \
        if (k != ObjectCounts_pack(1, 1, 0, 0)) { \
          break; \
        } \
        const void *echo_in_ptr = (const void*)a[0].b.ptr; \
        size_t echo_in_len = a[0].b.size / 1; \
        void *echo_out_ptr = (void*)a[1].b.ptr; \
        size_t echo_out_len = a[1].b.size / 1; \
        int32_t r = prefix##bufferEcho(me, echo_in_ptr, echo_in_len, echo_out_ptr, echo_out_len, &echo_out_len); \
        a[1].b.size = echo_out_len * 1; \
        return r; \
      } \
      case ITestUtils_OP_bufferPlus: { \
        if (k != ObjectCounts_pack(1, 1, 0, 0) || \
          a[0].b.size != 8 || \
          a[1].b.size != 4) { \
          break; \
        } \
        const struct { \
          uint32_t m_value_a; \
          uint32_t m_value_b; \
        } *i = a[0].b.ptr; \
        uint32_t *value_sum_ptr = (uint32_t*)a[1].b.ptr; \
        return prefix##bufferPlus(me, i->m_value_a, i->m_value_b, value_sum_ptr); \
      } \
      case ITestUtils_OP_invocationTransfer: { \
        if (k != ObjectCounts_pack(1, 1, 1, 1)) { \
          break; \
        } \
        const void *invocation_in_ptr = (const void*)a[0].b.ptr; \
        size_t invocation_in_len = a[0].b.size / 1; \
        void *invocation_out_ptr = (void*)a[1].b.ptr; \
        size_t invocation_out_len = a[1].b.size / 1; \
        int32_t r = prefix##invocationTransfer(me, invocation_in_ptr, invocation_in_len, a[2].o, invocation_out_ptr, invocation_out_len, &invocation_out_len, &a[3].o); \
        a[1].b.size = invocation_out_len * 1; \
        return r; \
      } \
    } \
    return Object_ERROR_INVALID; \
  }
