// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <stdint.h>
#include "object.h"

/*----------------------------------------------------------------------------
 * Declared in applib and usable in user-defined processes
 * -------------------------------------------------------------------------*/
extern Object gTVMEnv;

/*----------------------------------------------------------------------------
 * Used in applib and implemented in user-defined processes
 * -------------------------------------------------------------------------*/

/**
 * Description: Retrieve service Object tied to UID
 *
 * In:          uid: The Universal ID (UID) of the requested service.
 *              cred: The credentials object of the client requesting the
 * service.
 * Out:         objOut: The service object.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
int32_t tProcessOpen(uint32_t uid, Object cred, Object *objOut);

/**
 * Description: Release any remaining objects before process is killed.
 *
 * In:          void
 * Out:         void
 * Return:      void
 */
void tProcessShutdown(void);
