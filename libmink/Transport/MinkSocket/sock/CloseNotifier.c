// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "VmOsal.h"
#include "cdefs.h"
#include "check.h"
#include "CloseNotifier.h"
#include "Heap.h"
#include "ICloseHandler_invoke.h"
#include "IObject_invoke.h"
#include "IPrimordial.h"
#include "msforwarder.h"
#include "Utils.h"

typedef struct CloseHandler {
  int32_t refs;
  CloseHandlerFunc onEvent;
  void *data;
}CloseHandler;

struct CloseNotifier {
  int32_t refs;
  QNode node;
  Object handler;
  Object subNotifier;
};

static inline
int32_t CloseHandler_retain(CloseHandler *me)
{
  vm_osal_atomic_add(&me->refs, 1);

  return Object_OK;
}

static inline
int32_t CloseHandler_release(CloseHandler *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("Released closeHandler = %p, CloseHandlerFunc = %p\n", me, me->onEvent);
    me->onEvent = NULL;
    me->data = NULL;
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static
int32_t CloseHandler_onEvent(CloseHandler *me, int32_t event)
{
  if (NULL == me->onEvent) {
    LOG_ERR("Handler function is NULL\n");
    return Object_ERROR;
  }

  LOG_TRACE("Close Notification triggered on closeHandler = %p, CloseHandlerFunc \
             = %p, event=%d\n", me, me->onEvent, event);
  (*me->onEvent)(me->data, event);

  return Object_OK;
}

static
ICloseHandler_DEFINE_INVOKE(CloseHandler_invoke, CloseHandler_,
                                CloseHandler*);

int32_t CloseHandler_new(CloseHandlerFunc func, void *data, Object *objOut)
{
  CloseHandler *me = HEAP_ZALLOC_REC(CloseHandler);
  if (!me) {
    LOG_ERR("Memory allocation for CloseHandler failed\n");
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  me->onEvent = func;
  me->data = data;
  *objOut = (Object) {CloseHandler_invoke, me};

  return Object_OK;
}

static inline
int32_t CloseNotifier_retain(CloseNotifier *me)
{
  vm_osal_atomic_add(&me->refs, 1);

  return Object_OK;
}

static inline
int32_t CloseNotifier_release(CloseNotifier *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("Released CloseNotifier = %p, handler = %p, subNotifier = %p\n", me,
               &me->handler, &me->subNotifier);
    Object_ASSIGN_NULL(me->handler);
    Object_ASSIGN_NULL(me->subNotifier);
    QNode_dequeueIf(&me->node);
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static
IObject_DEFINE_INVOKE(CloseNotifier_invoke, CloseNotifier_, CloseNotifier*);

static
void CloseNotifier_attachToMSForwarder(CloseNotifier *me, MSForwarder *msFwd)
{
  vm_osal_mutex_lock(&msFwd->mutex);
  QList_appendNode(&msFwd->qlCloseNotifier, &me->node);
  vm_osal_mutex_unlock(&msFwd->mutex);

  return;
}

int32_t CloseNotifier_popFromMSForwarder(CloseNotifier **me, MSForwarder *msFwd)
{
  int32_t ret = Object_ERROR;

  vm_osal_mutex_lock(&msFwd->mutex);
  QNode *qNode = QList_pop(&msFwd->qlCloseNotifier);
  if (NULL == qNode) {
    *me = NULL;
    ret = Object_ERROR;
  } else {
    *me = c_containerof(qNode, CloseNotifier, node);
    ret = Object_OK;
  }
  vm_osal_mutex_unlock(&msFwd->mutex);

  return ret;
}

void CloseNotifier_notify(CloseNotifier *me, uint32_t event)
{
  ICloseHandler_onEvent(me->handler, event);

  return;
}

/*@brief: register handler to target and return notifier for deregistration
 *        the registration need to be broadcast to next endpoint if the target
 *        is a proxy. With the help of static primordial of MinkSocket,
 *        the function will be invoked recursively until reach the endpoint actually
 *        implementing the target.
 *
 *@param[target]: Penetrate the proxy until the actual target.
 *@param[handler]: Register handler to target, handler to deal with disconnected secenarios.
 *@param[subNotifier]: subNotifier is a subset of notifier.
*/
static
int32_t CloseNotifier_penetrateRegister(Object target, Object handler, Object *subNotifier)
{
  int32_t ret = Object_OK;
  Object counterPmd = Object_NULL;
  MSForwarder *msFwd = NULL;

  msFwd = MSForwarderFromObject(target);
  if (msFwd == NULL) {
    LOG_ERR("Fail in MSForwarderFromObject()\n");
    return Object_ERROR;
  }
  if (MSForwarder_derivePrimordial(msFwd, &counterPmd)) {
    LOG_ERR("Fail to get promordial\n");
    return Object_ERROR;
  }

  ret = IPrimordial_registerSubNotifier(counterPmd, target, handler, subNotifier);
  if (ret) {
    LOG_ERR("Fail to invoke primordial %d\n", ret);
  }

  Object_ASSIGN_NULL(counterPmd);

  return ret;
}

static
int32_t subCloseNotifier_new(Object handler, Object furtherNotifier, Object *objOut)
{
  CloseNotifier *me = HEAP_ZALLOC_REC(CloseNotifier);
  if (!me) {
    LOG_ERR("Memory allocation for sub CloseNotifier failed\n");
    return Object_ERROR_MEM;
  }

  me->refs = 1;
/*
  in multiple connection scenario, handler is marshallIn parameter
  only with reference from notifier can it survives in intermedium
  endpoint after registration broadcast return
  So notifier refers to handler to make it survived.
*/
  Object_INIT(me->handler, handler);
/*
  in multiple connection scenario, subNotifier is marshalOut parameter
  it was born in intermedium endpoint and still suvives after registration
  broadcast return.
  So notifier cannot refer to handler to avoid counts mismatch in furture notifier
*/
  me->subNotifier = furtherNotifier;
  QNode_construct(&me->node);

  *objOut = (Object) {CloseNotifier_invoke, me};

  return Object_OK;
}

int32_t CloseNotifier_subRegister(Object target, Object handler, Object *subNotifier)
{
  int32_t ret = Object_OK;
  Object furtherNotifier = Object_NULL;
  MSForwarder *msFwd = NULL;

  msFwd = MSForwarderFromObject(target);
  if (NULL == msFwd) {
    LOG_TRACE("Reach the end of closeNotifier registration\n");
    return Object_OK;
  }

  ret = CloseNotifier_penetrateRegister(target, handler, &furtherNotifier);
  if (ret) {
    LOG_ERR("Failed on CloseNotifier_penetrateRegister(), ret = %d\n", ret);
    return Object_ERROR;
  }

  ret = subCloseNotifier_new(handler, furtherNotifier, subNotifier);
  if (ret) {
    LOG_ERR("Failed on subCloseNotifier_new(), ret = %d\n", ret);
    Object_ASSIGN_NULL(furtherNotifier);
    return Object_ERROR;
  }
  CloseNotifier_attachToMSForwarder((CloseNotifier *)subNotifier->context, msFwd);

  return Object_OK;
}

int32_t CloseNotifier_new(CloseHandlerFunc func, void *data, Object target,
                          Object *objOut)
{
  int32_t ret = Object_OK;
  MSForwarder *msFwd = NULL;
  CloseNotifier *me = NULL;
  Object subNotifier = Object_NULL;

  if (NULL == func) {
    LOG_ERR("Invalid CloseHandlerFunc\n");
    return Object_ERROR;
  }

  msFwd = MSForwarderFromObject(target);
  if (NULL == msFwd) {
    LOG_MSG("Input target must be MSForwarder\n");
    return Object_ERROR;
  }

  me = HEAP_ZALLOC_REC(CloseNotifier);
  if (NULL == me) {
    LOG_ERR("Memory allocation for CloseNotifier failed\n");
    return Object_ERROR_MEM;
  }

  ret = CloseHandler_new(func, data, &me->handler);
  TRUE_OR_CLEAN(Object_OK == ret, "Create close handler object failed\n");

  ret = CloseNotifier_penetrateRegister(target, me->handler, &subNotifier);
  TRUE_OR_CLEAN(Object_OK == ret,
                "Failed on CloseNotifier_penetrateRegister(), ret = %d\n", ret);

  me->refs = 1;
  QNode_construct(&me->node);

  me->subNotifier = subNotifier;
  CloseNotifier_attachToMSForwarder(me, msFwd);

  *objOut = (Object) {CloseNotifier_invoke, me};

  return Object_OK;

cleanup:
  if (!Object_isNull(subNotifier)) {
    Object_ASSIGN_NULL(subNotifier);
  }
  if (!Object_isNull(me->handler)) {
    Object_ASSIGN_NULL(me->handler);
  }
  if (NULL != me) {
    HEAP_FREE_PTR(me);
  }
  *objOut = Object_NULL;

  return ret;
}
