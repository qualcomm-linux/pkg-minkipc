// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <assert.h>
#include <errno.h>
#include <linux/limits.h>

#include "EmbeddedProcessIDs.h"
#include "PlatformConfig.h"
#include "TUtils.h"
#ifndef OFFTARGET
#include "selinux/selinux.h"
#endif
#include "ServiceCenter.h"

// Load the seccom(?) profiles into memory. Needs to be executed before
// launching processes.
void LoadProfiles(void)
{
    int32_t ret;

    T_GUARD(ServiceCenter_loadServiceProfiles(
        CONFIGURE_DIR, EMBEDDED_DAEMON_TYPE | EMBEDDED_SERVICE_TYPE | DOWNLOADABLE_SERVICE_TYPE));

exit:
    LOG_MSG("Load service configure file %s\n", Object_isOK(ret) ? "successfully" : "failed");
}

static int32_t _setSepolicy(char *fileName)
{
    int32_t ret = Object_OK;
#ifndef OFFTARGET
    char *execcon = NULL;
    T_CALL(setexecfilecon(fileName, "system_u:object_r:unlabeled_t:s0"));
    T_CALL(getexeccon(&execcon));
    T_CHECK(execcon);
    LOG_MSG("execcon = %s", execcon);
    freecon(execcon);
#endif

exit:
    return ret;
}

int32_t LaunchProcess(int32_t fd, cid_t cid, char *const argv[], char *const envp[], pid_t *pidOut,
                      pid_t *pPidOut)
{
    int32_t ret = Object_ERROR;
    int32_t pid;
    void *serviceInfo = NULL;
    uint32_t type;
    uint32_t len = sizeof(uint32_t);
    char fileName[PATH_MAX] = {0};
    uint32_t nameLen = PATH_MAX;

    // Retrieve file based on CID
    for (uint32_t idx = 0; idx < embeddedProcessIDCount; idx++) {
        if (embeddedProcessIDList[idx].cid == cid) {
            T_GUARD(ServiceCenter_findService(embeddedProcessIDList[idx].uid, &serviceInfo));
            break;
        }
    }

    T_CHECK_ERR(serviceInfo != NULL, Object_ERROR_INVALID);

    T_GUARD(ServiceCenter_getServiceAttribute(serviceInfo, TYPE, (uint8_t *)&type, &len));

    pid = fork();
    T_CHECK(pid >= 0);
    if (pid == 0) {
        LOG_MSG("Hello, I am the child. cid = %d", cid);

        switch (type) {
            case EMBEDDED_DAEMON:
                T_GUARD(ServiceCenter_getServiceAttribute(serviceInfo, PATH, fileName, &nameLen));
                T_GUARD(_setSepolicy(fileName));
                LOG_MSG("execve for file = %s", fileName);
                execve(fileName, argv, envp);
                break;
            case EMBEDDED_SERVICE:
                T_GUARD(ServiceCenter_getServiceAttribute(serviceInfo, PATH, fileName, &nameLen));
                T_GUARD(_setSepolicy(fileName));
                LOG_MSG("execve for file = %s", fileName);
                execve(fileName, argv, envp);
                break;
            case DOWNLOADABLE_SERVICE:
                LOG_MSG("fexecve for file = %s", fileName);
                fexecve(fd, argv, envp);
                break;
            default:
                break;
        }

        LOG_MSG("ERRNO = %d", errno);
        assert(0);
    }

    LOG_MSG("I am the parent. The child's PID is %d", pid);

    *pidOut = pid;
    *pPidOut = pid;
    ret = Object_OK;

exit:
    return ret;
}
