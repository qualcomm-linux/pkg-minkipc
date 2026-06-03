// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "heap.h"
#include "ITTouchControlService_invoke.h"
#include "object.h"
#include "TUtils.h"

typedef struct {
    int32_t refs;
    int32_t controlFd;
} TTouchControlService;

static int32_t CTTouchControlService_releaseTouch(TTouchControlService *me)
{
    int32_t ret = Object_OK;
    ssize_t writtenBytes = 0;
    ssize_t readBytes = 0;
    int32_t retry = 5;
    char c = '\0';

    T_CHECK(me != NULL)
    if (me->controlFd < 0) {
        LOG_MSG("Touch already released from QTVM");
        goto exit;
    }

    /* Write into controlfd, which notifies the HLOS that the touch ownership
     * is released */
    writtenBytes = pwrite(me->controlFd, "0", 1, 0);
    T_CHECK_ERR(writtenBytes > 0, ITTouchControlService_ERROR_WRITE_CONTROLFD);

    /* Read back to confirm touch ownership is released back to HLOS */
    do {
        ret = Object_OK;
        readBytes = pread(me->controlFd, &c, 1, 0);
        T_CHECK_ERR(readBytes > 0, ITTouchControlService_ERROR_WRITE_CONTROLFD);
        if (c - '0' != 0) {
            LOG_MSG("Trusted touch is not yet released, retry: %d", retry);
            ret = ITTouchControlService_ERROR_READ_CONTROLFD;
        }
        retry--;
        usleep(50);
    } while(retry && (ret != Object_OK));

    if (ret == Object_OK) {
        LOG_MSG("Touch device is released from TVM now");
    }
    close(me->controlFd);
    me->controlFd = -1;

exit:
    return ret;
}

static int32_t CTTouchControlService_acquireTouch(TTouchControlService *me, const void *fileName, size_t fileNameLength)
{
    int32_t ret = Object_OK;
    char *controlFile = (char*) fileName;
    ssize_t writtenBytes = 0;
    ssize_t readBytes = 0;
    int32_t retry = 5;
    char c = '\0';

    T_CHECK_ERR(controlFile != NULL && controlFile[fileNameLength-1] == '\0', ITTouchControlService_ERROR_INVALID_CONTROLFD);
    T_CHECK(me != NULL);
    if (me->controlFd >= 0) {
        LOG_MSG("Touch already assigned to QTVM");
        goto exit;
    }
    T_CALL_CHECK_ERR(me->controlFd, open(controlFile, O_RDWR), me->controlFd >= 0,
                     ITTouchControlService_ERROR_OPEN_CONTROLFD,
                     "ControlFD open failed with error: %s", strerror(errno));

    do {
        ret = Object_OK;
        /* Write '1' to trusted_touch_enable node to accept touch device in VM */
        T_CALL_CHECK_ERR(writtenBytes, pwrite(me->controlFd, "1", 1, 0), writtenBytes > 0,
                         ITTouchControlService_ERROR_WRITE_CONTROLFD,
                         "Write to ControlFD failed with error: %s", strerror(errno));

        /* Read the controlfd and verify that the touchdata fd can now be accessed */
        readBytes = pread(me->controlFd, &c, 1, 0);
        LOG_MSG("readBytes : %zd, c : %c", readBytes, c);
        T_CHECK_ERR(readBytes > 0, ITTouchControlService_ERROR_WRITE_CONTROLFD);
        if (c - '0' != 1) {
            LOG_MSG("Touch device could not be assigned to VM, retry: %d", retry);
            ret = ITTouchControlService_ERROR_WRITE_CONTROLFD;
        }
        retry--;
        usleep(50);
    } while(retry && (ret != Object_OK));

    if (ret == Object_OK) {
        LOG_MSG("Touch device is assigned to VM now");
    } else {
        LOG_MSG("Touch device failure - attempt release !! ");
        /* Write into controlfd, which notifies the HLOS that the touch ownership
         * is released */
        writtenBytes = pwrite(me->controlFd, "0", 1, 0);
        T_CHECK_ERR(writtenBytes > 0, ITTouchControlService_ERROR_WRITE_CONTROLFD);
    }

exit:
    if (ret != Object_OK && me->controlFd >= 0) {
        close(me->controlFd);
        me->controlFd = -1;
    }
    return ret;
}

static int32_t CTTouchControlService_retain(TTouchControlService *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t CTTouchControlService_release(TTouchControlService *me)
{
    int32_t ret = Object_OK;
    if (me != NULL && atomicAdd(&me->refs, -1) == 0) {
        ret = CTTouchControlService_releaseTouch(me);
        HEAP_FREE_PTR(me);
    }
    return ret;
}

static ITTouchControlService_DEFINE_INVOKE(CTTouchControlService_invoke, CTTouchControlService_, TTouchControlService *);

int32_t CTTouchControlService_open(uint32_t uid, Object credentials, Object *objOut) {
    int32_t ret = Object_OK;
    TTouchControlService *me;

    (void)uid;
    (void)credentials;

    me = HEAP_ZALLOC_TYPE(TTouchControlService);
    if (!me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    me->controlFd = -1;

    *objOut = (Object) {CTTouchControlService_invoke, me};
    return ret;
}
