// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __XTZDCREDENTIALS_H
#define __XTZDCREDENTIALS_H

typedef struct XtzdCredentials XtzdCredentials;

#if defined (__cplusplus)
extern "C" {
#endif

int32_t XtzdCredentials_newFromCBO(Object linkCred, Object *objOut);

#if defined (__cplusplus)
}
#endif

#endif //__XTZDCREDENTIALS_H
