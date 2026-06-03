// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _IMODULE_HELPER_H_
#define _IMODULE_HELPER_H_

extern "C" {
#include "object.h"
}

/* Here will register an IModule with a dummy PID
 * because we don't really need a REAL process(TProcess) to test
 * TModule/IModule.
 */
void __registerIModule(int idx, Object *myModObj);

#endif  // _IMODULE_HELPER_H_
