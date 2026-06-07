// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __MY_TESTIMODULE_H
#define __MY_TESTIMODULE_H

#include "object.h"

/***********************************************************************
 * A simple module object, for testing
 * ********************************************************************/
typedef struct {
    int32_t refs;
    size_t count;
    uint32_t id;
    Object toBeReturned;
    Object lastRemoteCredentials;
} MyTestIModule;

int32_t MyTestIModule_invoke(ObjectCxt h, ObjectOp op, ObjectArg *a, ObjectCounts k);

/***********************************************************************
 * A simple service object, for testing
 * ********************************************************************/
typedef struct {
    int32_t refs;
} MyTestService;

int32_t MyTestService_invoke(ObjectCxt h, ObjectOp op, ObjectArg *a, ObjectCounts k);

#endif
