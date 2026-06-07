// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "cdefs.h"
#include "Heap.h"
#include "memscpy.h"
#include "MSMem.h"
#include "object.h"
#include "Utils.h"
#include "Profiling.h"

// FOR OFFTARGET TESTING
#ifdef STUB
#include "BufferAllocatorWrapper.h"
#endif

static int32_t CMSMem_retain(MSMem *me)
{
  vm_osal_atomic_add(&me->refs, 1);

  return Object_OK;
}

static int32_t CMSMem_release(MSMem *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("released mSMem = %p, dmaBufFd = %d, isLocal = %s.\n", me, me->dmaBufFd,
              me->isLocal ? "TRUE" : "FALSE");

    LOG_PERF("MSMem = %p, dmaFd = %d, startRelease \n", me, me->dmaBufFd);

    if ((!me->isLocal) && (me->gMarshalMemLock)) {
      LOG_TRACE("Remote MSMem case, lock mutex when close fd and cleanup qlist \n");
      vm_osal_mutex_lock(me->gMarshalMemLock);
    }

#ifdef STUB
    close_offtarget_unlink(me->dmaBufFd);
#else
    vm_osal_mem_close(me->dmaBufFd);
#endif

    if ((!me->isLocal) && (me->gMarshalMemLock)) {
      QNode_dequeueIf(&(me->node));
      vm_osal_mutex_unlock(me->gMarshalMemLock);
      LOG_TRACE("Remote MSMem case, unlocked mutex after close fd and cleanup qlist \n");
    }

    LOG_PERF("MSMem = %p, dmaFd = %d, endRelease \n", me, me->dmaBufFd);
    Object_RELEASE_IF(me->bufCallBack);
    // If MSMem was created from wmp, then it should sendClose to wmp
    Object_RELEASE_IF(me->dependency);

    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

int32_t CMSMem_invoke(ObjectCxt h, ObjectOp op, ObjectArg *a, ObjectCounts k)
{
  MSMem *me = (MSMem *)h;

  switch (ObjectOp_methodID(op)) {
    case Object_OP_release: {
      if (k != ObjectCounts_pack(0, 0, 0, 0)) {
        break;
      }
      return CMSMem_release(me);
    }

    case Object_OP_retain: {
      if (k != ObjectCounts_pack(0, 0, 0, 0)) {
        break;
      }
      return CMSMem_retain(me);
    }

    // Used for SMCI
    case Object_OP_unwrapFd: {
      if (k != ObjectCounts_pack(0, 1, 0, 0)) {
        break;
      }
      memscpy(a[0].b.ptr, a[0].b.size, &me->dmaBufFd, sizeof(me->dmaBufFd));
      return Object_OK;
    }
  }

  return Object_ERROR_INVALID;
}

bool isMSMem(Object obj, int32_t *fd)
{
  if (!fd) {
    return false;
  }
  *fd = -1;

  return (!Object_isNull(obj) &&
          MSMemFromObject(obj) != NULL &&
          Object_isOK(Object_unwrapFd(obj, fd)) &&
          *fd > 0);
}

MSMem *MSMemFromObject(Object obj)
{
  return (obj.invoke == CMSMem_invoke ? (MSMem *)obj.context : NULL);
}

static int32_t
MSMem_brew(int32_t fd, MSMem **me, bool isLocal, int64_t mpH, vm_osal_mutex *lockPtr)
{
  int32_t ret = Object_OK;

  if (!me || fd < 0) {
    return Object_ERROR;
  }

  *me = HEAP_ZALLOC_TYPE(MSMem);

  if (!(*me)) {
    return Object_ERROR_MEM;
  }

  (*me)->refs = 1;
  (*me)->gMarshalMemLock = lockPtr;
  (*me)->memParcelHandle = mpH;
  (*me)->dmaBufFd = fd;
  (*me)->isLocal = isLocal;

  return ret;
}

int32_t MSMem_new(int32_t fd, Object bufCallback, Object *objOut)
{
  int32_t ret = Object_OK;
  MSMem *me = NULL;

  if (Object_isNull(bufCallback)) {
    return Object_ERROR;
  }

  if (Object_isERROR(ret = MSMem_brew(fd, &me, true, 0, NULL))) {
    return ret;
  }

  Object_INIT(me->bufCallBack, bufCallback);

  *objOut = (Object){CMSMem_invoke, me};

  return ret;
}

#ifdef REMOTE_SHAREMM
int32_t MSMem_new_remote(int32_t fd, ITAccessPermissions_rules *confRules,
                         int64_t mpH, vm_osal_mutex *lockPtr, Object *objOut)
{
  int32_t ret = Object_OK;
  MSMem *me = NULL;

  if (!confRules) {
    return Object_ERROR;
  }

  if (Object_isERROR(ret = MSMem_brew(fd, &me, false, mpH, lockPtr))) {
    return ret;
  }

  memscpy(&(me->confRules), sizeof(ITAccessPermissions_rules), confRules,
          sizeof(ITAccessPermissions_rules));

  *objOut = (Object){CMSMem_invoke, me};

  return ret;
}
#endif // REMOTE_SHAREMM

#ifdef STUB
int32_t MSChangeToROFd(Object obj, int32_t fd)
{
  int32_t ret = Object_ERROR;
  int32_t dmaBufFd = -1;

  if (Object_isNull(obj) || fd < 0) return ret;
  if (isMSMem(obj, &dmaBufFd)) {
    MSMem *me = (MSMem *)obj.context;
    me->dmaBufFd = fd;
    ret = Object_OK;
  }
  return ret;
}
#endif
