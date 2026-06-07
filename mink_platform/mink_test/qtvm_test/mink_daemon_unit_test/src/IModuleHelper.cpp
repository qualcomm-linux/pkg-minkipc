// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
extern "C" {
#include "ICredentials.h"
#include "ITRegisterModule.h"
#include "CTRegisterModule.h"
#include "Credentials.h"
#include "CTRegisterModule_open.h"
#include "CTRegisterModule_priv.h"
}
#include <gtest/gtest.h>
#include "IModuleHelper.h"

/* Here will register an IModule with a dummy PID
 *   because we don't really need a REAL process(TProcess) to test TModule/IModule.
 */
void __registerIModule(int idx, Object *myModObj)
{
    char credStr[30];
    Credentials *cred;
    Object registerSVC = Object_NULL;

    snprintf(credStr, 30, "callerId=%d;foo=bar", idx);
    cred = Credentials_new(credStr, strlen(credStr), true);

    ASSERT_EQ(Object_OK,CTRegisterModule_open(CTRegisterModule_UID,
                                              Credentials_asICredentials(cred), &registerSVC));
    ASSERT_EQ(Object_OK, ITRegisterModule_registerIModule(registerSVC, *myModObj));

    ASSERT_EQ(Object_OK, ICredentials_release(Credentials_asICredentials(cred)));
    ASSERT_EQ(Object_OK, ITRegisterModule_release(registerSVC));
}
