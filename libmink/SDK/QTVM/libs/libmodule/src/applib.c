// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "CTRegisterModule.h"
#include "IContainerModule_invoke.h"
#include "ITEnv.h"
#include "ITRegisterModule.h"
#include "TUtils.h"
#include "backtrace.h"
#include "heap.h"
#include "minkipc.h"
#include "object.h"

#define MINKD_SOCKET_NAME "LE_SOCKET_minkd"
#ifndef OFFTARGET
#define FALLBACK_SOCKET_LOCATION "/dev/socket/minkd"
#else
#define FALLBACK_SOCKET_LOCATION "minkd_socket0"
#endif

#define priorityCtorDtor 200  // GCC reserves 0 - 100

typedef struct {
    int32_t refs;
    Object credentials;
    uint32_t uid;
} ContainerModule;

// ------------------------------------------------------------------------
// Global variable definitions
// ------------------------------------------------------------------------
static MinkIPC *gOpenerConn;
static sem_t gRegisterSem;
Object gTVMEnv;

// ------------------------------------------------------------------------
// External functions, defined by end-user:
// ------------------------------------------------------------------------
extern void tProcessShutdown(void);
extern int32_t tProcessOpen(uint32_t uid, Object cred, Object *objOut);

// ------------------------------------------------------------------------
// ContainerModule implements IModule:
// ------------------------------------------------------------------------
/**
 * Description: Open the service object hosted by this process
 *
 * In:          self: The IModule instance.
 *              uid: The Universal ID (UID) of the requested service.
 *              cred: The credentials object of the client requesting the
 *                    service.
 * Out:         objOut: The service object.
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t ContainerModule_open(ContainerModule *me, uint32_t id_val, Object credentials_val,
                                    Object *obj_ptr)
{
    return tProcessOpen(id_val, credentials_val, obj_ptr);
}

/* In QTVM, there is no need to call IModule_shutdown. Only the TModule should
 * have a reference to the ContainerModule object and it must call
 * Object_release to cause the process shutdown without incurring warnings from
 * race conditions. */
#define ContainerModule_shutdown(me) Object_OK

/**
 * Description: Signal this process that registration is complete.
 *
 * In:          self: The IModule instance.
 * Out:
 * Return:      Object_OK on success.
 *              Object_ERROR on failure.
 */
static int32_t ContainerModule_enable(ContainerModule *me)
{
    (void)me;
    // Increment (unlock) semaphore. Allow main thread to complete.
    sem_post(&gRegisterSem);
    LOG_MSG("Container Module enabled");
    return Object_OK;
}

static int32_t ContainerModule_retain(ContainerModule *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

/* Release all references, free the Module itself and initiate shutdown sequence
   of the process. */
static int32_t ContainerModule_release(ContainerModule *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        T_TRACE(Object_ASSIGN_NULL(me->credentials));
        T_TRACE(HEAP_FREE_PTR(me));
        T_TRACE(tProcessShutdown());
    }

    return Object_OK;
}

static IContainerModule_DEFINE_INVOKE(ContainerModule_invoke, ContainerModule_, ContainerModule *);

/**
 * Description: Register this process's IModule with minkd
 *              Three steps are needed to do this:
 *              1. Connect to minkd socket and receive the ITEnv object.
 *              2. Open the TRegisterModule service.
 *              3. Register the process's IModule with minkd.
 *
 * In:          void
 * Out:         void
 * Return:      Object_OK on success, fail otherwise.
 */
static int registerWithMinkd(void)
{
    int ret = 0;
    Object registerSvc = Object_NULL;
    Object CModule = Object_NULL;
    const char *env_sock = getenv(MINKD_SOCKET_NAME);

    ContainerModule *me = HEAP_ZALLOC_TYPE(ContainerModule);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    me->refs = 1;
    CModule = (Object){ContainerModule_invoke, me};

    T_CALL(sem_init(&gRegisterSem, 0, 0));

    // 1. Connect to the Mink socket to get ITEnv object
    gOpenerConn = MinkIPC_connect(env_sock, &gTVMEnv);
    T_CHECK(gOpenerConn != NULL && !Object_isNull(gTVMEnv));

    // 2. Open TRegisterModule service to receive service object instance
    T_CALL(ITEnv_open(gTVMEnv, CTRegisterModule_UID, &registerSvc));

    // 3. Register the ContainerModule for this process with minkd
    T_CALL(ITRegisterModule_registerIModule(registerSvc, CModule));

exit:
    Object_ASSIGN_NULL(registerSvc);
    Object_ASSIGN_NULL(CModule);
    if (ret) {
        Object_ASSIGN_NULL(gTVMEnv);
        exit(ret);
    }

    sem_wait(&gRegisterSem);
    sem_destroy(&gRegisterSem);

    return ret;
}

/**
 * Description: Method executed before main()
 *
 * In:          void
 * Out:         void
 * Return:      void
 */
static void __attribute__((constructor(priorityCtorDtor)))
premain(int argc, char *argv[], char *envp[])
{
    T_TRACE(registerWithMinkd());
    T_TRACE(register_backtrace_handlers(BACKTRACE_ALL_SIGNALS));
}
