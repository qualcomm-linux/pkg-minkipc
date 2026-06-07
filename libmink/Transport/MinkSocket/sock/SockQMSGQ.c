// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "SockQMSGQ.h"

#include <errno.h>
#include "cdefs.h"
#include "object.h"
#include "Types.h"
#include "Utils.h"

/*@brief: try to construct socket fd of AF_QMSGQ protocol,
*         which is connect oriented.
* @param: none
* @return: if success, return the fd, otherwise return -1.
*/
int SockQMSGQ_constructFd(void)
{
    int fd = -1;

    fd = socket(AF_QMSGQ, SOCK_SEQPACKET, 0);
    if (fd > 0) {
        LOG_MSG("construct sockfd %d of AF_QMSGQ\n", fd);
        return fd;
    }

    LOG_ERR("fail to construct sockfd of AF_QMSGQ fd = %d, errono = %d\n", fd, errno);

    return -1;
}

int SockQMSGQ_validate(int sockFd)
{
    struct sockaddr_vm QMSGQServer;
    socklen_t addrLen = sizeof(struct sockaddr_vm);

    return getsockname(sockFd, (void *)&QMSGQServer, &addrLen);
}

int SockQMSGQ_new(const char *address, struct sockaddr_vm *sockAddr, int *sockFd,
                  uint32_t *node, uint32_t *port)
{
    int fd = -1, addrValue = 0;

    if (*sockFd >= 0) {
        if (address == NULL){
            return 0;
        } else {
            LOG_ERR("Mistake invocation, sockFd and address should be exclusive\n");
            return -1;
        }
    } else {
        if (address == NULL) {
            LOG_ERR("Mistake invocation, sockFd and address should be exclusive but now both invalid\n");
            return -1;
        }
        addrValue = atoi(address);

        fd = SockQMSGQ_constructFd();
        if (fd < 0) {
            LOG_ERR("QMSGQ_open failed: errno %d\n", errno);
            return -1;
        }

        sockAddr->svm_family = AF_QMSGQ;
        sockAddr->svm_cid = VMADDR_CID_ANY;
        sockAddr->svm_port = addrValue;
        *sockFd = fd;
        *node = sockAddr->svm_cid;
        *port = sockAddr->svm_port;
    }

    return 0;
}

int SockQMSGQ_bind(int sockFd, struct sockaddr_vm *sockAddr, socklen_t addrLen)
{
    return bind(sockFd, (struct sockaddr *)sockAddr, addrLen);
}

int SockQMSGQ_listen(int sockFd, int32_t backlog)
{
    return listen(sockFd, backlog);
}

int SockQMSGQ_accept(int sockFd, struct sockaddr *addr, socklen_t *addrLen)
{
    return accept(sockFd, addr, addrLen);
}

int SockQMSGQ_connect(int sockFd, struct sockaddr_vm *addr, socklen_t addrLen)
{
    addr->svm_family = AF_VSOCK;
    return connect(sockFd, (struct sockaddr *)addr, addrLen);
}

int SockQMSGQ_recv(int sockFd, void *data, size_t size)
{
    while (1) {
        ssize_t recvLen = recv(sockFd, data, size, 0);
        if (recvLen < 0) {
            /* EINTR is thread was signaled, ignore the signal and retry */
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            LOG_ERR("sock %d failed on recv() with errno %d\n", sockFd, errno);
            return Object_ERROR_DEFUNCT;
        } else if (recvLen == 0) {
            // calling shutdown will cause recv() return 0
            return Object_ERROR_DEFUNCT;
        } else {
            break;
        }
    }
    return 0;
}

int SockQMSGQ_sendMsg(int sockFd, const void *buf, size_t len)
{
    ssize_t cb = 0;

    if (NULL == buf || len <= 0) {
        LOG_ERR("Buf is NULL or len is invalid\n");
        return -1;
    }

    while (len > 0) {
        cb = send(sockFd, buf, len, 0);
        if (cb <= 0) {
            LOG_ERR("QMSGQ Send failed: sockFd = %d, errno = %d\n", sockFd, errno);
            return -1;
        }

        if (cb <= (ssize_t)len) {
            buf = (cb + (char *)buf);
            len -= (size_t)cb;
        }
    }

    return 0;
}

int SockQMSGQ_sendVec(int sockFd, void *data, size_t dataSize, int32_t *fds, int32_t numFds,
                      uint32_t node, uint32_t port)
{
    size_t loadMax = MAX_QMSGQ_PAYLOAD;
    int ret = 0;

    (void)fds;
    (void)numFds;
    if (loadMax < dataSize) {
        LOG_ERR("Data size exceeds limitation of\n");
        return -1;
    }
    ret = SockQMSGQ_sendMsg(sockFd, data, dataSize);

    if (ret < 0) {
        LOG_ERR("SockQRTR_sendMsg failed: errno = %d, sockFd = %d, node = %d\n",
                errno, sockFd, node);
    }

    return ret;
}
