// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _CLOSENOTIFIER_H
#define _CLOSENOTIFIER_H

#include "VmOsal.h"
#include "msforwarder.h"
#include "object.h"
#include "qlist.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define EVENT_CLOSE       (0x00000020u)
#define EVENT_CRASH       (EVENT_CLOSE - 1)
#define EVENT_DELETE      (EVENT_CLOSE - 2)
#define EVENT_DETACH      (EVENT_CLOSE - 3)
#define EVENT_UNKNOWN     (0x0000F000u)

#define isCloseEvent(event)    (((event) & EVENT_CLOSE) != 0)

typedef void (*CloseHandlerFunc)(void *data, int32_t event);
typedef struct CloseNotifier CloseNotifier;

int32_t CloseNotifier_new(CloseHandlerFunc func, void *data, Object target,
                          Object *objOut);
int32_t CloseNotifier_subRegister(Object target, Object handler, Object *subNotifier);
int32_t CloseNotifier_popFromMSForwarder(CloseNotifier **me, MSForwarder *msFwd);
void CloseNotifier_notify(CloseNotifier *me, uint32_t event);

#if defined (__cplusplus)
}
#endif

#endif // _CLOSENOTIFIER_H
