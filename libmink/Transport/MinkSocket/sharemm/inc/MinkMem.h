// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _MINKMEM_H_
#define _MINKMEM_H_

#include "object.h"

/* Allocation flags for DMA buffer */
#define MINKMEM_ALLOC_DEFAULT                (unsigned int)0xF0000001

/* IPC flags for remote memory sharing */
#define MINKMEM_VM_UNSET             (unsigned int)0xFF000000
#define MINKMEM_VM_SHARE             (unsigned int)0xFF000001
#define MINKMEM_VM_LEND              (unsigned int)0xFF000002

#if defined(__cplusplus)
extern "C" {
#endif

/*
  Initialization of MinkMem
  Get called at the very beginning of MinkMem use

  Return:
    Object_OK on success
    Object_ERROR on failure
 */
int MinkMem_init(void);

/*
  De-initialization of MinkMem
  Get called at the very end of MinkMem use

  Return:
    Object_OK on success
    Object_ERROR on failure
 */
int MinkMem_deinit(void);

/*
  Allocate DMA buffer from specific DMA-BUF heap
  Return mapped virtual address
  Caller should check if the returned virtualAddr is not NULL

  Return:
    Non-NULL pointer on success
    NULL pointer on failure
 */
void *MinkMem_alloc(const char *heapName, unsigned int flags, size_t size);

/*
  Query corresponding DMA-BUF fd given virtual address
  This DMA buffer should be allocated from MinkMem
  MinkMem manages the relationship between virtAddr and fd
    so query doesnt need the DMA buffer size as input
  Caller should check if the returned DMA-BUF fd is not -1

  Return:
    Non (-1) valid fd on success
    -1 (invalid fd) on failure
 */
int MinkMem_virtAddrToFd(void *virtAddr);

/*
  Convert DMA-BUF fd to a memory object for MinkIPC
  The DMA buffer can be allocated from any allocator(e.g. RPCMem, MinkMem)
  If the DMA buffer is allocated by non-MinkMem allocator(e.g. RPCMem),
    then MinkMem simply wrap the fd as a memory object and releasing
    this memory object WONT close the fd.
  For MinkMem-allocated DMA-BUF, this will RETAIN the memory object.
  For non-MinkMem-allocated DMA-BUF, this will always return a NEW memory object.
  Destructor of ProxyBase calls Object_release() so it forces MinkMem
    to retain the returned memory object.
  TODO: Refine the ProxyBase/IMemObject.

  Out:
    objOut: non (Object_NULL) memory object

  Return:
    Object_OK on success
    Object_ERROR on failure
 */
int MinkMem_fdToMemObj(int dmaBufFd, Object *objOut);

/*
  Set the memory object's attributes including confinement and mapping
  The flags should be indicating either FOR_SHARE or FOR_LEND
  If the flags is FOR_LEND, then the virtAddr will be unmapped and invalid
  If you want to map the DMA buffer again, you can set memObj attribute
    to FOR_SHARE and get virtAddr again

  Return:
    Object_OK on success
    Object_ERROR on failure
 */
int MinkMem_setMemObjAttr(Object memObj, unsigned int flags);

/*
  Query corresponding DMA-BUF fd given virtual address
  This DMA buffer should be allocated from MinkMem
  And then convert DMA-BUF fd to a memory object for MinkIPC
  If the DMA buffer is allocated by non-MinkMem allocator(e.g. RPCMem),
    then it will return Object_NULL and Object_ERROR
  For MinkMem-allocated DMA-BUF, this will RETAIN the memory object.
  Destructor of ProxyBase calls Object_release() so it forces MinkMem
    to retain the returned memory object.
  TODO: Refine the ProxyBase/IMemObject.

  Out:
    objOut: non (Object_NULL) memory object

  Return:
    Object_OK on success
    Object_ERROR on failure
 */
int MinkMem_virtAddrToMemObj(void *virtAddr, Object *objOut);

/*
  Return mapped virtual address and its size given a memory object
  If the memory is not allocated by MinkMem,
    then it will return NULL pointer and zero size

  Out:
    virtAddrSize: DMA buffer size

  Return:
    Non-NULL pointer on success
    NULL pointer on failure
 */
void *MinkMem_memObjToVirtAddr(Object memObj, size_t *virtAddrSize);

/*
  Clean up all the related resources of the DMA buffer
  If the DMA buffer has been lent already, the virtAddr is invalid
    and it is just used for quering the DMA buffer
  Note that the refs of memObj might not be 0(so the fd might not be closed instantly)
    after below operation because MinkSocket's OT might be still holding it
    (waiting for MEM_RELEASE/MEM_RECLAIM).
 */
void MinkMem_free(void *virtAddr);

#if defined(__cplusplus)
}
#endif

#endif
