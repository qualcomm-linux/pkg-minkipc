// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "CTRegisterModule_open.h"
#include "ICredentials.h"
#include "ITEnv.h"
#include "ITRegisterModule_invoke.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "qlist.h"

#define MAX_WAIT_TIME_IN_SECONDS 1

static QLIST_DEFINE_INIT(qlistPendingIModules);

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pendingIModules = PTHREAD_COND_INITIALIZER;

typedef struct {
    QNode qn;
    uint32_t pid;
    Object credential;
    Object iModule;
} IModuleNode;

typedef struct CTRegisterModule {
    int32_t refs;
    Object credentials;
} CTRegisterModule;

/**
 * Delete node
 *
 * NOTE: this function is NOT thread-safe. gLock must be held for
 * the duration of this call. */
static void IModuleNode_delete(IModuleNode *me)
{
    // Remove QNode from list
    QNode_dequeueIf(&me->qn);
    // Release object
    Object_ASSIGN_NULL(me->iModule);
    Object_ASSIGN_NULL(me->credential);
    // Free list entry
    HEAP_FREE_PTR(me);
}

static int32_t CTRegisterModule_retain(CTRegisterModule *me)
{
    atomicAdd(&me->refs, 1);

    return Object_OK;
}

static int32_t CTRegisterModule_release(CTRegisterModule *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        Object_ASSIGN_NULL(me->credentials);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static int32_t CTRegisterModule_registerIModule(CTRegisterModule *me, Object object)
{
    uint32_t pid = 0;
    size_t lenOut = 0;
    int32_t ret = Object_OK;
    IModuleNode *iModNode = HEAP_ZALLOC_TYPE(IModuleNode);

    T_CHECK_ERR(iModNode, Object_ERROR_MEM);

    // get PID out of credentials stored in 'me'
    T_GUARD(ICredentials_getValueByName(me->credentials, "lpid", strlen("lpid"), &pid, sizeof(pid),
                                        &lenOut));

    // Add IModule
    LOG_MSG("Registering IModule for pid = %d", pid);
    iModNode->pid = pid;
    Object_ASSIGN(iModNode->iModule, object);
    Object_INIT(iModNode->credential, me->credentials);

    // Critical region begin
    pthread_mutex_lock(&gLock);

    QList_appendNode(&qlistPendingIModules, &iModNode->qn);
    // Wake-up all threads waiting on this CV to search for their IModule
    pthread_cond_broadcast(&pendingIModules);
    pthread_mutex_unlock(&gLock);
    // Critical region end

exit:
    // Clean-up on error
    if (ret) {
        IModuleNode_delete(iModNode);
    }

    return ret;
}

static ITRegisterModule_DEFINE_INVOKE(ITRegisterModule_invoke, CTRegisterModule_,
                                      CTRegisterModule *);

int32_t CTRegisterModule_getIModuleFromPendingList(uint32_t pid, Object *imodule)
{
    int ret = Object_OK;
    struct timespec maxWait = {0, 0};  // {sec, nsec}

    // Get reference time, i.e. this moment in time
    clock_gettime(CLOCK_REALTIME, &maxWait);
    // Set absolute time in which operation must be completed
    maxWait.tv_sec += MAX_WAIT_TIME_IN_SECONDS;

    // Critical region begin
    pthread_mutex_lock(&gLock);

    do {
        // locate the module based on the pid
        QNode *pqn, *pqnNext;
        QLIST_NEXTSAFE_FOR_ALL(&qlistPendingIModules, pqn, pqnNext)
        {
            IModuleNode *pmod = c_containerof(pqn, IModuleNode, qn);
            LOG_MSG("(%d) Examining IModuleNode with pid = %d", pid, pmod->pid);
            if (pmod->pid == pid) {
                // Transfer ownership to outbound object
                Object_INIT(*imodule, pmod->iModule);
                // Delete node
                IModuleNode_delete(pmod);
                goto exit;
            } else {
                // Due to the locks in ITProcessLoader_loadFromBuffer, launching a process is a
                // serialized operation. Any nodes in the list that aren't the one we're looking for
                // are stale and should be removed.
                LOG_MSG("Stale node found. Removing IModuleNode with pid = %d", pmod->pid);
                IModuleNode_delete(pmod);
            }
        }

        // Put thread to sleep if it didn't find its desired IModule
        T_GUARD(pthread_cond_timedwait(&pendingIModules, &gLock, &maxWait));
    } while (true);

exit:
    pthread_mutex_unlock(&gLock);
    // Critical region end

    if (ETIMEDOUT == ret) {
        ret = ITRegisterModule_ERROR_TIMEDOUT;
    }

    return ret;
}

//----------------------------------------------------------------
// Exported functions
//----------------------------------------------------------------

int32_t CTRegisterModule_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    int32_t ret = Object_OK;
    CTRegisterModule *me = NULL;

    T_CHECK_ERR(!Object_isNull(credentials), Object_ERROR_BADOBJ);

    me = HEAP_ZALLOC_TYPE(CTRegisterModule);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    me->refs = 1;
    // Expect credentials object to support LinkCredentials properties
    Object_INIT(me->credentials, credentials);

    *objOut = (Object){ITRegisterModule_invoke, me};

exit:
    return ret;
}
