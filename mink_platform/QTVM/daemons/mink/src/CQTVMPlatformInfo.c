// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "QTVMPlatformInfo.h"
#include "TUtils.h"

#include "object.h"

static Object gEnvCred = Object_NULL;

int32_t CQTVMPlatformInfo_setEnvCred(Object envCred)
{
    int32_t ret = Object_OK;

    //envCred is only initialized when the mink process is created.
    T_CHECK(Object_isNull(gEnvCred));
    Object_INIT(gEnvCred, envCred);

exit:
    return ret;
}

int32_t CQTVMPlatformInfo_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    (void)credentials;
    int32_t ret = Object_OK;

    T_CHECK(!Object_isNull(gEnvCred));
    Object_INIT(*objOut, gEnvCred);

exit:
    return ret;

}
