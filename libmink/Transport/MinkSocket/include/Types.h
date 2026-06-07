// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _TYPES_H_
#define _TYPES_H_

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    UNIX,
    QIPCRTR,
    VSOCK,
    SIMULATED,
    QMSGQ,
} IPC_TYPE;

typedef enum {
    DISCARD = 1,
    CLIENTDOWN,
    SERVERDOWN,
    SUBSYSDOWN,
    VSOCKCASE,
    QMSGQCASE,
}PREPROCESS_STATE;

#if defined(__cplusplus)
}
#endif

#endif //_TYPES_H_
