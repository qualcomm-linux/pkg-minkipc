// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __MSFORWARDER_H
#define __MSFORWARDER_H

#include "object.h"
#include "pthread.h"
#include "qlist.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct MinkSocket MinkSocket;

typedef struct MSForwarder {
  QNode node;
  int refs;
  int handle;
  pthread_mutex_t mutex;
  QList qlCloseNotifier;
  MinkSocket *conn;
} MSForwarder;

int32_t MSForwarder_new(MinkSocket *conn, int handle, Object *objOut);
MSForwarder *MSForwarderFromObject(Object obj);
/**
  Detach this MSForwarder from the remote handle and free its memory.
  Do not use this MSForwarder after calling detach.
**/
int32_t MSForwarder_detach(MSForwarder *me);
int32_t MSForwarder_derivePrimordial(MSForwarder *fwd, Object *pmd);

#if defined (__cplusplus)
}
#endif

#endif // __MSFORWARDER_H
