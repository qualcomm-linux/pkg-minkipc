// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <unistd.h>

#include "memscpy.h"
#include "Heap.h"
#include "fdwrapper.h"
#include "logging.h"
#include "qlist.h"

// FOR OFFTARGET TESTING
#ifdef STUB
#include "BufferAllocatorWrapper.h"
#endif

/*================================================================
 * DescriptorObject
 *================================================================*/

static inline int atomic_add(int *pn, int n)
{
  return __sync_add_and_fetch(pn, n);  // GCC builtin
}

static inline void
FdWrapper_delete(FdWrapper *me)
{
  ALOGV("deleted fdWrapper = %p, fd = %d, dependency = %p, needToCloseFd=%s\n", me,
                me->descriptor, &me->dependency, me->needToCloseFd ? "Yes" : "No");

  if (me->mapped) {
    munmap(me->virtAddr, me->bufSize);
  }

  /* There are 2 stubbed libraries(stub.c) in both platform and transport.
     Platform is using its own stubbed library so if fromMinkMem == false
     then we dont call stubbed interface to close the fd to avoid issues.
   */
  if (me->fromMinkMem) {
    if (me->needToCloseFd) {
#ifdef STUB
      close_offtarget_unlink(me->descriptor);
#else
      close(me->descriptor);
#endif
    }
  } else {
    if (me->needToCloseFd) {
      close(me->descriptor);
    }
  }

  Object_ASSIGN_NULL(me->dependency);
#ifdef REMOTE_SHAREMM
  Object_ASSIGN_NULL(me->confinement);
#endif // REMOTE_SHAREMM
  QNode_dequeueIf(&me->qn);
  HEAP_FREE_PTR(me);
}

int32_t FdWrapper_release(FdWrapper *me) {
   if (atomic_add(&me->refs, -1) == 0) {
     FdWrapper_delete(me);
   }
   return Object_OK;
}

static
int32_t FdWrapper_invoke(void *cxt, ObjectOp op, ObjectArg *args, ObjectCounts k)
{
  FdWrapper *me = (FdWrapper*) cxt;
  ObjectOp method = ObjectOp_methodID(op);

  switch (method) {
  case Object_OP_retain:
    atomic_add(&me->refs, 1);
    return Object_OK;

  case Object_OP_release:
    return FdWrapper_release(me);

  case Object_OP_unwrapFd:
    if (k != ObjectCounts_pack(0, 1, 0, 0)) {
      break;
    }
    memscpy(args[0].b.ptr, args[0].b.size,
            &me->descriptor, sizeof(me->descriptor));
    return Object_OK;
  }

  return Object_ERROR;
}

FdWrapper *FdWrapperFromObject(Object obj)
{
  return (obj.invoke == FdWrapper_invoke ? (FdWrapper*) obj.context : NULL);
}

FdWrapper *FdWrapper_newInternal(int fd, bool needToCloseFd, void *ptr,
                                 size_t bufSize, bool mapped, bool fromMinkMem,
                                 unsigned int ipcFlags)
{
  FdWrapper *me = HEAP_ZALLOC_REC(FdWrapper);

  if (!me) {
    ALOGE("Cannot allocate FdWrapper.\n");
    return NULL;
  }

  me->refs = 1;
  me->descriptor = fd;

  me->needToCloseFd = needToCloseFd;
  me->virtAddr = ptr;
  me->bufSize = bufSize;
  me->mapped = mapped;
  me->fromMinkMem = fromMinkMem;
  me->ipcFlags = ipcFlags;
  QNode_construct(&me->qn);

  return me;
}

Object FdWrapper_new(int fd)
{
  FdWrapper *me = FdWrapper_newInternal(fd, true, NULL, 0, false, false, 0);

  if (!me) {
    return Object_NULL;
  }

  return (Object) { FdWrapper_invoke, me };
}

// Return true if obj is an FdWrapper with a valid fd
// On success, the output parameter "fd" is populated
bool isWrappedFd(Object obj, int* fd) {
  if (!fd) {
    return false;
  }
  *fd = -1;

  return (!Object_isNull(obj) &&
          FdWrapperFromObject(obj) != NULL &&
          Object_isOK(Object_unwrapFd(obj, fd)) &&
          *fd > 0);
}

Object FdWrapperToObject(FdWrapper *context)
{
  if (!context) {
    return Object_NULL;
  }

  return (Object) { FdWrapper_invoke, context };
}

Object FdWrapper_newWithCloseFlags(int fd, bool needToCloseFd)
{
  FdWrapper *me = FdWrapper_newInternal(fd, needToCloseFd, NULL, 0, false, false, 0);

  if (!me) {
    return Object_NULL;
  }

  return (Object) { FdWrapper_invoke, me };
}
