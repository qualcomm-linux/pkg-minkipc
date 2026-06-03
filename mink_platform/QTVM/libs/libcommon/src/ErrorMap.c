// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "ErrorMap.h"
#include <stddef.h>
#include <stdint.h>

int32_t ErrorMap_convertWithDefault(ErrorMap const* me, int32_t error, int32_t generic)
{
    /* Transport errors pass unchanged */
    if (error < me->startConversionAt) {
        return error;
    }

    for (size_t i = 0; i < me->length; i++) {
        if (me->errors[i].errorFrom == error) {
            return me->errors[i].errorTo;
        }
    }

    return generic;
}

int32_t ErrorMap_convert(ErrorMap const* me, int32_t error)
{
    return ErrorMap_convertWithDefault(me, error, me->genericError);
}
