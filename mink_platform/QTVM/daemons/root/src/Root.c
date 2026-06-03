// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "ControlSocket.h"
#include "ITPrivilegedProcessManager.h"
#include "MemSCpy.h"
#include "MinkTypes.h"
#include "PlatformConfig.h"
#include "TEnv.h"
#include "TPrivilegedProcessManager.h"
#include "TUtils.h"
#include "backtrace.h"
#include "cdefs.h"
#include "libcontainer.h"
#include "minkipc.h"
#include "object.h"
#ifdef USE_SW_ENCRYPT
#include "CryptSetup.h"
#endif

typedef struct {
    cid_t cid;
    pid_t pid;
    pid_t pPid;
    const char *daemonName;
} DaemonInfo;

/* clang-format off */
static DaemonInfo gDaemonInfo[] = {
    {DAEMON_PRELAUNCHER, -1, -1, PRELAUNCHER_DAEMON_NAME},
    {DAEMON_MINK, -1, -1, MINK_DAEMON_NAME},
};
/* clang-format on */

static int32_t _startDaemon(DaemonInfo *daemonInfo)
{
    ITPPM_programData programData = {0};
    int32_t ret = Object_ERROR;

    memscpy(programData.fileName, sizeof(programData.fileName), daemonInfo->daemonName,
            strlen(daemonInfo->daemonName) + 1);
    T_CALL(TPPM_launch(0, daemonInfo->cid, &programData, &daemonInfo->pid, &daemonInfo->pPid));

exit:
    if (Object_isERROR(ret)) {
        LOG_MSG("Load %s daemon failed(%d).!", daemonInfo->daemonName, ret);
        // If the core daemon loads failed, Root daemon will exit and be restarted by systemd.
        abort();
    }

    return ret;
}

static void _diedProcessHandler(uint32_t pPid)
{
    int32_t ret;
    for (int32_t i = 0; i < C_LENGTHOF(gDaemonInfo); i++) {
        if (pPid == gDaemonInfo[i].pPid) {
            LOG_MSG("%s daemon has died and it is ready to restart.", gDaemonInfo[i].daemonName);

            gDaemonInfo[i].pPid = 0;
            ret = _startDaemon(&gDaemonInfo[i]);
            LOG_MSG("%s to restart %s.", Object_isOK(ret) ? "Succeed" : "Fail",
                    gDaemonInfo[i].daemonName);
            return;
        }
    }

    T_TRACE(TPPM_processDied(pPid));
}

static void *_signalHandler(void *arg)
{
    sigset_t *signalSet = (sigset_t *)arg;
    int32_t signalNum = 0;
    int32_t ret = Object_ERROR;
    int32_t status = -1;
    pid_t pPid;
    int32_t iter = 0;

    while (1) {
        ret = sigwait(signalSet, &signalNum);
        if (0 != ret) {
            LOG_MSG("sigwait error : %d", ret);
        }

        LOG_MSG("Got signal %s(%d)", strsignal(signalNum), signalNum);

        // Check if any of the child processes have been signaled
        if (SIGCHLD == signalNum) {
            while ((pPid = waitpid(-1, &status, WNOHANG)) > 0) {
                LOG_MSG("status = %d, iteration = %d", status, iter++);
                if (WIFEXITED(status)) {
                    LOG_MSG("%d exited, status = %d", pPid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    LOG_MSG("%d killed by signal %d", pPid, WTERMSIG(status));
                } else if (WIFSTOPPED(status)) {
                    LOG_MSG("%d stopped by signal %d", pPid, WSTOPSIG(status));
                } else if (WIFCONTINUED(status)) {
                    LOG_MSG("%d continued", pPid);
                }

                _diedProcessHandler(pPid);
            }
        }
    }

    return NULL;
}

int32_t main()
{
    Object module = Object_NULL;
    MinkIPC *service = NULL;
    int32_t ret = Object_OK;
    pthread_t signalThread;
    sigset_t signalSet;

    sigemptyset(&signalSet);
    sigaddset(&signalSet, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &signalSet, NULL);

    // create a signal handler thread to receive child's signal
    T_CALL(pthread_create(&signalThread, NULL, _signalHandler, &signalSet));

    T_TRACE(register_backtrace_handlers(BACKTRACE_ALL_SIGNALS));

    // Load minijail profiles into memory
    LoadProfiles();

    module = TEnv_new();
    if (Object_isNull(module)) {
        LOG_MSG("Error in CTPreLauncher_new\n");
        goto exit;
    }

    service = MinkIPC_startServiceModule(ROOT_SOCKET_NAME, module);
    T_CHECK(service != NULL);
    T_CALL(ControlSocket_enableSocketAttribute(ROOT_SOCKET_NAME));

#ifdef USE_SW_ENCRYPT
    T_TRACE(do_cryptsetup());
#endif

    for (int32_t i = 0; i < C_LENGTHOF(gDaemonInfo); i++) {
        T_CALL(_startDaemon(&gDaemonInfo[i]));
    }

    LOG_MSG("Serving TZD interface\n");
    MinkIPC_join(service);

exit:
    if (service) {
        MinkIPC_release(service);
    }

    return ret;
}
