// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include "SockVSOCK.h"
#include "Types.h"
#include "Utils.h"

/*@brief: try to construct socket fd of AF_VSOCK protocol. If it
*          fails, try again to construct socket fd of AF_QMSGQ
*          protocol. If it still fails, return error.
*
*  AF_QMSGS is more generic procotol including AF_VSOCK
*/
int32_t SockVSOCK_constructFd(void)
{
    int32_t fd = -1;

    fd = socket(AF_VSOCK, SOCK_DGRAM, 0);
    if (fd > 0) {
        LOG_TRACE("construct sockfd %d of AF_VSOCK\n", fd);
        return fd;
    }

    fd = socket(AF_QMSGQ, SOCK_DGRAM, 0);
    if (fd > 0) {
        LOG_TRACE("construct sockfd of AF_VSOCK failed, \
                construct sockfd %d of AF_QMSGS instead\n", fd);
        return fd;
    }

    LOG_ERR("fail to construct sockfd of AF_VSOCK or AF_QMSGQ\n");

    return -1;
}

int32_t SockVSOCK_new(const char *address, struct sockaddr_vm *sockAddr, int *sockFd,
                      uint32_t *node, uint32_t *port)
{
    int32_t fd = -1;
    int32_t addrValue = 0;

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

        fd = SockVSOCK_constructFd();
        if (fd < 0) {
            LOG_ERR("vsock_open failed: errno %d\n", errno);
            return -1;
        }

        sockAddr->svm_family = AF_VSOCK;
        sockAddr->svm_cid = VMADDR_CID_ANY; // Not used by underlying transport layer
        sockAddr->svm_port = addrValue; // Is address can be used for port on client or hardcoded?

        *sockFd = fd;
        *node = sockAddr->svm_cid;
        *port = sockAddr->svm_port;
    }

    return 0;
}

int SockVSOCK_bind(int sockFd, struct sockaddr_vm *sockAddr, socklen_t addrLen)
{
    sockAddr->svm_family = AF_VSOCK;
    sockAddr->svm_port = VSOCK_PORT_NO;
    sockAddr->svm_cid = VMADDR_CID_ANY;

    return bind(sockFd, (struct sockaddr *)sockAddr, addrLen);
}

int SockVSOCK_validate(int sockFd)
{
    struct sockaddr_vm vsockServer;
    socklen_t addrLen = sizeof(struct sockaddr_vm);

    return getsockname(sockFd, (void *)&vsockServer, &addrLen);
}

int32_t SockVSOCK_recvfrom(int sockFd, void *buf, size_t len, int32_t flags,
                           struct sockaddr_vm **srcAddr, socklen_t *addrLen)
{
    int32_t recvLen = -1;

    while (1) {
        recvLen = recvfrom(sockFd, buf, len, flags, (void *)*srcAddr, addrLen);
        if (recvLen < 0) {
            /* EINTR is thread was signaled, ignore the signal and retry */
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            LOG_ERR("sock %d failed on recvfrom() with errno %d\n", sockFd, errno);
            return -1;
        } else {
            break;
        }
    }

    return recvLen;
}

int SockVSOCK_preProcess(void *buf, size_t len, struct sockaddr_vm *srcAddr,
                         uint32_t *pktNode, uint32_t *pktPort)
{
    *pktNode = srcAddr->svm_cid;
    *pktPort = srcAddr->svm_port;

    return VSOCKCASE;
}

int SockVSOCK_connect(int sockFd, struct sockaddr_vm *addr, socklen_t addrLen)
{
    return connect(sockFd, (struct sockaddr *)addr, addrLen);
}

int32_t SockVSOCK_sendMsg(int sockFd, uint32_t node, uint32_t port,
                          const void *data, uint32_t size)
{
    struct sockaddr_vm client;

    memset(&client, 0, sizeof(client));
    client.svm_family = AF_VSOCK;
    client.svm_port = port;
    client.svm_cid = node;

    if (sendto(sockFd, data, size, 0, (struct sockaddr *)&client,
               sizeof(struct sockaddr_vm)) < 0) {
        LOG_ERR("sendto failed: sockFd = %d, errno = %d\n", sockFd, errno);
        return -1;
    }

    return 0;
}

int32_t SockVSOCK_sendVec(int sockFd, void *data, size_t dataSize, int32_t *fds,
                          int32_t numFds, uint32_t node, uint32_t port)
{
    size_t loadMax = MAX_VSOCK_PAYLOAD;
    int32_t ret = 0;

    (void)fds;
    (void)numFds;
    if (loadMax < dataSize) {
        LOG_ERR("Data size exceeds limitation\n");
        return -1;
    }

    ret = SockVSOCK_sendMsg(sockFd, node, port, data, dataSize);
    if (ret < 0) {
        LOG_ERR("vsock_sendto failed: sockFd = %d, node =%d, errno = %d\n",
                sockFd, node, errno);
    }

    return ret;
}
