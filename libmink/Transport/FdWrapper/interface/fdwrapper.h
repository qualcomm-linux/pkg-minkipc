// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __FDWRAPPER_H
#define __FDWRAPPER_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <sys/mman.h>

#include <stdbool.h>
#include "object.h"
#include "qlist.h"

typedef struct FdWrapper {
  QNode qn;
  int refs;
  int handle;
  int descriptor;
  Object dependency;
#ifdef REMOTE_SHAREMM
  Object confinement;
#endif // REMOTE_SHAREMM
  bool needToCloseFd;
  void *virtAddr;
  bool mapped;
  size_t bufSize;
  unsigned int ipcFlags;
  bool fromMinkMem;
} FdWrapper;

Object FdWrapper_new(int fd);
FdWrapper *FdWrapperFromObject(Object obj);
int32_t FdWrapper_release(FdWrapper *me);

bool isWrappedFd(Object obj, int* fd);
FdWrapper *FdWrapper_newInternal(int fd, bool needToCloseFd, void *ptr,
                                 size_t bufSize, bool mapped, bool fromMinkMem,
                                 unsigned int ipcFlags);

Object FdWrapperToObject(FdWrapper *context);
Object FdWrapper_newWithCloseFlags(int fd, bool needToCloseFd);

#if defined (__cplusplus)
}
#endif

#endif // __FdWrapper_H
