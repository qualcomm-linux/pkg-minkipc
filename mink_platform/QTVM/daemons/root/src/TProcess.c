// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "Elf.h"
#include "ElfFile.h"
#include "IModule.h"
#include "ITProcess_invoke.h"
#include "MemSCpy.h"
#include "TPrivilegedProcessManager_priv.h"
#include "TProcess.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"

struct TProcess {
    int32_t refs;
    // Parent PID (minijail-init)
    uint32_t pPid;
    // PID (grandchild of root daemon)
    uint32_t pid;
    DistId did;
    Object tMod;
    bool isZombie;
};

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;

static inline void _delete(TProcess *me)
{
    if (!me) {
        return;
    }
    Object_ASSIGN_NULL(me->tMod);
    T_TRACE(kill(me->pid, SIGKILL));
    HEAP_FREE_PTR(me);
}

static int32_t TProcess_release(TProcess *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        LOG_MSG("Delete me for PID = %d", me->pid);
        _delete(me);
    }

    return Object_OK;
}

static int32_t TProcess_retain(TProcess *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

/**
 * Force a process to shutdown, regardless of lifetime management. By the end,
 * the processes' IModule will have been released by its TModule and the
 * TProcess object reference will have been released by the PPM.
 *
 * In:     memObj: Memory Object of the buffer which contains the ELF
 * Out:    tProcCtrlObj: TProcessController of the new process
 *
 * Return: Object_OK if launched successfully
 *         ITProcessLoader_ERROR_PROC_ALREADY_LOADED if process already running
 *         all else if failure
 */
static int32_t TProcess_forceClose(TProcess *me)
{
    pthread_mutex_lock(&gLock);

    // isZombie is true means it's second time to come here
    // process is already killed at first time, so do nothing here
    if (me->isZombie) {
        goto exit;
    }

    if (!Object_isNull(me->tMod)) {
        T_TRACE(IModule_shutdown(me->tMod));
    }

    // make it a zombie object
    me->isZombie = true;

    // remove TProcess from Process Manager list
    T_TRACE(PPM_remove(me));

exit:
    pthread_mutex_unlock(&gLock);
    return Object_OK;
}

static int32_t TProcess_setTModule(TProcess *me, Object tMod)
{
    pthread_mutex_lock(&gLock);
    Object_ASSIGN(me->tMod, tMod);
    pthread_mutex_unlock(&gLock);

    return Object_OK;
}

static int32_t TProcess_getPID(TProcess *me, uint32_t *pid_ptr)
{
    *pid_ptr = me->pid;
    return Object_OK;
}

static int32_t TProcess_getParentPID(TProcess *me, uint32_t *pid_ptr)
{
    *pid_ptr = me->pPid;
    return Object_OK;
}

static int32_t TProcess_getDistId(TProcess *me, ITProcess_DistID *did)
{
    // Cast and copy contents to output buffer
    *did = *(ITProcess_DistID *)&(me->did);
    return Object_OK;
}

static ITProcess_DEFINE_INVOKE(CTProcess_invoke, TProcess_, TProcess *);

int32_t TProcess_new(pid_t pid, pid_t pPid, const DistId *did, Object *objOut)
{
    int32_t ret = Object_OK;
    TProcess *me = HEAP_ZALLOC_TYPE(TProcess);

    T_CHECK_ERR(me, Object_ERROR_MEM);
    T_CHECK(did);

    me->refs = 1;
    me->pid = pid;
    me->pPid = pPid;
    me->did = *did;

    *objOut = (Object){CTProcess_invoke, me};

exit:
    if (ret) {
        _delete(me);
    }

    return ret;
}
