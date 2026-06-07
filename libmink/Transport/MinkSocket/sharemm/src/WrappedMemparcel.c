// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "Confinement.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "IConfinement.h"
#include "MSMem.h"
#include "memscpy.h"
#include "ShareMemory.h"
#include "Utils.h"
#include "Profiling.h"
#include "WrappedMemparcel.h"

// ----------------------------------------------------------------------------
// Implement WrappedMemparcel
// ----------------------------------------------------------------------------

struct WrappedMemparcel {
  int refs;
  int64_t memparcelHandle;
  // It can be either FdWrapper or MSMem.
  Object wrappedFdObj;
};

static int32_t
WrappedMemparcel_delete(WrappedMemparcel* me)
{
  const int32_t retryMaxAttempts = 10;
  int32_t fd = -1, retryAttempt = 0, ret = Object_ERROR;
  // initial delay is 0ms, backoff is 10ms
  unsigned int delay = 0, backoff = 10000;
  // delay = 10 + 10 * (retryAttempt), the longest cumulative delay is 550ms
  uint64_t specialRules = 0;
  FdWrapper *fdw = NULL;
  MSMem *msmem = NULL;
  Object confObj = Object_NULL;

  if (!me) {
    LOG_ERR("WrappedMemparcel is NULL.\n");
    return Object_ERROR;
  }

  if (isWrappedFd(me->wrappedFdObj, &fd)) {
    fdw = FdWrapperFromObject(me->wrappedFdObj);
    if (!fdw) {
      LOG_ERR("wmp\'s wrappedFdObj is of INVALID fdwrapper.\n");
      return Object_ERROR;
    }
    confObj = fdw->confinement;
    if (Object_isNull(confObj) || (NULL == ConfinementFromObject(confObj))) {
      LOG_ERR("wmp\'s wrappedFdObj(fdwrapper) has INVALID confinement obj.\n");
      return Object_ERROR;
    }
    if (!Object_isOK(IConfinement_getSpecialRules(confObj, &specialRules))) {
      LOG_ERR("Checking specialRules of confinement of wmp\'s wrappedFdObj(fdwrapper) failed.\n");
      return Object_ERROR;
    }
  } else if (isMSMem(me->wrappedFdObj, &fd)) {
    msmem = MSMemFromObject(me->wrappedFdObj);
    if (!msmem) {
      LOG_ERR("wmp\'s wrappedFdObj is of INVALID MSMem.\n");
      return Object_ERROR;
    }
    specialRules = msmem->confRules.specialRules;
  } else {
    LOG_ERR("Type of wmp\'s wrappedFdObj cant be determined.\n");
    return Object_ERROR;
  }
  LOG_PERF("dmaFd = %d, wmp = %p, memparcelHdl = %u%09u, startDeleteWmp \n", fd, me,
           UINT64_HIGH(me->memparcelHandle), UINT64_LOW(me->memparcelHandle));

  if (!((ITAccessPermissions_smmuProxyControlled & specialRules) ||
        (ITAccessPermissions_mixedControlled & specialRules))) {
    LOG_TRACE("Reclaiming memparcelHdl = %u%09u and dmaBufFd = %d.\n",
              UINT64_HIGH(me->memparcelHandle), UINT64_LOW(me->memparcelHandle), fd);
    do {
      vm_osal_usleep(delay);
      ret = ShareMemory_ReclaimMemBuf(fd, me->memparcelHandle);
      if (Object_isOK(ret)) {
          break;
      }
      delay += backoff;
    } while (retryAttempt++ < retryMaxAttempts);

    if (Object_isERROR(ret)) {
      LOG_ERR("Retry MEM_RECLAIM failed eventually after %u ms.\n",
              retryMaxAttempts * (retryMaxAttempts + 1) * (backoff / 2 / 1000));
    }
  }
  LOG_PERF("dmaFd = %d, wmp = %p, memparcelHdl = %u%09u, endDeleteWmp \n", fd, me,
           UINT64_HIGH(me->memparcelHandle), UINT64_LOW(me->memparcelHandle));

  // Delete the WPM object:
  Object_ASSIGN_NULL(me->wrappedFdObj);
  HEAP_FREE_PTR(me);

  return Object_OK;
}

static int32_t
WrappedMemparcel_release(WrappedMemparcel* me)
{
  if (!me) {
    LOG_ERR("WrappedMemparcel is NULL.\n");
    return Object_ERROR;
  }

  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    return WrappedMemparcel_delete(me);
  }
  return Object_OK;
}

static int32_t
WrappedMemparcel_getConfinement(WrappedMemparcel* me, ITAccessPermissions_rules *outConfRules)
{
  MSMem *msmem = NULL;
  FdWrapper *fdw = NULL;
  int32_t fd = -1;
  Object confObj = Object_NULL;

  if (!me) {
    LOG_ERR("WrappedMemparcel is NULL.\n");
    return Object_ERROR;
  }

  if (outConfRules == NULL) {
    LOG_ERR("outConfRules is NULL.\n");
    return Object_ERROR;
  }

  if (isWrappedFd(me->wrappedFdObj, &fd)) {
    fdw = FdWrapperFromObject(me->wrappedFdObj);
    if (!fdw) {
      LOG_ERR("wmp\'s wrappedFdObj is of INVALID fdwrapper.\n");
      return Object_ERROR;
    }
    confObj = fdw->confinement;
    if (Object_isNull(confObj) || (NULL == ConfinementFromObject(confObj))) {
      LOG_ERR("wmp\'s wrappedFdObj(fdwrapper) has INVALID confinement obj.\n");
      return Object_ERROR;
    }
    return IConfinement_getConfinementRules(confObj, outConfRules);
  } else if (isMSMem(me->wrappedFdObj, &fd)) {
    msmem = MSMemFromObject(me->wrappedFdObj);
    if (!msmem) {
      LOG_ERR("wmp\'s wrappedFdObj is of INVALID MSMem.\n");
      return Object_ERROR;
    }
    memscpy(outConfRules, sizeof(ITAccessPermissions_rules),
            &msmem->confRules, sizeof(ITAccessPermissions_rules));
    return Object_OK;
  }

  LOG_ERR("Type of wmp\'s wrappedFdObj cant be determined.\n");

  return Object_ERROR;
}

static int32_t
WrappedMemparcel_getMemparcelHandle(WrappedMemparcel* me, int64_t *outWmpHdl)
{
  if (!me) {
    LOG_ERR("WrappedMemparcel is NULL.\n");
    return Object_ERROR;
  }

  if (outWmpHdl == NULL) {
    LOG_ERR("outWmpHdl is NULL.\n");
    return Object_ERROR;
  }

  *outWmpHdl = me->memparcelHandle;

  return Object_OK;
}

// Note that this operation will retain the wrappedFdObj,
// wrappedFdObj can be either FdWrapper or MSMem.
static int32_t
WrappedMemparcel_getWrappedFdObj(WrappedMemparcel* me, Object *outWrappedFdObj)
{
  int32_t fd = -1;

  if (!me) {
    LOG_ERR("WrappedMemparcel is NULL.\n");
    return Object_ERROR;
  }

  if (outWrappedFdObj == NULL) {
    LOG_ERR("outWrappedFdObj is NULL.\n");
    return Object_ERROR;
  }

  if (isWrappedFd(me->wrappedFdObj, &fd) || isMSMem(me->wrappedFdObj, &fd)) {
    Object_INIT(*outWrappedFdObj, me->wrappedFdObj);
    return Object_OK;
  }

  LOG_ERR("Type of wmp\'s wrappedFdObj cant be determined.\n");

  return Object_ERROR;
}

static int32_t
WrappedMemparcel_invoke(void *cxt, ObjectOp op, ObjectArg *args, ObjectCounts k)
{
  WrappedMemparcel *me = (WrappedMemparcel*) cxt;
  ObjectOp method = ObjectOp_methodID(op);

  if (!me) {
    LOG_ERR("WrappedMemparcel is NULL.\n");
    return Object_ERROR;
  }

  switch (method) {
    case Object_OP_retain: {
      vm_osal_atomic_add(&me->refs, 1);
      return Object_OK;
    }

    case Object_OP_release: {
      return WrappedMemparcel_release(me);
    }

    case WrappedMemparcel_OP_getConfinement: {
      if (k != ObjectCounts_pack(0, 1, 0, 0)) {
        break;
      }
      ITAccessPermissions_rules *confRules_ptr = (ITAccessPermissions_rules *) args[0].b.ptr;
      if (args[0].b.size != sizeof(ITAccessPermissions_rules)) {
        break;
      }
      return WrappedMemparcel_getConfinement(me, confRules_ptr);
    }

    case WrappedMemparcel_OP_getMemparcelHandle: {
      if (k != ObjectCounts_pack(0, 1, 0, 0)) {
        break;
      }
      int64_t *wmpHdl_ptr = (int64_t *) args[0].b.ptr;
      if (args[0].b.size != sizeof(int64_t)) {
        break;
      }
      return WrappedMemparcel_getMemparcelHandle(me, wmpHdl_ptr);
    }

    case WrappedMemparcel_OP_getWrappedFdObj: {
      if (k != ObjectCounts_pack(0, 0, 0, 1)) {
        break;
      }
      return WrappedMemparcel_getWrappedFdObj(me, &args[0].o);
    }
  }

  LOG_ERR("Invoking invalid method OR some of arg getting passed is invalid.\n");

  return Object_ERROR;
}

bool isWrappedMemparcel(Object obj)
{
  if (Object_isNull(obj)) {
    LOG_TRACE("Obj is NULL when checking if it is wrappedmemparcel.\n");
    return false;
  }

  return (obj.invoke == WrappedMemparcel_invoke ?
         true : false);
}

// Valid wrappedFdObj should meet one and only one requirement below:
//    i) FdWrapper with confinement object attached.
//   ii) Remote MSMem.
int32_t WrappedMemparcel_new(Object wrappedFdObj, MinkSocket* minksock,
                             uint32_t invId, Object *objOut)
{
  int ret = Object_ERROR;
  int fd = -1;
  Object confObj = Object_NULL;
  FdWrapper *fdw = NULL;
  MSMem *msmem = NULL;

  if (!minksock) {
    LOG_ERR("minksock is NULL.\n");
    return Object_ERROR;
  }

  if (!objOut) {
    LOG_ERR("objOut is NULL.\n");
    return Object_ERROR;
  }

  WrappedMemparcel* me = HEAP_ZALLOC_REC(WrappedMemparcel);
  if (!me) {
    LOG_ERR("Cant allocate mem for new wrappedmemparcel.\n");
    return Object_ERROR_MEM;
  }

  if (isWrappedFd(wrappedFdObj, &fd)) {
    fdw = FdWrapperFromObject(wrappedFdObj);
    if (!fdw) {
      LOG_ERR("wmp\'s wrappedFdObj is of INVALID fdwrapper.\n");
      goto bail;
    }
    confObj = fdw->confinement;
    if (Object_isNull(confObj) || (NULL == ConfinementFromObject(confObj))) {
      LOG_ERR("wmp\'s wrappedFdObj(fdwrapper) has INVALID confinement obj.\n");
      goto bail;
    }
  } else if (isMSMem(wrappedFdObj, &fd)) {
    msmem = MSMemFromObject(wrappedFdObj);
    if (!msmem) {
      LOG_ERR("wmp\'s wrappedFdObj is of INVALID MSMem.\n");
      goto bail;
    }
    if (!Object_isOK(CConfinement_new(&msmem->confRules, &confObj))) {
      LOG_ERR("Creating new confinement obj failed.\n");
      goto bail;
    }
  } else {
    LOG_ERR("Type of wmp\'s wrappedFdObj cant be determined.\n");
    goto bail;
  }

  ret = ShareMemory_GetMemParcelHandle(fd, confObj, MinkSocket_getDestVMName(minksock),
                                       &me->memparcelHandle);

  if (msmem) {
    // confObj in this case is a temp Object created by transport.
    Object_ASSIGN_NULL(confObj);
  }

  if (ret) {
    LOG_ERR("Failed to get memparcelHandle with ret = %d.\n", ret);
    goto bail;
  }

  LOG_TRACE("Sharing memory with dmaBufFd = %d and memparcelHdl = %u%09u.\n",
            fd, UINT64_HIGH(me->memparcelHandle), UINT64_LOW(me->memparcelHandle));

  me->refs = 1;
  Object_INIT(me->wrappedFdObj, wrappedFdObj);

  *objOut = (Object){WrappedMemparcel_invoke, me};

  LOG_PERF("msock = %p, invId = %u, dmaFd = %d, dmaSize = %u%09u, mpH = %u%09u, newWmp \n",
           minksock, invId, fd,
           UINT64_HIGH(vm_osal_getDMAFdSize(fd)), UINT64_LOW(vm_osal_getDMAFdSize(fd)),
           UINT64_HIGH(me->memparcelHandle), UINT64_LOW(me->memparcelHandle));


  return Object_OK;

bail:
  HEAP_FREE_PTR(me);

  return Object_ERROR;
}
