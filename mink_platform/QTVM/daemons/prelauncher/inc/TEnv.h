// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TVMENV_H
#define __TVMENV_H

#include "object.h"

typedef struct TEnv TEnv;

/**
 * New IEnv class on top of our registration framework.
 *
 * IEnv extends IOpener.
 * */
Object TEnv_new();

#endif  // __TVMENV_H
