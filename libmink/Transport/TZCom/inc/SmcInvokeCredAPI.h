// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __SMCINVOKECREDAPI_H_
#define __SMCINVOKECREDAPI_H_

typedef enum {
    Success = 0,
    UnAuthorisedUser,
    InvalidArguments,
    DeathNotification,
    PkgNameNotFound,
    CertificateErr,
    ConnectionErr,
    DataConversionErr,
    OOBErr,
    OOMErr,
    InsufficientBufferErr,
    BufferOverFlowErr,
    EncodingErr,
} smcInvokeCred_ErrorCode;

typedef enum {
    AttrUid = 1,
    AttrPkgFlags,
    AttrPkgName,
    AttrPkgCert,
    AttrPermissions,
    AttrSystemTime
} smcInvokeCred_Attr;

#endif
