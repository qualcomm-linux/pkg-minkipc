// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
/** @cond */
#pragma once

#include <stdint.h>
#include "object.h"
#include "ITestModule.h"

#define ITestModule_DEFINE_INVOKE(func, prefix, type) \
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
      case ITestModule_OP_getTypeId: { \
        if (k != ObjectCounts_pack(0, 1, 0, 0) || \
          a[0].b.size != 4) { \
          break; \
        } \
        uint32_t *id_ptr = (uint32_t*)a[0].b.ptr; \
        return prefix##getTypeId(me, id_ptr); \
      } \
      case ITestModule_OP_getId: { \
        if (k != ObjectCounts_pack(0, 1, 0, 0) || \
          a[0].b.size != 4) { \
          break; \
        } \
        uint32_t *id_ptr = (uint32_t*)a[0].b.ptr; \
        return prefix##getId(me, id_ptr); \
      } \
      case ITestModule_OP_getMultiId: { \
        if (k != ObjectCounts_pack(0, 1, 0, 0) || \
          a[0].b.size != 12) { \
          break; \
        } \
        struct { \
          uint32_t m_id_1; \
          uint32_t m_id_2; \
          uint32_t m_id_3; \
        } *o = a[0].b.ptr; \
        return prefix##getMultiId(me, &o->m_id_1, &o->m_id_2, &o->m_id_3); \
      } \
      case ITestModule_OP_setId: { \
        if (k != ObjectCounts_pack(1, 0, 0, 0) || \
          a[0].b.size != 4) { \
          break; \
        } \
        const uint32_t *id_ptr = (const uint32_t*)a[0].b.ptr; \
        return prefix##setId(me, *id_ptr); \
      } \
      case ITestModule_OP_setMultiId: { \
        if (k != ObjectCounts_pack(1, 0, 0, 0) || \
          a[0].b.size != 12) { \
          break; \
        } \
        const struct { \
          uint32_t m_id_1; \
          uint32_t m_id_2; \
          uint32_t m_id_3; \
        } *i = a[0].b.ptr; \
        return prefix##setMultiId(me, i->m_id_1, i->m_id_2, i->m_id_3); \
      } \
      case ITestModule_OP_setAndGetMultiId: { \
        if (k != ObjectCounts_pack(1, 1, 0, 0) || \
          a[0].b.size != 12 || \
          a[1].b.size != 12) { \
          break; \
        } \
        const struct { \
          uint32_t m_id_1; \
          uint32_t m_id_2; \
          uint32_t m_id_3; \
        } *i = a[0].b.ptr; \
        struct { \
          uint32_t m_id_1; \
          uint32_t m_id_2; \
          uint32_t m_id_3; \
        } *o = a[1].b.ptr; \
        return prefix##setAndGetMultiId(me, i->m_id_1, i->m_id_2, i->m_id_3, &o->m_id_1, &o->m_id_2, &o->m_id_3); \
      } \
      case ITestModule_OP_getObject: { \
        if (k != ObjectCounts_pack(0, 0, 0, 1)) { \
          break; \
        } \
        return prefix##getObject(me, &a[0].o); \
      } \
      case ITestModule_OP_getMultiObject: { \
        if (k != ObjectCounts_pack(0, 0, 0, 3)) { \
          break; \
        } \
        return prefix##getMultiObject(me, &a[0].o, &a[1].o, &a[2].o); \
      } \
      case ITestModule_OP_setObject: { \
        if (k != ObjectCounts_pack(0, 0, 1, 0)) { \
          break; \
        } \
        return prefix##setObject(me, a[0].o); \
      } \
      case ITestModule_OP_setMultiObject: { \
        if (k != ObjectCounts_pack(0, 0, 3, 0)) { \
          break; \
        } \
        return prefix##setMultiObject(me, a[0].o, a[1].o, a[2].o); \
      } \
      case ITestModule_OP_setAndGetObj: { \
        if (k != ObjectCounts_pack(0, 0, 1, 1)) { \
          break; \
        } \
        return prefix##setAndGetObj(me, a[0].o, &a[1].o); \
      } \
      case ITestModule_OP_setLocalFdObjAndGet: { \
        if (k != ObjectCounts_pack(0, 0, 1, 1)) { \
          break; \
        } \
        return prefix##setLocalFdObjAndGet(me, a[0].o, &a[1].o); \
      } \
      case ITestModule_OP_setMsforwardObjAndGet: { \
        if (k != ObjectCounts_pack(0, 0, 1, 1)) { \
          break; \
        } \
        return prefix##setMsforwardObjAndGet(me, a[0].o, &a[1].o); \
      } \
      case ITestModule_OP_unalignedSet: { \
        if (k != ObjectCounts_pack(2, 1, 0, 0) || \
          a[1].b.size != 4 || \
          a[2].b.size != 4) { \
          break; \
        } \
        const void *magic_id_withWrongSize_ptr = (const void*)a[0].b.ptr; \
        size_t magic_id_withWrongSize_len = a[0].b.size / 1; \
        const uint32_t *magic_id_withRightSize_ptr = (const uint32_t*)a[1].b.ptr; \
        uint32_t *pid_ptr = (uint32_t*)a[2].b.ptr; \
        return prefix##unalignedSet(me, magic_id_withWrongSize_ptr, magic_id_withWrongSize_len, *magic_id_withRightSize_ptr, pid_ptr); \
      } \
      case ITestModule_OP_maxArgs: { \
        if (k != ObjectCounts_pack(15, 15, 15, 15)) { \
          break; \
        } \
        const void *bi_1_ptr = (const void*)a[0].b.ptr; \
        size_t bi_1_len = a[0].b.size / 1; \
        const void *bi_2_ptr = (const void*)a[1].b.ptr; \
        size_t bi_2_len = a[1].b.size / 1; \
        const void *bi_3_ptr = (const void*)a[2].b.ptr; \
        size_t bi_3_len = a[2].b.size / 1; \
        const void *bi_4_ptr = (const void*)a[3].b.ptr; \
        size_t bi_4_len = a[3].b.size / 1; \
        const void *bi_5_ptr = (const void*)a[4].b.ptr; \
        size_t bi_5_len = a[4].b.size / 1; \
        const void *bi_6_ptr = (const void*)a[5].b.ptr; \
        size_t bi_6_len = a[5].b.size / 1; \
        const void *bi_7_ptr = (const void*)a[6].b.ptr; \
        size_t bi_7_len = a[6].b.size / 1; \
        const void *bi_8_ptr = (const void*)a[7].b.ptr; \
        size_t bi_8_len = a[7].b.size / 1; \
        const void *bi_9_ptr = (const void*)a[8].b.ptr; \
        size_t bi_9_len = a[8].b.size / 1; \
        const void *bi_10_ptr = (const void*)a[9].b.ptr; \
        size_t bi_10_len = a[9].b.size / 1; \
        const void *bi_11_ptr = (const void*)a[10].b.ptr; \
        size_t bi_11_len = a[10].b.size / 1; \
        const void *bi_12_ptr = (const void*)a[11].b.ptr; \
        size_t bi_12_len = a[11].b.size / 1; \
        const void *bi_13_ptr = (const void*)a[12].b.ptr; \
        size_t bi_13_len = a[12].b.size / 1; \
        const void *bi_14_ptr = (const void*)a[13].b.ptr; \
        size_t bi_14_len = a[13].b.size / 1; \
        const void *bi_15_ptr = (const void*)a[14].b.ptr; \
        size_t bi_15_len = a[14].b.size / 1; \
        void *bo_1_ptr = (void*)a[15].b.ptr; \
        size_t bo_1_len = a[15].b.size / 1; \
        void *bo_2_ptr = (void*)a[16].b.ptr; \
        size_t bo_2_len = a[16].b.size / 1; \
        void *bo_3_ptr = (void*)a[17].b.ptr; \
        size_t bo_3_len = a[17].b.size / 1; \
        void *bo_4_ptr = (void*)a[18].b.ptr; \
        size_t bo_4_len = a[18].b.size / 1; \
        void *bo_5_ptr = (void*)a[19].b.ptr; \
        size_t bo_5_len = a[19].b.size / 1; \
        void *bo_6_ptr = (void*)a[20].b.ptr; \
        size_t bo_6_len = a[20].b.size / 1; \
        void *bo_7_ptr = (void*)a[21].b.ptr; \
        size_t bo_7_len = a[21].b.size / 1; \
        void *bo_8_ptr = (void*)a[22].b.ptr; \
        size_t bo_8_len = a[22].b.size / 1; \
        void *bo_9_ptr = (void*)a[23].b.ptr; \
        size_t bo_9_len = a[23].b.size / 1; \
        void *bo_10_ptr = (void*)a[24].b.ptr; \
        size_t bo_10_len = a[24].b.size / 1; \
        void *bo_11_ptr = (void*)a[25].b.ptr; \
        size_t bo_11_len = a[25].b.size / 1; \
        void *bo_12_ptr = (void*)a[26].b.ptr; \
        size_t bo_12_len = a[26].b.size / 1; \
        void *bo_13_ptr = (void*)a[27].b.ptr; \
        size_t bo_13_len = a[27].b.size / 1; \
        void *bo_14_ptr = (void*)a[28].b.ptr; \
        size_t bo_14_len = a[28].b.size / 1; \
        void *bo_15_ptr = (void*)a[29].b.ptr; \
        size_t bo_15_len = a[29].b.size / 1; \
        int32_t r = prefix##maxArgs(me, bi_1_ptr, bi_1_len, bi_2_ptr, bi_2_len, bi_3_ptr, bi_3_len, bi_4_ptr, bi_4_len, bi_5_ptr, bi_5_len, bi_6_ptr, bi_6_len, bi_7_ptr, bi_7_len, bi_8_ptr, bi_8_len, bi_9_ptr, bi_9_len, bi_10_ptr, bi_10_len, bi_11_ptr, bi_11_len, bi_12_ptr, bi_12_len, bi_13_ptr, bi_13_len, bi_14_ptr, bi_14_len, bi_15_ptr, bi_15_len, bo_1_ptr, bo_1_len, &bo_1_len, bo_2_ptr, bo_2_len, &bo_2_len, bo_3_ptr, bo_3_len, &bo_3_len, bo_4_ptr, bo_4_len, &bo_4_len, bo_5_ptr, bo_5_len, &bo_5_len, bo_6_ptr, bo_6_len, &bo_6_len, bo_7_ptr, bo_7_len, &bo_7_len, bo_8_ptr, bo_8_len, &bo_8_len, bo_9_ptr, bo_9_len, &bo_9_len, bo_10_ptr, bo_10_len, &bo_10_len, bo_11_ptr, bo_11_len, &bo_11_len, bo_12_ptr, bo_12_len, &bo_12_len, bo_13_ptr, bo_13_len, &bo_13_len, bo_14_ptr, bo_14_len, &bo_14_len, bo_15_ptr, bo_15_len, &bo_15_len, a[30].o, a[31].o, a[32].o, a[33].o, a[34].o, a[35].o, a[36].o, a[37].o, a[38].o, a[39].o, a[40].o, a[41].o, a[42].o, a[43].o, a[44].o, &a[45].o, &a[46].o, &a[47].o, &a[48].o, &a[49].o, &a[50].o, &a[51].o, &a[52].o, &a[53].o, &a[54].o, &a[55].o, &a[56].o, &a[57].o, &a[58].o, &a[59].o); \
        a[15].b.size = bo_1_len * 1; \
        a[16].b.size = bo_2_len * 1; \
        a[17].b.size = bo_3_len * 1; \
        a[18].b.size = bo_4_len * 1; \
        a[19].b.size = bo_5_len * 1; \
        a[20].b.size = bo_6_len * 1; \
        a[21].b.size = bo_7_len * 1; \
        a[22].b.size = bo_8_len * 1; \
        a[23].b.size = bo_9_len * 1; \
        a[24].b.size = bo_10_len * 1; \
        a[25].b.size = bo_11_len * 1; \
        a[26].b.size = bo_12_len * 1; \
        a[27].b.size = bo_13_len * 1; \
        a[28].b.size = bo_14_len * 1; \
        a[29].b.size = bo_15_len * 1; \
        return r; \
      } \
      case ITestModule_OP_echo: { \
        if (k != ObjectCounts_pack(1, 1, 0, 0)) { \
          break; \
        } \
        const void *echo_in_ptr = (const void*)a[0].b.ptr; \
        size_t echo_in_len = a[0].b.size / 1; \
        void *echo_out_ptr = (void*)a[1].b.ptr; \
        size_t echo_out_len = a[1].b.size / 1; \
        int32_t r = prefix##echo(me, echo_in_ptr, echo_in_len, echo_out_ptr, echo_out_len, &echo_out_len); \
        a[1].b.size = echo_out_len * 1; \
        return r; \
      } \
      case ITestModule_OP_bufferOutNull: { \
        if (k != ObjectCounts_pack(0, 1, 0, 0)) { \
          break; \
        } \
        void *null_buffer_ptr = (void*)a[0].b.ptr; \
        size_t null_buffer_len = a[0].b.size / 1; \
        int32_t r = prefix##bufferOutNull(me, null_buffer_ptr, null_buffer_len, &null_buffer_len); \
        a[0].b.size = null_buffer_len * 1; \
        return r; \
      } \
      case ITestModule_OP_objectOutNull: { \
        if (k != ObjectCounts_pack(0, 0, 0, 2)) { \
          break; \
        } \
        return prefix##objectOutNull(me, &a[0].o, &a[1].o); \
      } \
      case ITestModule_OP_overMaxArgs: { \
        if (k != ObjectCounts_pack(16, 0, 0, 0)) { \
          break; \
        } \
        const void *bi_1_ptr = (const void*)a[0].b.ptr; \
        size_t bi_1_len = a[0].b.size / 1; \
        const void *bi_2_ptr = (const void*)a[1].b.ptr; \
        size_t bi_2_len = a[1].b.size / 1; \
        const void *bi_3_ptr = (const void*)a[2].b.ptr; \
        size_t bi_3_len = a[2].b.size / 1; \
        const void *bi_4_ptr = (const void*)a[3].b.ptr; \
        size_t bi_4_len = a[3].b.size / 1; \
        const void *bi_5_ptr = (const void*)a[4].b.ptr; \
        size_t bi_5_len = a[4].b.size / 1; \
        const void *bi_6_ptr = (const void*)a[5].b.ptr; \
        size_t bi_6_len = a[5].b.size / 1; \
        const void *bi_7_ptr = (const void*)a[6].b.ptr; \
        size_t bi_7_len = a[6].b.size / 1; \
        const void *bi_8_ptr = (const void*)a[7].b.ptr; \
        size_t bi_8_len = a[7].b.size / 1; \
        const void *bi_9_ptr = (const void*)a[8].b.ptr; \
        size_t bi_9_len = a[8].b.size / 1; \
        const void *bi_10_ptr = (const void*)a[9].b.ptr; \
        size_t bi_10_len = a[9].b.size / 1; \
        const void *bi_11_ptr = (const void*)a[10].b.ptr; \
        size_t bi_11_len = a[10].b.size / 1; \
        const void *bi_12_ptr = (const void*)a[11].b.ptr; \
        size_t bi_12_len = a[11].b.size / 1; \
        const void *bi_13_ptr = (const void*)a[12].b.ptr; \
        size_t bi_13_len = a[12].b.size / 1; \
        const void *bi_14_ptr = (const void*)a[13].b.ptr; \
        size_t bi_14_len = a[13].b.size / 1; \
        const void *bi_15_ptr = (const void*)a[14].b.ptr; \
        size_t bi_15_len = a[14].b.size / 1; \
        const void *bi_16_ptr = (const void*)a[15].b.ptr; \
        size_t bi_16_len = a[15].b.size / 1; \
        return prefix##overMaxArgs(me, bi_1_ptr, bi_1_len, bi_2_ptr, bi_2_len, bi_3_ptr, bi_3_len, bi_4_ptr, bi_4_len, bi_5_ptr, bi_5_len, bi_6_ptr, bi_6_len, bi_7_ptr, bi_7_len, bi_8_ptr, bi_8_len, bi_9_ptr, bi_9_len, bi_10_ptr, bi_10_len, bi_11_ptr, bi_11_len, bi_12_ptr, bi_12_len, bi_13_ptr, bi_13_len, bi_14_ptr, bi_14_len, bi_15_ptr, bi_15_len, bi_16_ptr, bi_16_len); \
      } \
      case ITestModule_OP_invocationTransferOut: { \
        if (k != ObjectCounts_pack(1, 1, 0, 1)) { \
          break; \
        } \
        const void *invocation_in_ptr = (const void*)a[0].b.ptr; \
        size_t invocation_in_len = a[0].b.size / 1; \
        void *invocation_out_ptr = (void*)a[1].b.ptr; \
        size_t invocation_out_len = a[1].b.size / 1; \
        int32_t r = prefix##invocationTransferOut(me, invocation_in_ptr, invocation_in_len, invocation_out_ptr, invocation_out_len, &invocation_out_len, &a[2].o); \
        a[1].b.size = invocation_out_len * 1; \
        return r; \
      } \
    } \
    return Object_ERROR_INVALID; \
  }
