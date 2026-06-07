// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "ControlSocket.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "MemSCpy.h"
#include "PlatformConfig.h"
#include "TUtils.h"

#define UID_ROOT 0
#define UID_SYSTEM 1000
#define GID_SYSTEM 1000

typedef struct {
    uid_t uid;
    gid_t gid;
    mode_t mode;
} SocketInfo;

/* clang-format off */
static const SocketInfo gSocketInfo[] = {
    // root socket attribute
    {UID_ROOT, GID_SYSTEM, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP},
    // prelauncher socket attribute
    {UID_ROOT, GID_SYSTEM, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP},
    // mink socket attribute
    {UID_SYSTEM, GID_SYSTEM, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH},
};
/* clang-format on */

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (EVENT_SIZE + NAME_MAX + 1)

const SocketInfo *_getSocketInfo(const char *name)
{
    if (strlen(ROOT_SOCKET_NAME) == strlen(name) &&
        0 == memcmp(name, ROOT_SOCKET_NAME, strlen(ROOT_SOCKET_NAME))) {
        return &gSocketInfo[0];
    } else if (strlen(PRELAUNCHER_SOCKET_NAME) == strlen(name) &&
               0 == memcmp(name, PRELAUNCHER_SOCKET_NAME, strlen(PRELAUNCHER_SOCKET_NAME))) {
        return &gSocketInfo[1];
    } else {
        return &gSocketInfo[2];
    }
}

/**
 * Description: Add the access attribute to the given socket file. After that, the socket file is
 *              enable to access by others.
 *
 * In:          name: target socket file name
 *
 *
 * Return:      Object_OK on success.
 *              All else on failure.
 */
int32_t ControlSocket_enableSocketAttribute(const char *name)
{
    const SocketInfo *socketInfo = NULL;
    int32_t ret = Object_OK;

    socketInfo = _getSocketInfo(name);
    T_GUARD(chmod(name, socketInfo->mode));
#ifndef OFFTARGET
    T_GUARD(chown(name, socketInfo->uid, socketInfo->gid));
#endif
    LOG_MSG("Set %s attribute successfully.", name);

exit:
    return ret;
}

int32_t ControlSocket_waitSocketEnable(int32_t fd, char *targetFileName)
{
    int32_t length;
    struct inotify_event *event;
    int32_t ret = Object_ERROR;
    char buffer[BUF_LEN] = {0};

    while (1) {
        // will block until at least one event occurs (unless interrupted by a
        // signal, in which case the call fails with the error EINTR). See INOTIFY(7).
        length = read(fd, buffer, BUF_LEN);
        if (length == -1) {
            // if error is EINTR, we should re-read again.
            T_CHECK(errno == EINTR);
            continue;
        }

        T_CHECK(length > EVENT_SIZE);
        event = (struct inotify_event *)(buffer);

        // Iterate through list of returned events.
        while (length > EVENT_SIZE) {
            length -= EVENT_SIZE;
            T_CHECK((event->mask & IN_ATTRIB) && (event->len > 0) && (event->len <= length));

            LOG_MSG("The file %s set new attribute.", event->name);
            if (strlen(event->name) == strlen(targetFileName) &&
                0 == memcmp(event->name, targetFileName, strlen(targetFileName))) {
                LOG_MSG("Found the file %s.", event->name);
                return Object_OK;
            }

            length -= event->len;
            event = (struct inotify_event *)((char *)event + EVENT_SIZE + event->len);
        }
    }

exit:
    return ret;
}

int32_t ControlSocket_initSocketDirNotify(int32_t *notifyFd, int32_t *watchFd)
{
    int32_t ret = Object_OK;

    *notifyFd = inotify_init1(IN_CLOEXEC);
    T_CHECK(*notifyFd > 0);

    // Set the socket dir(/dev/socket/) as a new watch. And we only focus on the file attribute
    // setting event under the socket dir.
    *watchFd = inotify_add_watch(*notifyFd, SOCKET_DIR_NAME, IN_ATTRIB);
    T_CHECK(*watchFd > 0);

exit:
    if (Object_isERROR(ret) && (*notifyFd > 0)) {
        (void)close(*notifyFd);
        *notifyFd = -1;
    }

    return ret;
}

void ControlSocket_freeNotify(int32_t notifyFd, int32_t watchFd)
{
    (void)inotify_rm_watch(notifyFd, watchFd);
    (void)close(notifyFd);
}

bool ControlSocket_checkSocketFile(const char *targetFile)
{
    if (access(targetFile, 0) == 0) {
        LOG_MSG("targetFile %s has created", targetFile);
        return true;
    } else {
        return false;
    }
}
