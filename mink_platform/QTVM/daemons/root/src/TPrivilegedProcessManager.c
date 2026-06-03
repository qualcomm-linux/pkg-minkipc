// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "CTPrivilegedProcessManager.h"
#include "CTRegisterModule_priv.h"
#include "DaemonAttribute.h"
#include "Elf.h"
#include "ElfFile.h"
#include "ITAutoStartManager.h"
#include "ITPrivilegedProcessManager.h"
#include "ITPrivilegedProcessManager_invoke.h"
#include "MSMem.h"
#include "MemSCpy.h"
#include "PlatformConfig.h"
#include "TProcess.h"
#include "TProcessController.h"
#include "TUtils.h"
#include "cdefs.h"
#include "fdwrapper.h"
#include "heap.h"
#include "libcontainer.h"
#include "qlist.h"
#include "vmmem_wrapper.h"

#define EXTRA_HEAP_MEM (5 * 1024 * 1024)  // 5MB
#define MAX_APP_NAME_LENGTH 100
#define SOCKET_ENV MINKD_SOCKET_ENV_VAR "=" MINKD_SOCKET_NAME

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;
// Use to notify Mink which service has died.
static Object gNotify = Object_NULL;

static struct {
    int32_t procCnt;
    QList procs;
    pthread_mutex_t listLock;
} gProcessManager = {
    0,
    {{&gProcessManager.procs.n, &gProcessManager.procs.n}},
    PTHREAD_MUTEX_INITIALIZER,
};

typedef struct TProcessNode_s {
    uint32_t cid;
    Object tProcObj;
    QNode qn;
} TProcessNode;

typedef struct {
    int32_t refs;
    Object credentials;
} TPPM;

/**
 * Delete node
 *
 * NOTE: this function is NOT thread-safe. ProcessManager.listLock must be held
 * for the duration of this call. */
static void TProcessNode_delete(TProcessNode *me)
{
    // Remove QNode from list
    QNode_dequeueIf(&me->qn);
    // Release TProcess Object
    Object_ASSIGN_NULL(me->tProcObj);
    // Free node struct
    HEAP_FREE_PTR(me);
}

/////////////////////////////////////////////
//     ProcessManager definition         ////
/////////////////////////////////////////////
static int32_t _add(Object tProcObj, uint32_t cid)
{
    int32_t ret = Object_OK;
    TProcessNode *me = NULL;

    me = HEAP_ZALLOC_TYPE(TProcessNode);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    // ProcessManager retains TProcess Object
    Object_ASSIGN(me->tProcObj, tProcObj);
    me->cid = cid;

    pthread_mutex_lock(&gProcessManager.listLock);
    QList_appendNode(&gProcessManager.procs, &me->qn);
    gProcessManager.procCnt++;

    LOG_MSG("Added TProcess to list");

exit:
    // Clean-up on error
    if (ret) {
        TProcessNode_delete(me);
    }

    pthread_mutex_unlock(&gProcessManager.listLock);

    return ret;
}

/*
 *  Description: Find a specific Process by distinguished id.
 *
 *  In:          did: distinguished id
 *               tprocObjOut: reference to TProcess object
 *  Return:      Object_OK if found or not found.
 *               TODO : should an error be returned if process is not found?
 *               TODO : how should we handle errors during DID lookup?
 */
static int32_t _findProcessByDistId(const ITProcess_DistID *did, Object *tprocObjOut)
{
    // It's OK if the process is not found in the list.
    int32_t ret = Object_OK;
    QNode *pqn;
    ITProcess_DistID lDid = {0};
    uint32_t pidOut = 0;

    pthread_mutex_lock(&gProcessManager.listLock);

    QLIST_FOR_ALL(&gProcessManager.procs, pqn)
    {
        TProcessNode *me = c_containerof(pqn, TProcessNode, qn);

        ITProcess_getPID(me->tProcObj, &pidOut);
        LOG_MSG("Inspecting TProcess : PID = %d", pidOut);
        T_GUARD(ITProcess_getDistId(me->tProcObj, &lDid));

        if (0 == memcmp(did, &lDid, sizeof(lDid))) {
            LOG_MSG("DistID match found!");
            *tprocObjOut = me->tProcObj;
            ret = Object_OK;
            break;
        }
    }

exit:
    pthread_mutex_unlock(&gProcessManager.listLock);
    return ret;
}

static int32_t _copyToMemFd(const ITPPM_programData *programData, int32_t fd, int32_t size,
                            cid_t cid, int32_t *memFd)
{
    int32_t ret = Object_ERROR;
    void *ptr = NULL;
    char mbinName[MAX_APP_NAME_LENGTH] = {0};

    // Map dmabuf fd to this process
    ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    T_CHECK(!(ptr == MAP_FAILED));

    // Create memFd
    snprintf(mbinName, MAX_APP_NAME_LENGTH, "TProcess_%u", cid);
    *memFd = memfd_create(mbinName, MFD_CLOEXEC | MFD_ALLOW_SEALING);
    T_CHECK(*memFd > 0);
    LOG_MSG("memFd = %d", *memFd);

    // Copy file contents from mapped region to memFd
    int n = write(*memFd, ptr + programData->offset, programData->fileSize);
    LOG_MSG("n = %d, ptr = %p, size = %lu", n, ptr + programData->offset, programData->fileSize);
    T_CHECK(programData->fileSize == n);

    // Make buffer read-only to protect against tampering inside tmpfs. Requires
    // additional selinux policy to enforce across processes.
    T_CHECK(!fcntl(*memFd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL));

    ret = Object_OK;

exit:
    if (ptr != NULL) {
        munmap(ptr, size);
    }

    if (ret) {
        *memFd = -1;
    }

    return ret;
}

int32_t TPPM_launch(int32_t fd, cid_t cid, const ITPPM_programData *programData, pid_t *pidOut,
                    pid_t *pPidOut)
{
    int32_t ret = Object_ERROR;
    int32_t excFd = fd;
    char *myargs[] = {(char *)programData->fileName, NULL};
    char *myenv[] = {SOCKET_ENV, NULL};
    int32_t memFd = 0;
    struct stat st;
    pid_t *pid[] = {pidOut, pPidOut};
    int32_t oomScoreAdj = CORE_DAEMON_OOM_SCORE_ADJ;

    T_CALL(fstat(fd, &st));

    // For TPs generated by qtvm sdk
    if (cid > DL_BASE && cid < CID_ENUM_MAX) {
        oomScoreAdj = SERVICE_DAEMON_OOM_SCORE_ADJ;
        T_CALL(_copyToMemFd(programData, fd, st.st_size, cid, &memFd));
        excFd = memFd;

        // To avoid the DMA buf FD from HLOS being copied to later subprocess.
        close(fd);
    }

    // TODO: error conversion?
    T_CALL(LaunchProcess(excFd, cid, myargs, myenv, pidOut, pPidOut));
    LOG_MSG("The child PID = %d; grandchild PID = %d", *pPidOut, *pidOut);

    // Set the core service and TA service's OOM attribute value.
    // TODO : It will move to libcontainer when libcontainer support the feature.
    for (int32_t i = 0; i < C_LENGTHOF(pid); ++i) {
        DaemonAttribute_setOOMAttribute(*(pid[i]), oomScoreAdj);
    }

exit:
    if (memFd > 0) {
        close(memFd);
    }

    return ret;
}

/**
 * Description: Call TProcess_forceClose on an object in the event the process
 *              has already died.
 *
 * In:          pPid: process ID of now-dead parent process
 * Return:      Object_OK if successful.
 *              Object_ERROR_INVALID if PID not found in list.
 */
int32_t TPPM_processDied(uint32_t pPid)
{
    int32_t ret = Object_OK;
    TProcessNode *me = NULL;
    Object procObj = Object_NULL;
    QNode *pqn;
    uint32_t pidOut = 0;
    uint32_t cid = 0;

    pthread_mutex_lock(&gLock);
    // Find TProcess based on parent PID and release, if found.
    pthread_mutex_lock(&gProcessManager.listLock);
    QLIST_FOR_ALL(&gProcessManager.procs, pqn)
    {
        me = c_containerof(pqn, TProcessNode, qn);
        T_GUARD(ITProcess_getParentPID(me->tProcObj, &pidOut));
        if (pidOut == pPid) {
            Object_ASSIGN(procObj, me->tProcObj);
            cid = me->cid;
            break;
        }
    }

    // TODO log error if PID not found in list of objects

exit:
    pthread_mutex_unlock(&gProcessManager.listLock);

    // Clean up TProcess and TModule in TVM
    if (!Object_isNull(procObj)) {
        ITProcess_forceClose(procObj);
        Object_ASSIGN_NULL(procObj);

        // Notify Mink which service has died through its cid.
        if (!Object_isNull(gNotify)) {
            ITAutoStartManager_notify(gNotify, cid);
        }
    }

    pthread_mutex_unlock(&gLock);
    return ret;
}

/**
 *  Description: Remove TProcessNode from list.
 *
 *  In:          tProc: pointer of TProcess structure
 *  Return:      void.
 */
void PPM_remove(TProcess *tProc)
{
    QNode *pqn;
    pthread_mutex_lock(&gProcessManager.listLock);
    QLIST_FOR_ALL(&gProcessManager.procs, pqn)
    {
        TProcessNode *me = c_containerof(pqn, TProcessNode, qn);
        if (me->tProcObj.context == tProc) {
            // Delete node and release reference to TProcess
            TProcessNode_delete(me);
            gProcessManager.procCnt--;
            break;
        }
    }

    pthread_mutex_unlock(&gProcessManager.listLock);
}

/////////////////////////////////////////////
//           PPM definition              ////
/////////////////////////////////////////////
static int32_t CTPPM_launch(TPPM *me, Object memObj, uint32_t cid, const ITProcess_DistID *did,
                            const ITPPM_programData *programData, uint32_t *pidPtr,
                            Object *tProcCtrlObj, Object *tProcObj)
{
    int32_t ret = Object_ERROR;
    Object tProc = Object_NULL;
    Object tProcCtrl = Object_NULL;
    FdWrapper *fdw = NULL;
    int32_t fd = -1;
    pid_t pid = 0;
    pid_t pPid = 0;

    pthread_mutex_lock(&gLock);

    // check if this process is launched
    T_CALL(_findProcessByDistId(did, &tProc));
    T_CHECK_ERR(Object_isNull(tProc), ITPPM_ERROR_PROC_ALREADY_LOADED);
    LOG_MSG("process not found");
    T_CHECK(programData);

    // check auth_info, cid, get policy

    // extract process fd from memory object
    if (!isMSMem(memObj, &fd)) {
        T_CALL_CHECK_ERR(fdw, FdWrapperFromObject(memObj), fdw != NULL, ITPPM_ERROR_INVALID_BUFFER);
        fd = (int32_t)fdw->descriptor;
    }

    LOG_MSG("fd = %d", fd);

    // Prepare for launch and then launch
    T_CALL_ERR(TPPM_launch(fd, cid, programData, &pid, &pPid), ITPPM_ERROR_PROC_NOT_LOADED);

    // create new TProcess object
    T_CALL(TProcess_new(pid, pPid, (DistId *)did, &tProc));

    // add TProcess object to Process Manager
    T_CALL(_add(tProc, cid));

    // create new TProcessController object
    T_CALL(TProcController_new(tProc, programData->neverUnload, &tProcCtrl));

    *tProcObj = tProc;
    *tProcCtrlObj = tProcCtrl;

exit:
    if (ret == Object_OK || ret == ITPPM_ERROR_PROC_ALREADY_LOADED) {
        // Populate PID regardless of whether the process previously existed.
        *pidPtr = (uint32_t)pid;
        LOG_MSG("CTPPM_launch Success");
    } else {
        Object_ASSIGN_NULL(tProc);
        Object_ASSIGN_NULL(tProcCtrl);
    }

    pthread_mutex_unlock(&gLock);

    return ret;
}

static int32_t CTPPM_registerNotify(TPPM *me, Object notify)
{
    // It will be called when Mink or Prelauncher Restart. So, we don't need to set NULL no matter
    // Mink dies or Prelauncher dies.
    LOG_MSG("Update Mink Notify Object!");
    pthread_mutex_lock(&gLock);
    Object_ASSIGN(gNotify, notify);
    pthread_mutex_unlock(&gLock);
    return Object_OK;
}

static int32_t CTPPM_shutdown(TPPM *me, uint32_t restart, uint32_t force)
{
    QNode *pqn, *pqnn;
    int32_t ret = Object_OK;

    LOG_MSG("trying to shutdown..");

    // Prevent shutdown/reboot VM and launching service from occuring at the same time.
    pthread_mutex_lock(&gLock);
    pthread_mutex_lock(&gProcessManager.listLock);

    if (!force) {
        QLIST_FOR_ALL(&gProcessManager.procs, pqn)
        {
            TProcessNode *me = c_containerof(pqn, TProcessNode, qn);
            // If a single D/L TP is running, don't shut down
            if ((DL_BASE <= me->cid) && (me->cid < EMBEDDED_BASE)) {
                ret = Object_ERROR_BUSY;
                goto exit;
            }
        }
    }

    QLIST_NEXTSAFE_FOR_ALL(&gProcessManager.procs, pqn, pqnn)
    {
        TProcessNode *nd = c_containerof(pqn, TProcessNode, qn);
        // Delete node and release reference to TProcess
        TProcessNode_delete(nd);
        gProcessManager.procCnt--;
    }

    if (restart) {
        LOG_MSG("Received reboot VM command. Begin reboot...\n");
#ifndef OFFTARGET
        ret = system("/bin/systemctl reboot");
#endif
    } else {
        LOG_MSG("Received shutdown VM command. Begin poweroff...\n");
#ifndef OFFTARGET
        ret = system("/bin/systemctl poweroff");
#endif
    }

exit:
    pthread_mutex_unlock(&gProcessManager.listLock);
    pthread_mutex_unlock(&gLock);

    return ret;
}

static int32_t CTPPM_retain(TPPM *me)
{
    atomicAdd(&me->refs, 1);

    return Object_OK;
}

static int32_t CTPPM_release(TPPM *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        Object_RELEASE_IF(me->credentials);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static ITPPM_DEFINE_INVOKE(CTPPM_invoke, CTPPM_, TPPM *);

int32_t CTPPM_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    int32_t ret = Object_OK;
    TPPM *me = HEAP_ZALLOC_TYPE(TPPM);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    me->refs = 1;
    Object_INIT(me->credentials, credentials);

    *objOut = (Object){CTPPM_invoke, me};

exit:
    return ret;
}
