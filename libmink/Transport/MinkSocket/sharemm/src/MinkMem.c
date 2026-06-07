// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "VmOsal.h"
#ifndef OFFTARGET
#include "BufferAllocator/BufferAllocatorWrapper.h"
#else
#include "BufferAllocatorWrapper.h"
#endif
#include "Heap.h"
#include "ITAccessPermissions.h"
#include "MinkMem.h"
#include "RemoteShareMemory.h"
#include "Utils.h"
#include "cdefs.h"
#include "fdwrapper.h"
#include "qlist.h"

#define SIZE_2MB ((size_t)0x200000)
#define ALIGN_SIZE_2MB(allocSize) ((allocSize) + (SIZE_2MB - 1)) & (~(SIZE_2MB - 1))

#define CHECK_ADDR_BETWEEN(po, baseAddr, maxAddr) \
        (((uintptr_t)(baseAddr) < (uintptr_t)(maxAddr)) && \
         ((uintptr_t)(baseAddr) <= (uintptr_t)(po)) && \
         ((uintptr_t)(po) < (uintptr_t)(maxAddr)))

#define CHECK_ADDR_EQUAL(po, baseAddr) \
          ((uintptr_t)(po) == (uintptr_t)(baseAddr))

#define QUERY_TYPE_DMABUFFD       1
#define QUERY_TYPE_VIRTADDR       2

/*
  So far, MinkIPC interfaces supporting RPCMem are only implemented for Android and OFFTARGET
  For QTVM, client should resort to Mink Memory Service for DMA buffer allocation
 */

typedef struct MinkMemMonitor {
  QList mapInfoList;
  // To ensure the variable will be refreshed each time for each reading thread
  volatile bool initialized;
  vm_osal_mutex mutex;
  /* BufferAllocator can be used to allocate buffers
       from various DMA-BUF heaps
     It will store various DMA-BUF heaps fds once these
       DMA-BUF heaps are opened
     So, lets reuse the information for concurrent threads
       without creating different BufferAllocators
   */
  BufferAllocator *bufferAllocator;
  int initCounter;
} MinkMemMonitor;

static MinkMemMonitor monitor = {
  .initialized = false
};

static void MinkMem_initInternal(void)
{
  if (0 != vm_osal_mutex_init(&monitor.mutex, NULL)) {
    LOG_ERR("Failed to initialize mutex of MinkMemMonitor.\n");
    return;
  }
  monitor.bufferAllocator = NULL;
  monitor.initCounter = 0;
  QList_construct(&monitor.mapInfoList);
  monitor.initialized = true;
}

// TODO: Try easier implementation
int MinkMem_init(void)
{
  static pthread_once_t minkMemInitOnceCtl = PTHREAD_ONCE_INIT;

  pthread_once(&minkMemInitOnceCtl, MinkMem_initInternal);
  if (!monitor.initialized) {
    LOG_ERR("Failed to MinkMem_init.\n");
    return Object_ERROR;
  }

  vm_osal_mutex_lock(&monitor.mutex);
  ++monitor.initCounter;

  // Create bufferAllocator only if it is NULL
  if (!monitor.bufferAllocator) {
    monitor.bufferAllocator = CreateDmabufHeapBufferAllocator();
    if (!monitor.bufferAllocator) {
      LOG_ERR("Failed to create bufferAllocator.\n");
      --monitor.initCounter;
      vm_osal_mutex_unlock(&monitor.mutex);
      return Object_ERROR_MEM;
    }
  }

  vm_osal_mutex_unlock(&monitor.mutex);
  return Object_OK;
}

int MinkMem_deinit(void)
{
  QNode *pQn = NULL, *pQnNext = NULL;
  FdWrapper *fdw = NULL;
  Object memObj = Object_NULL;

  if (!monitor.initialized) {
    LOG_ERR("The MinkMem hasnt been initialized yet.\n");
    return Object_ERROR_INVALID;
  }

  vm_osal_mutex_lock(&monitor.mutex);
  --monitor.initCounter;
  // If there is a thread still using MinkMem, then dont free the bufferAllocator
  if (monitor.initCounter > 0) {
    vm_osal_mutex_unlock(&monitor.mutex);
    return Object_OK;
  }

  // No thread is using MinkMem, then lets free the bufferAllocator
  if (!monitor.bufferAllocator) {
    LOG_ERR("The bufferAllocator is unexpectedly NULL.\n");
    vm_osal_mutex_unlock(&monitor.mutex);
    return Object_ERROR_INVALID;
  }

  FreeDmabufHeapBufferAllocator(monitor.bufferAllocator);
  monitor.bufferAllocator = NULL;

  if (!QList_isEmpty(&monitor.mapInfoList)) {
    LOG_ERR("There is still some resource unreleased in mapInfoList.\n");

    QLIST_NEXTSAFE_FOR_ALL(&monitor.mapInfoList, pQn, pQnNext) {
      fdw = c_containerof(pQn, FdWrapper, qn);
      if (!fdw) {
        LOG_ERR("Got unexpectedly NULL MapInfo item.\n");
        continue;
      }
      QNode_dequeue(&fdw->qn);
      memObj = FdWrapperToObject(fdw);
      Object_ASSIGN_NULL(memObj);
    }

    vm_osal_mutex_unlock(&monitor.mutex);
    return Object_ERROR;
  }

  vm_osal_mutex_unlock(&monitor.mutex);

  return Object_OK;
}

/*
  This is an internal function and
    caller should take care of the mutex and initialization.
  Caller should make sure the virtAddr is not NULL.
 */
static bool MinkMem_checkExistingVirtAddr(void *virtAddr)
{
  bool existing = false;
  QNode *pQn = NULL;
  FdWrapper *fdw = NULL;

  QLIST_FOR_ALL(&monitor.mapInfoList, pQn) {
    fdw = c_containerof(pQn, FdWrapper, qn);
    if (!fdw) {
      LOG_ERR("Got unexpectedly NULL MapInfo item.\n");
      continue;
    }

    /*  MinkMem_setMemObjAttr() for MEM_LEND unmaps the va
          which will make the va available for alloc/map.
        We have no choice to make this compromise.
     */
    if (CHECK_ADDR_EQUAL(virtAddr, fdw->virtAddr)) {
      existing = true;
      LOG_ERR("There is already a dmaBufFd=%d being mapped to va=%p.\n",
              fdw->descriptor, virtAddr);
      break;
    }
  }

  return existing;
}

void *MinkMem_alloc(const char *heapName, unsigned int flags, size_t size)
{
  int bufFd = -1;
  void *ptr = NULL;
  FdWrapper *fdw = NULL;

  /* For now, the flags are not getting into use.
     All allocFlags of allocated buffer will be set to DEFAULT.
   */
  (void)flags;

  if (!monitor.initialized) {
    LOG_ERR("The MinkMem hasnt been initialized yet.\n");
    return NULL;
  }

  if (!heapName || !size) {
    LOG_ERR("Invalid heapName or size.\n");
    return NULL;
  }

  vm_osal_mutex_lock(&monitor.mutex);

  if (!monitor.bufferAllocator) {
    LOG_ERR("MinkMem hasnt been initialized correctly cause the bufferAllocator is NULL.\n");
    goto bail;
  }

  bufFd = DmabufHeapAlloc(monitor.bufferAllocator, heapName, ALIGN_SIZE_2MB(size), 0, 0);
  if (bufFd <= 0) {
    LOG_ERR("Failed to allocate DMA buffer from %s heap.\n", heapName);
    goto bail;
  }

  ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
  if (MAP_FAILED == ptr) {
    LOG_ERR("Failed to map DMA buffer into address space.\n");
    goto bail;
  }

  if (MinkMem_checkExistingVirtAddr(ptr)) {
    LOG_ERR("Cannot map different DMA buffers into same vma.\n");
    goto bail;
  }

  fdw = FdWrapper_newInternal(bufFd, true, ptr, size, true, true, MINKMEM_VM_UNSET);
  if (!fdw) {
    LOG_ERR("Failed to allocate FdWrapper.\n");
    goto bail;
  }

  QList_appendNode(&monitor.mapInfoList, &fdw->qn);

  vm_osal_mutex_unlock(&monitor.mutex);

  return ptr;

bail:
  vm_osal_mutex_unlock(&monitor.mutex);

  if (!ptr && MAP_FAILED != ptr) {
    munmap(ptr, size);
  }
  if (bufFd > 0) {
#ifdef STUB
    close_offtarget_unlink(bufFd);
#else
    vm_osal_mem_close(bufFd);
#endif
  }

  return NULL;
}

int MinkMem_virtAddrToFd(void *virtAddr)
{
  int fd = -1;
  QNode *pQn = NULL;
  FdWrapper *fdw = NULL;

  if (!monitor.initialized) {
    LOG_ERR("The MinkMem hasnt been initialized yet.\n");
    return -1;
  }

  if (!virtAddr) {
    LOG_ERR("Invalid virtAddr.\n");
    return -1;
  }

  vm_osal_mutex_lock(&monitor.mutex);

  QLIST_FOR_ALL(&monitor.mapInfoList, pQn) {
    fdw = c_containerof(pQn, FdWrapper, qn);
    if (!fdw) {
      LOG_ERR("Got unexpectedly NULL MapInfo item.\n");
      continue;
    }

    /*  MinkMem_setMemObjAttr() for MEM_LEND unmaps the va
          which will make the va available for alloc/map.
        We have no choice to make this compromise.
     */
    if (CHECK_ADDR_EQUAL(virtAddr, fdw->virtAddr)) {
      fd = fdw->descriptor;
      break;
    }
  }

  vm_osal_mutex_unlock(&monitor.mutex);

  if (-1 == fd) {
    LOG_ERR("Cannot locate corresponding DMA-BUF fd.\n");
  }

  return fd;
}

static inline
int MinkMem_updateShareMapInfo(FdWrapper *fdw, unsigned int flags)
{
  int ret = Object_ERROR;
  const ITAccessPermissions_rules lendConfRules = (ITAccessPermissions_rules){
    {{0, 0}}, {{0, 0}}, ITAccessPermissions_removeSelfAccess
  };
  Object memObj = FdWrapperToObject(fdw);

  if (flags == MINKMEM_VM_LEND) {
    if (Object_isERROR(RemoteShareMemory_attachConfinement(&lendConfRules, &memObj))) {
      LOG_ERR("Cannot attach confinementRules.\n");
      return Object_ERROR_UNAVAIL;
    }
    munmap(fdw->virtAddr, fdw->bufSize);
    fdw->mapped = false;
    fdw->ipcFlags = MINKMEM_VM_LEND;
    ret = Object_OK;
  } else if (flags == MINKMEM_VM_SHARE) {
    // Explicitly set the flags to SHARE
    fdw->ipcFlags = MINKMEM_VM_SHARE;
    ret = Object_OK;
  }

  return ret;
}

static inline
int MinkMem_updateLendMapInfo(FdWrapper *fdw, unsigned int flags)
{
  int ret = Object_ERROR;
  void *ptr = NULL;

  if (flags == MINKMEM_VM_LEND) {
    // Do nothing cause confinement was already set and virtAddr was already unmapped
    ret = Object_OK;
  } else if (flags == MINKMEM_VM_SHARE) {
    ptr = mmap(NULL, fdw->bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, fdw->descriptor, 0);
    if (MAP_FAILED == ptr) {
      LOG_ERR("Failed to map DMA buffer into address space.\n");
      return Object_ERROR_UNAVAIL;
    }
    fdw->ipcFlags = MINKMEM_VM_SHARE;
    fdw->virtAddr = ptr;
    fdw->mapped = true;
    // Reset the confinement cause it was set for LEND before
    Object_ASSIGN_NULL(fdw->confinement);
    ret = Object_OK;
  }

  return ret;
}

static int MinkMem_updateMapInfo(FdWrapper *fdw, unsigned int flags)
{
  int ret = Object_ERROR;

  if ((fdw->ipcFlags == MINKMEM_VM_UNSET || fdw->ipcFlags == MINKMEM_VM_SHARE)
      && fdw->mapped) {
    ret = MinkMem_updateShareMapInfo(fdw, flags);
    if (Object_isERROR(ret)) {
      LOG_ERR("Failed to update share mapInfo with ret=%d.\n", ret);
    }
  } else if (fdw->ipcFlags == MINKMEM_VM_LEND && !fdw->mapped) {
    ret = MinkMem_updateLendMapInfo(fdw, flags);
    if (Object_isERROR(ret)) {
      LOG_ERR("Failed to update lend mapInfo with ret=%d.\n", ret);
    }
  } else {
    LOG_ERR("When asking for flags=%u, unexpected ipcFlags=%u and mappedStatus=%s.\n",
            flags, fdw->ipcFlags, fdw->mapped ? "mapped" : "unmapped");
    ret = Object_ERROR_INVALID;
  }

  return ret;
}

static int MinkMem_queryMemObj(int queryType, int dmaBufFd, void *virtAddr,
                               bool *found, Object *objOut)
{
  int ret = Object_OK;
  QNode *pQn = NULL;
  FdWrapper *fdw = NULL;
  bool isEntry = false;

  if (!monitor.initialized) {
    LOG_ERR("The MinkMem hasnt been initialized yet.\n");
    return Object_ERROR;
  }

  if (!found || !objOut) {
    LOG_ERR("Either found or objOut is invalid.\n");
    return Object_ERROR_INVALID;
  }

  *found = false;
  *objOut = Object_NULL;

  if (queryType != QUERY_TYPE_DMABUFFD && queryType != QUERY_TYPE_VIRTADDR) {
    LOG_ERR("Invalid queryType=%d.\n", queryType);
    return Object_ERROR_INVALID;
  }

  if (queryType == QUERY_TYPE_DMABUFFD && dmaBufFd <= 0) {
    LOG_ERR("Invalid dmaBufFd.\n");
    return Object_ERROR_INVALID;
  }

  if (queryType == QUERY_TYPE_VIRTADDR && !virtAddr) {
    LOG_ERR("Invalid virtAddr.\n");
    return Object_ERROR_INVALID;
  }

  vm_osal_mutex_lock(&monitor.mutex);

  QLIST_FOR_ALL(&monitor.mapInfoList, pQn) {
    fdw = c_containerof(pQn, FdWrapper, qn);
    if (!fdw) {
      LOG_ERR("Got unexpectedly NULL MapInfo item.\n");
      continue;
    }

    if (queryType == QUERY_TYPE_DMABUFFD) {
      isEntry = (dmaBufFd == fdw->descriptor);
    } else if (queryType == QUERY_TYPE_VIRTADDR) {
      /*  MinkMem_setMemObjAttr() for MEM_LEND unmaps the va
            which will make the va available for alloc/map.
          We have no choice to make this compromise.
       */
      isEntry = CHECK_ADDR_EQUAL(virtAddr, fdw->virtAddr);
    }

    if (isEntry) {
      *found = true;
      *objOut = FdWrapperToObject(fdw);
      Object_retain(*objOut);
      break;
    }
  }

  vm_osal_mutex_unlock(&monitor.mutex);

  return ret;
}

int MinkMem_setMemObjAttr(Object memObj, unsigned int flags)
{
  int ret = Object_ERROR;
  FdWrapper *fdw = FdWrapperFromObject(memObj);
  const ITAccessPermissions_rules lendConfRules = (ITAccessPermissions_rules){
    {{0, 0}}, {{0, 0}}, ITAccessPermissions_removeSelfAccess
  };

  if (Object_isNull(memObj) || !fdw) {
    LOG_ERR("Invalid memObj.\n");
    return Object_ERROR_INVALID;
  }

  if (flags != MINKMEM_VM_SHARE &&
      flags != MINKMEM_VM_LEND) {
    LOG_ERR("Invalid ipcFlags.\n");
    return Object_ERROR_INVALID;
  }

  if (fdw->fromMinkMem) {
    ret = MinkMem_updateMapInfo(fdw, flags);
  } else {
    if (flags == MINKMEM_VM_SHARE) {
      // Reset the confinement cause it might be set for LEND before
      Object_ASSIGN_NULL(fdw->confinement);
      ret = Object_OK;
    } else if (flags == MINKMEM_VM_LEND) {
      ret = RemoteShareMemory_attachConfinement(&lendConfRules, &memObj);
      if (Object_isERROR(ret)) {
        LOG_ERR("Failed to attach confinement.\n");
      }
    }
  }

  return ret;
}

int MinkMem_fdToMemObj(int dmaBufFd, Object *objOut)
{
  int ret = Object_ERROR;
  bool found = false;

  if (!objOut) {
    LOG_ERR("Invalid objOut.\n");
    return Object_ERROR_INVALID;
  }
  *objOut = Object_NULL;

  if (dmaBufFd <= 0) {
    LOG_ERR("Invalid dmaBufFd.\n");
    return Object_ERROR_INVALID;
  }

  ret = MinkMem_queryMemObj(QUERY_TYPE_DMABUFFD, dmaBufFd, NULL, &found, objOut);
  if (Object_isERROR(ret)) {
    LOG_ERR("Failed to MinkMem_queryMemObj with ret=%d.\n", ret);
    return ret;
  }

  /*  The DMA buffer is allocated by other allocator other than MinkMem e.g. RPCMem
      We dont record its information in our MapInfoList
      We just wrap it as a memory object and we guarantee that
        releasing this memory object wont close the DMA-BUF fd
  */
  if (!found) {
    *objOut = FdWrapper_newWithCloseFlags(dmaBufFd, false);
    if (Object_isNull(*objOut)) {
      LOG_ERR("Cannot create memory obj.\n");
      ret = Object_ERROR_MEM;
    }
  }

  return ret;
}

int MinkMem_virtAddrToMemObj(void *virtAddr, Object *objOut)
{
  int ret = Object_ERROR;
  bool found = false;

  if (!objOut) {
    LOG_ERR("Invalid objOut.\n");
    return Object_ERROR_INVALID;
  }
  *objOut = Object_NULL;

  if (!virtAddr) {
    LOG_ERR("Invalid virtAddr.\n");
    return Object_ERROR_INVALID;
  }

  ret = MinkMem_queryMemObj(QUERY_TYPE_VIRTADDR, -1, virtAddr, &found, objOut);
  if (Object_isERROR(ret)) {
    LOG_ERR("Failed to MinkMem_queryMemObj with ret=%d.\n", ret);
    return ret;
  }

  /*  The DMA buffer is allocated by other allocator other than MinkMem e.g. RPCMem
      We cant locate the DMA-BUF fd as per the virtAddr
  */
  if (!found) {
    LOG_ERR("This virtAddr is not monitored by MinkMem.\n");
    return Object_ERROR_INVALID;
  }

  return ret;
}

void *MinkMem_memObjToVirtAddr(Object memObj, size_t *virtAddrSize)
{
  void *ptr = NULL;
  FdWrapper *fdw = FdWrapperFromObject(memObj);

  if (!virtAddrSize) {
    LOG_ERR("virtAddrSize is NULL.\n");
    return NULL;
  }

  *virtAddrSize = 0;

  if (Object_isNull(memObj) || !fdw) {
    LOG_ERR("Memory object is invalid.\n");
    return NULL;
  }

  if (fdw->fromMinkMem) {
    if ((fdw->ipcFlags == MINKMEM_VM_UNSET || fdw->ipcFlags == MINKMEM_VM_SHARE) && fdw->mapped) {
      ptr = fdw->virtAddr;
      *virtAddrSize = fdw->bufSize;
    } else if (fdw->ipcFlags == MINKMEM_VM_LEND && !fdw->mapped) {
      LOG_ERR("The DMA-BUF is meant for MEM_LEND. It shouldnt be mapped.\n");
    } else {
      LOG_ERR("When asking for mapped virtAddr, unexpected ipcFlags=%u and mappedStatus=%s.\n",
              fdw->ipcFlags, fdw->mapped ? "mapped" : "unmapped");
    }
  } else {
    LOG_ERR("This memObj is not monitored by MinkMem.\n");
  }

  return ptr;
}

void MinkMem_free(void *virtAddr)
{
  QNode *pQn = NULL, *pQnNext = NULL;
  FdWrapper *fdw = NULL, *removingFdw = NULL;
  Object memObj = Object_NULL;

  if (!monitor.initialized) {
    LOG_ERR("The MinkMem hasnt been initialized yet.\n");
    return;
  }

  if (!virtAddr) {
    LOG_ERR("Invalid virtAddr.\n");
    return;
  }

  vm_osal_mutex_lock(&monitor.mutex);

  QLIST_NEXTSAFE_FOR_ALL(&monitor.mapInfoList, pQn, pQnNext) {
    fdw = c_containerof(pQn, FdWrapper, qn);
    if (!fdw) {
      LOG_ERR("Got unexpectedly NULL MapInfo item.\n");
      continue;
    }

    if (CHECK_ADDR_EQUAL(virtAddr, fdw->virtAddr)) {
      removingFdw = fdw;
      QNode_dequeue(&fdw->qn);
      break;
    }
  }

  vm_osal_mutex_unlock(&monitor.mutex);

  if (!removingFdw) {
    LOG_ERR("Cannot locate corresponding DMA buffer in MapInfoList.\n");
  } else {
    memObj = FdWrapperToObject(removingFdw);
    /* Note that the refs of memObj might not be 0(so the fd might not be closed instantly)
       after below operation because MinkSocket's OT might be still holding it
       (waiting for MEM_RELEASE/MEM_RECLAIM).
     */
    Object_ASSIGN_NULL(memObj);
  }
}
