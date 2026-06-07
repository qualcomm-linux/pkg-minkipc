// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef _COMMON_BASIC_FUNC_TEST_H_
#define _COMMON_BASIC_FUNC_TEST_H_

#include "msforwarder.h"
#include "object.h"

#define qt_local(o) qt_assert(MSForwarderFromObject(o) == NULL)
#define qt_remote(o) qt_assert(MSForwarderFromObject(o) != NULL)
#define qt_fd(o) qt_assert(FdWrapperFromObject(o) != NULL)

#if defined (__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* positive part start */
int32_t positiveMarshallingSendNullBIGetSingleBO(Object obj);
int32_t positiveMarshallingSendSingleBIGetSingleBO(Object obj);
int32_t positiveMarshallingSendSingleBIGetMultiBO(Object obj);
int32_t positiveMarshallingSendMultiBIGetMultiBO(Object obj);
int32_t positiveMarshallingSendNullOIGetSingleOO(Object obj);
int32_t positiveMarshallingSendSingleOIGetSingleOO(Object obj);
int32_t positiveMarshallingSendSingleOIGetMultiOO(Object obj);
int32_t positiveMarshallingSendMultiOIGetMultiOO(Object obj);
int32_t positiveMarshallingSendRemoteOIGetOO(Object obj);
int32_t positiveMarshallingSendLocalFdOIGetOO(Object obj);
int32_t positiveMarshallingMaxArgs(Object obj);
int32_t positiveMarshallingDataAligned(Object obj);
int32_t positiveMarshallingBigBufferBIBO(Object obj);
int32_t positiveServiceRegisterDeregisterLocal(Object registerServiceObj);
int32_t positiveServiceRegisterDeregisterRemote(Object registerServiceObj);
/* positive part end */

/* negative part start */
int32_t negativeMarshallingNullBuffNonzeroLenBO(Object obj);
int32_t negativeMarshallingOverMaxArgs(Object obj);
int32_t negativeServiceRegisterInvalidUID(Object registerServiceObj);
int32_t negativeServiceRegisterOverMax(Object registerServiceObj);
int32_t negativeServiceDoubleRegister(Object registerServiceObj);
/* negative part end */

#if defined (__cplusplus)
}
#endif /* __cplusplus */

#endif /* _COMMON_BASIC_FUNC_TEST_H_ */
