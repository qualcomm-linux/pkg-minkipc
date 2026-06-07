// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "object.h"

typedef struct Credentials Credentials;

#if defined(__cplusplus)
extern "C" {
#endif

/** New Credentials.
 *
 * It's the responsability of the caller to make sure the buffer is valid for
 * the lifetime of the object, unless the doCopy is true */
Credentials *Credentials_new(void *buffer, size_t bufferLen, bool doCopy);

/** As an IIO object, not retained. */
OBJECT_NOT_RETAINED Object Credentials_asIIO(Credentials *me);

/** Release it */
int32_t Credentials_release(Credentials *me);

/** Set return value for IClientCredentials_read() */
void Credentials_setReturnValue(Credentials *me, int32_t retValue);

/** As an ICredentials object, not retained. */
OBJECT_NOT_RETAINED Object Credentials_asICredentials(Credentials *me);

/** Get the number of times it is queried via the IIO_readAtOffset interface. */
size_t Credentials_getCallCount(Credentials const *me);

#if defined(__cplusplus)
}
#endif

#endif  // CREDENTIALS_H
