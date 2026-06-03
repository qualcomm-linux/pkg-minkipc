// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "TModule.h"
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include "IDSet.h"
#include "IModule_invoke.h"
#include "MinkHub.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "pthread.h"
#include "qlist.h"

typedef struct {
    int32_t refs;
    uint32_t key;
    bool isLocal;
    pthread_rwlock_t sessionLock;
    MinkHub *hub;
    MinkHubSession *session;
    IDSet privileges;
    QNode node;
    Object procCred;
} TModule;

static pthread_rwlock_t gListLock = PTHREAD_RWLOCK_INITIALIZER;
static QLIST_DEFINE_INIT(gTModList);

static int32_t _createTModule(MinkHub *hub, uint32_t key, bool isLocal, TModule **tmod)
{
    int32_t ret = Object_OK;
    TModule *temp = NULL;

    temp = HEAP_ZALLOC_TYPE(TModule);
    T_CHECK_ERR(temp != NULL, Object_ERROR_MEM);

    temp->refs = 1;
    temp->key = key;
    temp->hub = hub;
    temp->isLocal = isLocal;
    pthread_rwlock_init(&temp->sessionLock, 0);
    IDSet_zconstruct(&temp->privileges);
    QNode_construct(&temp->node);

    *tmod = temp;

exit:
    return ret;
}

static int32_t TModule_open(TModule *me, uint32_t uid, Object linkCred, Object *objOut)
{
    return IModule_ERROR_NOT_FOUND;
}

static int32_t TModule_shutdown(TModule *me)
{
    pthread_rwlock_wrlock(&me->sessionLock);
    if (me->session != NULL) {
        int32_t ret = MinkHub_destroySession(me->session);
        LOG_MSG("Unregister %u session.Ret = %d\n", me->key, ret);
        me->session = NULL;
    }

    pthread_rwlock_unlock(&me->sessionLock);
    return Object_OK;
}

static int32_t TModule_release(TModule *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        pthread_rwlock_wrlock(&gListLock);
        QNode_dequeueIf(&me->node);
        pthread_rwlock_unlock(&gListLock);
        TModule_shutdown(me);
        pthread_rwlock_destroy(&me->sessionLock);
        Object_ASSIGN_NULL(me->procCred);
        IDSet_destruct(&me->privileges);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static int32_t TModule_retain(TModule *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static IModule_DEFINE_INVOKE(TModule_invoke, TModule_, TModule *);

int32_t TModule_localOpen(Object tMod, uint32_t uid, Object linkCred, Object *objOut)
{
    int32_t ret = Object_OK;
    TModule *me = (TModule *)tMod.context;
    Object cred = me->isLocal ? me->procCred : linkCred;

    ret = MinkHub_localOpen(me->hub, me->isLocal, cred, uid, objOut);
    LOG_MSG("%s to open local service(%x). ret = %d\n", Object_isOK(ret) ? "Succeed" : "Fail", uid,
            ret);

    // Translate from MinkHub error to IModule invoke error
    if (MINKHUB_ERROR_SERVICE_NOT_FOUND == ret) {
        ret = IModule_ERROR_NOT_FOUND;
    }

    return ret;
}

int32_t TModule_remoteOpen(Object tMod, uint32_t uid, Object *objOut)
{
    int32_t ret = Object_OK;
    TModule *me = (TModule *)tMod.context;

    do {
        ret = pthread_rwlock_tryrdlock(&me->sessionLock);
    } while (ret == EAGAIN);

    // When the writer holds the lock, it means that it is releasing the session.
    if (ret == EBUSY) {
        return IModule_ERROR_NOT_FOUND;
    }

    ret = MinkHub_remoteOpen(me->hub, me->session, me->procCred, uid, objOut);
    LOG_MSG("%s to open remote service(%x).ret = %d\n", Object_isOK(ret) ? "Succeed" : "Fail", uid,
            ret);

    pthread_rwlock_unlock(&me->sessionLock);
    return ret;
}

bool TModule_checkPrivilege(Object tMod, uint32_t uid)
{
    TModule *me = (TModule *)tMod.context;
    return IDSet_test(&me->privileges, uid);
}

int32_t TModule_findTModule(uint32_t key, Object *tMod)
{
    int32_t ret = Object_OK;
    QNode *node = NULL;
    Object tmp = Object_NULL;

    pthread_rwlock_rdlock(&gListLock);
    QLIST_FOR_ALL(&gTModList, node)
    {
        TModule *tmod = c_containerof(node, TModule, node);
        if (tmod->key == key) {
            tmp = (Object){TModule_invoke, tmod};
            Object_INIT(*tMod, tmp);
            ret = Object_OK;
            goto exit;
        }
    }

    *tMod = Object_NULL;
    ret = Object_ERROR_INVALID;

exit:
    pthread_rwlock_unlock(&gListLock);
    return ret;
}

int32_t TModule_createRemoteTModule(MinkHub *hub, Object *objOut)
{
    int32_t ret = Object_OK;
    TModule *tmod = NULL;

    T_GUARD(_createTModule(hub, 0, false, &tmod));

    *objOut = (Object){TModule_invoke, tmod};

exit:
    return ret;
}

int32_t TModule_registerTModule(MinkHub *hub, uint32_t key, const uint32_t *serviceIds,
                                uint32_t serviceIdsLen, const uint32_t *privilegeIds,
                                uint32_t privilegeIdsLen, Object procCred, Object iMod,
                                Object *objOut)
{
    int32_t ret = Object_OK;
    TModule *tmod = NULL;

    // Check whether the service has been registered.
    ret = TModule_findTModule(key, objOut);
    T_CHECK_ERR(Object_isERROR(ret), ret);

    T_GUARD(_createTModule(hub, key, true, &tmod));

    for (uint32_t index = 0; index < privilegeIdsLen; ++index) {
        T_GUARD(IDSet_set(&tmod->privileges, privilegeIds[index]));
    }

    Object_INIT(tmod->procCred, procCred);

    T_GUARD(MinkHub_createSession(hub, &tmod->session));
    T_GUARD(MinkHub_registerServices(tmod->session, serviceIds, serviceIdsLen, iMod));

    pthread_rwlock_wrlock(&gListLock);
    QList_appendNode(&gTModList, &tmod->node);
    pthread_rwlock_unlock(&gListLock);

    *objOut = (Object){TModule_invoke, tmod};

exit:
    if (Object_isERROR(ret) && tmod != NULL) {
        TModule_release(tmod);
    }
    return ret;
}