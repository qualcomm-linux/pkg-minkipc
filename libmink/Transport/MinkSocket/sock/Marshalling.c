// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "cdefs.h"
#include "check.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "memscpy.h"
#include "msforwarder.h"
#include "Marshalling.h"
#include "MSMem.h"
#include "Primordial.h"
#include "Utils.h"
#include "Profiling.h"

#ifdef REMOTE_SHAREMM
#include "Confinement.h"
#include "IConfinement.h"
#include "ITAccessPermissions.h"
#include "RemoteShareMemory.h"
#include "ShareMemory.h"
#include "WrappedMemparcel.h"
#endif // REMOTE_SHAREMM

/* flags for marshalling */
#define LXCOM_NULL_OBJECT               0
#define LXCOM_CALLER_OBJECT             1
#define LXCOM_CALLEE_OBJECT             2
#define LXCOM_DESCRIPTOR_OBJECT         4
#define LXCOM_MEMPARCEL_OBJECT          8
#define LXCOM_MEMPARCEL_INFO            16
#define LXCOM_MEMPARCEL_SPECIALRULES    32

#define NULL_OBJECT_HANDLE      UINT16_MAX
#define INVALID_OBJECT_HANDLE   (UINT16_MAX - 1)

#define MEMOBJ_FDWRAPPER 0
#define MEMOBJ_MSMEM     1

#define MARSHAL_ERROR_MISMATCH (-1000)

static uint8_t PadBuf[8] = {0xF};

//return how much to add for the alignment
#define PADDED(x)     ({   \
  size_t sizeAligned = 0;  \
  if (0 != x) {            \
    sizeAligned = ((size_t)((x) + (((uint64_t)(~(x)) + 1) & (LXCOM_MSG_ALIGNMENT - 1)))); \
  }   \
  sizeAligned; })

#define ObjectCounts_numObjects(k)  (ObjectCounts_numOI(k) + \
                                     ObjectCounts_numOO(k))

#define ObjectCounts_indexObjects(k) \
  ObjectCounts_indexOI(k)

#define ObjectCounts_indexBUFFERS(k) \
  ObjectCounts_indexBI(k)

#define ObjectCounts_numBUFFERS(k) \
  (ObjectCounts_numBI(k) + ObjectCounts_numBO(k))

#define ObjectCounts_numIn(k) \
  (ObjectCounts_numBUFFERS(k) + ObjectCounts_numOI(k))

#define ObjectCounts_numOut(k) \
  (ObjectCounts_numBO(k) + ObjectCounts_numOO(k))

#define Sizeof_invReq(k, extraOI) \
  (c_offsetof(lxcom_inv_req, a) + (ObjectCounts_numIn(k) + extraOI) * sizeof(lxcom_arg))

#define Sizeof_invSucc(k, extraOO) \
  (c_offsetof(lxcom_inv_succ, a) + (ObjectCounts_numOut(k) + extraOO) * sizeof(lxcom_arg))

#define Sizeof_dataInput(args, k, idxNow, sizeOnWire)    ({     \
  size_t __sizeExtend = PADDED((sizeOnWire));                   \
  CONTINUE_ARGS(__ii, (idxNow), k, BI) {                        \
    __sizeExtend += PADDED(args[__ii].b.size);                  \
  }                                                             \
  __sizeExtend; })

#define Sizeof_dataOutput(args, k, idxNow, sizeOnWire)    ({    \
  size_t __sizeExtend = PADDED((sizeOnWire));                   \
  CONTINUE_ARGS(__ii, (idxNow), k, BO) {                        \
    __sizeExtend += PADDED(args[__ii].b.size);                  \
  }                                                             \
  __sizeExtend; })

// Memparcel handles are 64 bits, arg handles are 16 bits, so we need 4
#define NUM_EXTRA_ARGS_PER_MEMPARCEL 4

// ITAccessPermissions_rules.specialRules are 64 bits
// We just need specialRules in minksocket for now
// TODO:
//   i. send whole confinement to another VM efficiently
//  ii. check if via BI is practicable
#define NUM_EXTRA_ARGS_SPECIALRULES 4

static QList gSharedMemList = { { &gSharedMemList.n, &gSharedMemList.n } };
static vm_osal_mutex gSharedMemMutex = PTHREAD_MUTEX_INITIALIZER;

static inline int32_t
Marshal_retrieveDmabuf(QList *qlist, vm_osal_mutex *mtx, int64_t memparcelHandle, int *fdOut)
{
  int ret = Object_ERROR;
  QNode *pQn = NULL;
  MSMem *msm = NULL;

  if (memparcelHandle < 0 || !fdOut || !qlist || !mtx) {
    LOG_ERR("Wrong input.\n");
    goto exit;
  }

  vm_osal_mutex_lock(mtx);
  QLIST_FOR_ALL(qlist, pQn) {
    msm = c_containerof(pQn, MSMem, node);
    if (!msm) {
      LOG_ERR("Got unexpectedly NULL MemList item.\n");
      continue;
    }
    if (msm->memParcelHandle == memparcelHandle) {
      ret = Object_OK;
      // Note that two fdwrappers will point to the same memory buffer
      *fdOut = dup(msm->dmaBufFd);
      break;
    }
  }
  vm_osal_mutex_unlock(mtx);

exit:
  return ret;
}

static inline int32_t Marshal_enqueueMemObj(QList *qlist, vm_osal_mutex *mtx, MSMem* msm)
{
  if (!qlist || !mtx || !msm){
    LOG_ERR("Wrong input.\n");
    return Object_ERROR;
  }

  vm_osal_mutex_lock(mtx);
  QList_appendNode(qlist, &(msm->node));
  vm_osal_mutex_unlock(mtx);

  return Object_OK;
}

static inline void
MarshalOut_configLocalMemExtraArgs(MinkSocket *minksock, size_t *numExtra)
{
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    // Reset the version.
    version = MINKSOCK_VER_UNINITIALIZED;
  }

  if (version >= MINKSOCK_VER_INDIRECT_MEM) {
    *numExtra += NUM_EXTRA_ARGS_SPECIALRULES;
  }
}

static inline void
MarshalIn_configLocalMemExtraArgs(MinkSocket *minksock, size_t *numExtra, size_t *ii)
{
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    // Reset the version.
    version = MINKSOCK_VER_UNINITIALIZED;
  }

  if (version >= MINKSOCK_VER_INDIRECT_MEM) {
    *numExtra += NUM_EXTRA_ARGS_SPECIALRULES;
    *ii += NUM_EXTRA_ARGS_SPECIALRULES;
  }
}

#define COUNT_NUM_EXTRA_ARGS_OUTBOUND(k, a, section, minksock) ({  \
  size_t __numExtra = 0;                                           \
  FOR_ARGS(i, k, section) {                                        \
    int fd;                                                        \
    if (isWrappedFd(a[i].o, &fd) || isMSMem(a[i].o, &fd)) {        \
      if (MinkSocket_isRemote(minksock)) {                         \
        __numExtra += NUM_EXTRA_ARGS_PER_MEMPARCEL;                \
        __numExtra += NUM_EXTRA_ARGS_SPECIALRULES;                 \
      } else {                                                     \
        MarshalOut_configLocalMemExtraArgs(minksock, &__numExtra); \
      }                                                            \
    }                                                              \
  }                                                                \
  __numExtra; })

#define COUNT_NUM_EXTRA_ARGS_INBOUND(k, a, section, start, minksock) ({   \
  size_t __numExtra = 0;                                                  \
  size_t __ii = start;                                                    \
  FOR_ARGS(i, k, section) {                                               \
    uint16_t flags = a[__ii].o.flags;                                     \
    if (flags & LXCOM_MEMPARCEL_OBJECT) {                                 \
      __numExtra += NUM_EXTRA_ARGS_PER_MEMPARCEL;                         \
      __ii += NUM_EXTRA_ARGS_PER_MEMPARCEL;                               \
      __numExtra += NUM_EXTRA_ARGS_SPECIALRULES;                          \
      __ii += NUM_EXTRA_ARGS_SPECIALRULES;                                \
    } else if (flags & LXCOM_DESCRIPTOR_OBJECT) {                         \
      MarshalIn_configLocalMemExtraArgs(minksock, &__numExtra,            \
                                        &__ii);                           \
    }                                                                     \
    __ii++;                                                               \
  }                                                                       \
  __numExtra; })

#define CHECK_MAX_ARGS(k, dir, numExtra)                           \
  do {                                                             \
    int numArgs = ObjectCounts_num##dir(k);                        \
    int numTotalArgs = numArgs + numExtraArgs;                     \
    if (numTotalArgs > LXCOM_MAX_ARGS) {                           \
      LOG_ERR("Too many args: %d (args : %d, extra args: %d)\n",   \
        (uint32_t)numTotalArgs, (uint32_t)numArgs,                 \
        (uint32_t)numExtraArgs);                                   \
      return Object_ERROR_MAXARGS;                                 \
    }                                                              \
  } while(0)

// To avoid potential risk on present platform, we just do simple check.
// TODO: we need to check it with ObjectCounts k in next generation.
#define CHECK_OBJ_INDEX_CLEAN(i)                              \
  do {                                                        \
    if ((i) >= LXCOM_MAX_ARGS) {                              \
      LOG_ERR("Out of index: index = %d\n", (uint32_t)(i));   \
      ERR_CLEAN(Object_ERROR_MAXARGS);                        \
    }                                                         \
  } while (0)

// To avoid potential risk on present platform, we just do simple check.
// TODO: we need to check it with ObjectCounts k in next generation.
#define CHECK_OBJ_INDEX_RETURN(i)                             \
  do {                                                        \
    if ((i) >= LXCOM_MAX_ARGS) {                              \
      LOG_ERR("Out of index: index = %d\n", (uint32_t)(i));   \
      return Object_ERROR_MAXARGS;                            \
    }                                                         \
  } while (0)

#define CONTINUE_OR_RETURN(func, caseStr)   ({                     \
  err = (func);                                                    \
  if (Object_isOK(err)) {                                          \
    continue;                                                      \
  } else if(MARSHAL_ERROR_MISMATCH != err) {                       \
    LOG_ERR("Failed to marshal %s with err=%d.\n", caseStr, err);  \
    return err;                                                    \
  } else {                                                         \
    /*go on next attempt on other scenario*/                       \
  }  })

#define CONTINUE_OR_CLEANUP(func, caseStr)   ({                    \
  err = (func);                                                    \
  if (Object_isOK(err)) {                                          \
    continue;                                                      \
  } else if(MARSHAL_ERROR_MISMATCH != err) {                       \
    LOG_ERR("Failed to marshal %s with err=%d.\n", caseStr, err);  \
    goto cleanup;                                                  \
  } else {                                                         \
    /*go on next attempt on other scenario*/                       \
  }  })

static
void Flatdata_dump(const void *data, size_t size)
{
  const char *byte = (char *)data;

  for (size_t i = 0; i < size; i++) {
    for (int j = 7; j >= 0; j--) {
      printf("%c", (byte[i] & (1 << j)) ? '1' : '0');
    }
    printf(" ");
  }
  printf("\n");
}

/* Marshalling scenario for Object_NULL.
 */
static inline
int32_t MarshalOut_nullObject(Object obj,lxcom_arg *args, int32_t pos)
{
  if (Object_isNull(obj)) {
    args[pos].o.flags = LXCOM_NULL_OBJECT;
    args[pos].o.handle = NULL_OBJECT_HANDLE;
    return Object_OK;
  }

  return MARSHAL_ERROR_MISMATCH;
}

static inline
int32_t MarshalIn_nullObject(lxcom_arg *args, Object *obj, int32_t pos)
{
  uint16_t flags = args[pos].o.flags;
  //uint16_t handle = args[pos].o.handle;

  if (LXCOM_NULL_OBJECT == flags) {
    *obj = Object_NULL;
    return Object_OK;
  }

  return MARSHAL_ERROR_MISMATCH;
}

static inline
int32_t MarshalOut_caller_nullObject(Object obj,lxcom_arg *args, int32_t pos)
{
  return MarshalOut_nullObject(obj, args, pos);
}

static inline
int32_t MarshalOut_callee_nullObject(Object obj,lxcom_arg *args, int32_t pos)
{
  return MarshalOut_nullObject(obj, args, pos);
}

static inline
int32_t MarshalIn_callee_nullObject(lxcom_arg *args, Object *obj, int32_t pos)
{
  return MarshalIn_nullObject(args, obj, pos);
}

static inline
int32_t MarshalIn_caller_nullObject(lxcom_arg *args, Object *obj, int32_t pos)
{
  return MarshalIn_nullObject(args, obj, pos);
}

#ifdef REMOTE_SHAREMM

/* Marshalling scenario for MSForwarder.
 */
static inline int32_t
MarshalIn_specialMsforwarder(lxcom_arg *a, int32_t *pos, Object *obj)
{
  Object wrappedFd = Object_NULL;
  int32_t ret = Object_ERROR;
  (void)a;

  if (!isWrappedMemparcel(*obj)) {
    return Object_ERROR_BADOBJ;
  }

  ret = IWrappedMemparcel_getWrappedFdObj(*obj, &wrappedFd);
  if (!Object_isOK(ret) || Object_isNull(wrappedFd)) {
    return Object_ERROR;
  }

  // Decrease ref of wrappedmemparcel by 1.
  IWrappedMemparcel_release(*obj);

  *obj = wrappedFd;
  *pos += NUM_EXTRA_ARGS_PER_MEMPARCEL;
  *pos += NUM_EXTRA_ARGS_SPECIALRULES;

  LOG_TRACE("Marshalled msforwarder which is exactly wrappedmemparcel.\n");

  return Object_OK;
}

#endif // REMOTE_SHAREMM

static inline
int32_t MarshalOut_msforwarder(MinkSocket *minksock, Object obj, lxcom_arg *args,
                               int32_t pos, int32_t objFlag)
{
  MSForwarder *msf = MSForwarderFromObject(obj);

  if (!msf || msf->conn != minksock) {
    return MARSHAL_ERROR_MISMATCH;
  }

  args[pos].o.flags = objFlag;
  args[pos].o.handle = msf->handle;

  return Object_OK;
}

static inline
int32_t MarshalIn_msforwarder(MinkSocket *minksock, ObjectTable *objTable, lxcom_arg *args,
                              Object *obj, int32_t *pos, int32_t objFlag)
{
  uint16_t flags = args[*pos].o.flags;
  uint16_t handle = args[*pos].o.handle;
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;

  if (!(flags & objFlag)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    return Object_ERROR_UNAVAIL;
  }

  *obj = ObjectTable_recoverObject(objTable, handle);
  if (Object_isNull(*obj)) {
    LOG_ERR("Fail to recover handle=%d from objTable=%p.\n", handle, objTable);
    return Object_ERROR_UNAVAIL;
  }

#ifdef REMOTE_SHAREMM
  if (flags & LXCOM_MEMPARCEL_OBJECT) {
    int32_t ret = MarshalIn_specialMsforwarder(args, pos, obj);
    if (!Object_isOK(ret)) {
      Object_release(*obj);
      LOG_ERR("Fail on MarshalIn_specialMsforwarder() in the position[%d]\n", *pos);
      return Object_ERROR_UNAVAIL;
    }
  }
#endif // REMOTE_SHAREMM

  if (version >= MINKSOCK_VER_INDIRECT_MEM &&
      (flags & LXCOM_DESCRIPTOR_OBJECT)) {
    *pos += NUM_EXTRA_ARGS_SPECIALRULES;
  }

  return Object_OK;
}

static inline
int32_t MarshalOut_caller_msforwarder(MinkSocket *minksock, Object obj,
                                      lxcom_arg *args, int32_t pos)
{
  return MarshalOut_msforwarder(minksock, obj, args, pos, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalOut_callee_msforwarder(MinkSocket *minksock, Object obj,
                                      lxcom_arg *args, int32_t pos)
{
  return MarshalOut_msforwarder(minksock, obj, args, pos, LXCOM_CALLER_OBJECT);
}

static inline
int32_t MarshalIn_callee_msforwarder(MinkSocket *minksock, ObjectTable *objTable, lxcom_arg *args,
                                     Object *obj, int32_t *pos)
{
  return MarshalIn_msforwarder(minksock, objTable, args, obj, pos, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalIn_caller_msforwarder(MinkSocket *minksock, ObjectTable *objTable, lxcom_arg *args,
                                     Object *obj, int32_t *pos)
{
  return MarshalIn_msforwarder(minksock, objTable, args, obj, pos, LXCOM_CALLER_OBJECT);
}

/* Marshalling scenario for localGenericMem.
 */
static inline int32_t
MarshalOut_forwardMemDep(MinkSocket *minksock, Object obj, uint16_t type,
                         lxcom_arg *args, int32_t *pos, bool *forwarded,
                         uint16_t objFlags)
{
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;
  FdWrapper *fdw = NULL;
  MSMem *msmem = NULL;
  MSForwarder *msf = NULL;
  Object dependency = Object_NULL;

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    return Object_ERROR_UNAVAIL;
  }

  *forwarded = false;

  switch (type) {
    case MEMOBJ_FDWRAPPER:
      fdw = FdWrapperFromObject(obj);
      if (fdw == NULL) {
        LOG_ERR("Unexpected error occurs as fdwrapper.\n");
        return Object_ERROR_UNAVAIL;
      }
      dependency = fdw->dependency;
      break;
    case MEMOBJ_MSMEM:
      msmem = MSMemFromObject(obj);
      if (msmem == NULL) {
        LOG_ERR("Unexpected error occurs as msmem.\n");
        return Object_ERROR_UNAVAIL;
      }
      dependency = msmem->dependency;
      break;
    default:
      LOG_ERR("Invalid MO type:%d!\n", type);
      return Object_ERROR_INVALID;
  }

  if (!Object_isNull(dependency)) {
    msf = MSForwarderFromObject(dependency);
    if (msf && msf->conn == minksock) {
      *forwarded = true;
      args[*pos].o.flags = objFlags;
      args[*pos].o.handle = msf->handle;

      if (objFlags & LXCOM_MEMPARCEL_OBJECT) {
        *pos += NUM_EXTRA_ARGS_PER_MEMPARCEL;
        *pos += NUM_EXTRA_ARGS_SPECIALRULES;
      }

      if (version >= MINKSOCK_VER_INDIRECT_MEM && (objFlags & LXCOM_DESCRIPTOR_OBJECT)) {
        *pos += NUM_EXTRA_ARGS_SPECIALRULES;
      }

      LOG_TRACE("Forwarded dependency from memory object with handle=%d,msf=%p,minksock=%p.\n",
                msf->handle, msf, minksock);
    }
  }

  return Object_OK;
}

static inline int32_t
MarshalOut_retainLocalGenericMemDep(MinkSocket *minksock, ObjectTable *objTable,
                                    Object obj, lxcom_arg *args, int32_t *pos)
{
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;
  int32_t handle = -1;
  FdWrapper *fdw = NULL;
  MSForwarder *msf = NULL;

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    return Object_ERROR_UNAVAIL;
  }

  fdw = FdWrapperFromObject(obj);
  if (fdw == NULL) {
    LOG_ERR("Unexpected error occurs as fdwrapper.\n");
    return Object_ERROR_UNAVAIL;
  }

  if (!Object_isNull(fdw->dependency)) {
    msf = MSForwarderFromObject(fdw->dependency);
    if (msf && msf->conn == minksock) {
      handle = ObjectTable_addObject(objTable, fdw->dependency);
      if (handle == -1) {
        LOG_ERR("Failed to add fdw.dep to OT with minksock=%p,objTable=%p.\n",
                minksock, objTable);
        return Object_ERROR_KMEM;
      }

      if (version >= MINKSOCK_VER_INDIRECT_MEM) {
        args[*pos - NUM_EXTRA_ARGS_SPECIALRULES].o.handle = handle;
      } else {
        args[*pos].o.handle = handle;
      }

      LOG_TRACE("Added fdw.dep to OT with fd=%d,handle=%d,msf=%p,minksock=%p.\n",
                fdw->descriptor, handle, msf, minksock);
    }
  }

  return Object_OK;
}

static inline
int32_t MarshalOut_localGenericMem(MinkSocket *minksock, Object obj, lxcom_arg *args,
                                   int32_t *pos, int *fds, size_t *fdCnts)
{
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;
  int32_t memFd = -1;
  FdWrapper *fdw = FdWrapperFromObject(obj);
  uint64_t specialRules = 0;
  Object conf = Object_NULL;

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    return Object_ERROR_UNAVAIL;
  }

  if (fdw == NULL) {
    LOG_ERR("Unexpected error occurs as fdwrapper.\n");
    return Object_ERROR_UNAVAIL;
  }
  memFd = fdw->descriptor;

  args[*pos].o.flags = LXCOM_DESCRIPTOR_OBJECT;
  args[*pos].o.handle = INVALID_OBJECT_HANDLE;

  fds[*fdCnts] = memFd;
  *fdCnts += 1;

  if (version >= MINKSOCK_VER_INDIRECT_MEM) {
    conf = fdw->confinement;
    if (Object_isNull(conf) || !ConfinementFromObject(conf) ||
        Object_isERROR(IConfinement_getSpecialRules(conf, &specialRules))) {
      LOG_TRACE("No valid confinement rules with fdw=%p. Passing default rules.\n", fdw);
      specialRules = ITAccessPermissions_keepSelfAccess;
    }

    for (int i = 0; i < NUM_EXTRA_ARGS_SPECIALRULES; i++) {
      *pos += 1;
      args[*pos].o.flags = LXCOM_MEMPARCEL_SPECIALRULES;
      args[*pos].o.handle = (uint16_t) (specialRules >> i * 16);
    }

    LOG_TRACE("Marshalled local specialRules = %u%09u.\n",
              UINT64_HIGH(specialRules), UINT64_LOW(specialRules));
  }

  LOG_TRACE("Marshalled local genericMem: fd = %d.\n", memFd);

  return Object_OK;
}

static inline
int32_t MarshalIn_localGenericMem(MinkSocket *minksock, lxcom_arg *args, int *fds,
                                  size_t fdCnts, Object *obj, int32_t *pos, size_t *fdIdx)
{
  uint16_t flags = args[*pos].o.flags;
  uint16_t handle = args[*pos].o.handle;
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;
  int32_t ret = Object_ERROR;
  FdWrapper *fdw = NULL;
  Object msf = Object_NULL;
  uint16_t handleChunk = 0;
  uint64_t specialRules = 0;
  ITAccessPermissions_rules *confRules = NULL;

  if (!(flags & LXCOM_DESCRIPTOR_OBJECT)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  if (*fdIdx >= fdCnts) {
    LOG_ERR("Fds exceed the limit.\n");
    return Object_ERROR_MAXARGS;
  }

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    return Object_ERROR_UNAVAIL;
  }

  *obj = FdWrapper_new(fds[*fdIdx]);
  if (Object_isNull(*obj)) {
    LOG_ERR("Failed to create FdWrapper.\n");
    return Object_ERROR_UNAVAIL;
  }

  if ((handle != INVALID_OBJECT_HANDLE) && (handle != UINT16_MAX)) {
    ret = MSForwarder_new(minksock, handle, &msf);
    if (ret) {
      LOG_ERR("Failed to create MSForwarder.\n");
      return Object_ERROR_UNAVAIL;
    }
    fdw = FdWrapperFromObject(*obj);
    if (fdw == NULL) {
      LOG_ERR("Unexpected error occurs as fdwrapper.\n");
      return Object_ERROR_UNAVAIL;
    }
    fdw->dependency = msf;
  }

  if (version >= MINKSOCK_VER_INDIRECT_MEM) {
    for (int i = 0; i < NUM_EXTRA_ARGS_SPECIALRULES; i++) {
      *pos += 1;
      if (!(args[*pos].o.flags & LXCOM_MEMPARCEL_SPECIALRULES)) {
        LOG_ERR("Error ObjectFlag, expected:%d, actual:%d.\n",
                LXCOM_MEMPARCEL_SPECIALRULES, args[*pos].o.flags);
        return Object_ERROR_UNAVAIL;
      }
      handleChunk = args[*pos].o.handle;
      specialRules |= ((uint64_t)handleChunk << i * 16);
    }

    confRules = HEAP_ZALLOC_REC(ITAccessPermissions_rules);
    if (!confRules) {
      LOG_ERR("Failed to create confinementRules.\n");
      return Object_ERROR_UNAVAIL;
    }
    confRules->specialRules = specialRules;

    if (Object_isERROR(RemoteShareMemory_attachConfinement(confRules, obj))) {
      HEAP_FREE_PTR(confRules);
      LOG_ERR("Attaching confinement rules failed.\n");
      return Object_ERROR_UNAVAIL;
    }
    HEAP_FREE_PTR(confRules);

    LOG_TRACE("Marshalled local specialRules = %u%09u.\n",
              UINT64_HIGH(specialRules), UINT64_LOW(specialRules));
  }

  LOG_TRACE("Marshalled localMem with fd=%d, handle=%d, minksock=%p.\n",
            fds[*fdIdx], handle, minksock);

  // So we don't double close if an error occurs.
  fds[*fdIdx] = -1;
  *fdIdx += 1;

  return Object_OK;
}

static inline
int32_t MarshalOut_caller_localGenericMem(MinkSocket *minksock, Object obj,
                                          lxcom_arg *args, int32_t *pos,
                                          int *fds, size_t *fdCnts, uint32_t numFds)
{
  uint16_t objFlags = LXCOM_CALLEE_OBJECT;
  bool forwarded = false;
  int32_t ret = Object_ERROR;
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;

  if (MinkSocket_isRemote(minksock) || !FdWrapperFromObject(obj)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  if (*fdCnts >= numFds) {
    LOG_ERR("Fds exceed the limit.\n");
    return Object_ERROR_MAXARGS;
  }

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    return Object_ERROR_UNAVAIL;
  }

  if (version >= MINKSOCK_VER_INDIRECT_MEM) {
    objFlags |= LXCOM_DESCRIPTOR_OBJECT;
  }

  ret = MarshalOut_forwardMemDep(minksock, obj, MEMOBJ_FDWRAPPER, args,
                                 pos, &forwarded, objFlags);
  if (!Object_isOK(ret)) {
    LOG_ERR("Failed to check local fdw.dep with ret=%d.\n", ret);
    return ret;
  }

  if (!forwarded) {
    ret = MarshalOut_localGenericMem(minksock, obj, args, pos, fds, fdCnts);
    if (!Object_isOK(ret)) {
      LOG_ERR("Failed to marshal local genericMem with ret=%d.\n", ret);
      return ret;
    }
  }

  return Object_OK;
}

static inline
int32_t MarshalOut_callee_localGenericMem(MinkSocket *minksock, ObjectTable *objTable,
                                          Object obj, lxcom_arg *args, int32_t *pos,
                                          int *fds, size_t *fdCnts, size_t numFds)
{
  int32_t ret = Object_ERROR;

  if (MinkSocket_isRemote(minksock) || !FdWrapperFromObject(obj)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  if (*fdCnts >= numFds) {
    LOG_ERR("Fds exceed the limit.\n");
    return Object_ERROR_MAXARGS;
  }

  ret = MarshalOut_localGenericMem(minksock, obj, args, pos, fds, fdCnts);
  if (!Object_isOK(ret)) {
    LOG_ERR("Failed to marshal local genericMem with ret=%d.\n", ret);
    return ret;
  }
  ret = MarshalOut_retainLocalGenericMemDep(minksock, objTable, obj, args, pos);
  if (!Object_isOK(ret)) {
    LOG_ERR("Failed to check local fdw.dep with ret=%d.\n", ret);
    return ret;
  }

  return Object_OK;
}

static inline
int32_t MarshalIn_caller_localGenericMem(MinkSocket *minksock, lxcom_arg *args,
                                         int *fds, size_t fdCnts, Object *obj,
                                         int32_t *pos, size_t *fdIdx)
{
  return MarshalIn_localGenericMem(minksock, args, fds, fdCnts, obj, pos, fdIdx);
}

static inline
int32_t MarshalIn_callee_localGenericMem(MinkSocket *minksock, lxcom_arg *args,
                                         int *fds, size_t fdCnts, Object *obj,
                                         int32_t *pos, size_t *fdIdx)
{
  return MarshalIn_localGenericMem(minksock, args, fds, fdCnts, obj, pos, fdIdx);
}

#ifdef REMOTE_SHAREMM

/* Marshalling scenario for remoteGenericMem.
 */
static inline int32_t
MarshalOut_checkGenericConfinement(Object obj, uint64_t *specialRules)
{
  FdWrapper *fdw = NULL;
  Object conf = Object_NULL;

  fdw = FdWrapperFromObject(obj);
  if (fdw == NULL) {
    LOG_ERR("Input non-fdwrapper object!\n");
    return Object_ERROR_INVALID;
  }

  conf = fdw->confinement;
  if (Object_isNull(conf)) {
    if (!Object_isOK(RemoteShareMemory_attachConfinement(NULL, &obj))) {
      LOG_ERR("Attach default rules of MEM_SHARE failed!\n");
      return Object_ERROR_UNAVAIL;
    }
    conf = fdw->confinement;
    LOG_TRACE("Adopt default confinement because not configured.\n");
  } else if (NULL == ConfinementFromObject(conf)) {
    LOG_ERR("FdWrapper has non-confinement!\n");
    return Object_ERROR_BADOBJ;
  }

  if (!Object_isOK(IConfinement_getSpecialRules(conf, specialRules))) {
    LOG_ERR("Unexpected failure happens when IConfinement_getSpecialRules!\n");
    return Object_ERROR_UNAVAIL;
  }

  LOG_TRACE("Checked genericConf where specialRules = %u%09u\n",
            UINT64_HIGH(*specialRules), UINT64_LOW(*specialRules));

  return Object_OK;
}

/* @breif: Check and set confinement for memory object.
 */
static inline int32_t
MarshalOut_populateWrappedMemparcel(MinkSocket *minksock, ObjectTable *objTable, uint32_t invId,
                                    Object obj, uint64_t specialRules, lxcom_arg *args,
                                    int32_t *pos, uint16_t objFlag)
{
  int32_t handle = -1;
  int64_t memparcelHdl = -1;
  bool retrieved = false;
  Object wmp = Object_NULL;

  LOG_PERF("msock = %p, invId = %u, wmp = %p, startShareLend \n", minksock, invId, &wmp);

  if (Object_OK == ObjectTable_retrieveMemObj(objTable, obj, &wmp)) {
    retrieved = true;
  } else {
    if (!Object_isOK(WrappedMemparcel_new(obj, minksock, invId, &wmp))) {
      LOG_ERR("Failed to create wrappedmemparcel.\n");
      return Object_ERROR_UNAVAIL;
    }
  }

  handle = ObjectTable_addObject(objTable, wmp);
  if (handle == -1) {
    LOG_ERR("Failed to add wmp to objTable.\n");
    IWrappedMemparcel_release(wmp);
    return Object_ERROR_KMEM;
  }

  if(!retrieved) {
    // WrappedMemparcel_new() extra retains the wmp
    // Release it to ensure only objectTable retaining the wmp
    IWrappedMemparcel_release(wmp);
  }

  LOG_PERF("msock = %p, invId = %u, wmp = %p, endShareLend \n", minksock, invId, &wmp);

  args[*pos].o.flags = objFlag | LXCOM_MEMPARCEL_OBJECT;
  args[*pos].o.handle = handle;

  if (!Object_isOK(IWrappedMemparcel_getMemparcelHandle(wmp, &memparcelHdl))) {
    LOG_ERR("Failed to get memparcelHdl.\n");
    return Object_ERROR_UNAVAIL;
  }

  for (int i = 0; i < NUM_EXTRA_ARGS_PER_MEMPARCEL; i++) {
    *pos += 1;
    args[*pos].o.flags = LXCOM_MEMPARCEL_INFO;
    args[*pos].o.handle = (uint16_t) (memparcelHdl >> i * 16);
  }

  for (int i = 0; i < NUM_EXTRA_ARGS_SPECIALRULES; i++) {
    *pos += 1;
    args[*pos].o.flags = LXCOM_MEMPARCEL_SPECIALRULES;
    args[*pos].o.handle = (uint16_t) (specialRules >> i * 16);
  }

  LOG_TRACE("remote WrappedFd handle = %d, minkSocket = %p, mpH = %u%09u, specialRules = %u%09ux\n",
            handle, minksock, UINT64_HIGH(memparcelHdl), UINT64_LOW(memparcelHdl),
            UINT64_HIGH(specialRules), UINT64_LOW(specialRules));

  return Object_OK;
}

static inline
int32_t MarshalOut_remoteGenericMem(MinkSocket *minksock, ObjectTable *objTable, uint32_t invId,
                                    Object obj, lxcom_arg *args, int32_t *pos, int32_t objFlag)
{
  bool forwarded = false;
  int32_t ret = Object_ERROR;
  uint64_t specialRules = 0;

  if (!MinkSocket_isRemote(minksock) || !FdWrapperFromObject(obj)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  if (LXCOM_CALLEE_OBJECT == objFlag) {
    ret = MarshalOut_forwardMemDep(minksock, obj, MEMOBJ_FDWRAPPER, args, pos,
                               &forwarded, LXCOM_CALLER_OBJECT | LXCOM_MEMPARCEL_OBJECT);
    if (!Object_isOK(ret)) {
      LOG_ERR("Failed to check fdw.dep with ret=%d.\n", ret);
      return Object_ERROR_UNAVAIL;
    }
  }

  if (!forwarded) {
    ret = MarshalOut_checkGenericConfinement(obj, &specialRules);
    if (!Object_isOK(ret)) {
      LOG_ERR("Failed to check confinement for generic MO with ret=%d.\n", ret);
      return Object_ERROR_UNAVAIL;
    }

    ret = MarshalOut_populateWrappedMemparcel(minksock, objTable, invId, obj, specialRules,
                                              args, pos, objFlag);
    if (!Object_isOK(ret)) {
      LOG_ERR("Failed to populate wrappedMemparcel with ret=%d.\n", ret);
      return Object_ERROR_UNAVAIL;
    }
  }

  return Object_OK;
}

static inline
int32_t MarshalIn_remoteGenericMem(MinkSocket *minksock, uint32_t invId, lxcom_arg *args,
                                   Object *obj, int32_t *pos, int32_t objFlag)
{
  uint16_t flags = args[*pos].o.flags;
  uint16_t handle = args[*pos].o.handle, handleChunk = 0;
  int32_t ret = Object_OK, dmaFd = 0;
  int64_t memparcelHandle = 0;
  uint64_t specialRules = 0;
  ITAccessPermissions_rules *wmpConfRules = NULL;
  MSMem *msmem = NULL;
  Object wmpMsf = Object_NULL;

  if (!(flags & objFlag) || !(flags & LXCOM_MEMPARCEL_OBJECT)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  for (int i = 0; i < NUM_EXTRA_ARGS_PER_MEMPARCEL; i++) {
    *pos += 1;
    if (!(args[*pos].o.flags & LXCOM_MEMPARCEL_INFO)) {
      LOG_ERR("Error ObjectFlag, expected:%d, actual:%d.\n",
              LXCOM_MEMPARCEL_INFO, args[*pos].o.flags);
      ret = Object_ERROR_UNAVAIL;
      goto cleanup;
    }
    handleChunk = args[*pos].o.handle;
    memparcelHandle |= ((int64_t)handleChunk << i * 16);
  }

  for (int i = 0; i < NUM_EXTRA_ARGS_SPECIALRULES; i++) {
    *pos += 1;
    if (!(args[*pos].o.flags & LXCOM_MEMPARCEL_SPECIALRULES)) {
      LOG_ERR("Error ObjectFlag, expected:%d, actual:%d.\n",
              LXCOM_MEMPARCEL_SPECIALRULES, args[*pos].o.flags);
      ret = Object_ERROR_UNAVAIL;
      goto cleanup;
    }
    handleChunk = args[*pos].o.handle;
    specialRules |= ((uint64_t)handleChunk << i * 16);
  }

  wmpConfRules = HEAP_ZALLOC_REC(ITAccessPermissions_rules);
  if (!wmpConfRules) {
    LOG_ERR("Failed to create confinementRules.\n");
    ret = Object_ERROR_UNAVAIL;
    goto cleanup;
  }
  wmpConfRules->specialRules = specialRules;

  LOG_PERF("msock = %p, invId = %u, mpH = %u%09u, startGetMSMem \n", minksock, invId,
           UINT64_HIGH(memparcelHandle), UINT64_LOW(memparcelHandle));

  if (Object_isOK(Marshal_retrieveDmabuf(&gSharedMemList, &gSharedMemMutex, memparcelHandle,
                                         &dmaFd)))
  {
    ret = MSMem_new_remote(dmaFd, wmpConfRules, memparcelHandle, &gSharedMemMutex, obj);
    LOG_TRACE("Retrieved marshalled MSMem with dmaFD = %d \n", dmaFd);
  } else {
    ret = ShareMemory_GetMSMem(memparcelHandle, MinkSocket_getDestVMName(minksock),
                               wmpConfRules, &gSharedMemMutex, obj);
  }
  HEAP_FREE_PTR(wmpConfRules);

  if (!Object_isOK(ret) || Object_isNull(*obj)) {
    LOG_ERR("Failed to get MSM with ret=%d.\n", ret);
    ret = Object_ERROR_UNAVAIL;
    goto cleanup;
  }

  ret = MSForwarder_new(minksock, handle, &wmpMsf);
  if (ret) {
    LOG_ERR("Failed to create MSForwarder.\n");
    ret = Object_ERROR_UNAVAIL;
    goto cleanup;
  }

  msmem = MSMemFromObject(*obj);
  if (!msmem) {
    LOG_ERR("Unexpected error occurs as msmem.\n");
    ret = Object_ERROR_UNAVAIL;
    goto cleanup;
  }
  msmem->dependency = wmpMsf;
  wmpMsf = Object_NULL;

  ret = Marshal_enqueueMemObj(&gSharedMemList, &gSharedMemMutex, msmem);
  if (ret) {
    LOG_ERR("Failed to enqueue MSMem.\n");
    ret = Object_ERROR_UNAVAIL;
    goto cleanup;
  }

  LOG_PERF("msock = %p, invId = %u, fd = %d, dmaSize = %u%09u, endGetMSMem \n", minksock, invId,
           msmem->dmaBufFd, UINT64_HIGH(vm_osal_getDMAFdSize(msmem->dmaBufFd)),
           UINT64_LOW(vm_osal_getDMAFdSize(msmem->dmaBufFd)));

  LOG_TRACE("Marshalled remoteMem mPH = %u%09u, dmaBufFd = %d,handle = %d, minksock = %p\n",
            UINT64_HIGH(memparcelHandle), UINT64_LOW(memparcelHandle),
            msmem->dmaBufFd, handle, minksock);

cleanup:
  if (ret !=  Object_OK) {
    if (!Object_isNull(wmpMsf)) {
      Object_ASSIGN_NULL(wmpMsf);
    }
    if (!Object_isNull(*obj)) {
      Object_ASSIGN_NULL(*obj);
    }
  }
  return ret;
}

static inline
int32_t MarshalOut_caller_remoteGenericMem(MinkSocket *minksock, ObjectTable *objTable,
                                           uint32_t invId, Object obj, lxcom_arg *args,
                                           int32_t *pos)
{
  return MarshalOut_remoteGenericMem(minksock, objTable, invId, obj, args,
                                     pos, LXCOM_CALLER_OBJECT);
}

static inline
int32_t MarshalOut_callee_remoteGenericMem(MinkSocket *minksock, ObjectTable *objTable,
                                           uint32_t invId, Object obj, lxcom_arg *args,
                                           int32_t *pos)
{
  return MarshalOut_remoteGenericMem(minksock, objTable, invId, obj, args,
                                     pos, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalIn_caller_remoteGenericMem(MinkSocket *minksock, uint32_t invId, lxcom_arg *args,
                                          Object *obj, int32_t *pos)
{
  return MarshalIn_remoteGenericMem(minksock, invId, args, obj, pos, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalIn_callee_remoteGenericMem(MinkSocket *minksock, uint32_t invId, lxcom_arg *args,
                                          Object *obj, int32_t *pos)
{
  return MarshalIn_remoteGenericMem(minksock, invId, args, obj, pos, LXCOM_CALLER_OBJECT);
}

#endif // REMOTE_SHAREMM

/* Marshalling scenario for localMSMem
 */
static inline
int32_t MarshalOut_caller_localMSMem(MinkSocket *minksock, ObjectTable *objTable,
                                     Object obj, lxcom_arg *args, int32_t *pos,
                                     int *fds, size_t *fdCnts, uint32_t numFds)
{
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;
  int32_t handle = -1, memFd = -1;
  MSMem *msmem = MSMemFromObject(obj);
  uint64_t specialRules = 0;

  if (MinkSocket_isRemote(minksock) || !MSMemFromObject(obj)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  if (*fdCnts >= numFds) {
    LOG_ERR("Fds exceed the limit.\n");
    return Object_ERROR_MAXARGS;
  }

  if (!msmem) {
    LOG_ERR("Unexpected error occurs as msmem.\n");
    return Object_ERROR_UNAVAIL;
  }
  memFd = msmem->dmaBufFd;

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    return Object_ERROR_UNAVAIL;
  }

  args[*pos].o.flags = LXCOM_DESCRIPTOR_OBJECT;
  handle = ObjectTable_addObject(objTable, obj);
  if (handle == -1) {
    LOG_ERR("Failed to add msmem to objTable.\n");
    return Object_ERROR_KMEM;
  }
  args[*pos].o.handle = handle;

  fds[*fdCnts] = memFd;
  *fdCnts += 1;

  if (version >= MINKSOCK_VER_INDIRECT_MEM) {
    specialRules = msmem->confRules.specialRules;

    for (int i = 0; i < NUM_EXTRA_ARGS_SPECIALRULES; i++) {
      *pos += 1;
      args[*pos].o.flags = LXCOM_MEMPARCEL_SPECIALRULES;
      args[*pos].o.handle = (uint16_t) (specialRules >> i * 16);
    }

    LOG_TRACE("Marshalled local specialRules = %u%09u.\n",
               UINT64_HIGH(specialRules), UINT64_LOW(specialRules));
  }

  LOG_TRACE("Marshalled local MSMem: fd=%d, handle=%d.\n", memFd, handle);

  return Object_OK;
}

static inline
int32_t MarshalOut_callee_localMSMem(MinkSocket *minksock, ObjectTable *objTable,
                                     Object obj, lxcom_arg *args, int32_t *pos,
                                     int *fds, size_t *fdCnts, size_t numFds)
{
  return MarshalOut_caller_localMSMem(minksock, objTable, obj, args,
                                      pos, fds, fdCnts, numFds);
}

static inline
int32_t MarshalIn_caller_localMSMem(MinkSocket *minksock, lxcom_arg *args,
                                    int *fds, size_t fdCnts, Object *obj,
                                    int32_t *pos, size_t *fdIdx)
{
  return MarshalIn_localGenericMem(minksock, args, fds, fdCnts, obj, pos, fdIdx);
}

static inline
int32_t MarshalIn_callee_localMSMem(MinkSocket *minksock, lxcom_arg *args,
                                    int *fds, size_t fdCnts, Object *obj,
                                    int32_t *pos, size_t *fdIdx)
{
  return MarshalIn_localGenericMem(minksock, args, fds, fdCnts, obj, pos, fdIdx);
}

#ifdef REMOTE_SHAREMM

/* Marshalling scenario for remoteMSMem
 */
static inline int32_t
MarshalOut_checkMSMemConfinement(Object obj, uint64_t *specialRules)
{
  uint64_t maskSpecialRules = 0;
  MSMem *msmem = MSMemFromObject(obj);

  if (!msmem) {
    LOG_ERR("Unexpected error occurs as MSMem.\n");
    return Object_ERROR_BADOBJ;
  }

  if (msmem->isLocal) {
    // Set it for MEM_SHARE.
    msmem->confRules.specialRules = ITAccessPermissions_keepSelfAccess;
    LOG_TRACE("Set MEM_SHARE for the MSMem(fd=%d) originating from local.\n",
              msmem->dmaBufFd);
  }

  *specialRules = msmem->confRules.specialRules;

  /*  Remote MSMem (from HLOS) of such specialRules is hopping around QTVM and OEMVM.
   *  ACCEPTs of such DMA buffer are executed by different kernel modules:
   *    i)  QTVM: SMMU Proxy
   *   ii) OEMVM: MEM-BUF
   *  When reaching out to counterpart VM, we should get specialRules flipped over:
   *    i) ITAccessPermissions_mixedControlled -> ITAccessPermissions_smmuProxyControlled
   *   ii) ITAccessPermissions_smmuProxyControlled -> ITAccessPermissions_mixedControlled
   *  But we dont change the original specialRules in MSMem.
   */
  if ((ITAccessPermissions_mixedControlled & *specialRules) ||
      (ITAccessPermissions_smmuProxyControlled & *specialRules)) {
    maskSpecialRules = 
            ITAccessPermissions_mixedControlled | ITAccessPermissions_smmuProxyControlled;
    *specialRules ^= maskSpecialRules;
    LOG_TRACE("specialRules is getting flipped over.\n");
  }

  LOG_TRACE("Checked msmem.conf with fd = %d, specialRules = %u%09u.\n", msmem->dmaBufFd,
             UINT64_HIGH(*specialRules), UINT64_LOW(*specialRules));

  return Object_OK;
}

static inline
int32_t MarshalOut_remoteMSMem(MinkSocket *minksock, ObjectTable *objTable, uint32_t invId,
                               Object obj, lxcom_arg *args, int32_t *pos,
                               int32_t flagDep, int32_t objFlag)
{
  bool forwarded = false;
  int32_t ret = Object_ERROR;
  uint64_t specialRules = 0;

  if (!MinkSocket_isRemote(minksock) || !MSMemFromObject(obj)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  ret = MarshalOut_forwardMemDep(minksock, obj, MEMOBJ_MSMEM, args, pos,
                                 &forwarded, flagDep | LXCOM_MEMPARCEL_OBJECT);
  if (!Object_isOK(ret)) {
    LOG_ERR("Failed to check MSMem.dep with ret=%d.\n", ret);
    return ret;
  }

  if (!forwarded) {
    ret = MarshalOut_checkMSMemConfinement(obj, &specialRules);
    if (!Object_isOK(ret)) {
      LOG_ERR("Failed to check confinement of MSMem with ret=%d.\n", ret);
      return Object_ERROR_UNAVAIL;
    }

    ret = MarshalOut_populateWrappedMemparcel(minksock, objTable, invId, obj, specialRules,
                                              args, pos, objFlag);
    if (!Object_isOK(ret)) {
      LOG_ERR("Failed to populate wrappedMemparcel with ret=%d.\n", ret);
      return Object_ERROR_UNAVAIL;
    }
  }

  return Object_OK;
}

static inline
int32_t MarshalOut_caller_remoteMSMem(MinkSocket *minksock, ObjectTable *objTable, uint32_t invId,
                                      Object obj, lxcom_arg *args, int32_t *pos)
{
  return MarshalOut_remoteMSMem(minksock, objTable, invId, obj, args, pos,
                                LXCOM_CALLEE_OBJECT, LXCOM_CALLER_OBJECT);
}

static inline
int32_t MarshalOut_callee_remoteMSMem(MinkSocket *minksock, ObjectTable *objTable, uint32_t invId,
                                      Object obj, lxcom_arg *args, int32_t *pos)
{
  return MarshalOut_remoteMSMem(minksock, objTable, invId, obj, args, pos,
                                LXCOM_CALLER_OBJECT, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalIn_caller_remoteMSMem(MinkSocket *minksock, uint32_t invId,
                                     lxcom_arg *args, Object *obj, int32_t *pos)
{
  return MarshalIn_callee_remoteGenericMem(minksock, invId, args, obj, pos);
}

static inline
int32_t MarshalIn_callee_remoteMSMem(MinkSocket *minksock, uint32_t invId, lxcom_arg *args,
                                     Object *obj, int32_t *pos)
{
  return MarshalIn_callee_remoteGenericMem(minksock, invId, args, obj, pos);
}

#endif // REMOTE_SHAREMM

/* Marshalling scenario for genericObject.
 */
static inline
int32_t MarshalOut_genericObject(ObjectTable *objTable, Object obj,
                                 lxcom_arg *args, int32_t pos, int32_t objFlag)
{
  int32_t handle = -1;

  handle = ObjectTable_addObject(objTable, obj);
  if (handle == -1) {
    LOG_ERR("Failed to add obj.context=%p to objTable=%p.\n", obj.context, objTable);
    return Object_ERROR_KMEM;
  }
  args[pos].o.flags = objFlag;
  args[pos].o.handle = handle;

  return Object_OK;
}

static inline
int32_t MarshalIn_genericObject(MinkSocket *minksock, lxcom_arg *args,
                                Object *obj, int32_t pos, int32_t objFlag)
{
  int32_t ret = Object_ERROR;
  uint16_t flags = args[pos].o.flags;
  uint16_t handle = args[pos].o.handle;

  if (!(flags & objFlag) || (flags & LXCOM_MEMPARCEL_OBJECT)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  ret = MSForwarder_new(minksock, handle, obj);
  if (!Object_isOK(ret) || Object_isNull(*obj)) {
    LOG_ERR("Fail on MSForwarder_new() in the position[%d]\n", pos);
    return Object_ERROR_UNAVAIL;
  }

  return Object_OK;
}

static inline
int32_t MarshalOut_caller_genericObject(ObjectTable *objTable, Object obj,
                                        lxcom_arg *args, int32_t pos)
{
  return MarshalOut_genericObject(objTable, obj, args, pos, LXCOM_CALLER_OBJECT);
}

static inline
int32_t MarshalOut_callee_genericObject(ObjectTable *objTable, Object obj,
                                        lxcom_arg *args, int32_t pos)
{
  return MarshalOut_genericObject(objTable, obj, args, pos, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalIn_callee_genericObject(MinkSocket *minksock, lxcom_arg *args,
                                       Object *obj, int32_t pos)
{
  return MarshalIn_genericObject(minksock, args, obj, pos, LXCOM_CALLER_OBJECT);
}

static inline
int32_t MarshalIn_caller_genericObject(MinkSocket *minksock, lxcom_arg *args,
                                       Object *obj, int32_t pos)
{
  return MarshalIn_genericObject(minksock, args, obj, pos, LXCOM_CALLEE_OBJECT);
}

static
int32_t MarshalIn_reserveBuffer(ObjectArg *args, lxcom_inv_req *req,
                                void **bufResv, size_t sizeResv)
{
  size_t sizeActual = 0;
  size_t sizeRemain = 0;
  size_t remain = 0;
  size_t pad = 0;
  void *ptr = NULL;

  FOR_ARGS(i, req->k, BO) {
    args[i].b.size = req->a[i].size;
    if(0 != args[i].b.size) {
      sizeActual += PADDED(args[i].b.size);
    } else {
      sizeActual += LXCOM_MSG_ALIGNMENT;
    }

    LOG_TRACE("Callee is requested for bufOut[%d] in size = %d\n",
               i, req->a[i].size);
  }

  if (sizeActual > sizeResv) {
    if (sizeActual > MSG_BUFFER_MAX - 4) {
      LOG_ERR("reserved buffer size for BO exceeds the maximum\n");
      return Object_ERROR_INVALID;
    }
    ptr = HEAP_ZALLOC(sizeActual + 4);
    if (NULL == ptr) {
      LOG_ERR("Allocate reservedBuffer failed\n");
      return Object_ERROR_KMEM;
    }
    *bufResv = ptr;
  } else {
    ptr = *bufResv;
  }

  sizeRemain = sizeActual + 4;
  FOR_ARGS(i, req->k, BO) {
    remain = (uintptr_t) ptr % LXCOM_MSG_ALIGNMENT;
    if (remain) {
      pad = LXCOM_MSG_ALIGNMENT - remain;
      if (pad > sizeRemain || pad + args[i].b.size > sizeRemain) {
        LOG_ERR("Weird error in reserved buffer\n");
        return Object_ERROR_INVALID;
      }
    }
    args[i].b.ptr = (char *)ptr + pad;
    ptr = (char *)ptr + pad + args[i].b.size;
    sizeRemain -= pad + args[i].b.size;
  }

  return Object_OK;
}

static
int32_t Flatdata_extendAndCopy(FlatData **data, size_t sizeOnWire,
                               size_t sizeExtend, size_t sizeMax)
{
  FlatData *ptr = NULL;

  if (NULL == *data || 0 == sizeExtend) {
    LOG_ERR("invalid input\n");
    return Object_ERROR_INVALID;
  }

  if (sizeExtend > sizeMax) {
    LOG_ERR("Flatdata extending size exceeds the maximum\n");
    return Object_ERROR_INVALID;
  }

  ptr = (FlatData *)HEAP_ZALLOC(sizeExtend);
  if (NULL == ptr) {
    LOG_ERR("Allocate flatdata failed\n");
    return Object_ERROR_KMEM;
  }

  memscpy(ptr, sizeOnWire, *data, sizeOnWire);
  *data = ptr;

  return Object_OK;
}

static
int32_t MarshalOut_caller_buffer(ObjectArg arg, FlatData *data,
                                 size_t sizeOnWire, size_t sizeMax, size_t *sizeAdded)
{
  int32_t pad = PADDED(sizeOnWire) - sizeOnWire;

  *sizeAdded = 0;
  if (sizeOnWire + pad + arg.b.size > sizeMax) {
    LOG_TRACE("Buffer outboundary occurs, try to extend\n");
    return Object_ERROR_MAXARGS;
  }

  if (pad) {
    memscpy((char *)data + sizeOnWire, pad, PadBuf, pad);
    sizeOnWire += pad;
  }
  memscpy((char *)data + sizeOnWire, arg.b.size, arg.b.ptr, arg.b.size);

  *sizeAdded = pad + arg.b.size;

  return Object_OK;
}

static
int32_t MarshalIn_callee_buffer(FlatData *data, ObjectArg *arg, size_t sizeArg,
                                size_t sizeOnWire, size_t sizeMax, size_t *sizeAdded)
{
  size_t pad = PADDED(sizeOnWire) - sizeOnWire;

  *sizeAdded = 0;
  if (sizeOnWire + pad > sizeMax) {
    LOG_TRACE("Buffer outboundary occurs, try to extend\n");
    return Object_ERROR_MAXARGS;
  }

  if (pad) {
    sizeOnWire += pad;
  }
  arg->b.ptr = (uint8_t *)data->buf + sizeOnWire;
  arg->b.size = sizeArg;

  *sizeAdded = pad + sizeArg;

  return Object_OK;
}

static inline
int32_t MarshalOut_callee_buffer(ObjectArg arg, FlatData *data,
                                 size_t sizeOnWire, size_t sizeMax, size_t *sizeAdded)
{
  return MarshalOut_caller_buffer(arg, data, sizeOnWire, sizeMax, sizeAdded);
}

static
int32_t MarshalIn_caller_buffer(FlatData *data, ObjectArg *arg, size_t sizeArg,
                                size_t sizeOnWire, size_t sizeMax, size_t *sizeAdded)
{
  size_t pad = PADDED(sizeOnWire) - sizeOnWire;

  *sizeAdded = 0;
  if (sizeOnWire + pad + sizeArg> sizeMax) {
    LOG_TRACE("Buffer outboundary occurs, try to extend\n");
    return Object_ERROR_MAXARGS;
  }

  if (pad) {
    sizeOnWire += pad;
  }
  memscpy(arg->b.ptr, sizeArg, (uint8_t *)data->buf + sizeOnWire, sizeArg);

  LOG_TRACE("Caller receives bufOut in actual size = %zu, compared to the given\
             size = %zu\n", sizeArg, arg->b.size);

  arg->b.size = sizeArg;
  *sizeAdded = pad + sizeArg;

  return Object_OK;
}

/*@brief: marshalling when caller sends out message
 *        transform data from "ObjectArg[] + ObjectCounts" to "flatdata + fds[]"
 *        flatdata structure:
 *        |1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|......|
 *        |        lxcom_inv_req         |pad|     args.b     |pad|  args.b |pad|..... |
*/
int32_t MarshalOut_caller(MinkSocket *minksock, ObjectTable *objTable, uint32_t invId,
                          int32_t handle, ObjectOp op, ObjectArg *args, ObjectCounts k,
                          FlatData **data, size_t *sizeData,
                          int *fds, size_t *numFds)
{
  int32_t err = Object_ERROR;
  size_t numExtraArgs = COUNT_NUM_EXTRA_ARGS_OUTBOUND(k, args, OI, minksock);
  size_t sizeOnWire = Sizeof_invReq(k, numExtraArgs);
  size_t sizeLimit = MSG_BUFFER_PREALLOC;
  size_t sizeExtend = 0;
  size_t sizeAdded = 0;
  FlatData *buff = *data;
  lxcom_inv_req *invReq = NULL;

  CHECK_MAX_ARGS(k, In, numExtraArgs);
  C_ZERO(*buff);
  for (int i = 0; i < *numFds; i++) {
    fds[i] = -1;
  }

  FOR_ARGS(i, k, BI) {
    err = MarshalOut_caller_buffer(args[i], buff, sizeOnWire, sizeLimit, &sizeAdded);
    sizeOnWire += sizeAdded;
    if (err) {
      sizeExtend = Sizeof_dataInput(args, k, i, sizeOnWire);
      err = Flatdata_extendAndCopy(data, sizeOnWire, sizeExtend, MSG_BUFFER_MAX);
      if (err) {
        LOG_ERR("Flatdata_extendAndCopy failed with error %d\n", err);
        return err;
      }
      buff = *data;
      sizeLimit = sizeExtend;
      i--;
    }
  }
  *sizeData = sizeOnWire;

  FOR_ARGS(i, k, BO) {
    if (!args[i].b.ptr && args[i].b.size != 0) {
      LOG_ERR("invalid BO parameter\n");
      return Object_ERROR_INVALID;
    }
  }

  invReq = (lxcom_inv_req *)&buff->msg;
  FOR_ARGS(i, k, BUFFERS) {
    invReq->a[i].size = args[i].b.size;
  }

  invReq->hdr.type = LXCOM_REQUEST;
  invReq->handle = handle;
  invReq->op = op;
  invReq->k = k;
  invReq->hdr.size = *sizeData;

  //to match the iOI++ at the beginning of every object marshalling iteration
  int32_t iOI = (int32_t)(ObjectCounts_indexOI(k)) - 1;
  size_t fdIndex = 0;
  FOR_ARGS(i, k, OI) {
    iOI++;
    if (iOI < 0 || iOI >= LXCOM_MAX_ARGS) {
      LOG_ERR("invalid array position\n");
      return Object_ERROR_MAXARGS;
    }

    if (isPrimordialOrPrimordialFwd(args[i].o)) {
      LOG_ERR("promordial or primordialFwd cannot be MinkIPC param\n");
      return Object_ERROR;
    }

    CONTINUE_OR_RETURN(MarshalOut_caller_nullObject(args[i].o, invReq->a, iOI),
                       "ObjectNull");
    CONTINUE_OR_RETURN(MarshalOut_caller_msforwarder(minksock, args[i].o, invReq->a,
                       iOI), "msforwarder");
    CONTINUE_OR_RETURN(MarshalOut_caller_localGenericMem(minksock, args[i].o,
                       invReq->a, &iOI, fds, &fdIndex, *numFds), "local genericMem");
    CONTINUE_OR_RETURN(MarshalOut_caller_localMSMem(minksock, objTable, args[i].o,
                       invReq->a, &iOI, fds, &fdIndex, *numFds), "local MSMem");
#ifdef REMOTE_SHAREMM
    CONTINUE_OR_RETURN(MarshalOut_caller_remoteGenericMem(minksock, objTable, invId,
                       args[i].o, invReq->a, &iOI), "remote genericMem");
    CONTINUE_OR_RETURN(MarshalOut_caller_remoteMSMem(minksock, objTable, invId,
                       args[i].o, invReq->a, &iOI), "remote MSMem");
#endif // REMOTE_SHAREMM
    CONTINUE_OR_RETURN(MarshalOut_caller_genericObject(objTable, args[i].o,
                       invReq->a, iOI), "generic obj");

    //generally, object must match one of the marshalling situations above.
    LOG_ERR("never matched marshalling situation of object occurs\n");
    return Object_ERROR;
  }
  *numFds = fdIndex;

  return Object_OK;
}

/*@brief: marshalling when callee receives in message
 *        transform data from "flatdata + fds[]" to "ObjectArg[] + ObjectCounts"
*/
int32_t MarshalIn_callee(MinkSocket *minksock, ObjectTable *objTable,
                         FlatData *data, size_t sizeData,
                         int *fds, size_t numFds,
                         ObjectArg *args, void **bufResv, size_t sizeResv)
{
  int32_t err = Object_ERROR;
  lxcom_inv_req *invReq = (lxcom_inv_req *)&data->msg;
  size_t numExtraArgs = COUNT_NUM_EXTRA_ARGS_INBOUND(invReq->k, invReq->a, OI,
                                                     ObjectCounts_indexOI(invReq->k),
                                                     minksock);
  size_t sizeReq = Sizeof_invReq(invReq->k, numExtraArgs);
  size_t sizeOnWire = sizeReq;
  size_t sizeAdded = 0;

  CHECK_MAX_ARGS(invReq->k, In, numExtraArgs);
  if (sizeReq > invReq->hdr.size) {
    LOG_ERR("Unexpected error on flatdata size\n");
    return Object_ERROR_MAXARGS;
  }

  FOR_ARGS(i, invReq->k, BI) {
    err = MarshalIn_callee_buffer(data, &args[i], invReq->a[i].size, sizeOnWire,
                                  invReq->hdr.size, &sizeAdded);
    if (err) {
      LOG_ERR("MarshalIn_callee_buffer failed with error %d\n", err);
      return err;
    }
    sizeOnWire += sizeAdded;
  }

  if (0 != ObjectCounts_numBO(invReq->k)) {
    err = MarshalIn_reserveBuffer(args, invReq, bufResv, sizeResv);
    if (err) {
      LOG_ERR("MarshalIn_reserveBuffer failed with error %d\n", err);
      return err;
    }
  }

  //to match the iOI++ at the beginning of every object marshalling iteration
  int32_t iOI = (int32_t)(ObjectCounts_indexOI(invReq->k)) - 1;
  size_t fdIndex = 0;
  FOR_ARGS(i, data->msg.req.k, OI) {
    iOI++;
    if (iOI < 0 || iOI >= LXCOM_MAX_ARGS) {
      LOG_ERR("invalid array position\n");
      return Object_ERROR_MAXARGS;
    }

    CONTINUE_OR_RETURN(MarshalIn_callee_nullObject(data->msg.req.a, &args[i].o,
                       iOI), "ObjectNull");
    CONTINUE_OR_RETURN(MarshalIn_callee_msforwarder(minksock, objTable, data->msg.req.a,
                       &args[i].o, &iOI), "msforwarder");
    CONTINUE_OR_RETURN(MarshalIn_callee_localGenericMem(minksock, data->msg.req.a,
                       fds, numFds, &args[i].o, &iOI, &fdIndex), "localGenericMem");
    CONTINUE_OR_RETURN(MarshalIn_callee_localMSMem(minksock, data->msg.req.a, fds,
                       numFds, &args[i].o, &iOI, &fdIndex), "localMSMem");
#ifdef REMOTE_SHAREMM
    CONTINUE_OR_RETURN(MarshalIn_callee_remoteGenericMem(minksock,  data->msg.req.hdr.invoke_id,
                       data->msg.req.a, &args[i].o, &iOI), "remoteGenericMem");
    CONTINUE_OR_RETURN(MarshalIn_callee_remoteMSMem(minksock, data->msg.req.hdr.invoke_id,
                       data->msg.req.a, &args[i].o, &iOI), "remoteMSMem");
#endif // REMOTE_SHAREMM
    CONTINUE_OR_RETURN(MarshalIn_callee_genericObject(minksock, data->msg.req.a,
                       &args[i].o, iOI), "generic obj");

    //generally, object must match one of the marshalling situations above.
    LOG_ERR("never matched marshalling situation of object occurs\n");
    return Object_ERROR;
  }

  return Object_OK;
}

/*@brief: release input/resquest data after processing MarshalIn_callee()
 */
void MarshalIn_calleeRelease(int32_t invokeCounts, ObjectArg *invokeArgs,
                             int *fds, int numFds)
{
  if (invokeArgs != NULL) {
    //Release all input Object references since we're done with them
    FOR_ARGS(i, invokeCounts, OI) {
      Object_ASSIGN_NULL(invokeArgs[i].o);
    }
  }

  if (fds != NULL) {
    for (int i = 0; i < numFds; i++) {
      //close all fds found on input
      if (fds[i] != -1) {
        vm_osal_mem_close(fds[i]);
        fds[i] = -1;
      }
    }
  }
}

/*@brief: marshalling when callee replies back message
 *        transform data from "ObjectArg[] + ObjectCounts" to "flatdata + fds[]"
*/
int32_t MarshalOut_callee(MinkSocket *minksock, ObjectTable *objTable,
                          uint32_t invId, ObjectArg *args, ObjectCounts k,
                          FlatData **data, size_t *sizeData,
                          int *fds, size_t *numFds)
{
  int32_t err = Object_ERROR;
  size_t numExtraArgs = COUNT_NUM_EXTRA_ARGS_OUTBOUND(k, args, OO, minksock);
  size_t sizeOnWire = Sizeof_invSucc(k, numExtraArgs);
  size_t sizeLimit = MSG_BUFFER_PREALLOC;
  size_t sizeExtend = 0;
  size_t sizeAdded = 0;
  FlatData *buff = *data;
  lxcom_inv_succ *invSucc = (lxcom_inv_succ *)&buff->msg;

  CHECK_MAX_ARGS(k, Out, numExtraArgs);
  C_ZERO(*buff);
  for (int i = 0; i < *numFds; i++) {
    fds[i] = -1;
  }

  // To prevent size_t from underflow
  int32_t j = 0;
  FOR_ARGS(i, k, BO) {
    invSucc->a[j].size = args[i].b.size;
    err = MarshalOut_callee_buffer(args[i], buff, sizeOnWire, sizeLimit, &sizeAdded);
    sizeOnWire += sizeAdded;
    if (err) {
      sizeExtend = Sizeof_dataOutput(args, k, i, sizeOnWire);
      err = Flatdata_extendAndCopy(data, sizeOnWire, sizeExtend, MSG_BUFFER_MAX);
      if (err) {
        LOG_ERR("Flatdata_extendAndCopy failed with error %d\n", err);
        return err;
      }
      buff = *data;
      invSucc = (lxcom_inv_succ *)&buff->msg;
      sizeLimit = sizeExtend;
      i--;
      j--;
    }
    j++;

    LOG_TRACE("Callee responds bufOut[%d] in actual size = %zu", i, args[i].b.size);
  }
  *sizeData = sizeOnWire;

  invSucc->hdr.type = LXCOM_SUCCESS;
  invSucc->hdr.invoke_id = invId;
  invSucc->hdr.size = *sizeData;

  //to match the iOO++ at the beginning of every object marshalling iteration
  int32_t iOO = (int32_t)(ObjectCounts_numBO(k)) - 1;
  size_t fdIndex = 0;
  FOR_ARGS(i, k, OO) {
    iOO++;
    if (iOO < 0 || iOO >= LXCOM_MAX_ARGS) {
      LOG_ERR("invalid array position\n");
      return Object_ERROR_MAXARGS;
    }

    if (isPrimordialOrPrimordialFwd(args[i].o)) {
      LOG_ERR("promordial or primordialFwd cannot be MinkIPC param\n");
      return Object_ERROR;
    }

    CONTINUE_OR_RETURN(MarshalOut_callee_nullObject(args[i].o, invSucc->a, iOO),
                       "ObjectNull");
    CONTINUE_OR_RETURN(MarshalOut_callee_msforwarder(minksock, args[i].o, invSucc->a,
                       iOO), "msforwarder");
    CONTINUE_OR_RETURN(MarshalOut_callee_localGenericMem(minksock, objTable, args[i].o,
                       invSucc->a, &iOO, fds, &fdIndex, *numFds), "local genericMem");
    CONTINUE_OR_RETURN(MarshalOut_callee_localMSMem(minksock, objTable, args[i].o,
                       invSucc->a, &iOO, fds, &fdIndex, *numFds), "local MSMem");
#ifdef REMOTE_SHAREMM
    CONTINUE_OR_RETURN(MarshalOut_callee_remoteGenericMem(minksock, objTable, invId,
                       args[i].o, invSucc->a, &iOO), "remote genericMem");
    CONTINUE_OR_RETURN(MarshalOut_callee_remoteMSMem(minksock, objTable, invId,
                       args[i].o, invSucc->a, &iOO), "remote MSMem");
#endif // REMOTE_SHAREMM
    CONTINUE_OR_RETURN(MarshalOut_callee_genericObject(objTable, args[i].o, invSucc->a,
                       iOO), "generic obj");

    //generally, object must match one of the marshalling situations above.
    LOG_ERR("never matched marshalling situation of object occurs\n");
    return Object_ERROR;
  }
  *numFds = fdIndex;

  return Object_OK;
}

/*@brief: release output/response data after processing MarshalOut_callee()
 */
void MarshalOut_calleeRelease(char *resvBuf, void *ptrResv,
                              Object targetObj, int32_t invokeCounts,
                              ObjectArg *invokeArgs, int32_t errInvoke)
{
  if (ptrResv != NULL && ptrResv != resvBuf) {
    HEAP_FREE_PTR(ptrResv);
  }

  if (!Object_isNull(targetObj)) {
    //Release all output Object references since we're done with them
    if (Object_isOK(errInvoke) && invokeArgs != NULL) {
      FOR_ARGS(i, invokeCounts, OO) {
        Object_ASSIGN_NULL(invokeArgs[i].o);
      }
    }
    Object_release(targetObj);
  }
}

/*@brief: marshalling when caller receives in message
 *        transform data from "flatdata + fds[]" to "ObjectArg[] + ObjectCounts"
*/
int32_t MarshalIn_caller(MinkSocket *minksock, ObjectTable *objTable,
                         FlatData *data, size_t sizeData,
                         int *fds, size_t numFds,
                         int32_t k, ObjectArg *args)
{
  int32_t err = Object_OK;
  lxcom_inv_succ *invSucc = (lxcom_inv_succ *)&data->msg;
  size_t numExtraArgs = COUNT_NUM_EXTRA_ARGS_INBOUND(k, invSucc->a, OO,
                                                     ObjectCounts_numBO(k), minksock);
  size_t sizeSucc = Sizeof_invSucc(k, numExtraArgs);
  size_t sizeOnWire = sizeSucc;
  size_t sizeAdded = 0;
  size_t j = 0;

  if (sizeSucc > invSucc->hdr.size) {
    LOG_ERR("Unexpected error on flatdata size\n");
    return Object_ERROR_MAXARGS;
  }
  if (ObjectCounts_total(k) > C_LENGTHOF(invSucc->a)) {
    LOG_ERR("ObjectArgs format error\n");
    return Object_ERROR_MAXARGS;
  }

  FOR_ARGS(i, k, BO) {
    err = MarshalIn_caller_buffer(data, &args[i], invSucc->a[j].size,
                                  sizeOnWire, invSucc->hdr.size, &sizeAdded);
    if (err) {
      LOG_ERR("MarshalIn_caller_buffer failed with error %d\n", err);
      return err;
    }

    sizeOnWire += sizeAdded;
    j++;
  }

  //to match the iOO++ at the beginning of every object marshalling iteration
  int32_t iOO = (int32_t)(ObjectCounts_numBO(k)) - 1;
  size_t fdIndex = 0;
  FOR_ARGS(i, k, OO) {
    iOO++;
    if (iOO < 0 || iOO >= LXCOM_MAX_ARGS) {
      LOG_ERR("invalid array position\n");
      return Object_ERROR_MAXARGS;
    }

    CONTINUE_OR_CLEANUP(MarshalIn_caller_nullObject(invSucc->a, &args[i].o, iOO),
                       "ObjectNull");
    CONTINUE_OR_CLEANUP(MarshalIn_caller_msforwarder(minksock, objTable, invSucc->a, &args[i].o,
                        &iOO), "msforwarder");
    CONTINUE_OR_CLEANUP(MarshalIn_caller_localGenericMem(minksock, invSucc->a, fds,
                        numFds, &args[i].o, &iOO, &fdIndex), "localGenericMem");
    CONTINUE_OR_CLEANUP(MarshalIn_caller_localMSMem(minksock, invSucc->a, fds, numFds,
                        &args[i].o, &iOO, &fdIndex), "localMSMem");
#ifdef REMOTE_SHAREMM
    CONTINUE_OR_CLEANUP(MarshalIn_caller_remoteGenericMem(minksock, invSucc->hdr.invoke_id,
                        invSucc->a, &args[i].o, &iOO), "remoteGenericMem");
    CONTINUE_OR_CLEANUP(MarshalIn_caller_remoteMSMem(minksock, invSucc->hdr.invoke_id,
                        invSucc->a, &args[i].o, &iOO), "remoteMSMem");
#endif // REMOTE_SHAREMM
    CONTINUE_OR_CLEANUP(MarshalIn_caller_genericObject(minksock, invSucc->a, &args[i].o,
                        iOO), "generic obj");

    //generally, object must match one of the marshalling situations above.
    LOG_ERR("never matched marshalling situation of object occurs\n");
    return Object_ERROR;
  }

  return Object_OK;

cleanup:
  FOR_ARGS(i, k, OO) {
    Object_ASSIGN_NULL(args[i].o);
  }

  for (int i = 0; i < numFds; i++) {
    if (fds[i] != -1) {
      vm_osal_mem_close(fds[i]);
    }
  }

  return err;
}

void Marshal_perfEntryTag(MinkSocket *minksock, uint32_t ipcType, FlatData *Msg)
{
  switch (ipcType) {
    case LXCOM_REQUEST:
      LOG_PERF("msock = %p, invId = %u, h = %d, op = %d, recvREQUEST \n",
               minksock, Msg->msg.req.hdr.invoke_id, Msg->msg.req.handle, Msg->msg.req.op);
      break;
    case LXCOM_SUCCESS:
      LOG_PERF("msock = %p, invId = %u, recvSUCCESS \n", minksock, Msg->msg.succ.hdr.invoke_id);
      break;
    case LXCOM_ERROR:
      LOG_PERF("msock = %p, invId = %u, recvERROR \n", minksock, Msg->msg.succ.hdr.invoke_id);
      break;
    case LXCOM_CLOSE:
      LOG_PERF("msock = %p, closeId = %u, recvCLOSE \n", minksock, Msg->msg.close.handle);
      break;
    default:
      LOG_ERR("Error, unsupported IPC type = %u \n", ipcType);
      break;
  }
}
