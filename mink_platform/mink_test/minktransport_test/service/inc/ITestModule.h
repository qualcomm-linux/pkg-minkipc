// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <stdint.h>
#include "object.h"

#define ITestModule_ERROR_NOT_FOUND INT32_C(10)
#define ITestModule_ERROR_NAME_SIZE INT32_C(11)
#define ITestModule_ERROR_VALUE_SIZE INT32_C(12)

#define ITestModule_OP_getTypeId              0
#define ITestModule_OP_getId                  1
#define ITestModule_OP_getMultiId             2
#define ITestModule_OP_setId                  3
#define ITestModule_OP_setMultiId             4
#define ITestModule_OP_setAndGetMultiId       5
#define ITestModule_OP_getObject              6
#define ITestModule_OP_getMultiObject         7
#define ITestModule_OP_setObject              8
#define ITestModule_OP_setMultiObject         9
#define ITestModule_OP_setAndGetObj           10
#define ITestModule_OP_setLocalFdObjAndGet    11
#define ITestModule_OP_setMsforwardObjAndGet  12
#define ITestModule_OP_unalignedSet           13
#define ITestModule_OP_maxArgs                14
#define ITestModule_OP_echo                   15
#define ITestModule_OP_bufferOutNull          16
#define ITestModule_OP_objectOutNull          17
#define ITestModule_OP_overMaxArgs            18
#define ITestModule_OP_invocationTransferOut  19

static inline int32_t
ITestModule_release(Object self)
{
  return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
ITestModule_retain(Object self)
{
  return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
ITestModule_getTypeId(Object self, uint32_t *id_ptr)
{
  ObjectArg a[1]={{{0,0}}};
  a[0].b = (ObjectBuf) { id_ptr, sizeof(uint32_t) };

  return Object_invoke(self, ITestModule_OP_getTypeId, a, ObjectCounts_pack(0, 1, 0, 0));
}

static inline int32_t
ITestModule_getId(Object self, uint32_t *id_ptr)
{
  ObjectArg a[1]={{{0,0}}};
  a[0].b = (ObjectBuf) { id_ptr, sizeof(uint32_t) };

  return Object_invoke(self, ITestModule_OP_getId, a, ObjectCounts_pack(0, 1, 0, 0));
}

static inline int32_t
ITestModule_getMultiId(Object self, uint32_t *id_1_ptr, uint32_t *id_2_ptr, uint32_t *id_3_ptr)
{
  ObjectArg a[1]={{{0,0}}};
  struct {
    uint32_t m_id_1;
    uint32_t m_id_2;
    uint32_t m_id_3;
  } o = {0};
  a[0].b = (ObjectBuf) { &o, 12 };

  int32_t result = Object_invoke(self, ITestModule_OP_getMultiId, a, ObjectCounts_pack(0, 1, 0, 0));

  *id_1_ptr = o.m_id_1;
  *id_2_ptr = o.m_id_2;
  *id_3_ptr = o.m_id_3;

  return result;
}

static inline int32_t
ITestModule_setId(Object self, uint32_t id_val)
{
  ObjectArg a[1]={{{0,0}}};
  a[0].b = (ObjectBuf) { &id_val, sizeof(uint32_t) };

  return Object_invoke(self, ITestModule_OP_setId, a, ObjectCounts_pack(1, 0, 0, 0));
}

static inline int32_t
ITestModule_setMultiId(Object self, uint32_t id_1_val, uint32_t id_2_val, uint32_t id_3_val)
{
  ObjectArg a[1]={{{0,0}}};
  struct {
    uint32_t m_id_1;
    uint32_t m_id_2;
    uint32_t m_id_3;
  } i;
  a[0].b = (ObjectBuf) { &i, 12 };
  i.m_id_1 = id_1_val;
  i.m_id_2 = id_2_val;
  i.m_id_3 = id_3_val;

  return Object_invoke(self, ITestModule_OP_setMultiId, a, ObjectCounts_pack(1, 0, 0, 0));
}

static inline int32_t
ITestModule_setAndGetMultiId(Object self, uint32_t id_1_val, uint32_t id_2_val, uint32_t id_3_val, uint32_t *id_1_ptr, uint32_t *id_2_ptr, uint32_t *id_3_ptr)
{
  ObjectArg a[2]={{{0,0}}};
  struct {
    uint32_t m_id_1;
    uint32_t m_id_2;
    uint32_t m_id_3;
  } i;
  a[0].b = (ObjectBuf) { &i, 12 };
  struct {
    uint32_t m_id_1;
    uint32_t m_id_2;
    uint32_t m_id_3;
  } o = {0};
  a[1].b = (ObjectBuf) { &o, 12 };
  i.m_id_1 = id_1_val;
  i.m_id_2 = id_2_val;
  i.m_id_3 = id_3_val;

  int32_t result = Object_invoke(self, ITestModule_OP_setAndGetMultiId, a, ObjectCounts_pack(1, 1, 0, 0));

  *id_1_ptr = o.m_id_1;
  *id_2_ptr = o.m_id_2;
  *id_3_ptr = o.m_id_3;

  return result;
}

static inline int32_t
ITestModule_getObject(Object self, Object *obj_ptr)
{
  ObjectArg a[1]={{{0,0}}};

  int32_t result = Object_invoke(self, ITestModule_OP_getObject, a, ObjectCounts_pack(0, 0, 0, 1));

  *obj_ptr = a[0].o;

  return result;
}

static inline int32_t
ITestModule_getMultiObject(Object self, Object *obj_1_ptr, Object *obj_2_ptr, Object *obj_3_ptr)
{
  ObjectArg a[3]={{{0,0}}};

  int32_t result = Object_invoke(self, ITestModule_OP_getMultiObject, a, ObjectCounts_pack(0, 0, 0, 3));

  *obj_1_ptr = a[0].o;
  *obj_2_ptr = a[1].o;
  *obj_3_ptr = a[2].o;

  return result;
}

static inline int32_t
ITestModule_setObject(Object self, Object obj_val)
{
  ObjectArg a[1]={{{0,0}}};
  a[0].o = obj_val;

  return Object_invoke(self, ITestModule_OP_setObject, a, ObjectCounts_pack(0, 0, 1, 0));
}

static inline int32_t
ITestModule_setMultiObject(Object self, Object obj_1_val, Object obj_2_val, Object obj_3_val)
{
  ObjectArg a[3]={{{0,0}}};
  a[0].o = obj_1_val;
  a[1].o = obj_2_val;
  a[2].o = obj_3_val;

  return Object_invoke(self, ITestModule_OP_setMultiObject, a, ObjectCounts_pack(0, 0, 3, 0));
}

static inline int32_t
ITestModule_setAndGetObj(Object self, Object obj_toSend_val, Object *obj_toGet_ptr)
{
  ObjectArg a[2]={{{0,0}}};
  a[0].o = obj_toSend_val;

  int32_t result = Object_invoke(self, ITestModule_OP_setAndGetObj, a, ObjectCounts_pack(0, 0, 1, 1));

  *obj_toGet_ptr = a[1].o;

  return result;
}

static inline int32_t
ITestModule_setLocalFdObjAndGet(Object self, Object fdObj_val, Object *fdObj_toGet_ptr)
{
  ObjectArg a[2]={{{0,0}}};
  a[0].o = fdObj_val;

  int32_t result = Object_invoke(self, ITestModule_OP_setLocalFdObjAndGet, a, ObjectCounts_pack(0, 0, 1, 1));

  *fdObj_toGet_ptr = a[1].o;

  return result;
}

static inline int32_t
ITestModule_setMsforwardObjAndGet(Object self, Object mfObj_val, Object *mfObj_toGet_ptr)
{
  ObjectArg a[2]={{{0,0}}};
  a[0].o = mfObj_val;

  int32_t result = Object_invoke(self, ITestModule_OP_setMsforwardObjAndGet, a, ObjectCounts_pack(0, 0, 1, 1));

  *mfObj_toGet_ptr = a[1].o;

  return result;
}

static inline int32_t
ITestModule_unalignedSet(Object self, const void *magic_id_withWrongSize_ptr, size_t magic_id_withWrongSize_len, uint32_t magic_id_withRightSize_val, uint32_t *pid_ptr)
{
  ObjectArg a[3]={{{0,0}}};
  a[0].bi = (ObjectBufIn) { magic_id_withWrongSize_ptr, magic_id_withWrongSize_len * 1 };
  a[1].b = (ObjectBuf) { &magic_id_withRightSize_val, sizeof(uint32_t) };
  a[2].b = (ObjectBuf) { pid_ptr, sizeof(uint32_t) };

  return Object_invoke(self, ITestModule_OP_unalignedSet, a, ObjectCounts_pack(2, 1, 0, 0));
}

static inline int32_t
ITestModule_maxArgs(Object self, const void *bi_1_ptr, size_t bi_1_len, const void *bi_2_ptr, size_t bi_2_len, const void *bi_3_ptr, size_t bi_3_len, const void *bi_4_ptr, size_t bi_4_len, const void *bi_5_ptr, size_t bi_5_len, const void *bi_6_ptr, size_t bi_6_len, const void *bi_7_ptr, size_t bi_7_len, const void *bi_8_ptr, size_t bi_8_len, const void *bi_9_ptr, size_t bi_9_len, const void *bi_10_ptr, size_t bi_10_len, const void *bi_11_ptr, size_t bi_11_len, const void *bi_12_ptr, size_t bi_12_len, const void *bi_13_ptr, size_t bi_13_len, const void *bi_14_ptr, size_t bi_14_len, const void *bi_15_ptr, size_t bi_15_len, void *bo_1_ptr, size_t bo_1_len, size_t *bo_1_lenout, void *bo_2_ptr, size_t bo_2_len, size_t *bo_2_lenout, void *bo_3_ptr, size_t bo_3_len, size_t *bo_3_lenout, void *bo_4_ptr, size_t bo_4_len, size_t *bo_4_lenout, void *bo_5_ptr, size_t bo_5_len, size_t *bo_5_lenout, void *bo_6_ptr, size_t bo_6_len, size_t *bo_6_lenout, void *bo_7_ptr, size_t bo_7_len, size_t *bo_7_lenout, void *bo_8_ptr, size_t bo_8_len, size_t *bo_8_lenout, void *bo_9_ptr, size_t bo_9_len, size_t *bo_9_lenout, void *bo_10_ptr, size_t bo_10_len, size_t *bo_10_lenout, void *bo_11_ptr, size_t bo_11_len, size_t *bo_11_lenout, void *bo_12_ptr, size_t bo_12_len, size_t *bo_12_lenout, void *bo_13_ptr, size_t bo_13_len, size_t *bo_13_lenout, void *bo_14_ptr, size_t bo_14_len, size_t *bo_14_lenout, void *bo_15_ptr, size_t bo_15_len, size_t *bo_15_lenout, Object oi_1_val, Object oi_2_val, Object oi_3_val, Object oi_4_val, Object oi_5_val, Object oi_6_val, Object oi_7_val, Object oi_8_val, Object oi_9_val, Object oi_10_val, Object oi_11_val, Object oi_12_val, Object oi_13_val, Object oi_14_val, Object oi_15_val, Object *oo_1_ptr, Object *oo_2_ptr, Object *oo_3_ptr, Object *oo_4_ptr, Object *oo_5_ptr, Object *oo_6_ptr, Object *oo_7_ptr, Object *oo_8_ptr, Object *oo_9_ptr, Object *oo_10_ptr, Object *oo_11_ptr, Object *oo_12_ptr, Object *oo_13_ptr, Object *oo_14_ptr, Object *oo_15_ptr)
{
  ObjectArg a[60]={{{0,0}}};
  a[0].bi = (ObjectBufIn) { bi_1_ptr, bi_1_len * 1 };
  a[1].bi = (ObjectBufIn) { bi_2_ptr, bi_2_len * 1 };
  a[2].bi = (ObjectBufIn) { bi_3_ptr, bi_3_len * 1 };
  a[3].bi = (ObjectBufIn) { bi_4_ptr, bi_4_len * 1 };
  a[4].bi = (ObjectBufIn) { bi_5_ptr, bi_5_len * 1 };
  a[5].bi = (ObjectBufIn) { bi_6_ptr, bi_6_len * 1 };
  a[6].bi = (ObjectBufIn) { bi_7_ptr, bi_7_len * 1 };
  a[7].bi = (ObjectBufIn) { bi_8_ptr, bi_8_len * 1 };
  a[8].bi = (ObjectBufIn) { bi_9_ptr, bi_9_len * 1 };
  a[9].bi = (ObjectBufIn) { bi_10_ptr, bi_10_len * 1 };
  a[10].bi = (ObjectBufIn) { bi_11_ptr, bi_11_len * 1 };
  a[11].bi = (ObjectBufIn) { bi_12_ptr, bi_12_len * 1 };
  a[12].bi = (ObjectBufIn) { bi_13_ptr, bi_13_len * 1 };
  a[13].bi = (ObjectBufIn) { bi_14_ptr, bi_14_len * 1 };
  a[14].bi = (ObjectBufIn) { bi_15_ptr, bi_15_len * 1 };
  a[15].b = (ObjectBuf) { bo_1_ptr, bo_1_len * 1 };
  a[16].b = (ObjectBuf) { bo_2_ptr, bo_2_len * 1 };
  a[17].b = (ObjectBuf) { bo_3_ptr, bo_3_len * 1 };
  a[18].b = (ObjectBuf) { bo_4_ptr, bo_4_len * 1 };
  a[19].b = (ObjectBuf) { bo_5_ptr, bo_5_len * 1 };
  a[20].b = (ObjectBuf) { bo_6_ptr, bo_6_len * 1 };
  a[21].b = (ObjectBuf) { bo_7_ptr, bo_7_len * 1 };
  a[22].b = (ObjectBuf) { bo_8_ptr, bo_8_len * 1 };
  a[23].b = (ObjectBuf) { bo_9_ptr, bo_9_len * 1 };
  a[24].b = (ObjectBuf) { bo_10_ptr, bo_10_len * 1 };
  a[25].b = (ObjectBuf) { bo_11_ptr, bo_11_len * 1 };
  a[26].b = (ObjectBuf) { bo_12_ptr, bo_12_len * 1 };
  a[27].b = (ObjectBuf) { bo_13_ptr, bo_13_len * 1 };
  a[28].b = (ObjectBuf) { bo_14_ptr, bo_14_len * 1 };
  a[29].b = (ObjectBuf) { bo_15_ptr, bo_15_len * 1 };
  a[30].o = oi_1_val;
  a[31].o = oi_2_val;
  a[32].o = oi_3_val;
  a[33].o = oi_4_val;
  a[34].o = oi_5_val;
  a[35].o = oi_6_val;
  a[36].o = oi_7_val;
  a[37].o = oi_8_val;
  a[38].o = oi_9_val;
  a[39].o = oi_10_val;
  a[40].o = oi_11_val;
  a[41].o = oi_12_val;
  a[42].o = oi_13_val;
  a[43].o = oi_14_val;
  a[44].o = oi_15_val;

  int32_t result = Object_invoke(self, ITestModule_OP_maxArgs, a, ObjectCounts_pack(15, 15, 15, 15));

  *bo_1_lenout = a[15].b.size / 1;
  *bo_2_lenout = a[16].b.size / 1;
  *bo_3_lenout = a[17].b.size / 1;
  *bo_4_lenout = a[18].b.size / 1;
  *bo_5_lenout = a[19].b.size / 1;
  *bo_6_lenout = a[20].b.size / 1;
  *bo_7_lenout = a[21].b.size / 1;
  *bo_8_lenout = a[22].b.size / 1;
  *bo_9_lenout = a[23].b.size / 1;
  *bo_10_lenout = a[24].b.size / 1;
  *bo_11_lenout = a[25].b.size / 1;
  *bo_12_lenout = a[26].b.size / 1;
  *bo_13_lenout = a[27].b.size / 1;
  *bo_14_lenout = a[28].b.size / 1;
  *bo_15_lenout = a[29].b.size / 1;
  *oo_1_ptr = a[45].o;
  *oo_2_ptr = a[46].o;
  *oo_3_ptr = a[47].o;
  *oo_4_ptr = a[48].o;
  *oo_5_ptr = a[49].o;
  *oo_6_ptr = a[50].o;
  *oo_7_ptr = a[51].o;
  *oo_8_ptr = a[52].o;
  *oo_9_ptr = a[53].o;
  *oo_10_ptr = a[54].o;
  *oo_11_ptr = a[55].o;
  *oo_12_ptr = a[56].o;
  *oo_13_ptr = a[57].o;
  *oo_14_ptr = a[58].o;
  *oo_15_ptr = a[59].o;

  return result;
}

static inline int32_t
ITestModule_echo(Object self, const void *echo_in_ptr, size_t echo_in_len, void *echo_out_ptr, size_t echo_out_len, size_t *echo_out_lenout)
{
  ObjectArg a[2]={{{0,0}}};
  a[0].bi = (ObjectBufIn) { echo_in_ptr, echo_in_len * 1 };
  a[1].b = (ObjectBuf) { echo_out_ptr, echo_out_len * 1 };

  int32_t result = Object_invoke(self, ITestModule_OP_echo, a, ObjectCounts_pack(1, 1, 0, 0));

  *echo_out_lenout = a[1].b.size / 1;

  return result;
}

static inline int32_t
ITestModule_bufferOutNull(Object self, void *null_buffer_ptr, size_t null_buffer_len, size_t *null_buffer_lenout)
{
  ObjectArg a[1]={{{0,0}}};
  a[0].b = (ObjectBuf) { null_buffer_ptr, null_buffer_len * 1 };

  int32_t result = Object_invoke(self, ITestModule_OP_bufferOutNull, a, ObjectCounts_pack(0, 1, 0, 0));

  *null_buffer_lenout = a[0].b.size / 1;

  return result;
}

static inline int32_t
ITestModule_objectOutNull(Object self, Object *null_obj_ptr, Object *obj_ptr)
{
  ObjectArg a[2]={{{0,0}}};

  int32_t result = Object_invoke(self, ITestModule_OP_objectOutNull, a, ObjectCounts_pack(0, 0, 0, 2));

  *null_obj_ptr = a[0].o;
  *obj_ptr = a[1].o;

  return result;
}

static inline int32_t
ITestModule_overMaxArgs(Object self, const void *bi_1_ptr, size_t bi_1_len, const void *bi_2_ptr, size_t bi_2_len, const void *bi_3_ptr, size_t bi_3_len, const void *bi_4_ptr, size_t bi_4_len, const void *bi_5_ptr, size_t bi_5_len, const void *bi_6_ptr, size_t bi_6_len, const void *bi_7_ptr, size_t bi_7_len, const void *bi_8_ptr, size_t bi_8_len, const void *bi_9_ptr, size_t bi_9_len, const void *bi_10_ptr, size_t bi_10_len, const void *bi_11_ptr, size_t bi_11_len, const void *bi_12_ptr, size_t bi_12_len, const void *bi_13_ptr, size_t bi_13_len, const void *bi_14_ptr, size_t bi_14_len, const void *bi_15_ptr, size_t bi_15_len, const void *bi_16_ptr, size_t bi_16_len)
{
  ObjectArg a[16]={{{0,0}}};
  a[0].bi = (ObjectBufIn) { bi_1_ptr, bi_1_len * 1 };
  a[1].bi = (ObjectBufIn) { bi_2_ptr, bi_2_len * 1 };
  a[2].bi = (ObjectBufIn) { bi_3_ptr, bi_3_len * 1 };
  a[3].bi = (ObjectBufIn) { bi_4_ptr, bi_4_len * 1 };
  a[4].bi = (ObjectBufIn) { bi_5_ptr, bi_5_len * 1 };
  a[5].bi = (ObjectBufIn) { bi_6_ptr, bi_6_len * 1 };
  a[6].bi = (ObjectBufIn) { bi_7_ptr, bi_7_len * 1 };
  a[7].bi = (ObjectBufIn) { bi_8_ptr, bi_8_len * 1 };
  a[8].bi = (ObjectBufIn) { bi_9_ptr, bi_9_len * 1 };
  a[9].bi = (ObjectBufIn) { bi_10_ptr, bi_10_len * 1 };
  a[10].bi = (ObjectBufIn) { bi_11_ptr, bi_11_len * 1 };
  a[11].bi = (ObjectBufIn) { bi_12_ptr, bi_12_len * 1 };
  a[12].bi = (ObjectBufIn) { bi_13_ptr, bi_13_len * 1 };
  a[13].bi = (ObjectBufIn) { bi_14_ptr, bi_14_len * 1 };
  a[14].bi = (ObjectBufIn) { bi_15_ptr, bi_15_len * 1 };
  a[15].bi = (ObjectBufIn) { bi_16_ptr, bi_16_len * 1 };

  return Object_invoke(self, ITestModule_OP_overMaxArgs, a, ObjectCounts_pack(16, 0, 0, 0));
}

static inline int32_t
ITestModule_invocationTransferOut(Object self, const void *invocation_in_ptr, size_t invocation_in_len, void *invocation_out_ptr, size_t invocation_out_len, size_t *invocation_out_lenout, Object *invocation_obj_out_ptr)
{
  ObjectArg a[3]={{{0,0}}};
  a[0].bi = (ObjectBufIn) { invocation_in_ptr, invocation_in_len * 1 };
  a[1].b = (ObjectBuf) { invocation_out_ptr, invocation_out_len * 1 };

  int32_t result = Object_invoke(self, ITestModule_OP_invocationTransferOut, a, ObjectCounts_pack(1, 1, 0, 1));

  *invocation_out_lenout = a[1].b.size / 1;
  *invocation_obj_out_ptr = a[2].o;

  return result;
}
