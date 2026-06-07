// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>

extern "C" {
#include "CSampleService_open.h"
}

// ------------------------------------------------------------------------
// Global variable definitions
// ------------------------------------------------------------------------
static sem_t gShutdownLock;

// ------------------------------------------------------------------------
// Methods
// ------------------------------------------------------------------------
/**
 * Description: The main function.
 *
 * In:          argc: The number of arguments.
 *              argv: The argument values.
 * Out:         void
 * Return:      0 on success.
 */
int main(int argc, char *argv[])
{
    int ret = 0;

    if (sem_init(&gShutdownLock, 0, 0) != 0) {
        printf("Failed to initialize semaphore");
        return -1;
    }

    printf("%s() ", __func__);
    for (int i = 1; i < argc; i++) {
        printf(" %s ", argv[i]);
    }
    printf("\n");

    // Initialize structures or connections before service becomes available to
    // other processes.

    // Decrement (lock) the semaphore. Put to sleep indefinitely.
    if (sem_wait(&gShutdownLock) != 0) {
        printf("Failed to wait on semaphore\n");
        return -1;
    }

    return ret;
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Description: Open service by providing the services Unique ID as well as the
 *              ICredentials object of the caller, to uniquely identify it.
 *
 * In:          uid:    The unique ID of the requested service.
 *              cred:   The ICredentials object of the caller.
 * Out:         objOut: The service object.
 * Return:      Object_OK on success.
 */
int32_t tProcessOpen(uint32_t uid, Object cred, Object *objOut)
{
    return CSampleService_open(uid, cred, objOut);
}

/**
 * Description: Release any remaining objects before process is killed.
 *
 * In:          void
 * Out:         void
 * Return:      void
 */
void tProcessShutdown(void)
{
    // Increment (unlock) semaphore. Allow main thread to complete.
    sem_post(&gShutdownLock);
    printf("Posted on semaphore. Beginning process exit.\n");
}

#ifdef __cplusplus
}
#endif
