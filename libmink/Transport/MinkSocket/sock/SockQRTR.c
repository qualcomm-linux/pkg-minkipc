// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <sys/socket.h>
#include "SockQRTR.h"
#include "Types.h"
#include "Utils.h"

#define MAX_RETRY_COUNT 5

int32_t SockQRTR_new(const char *address, struct sockaddr_qrtr *sockAddr, int *sockFd,
                     uint32_t *node, uint32_t *port)
{
    int32_t addrValue = 0;
    struct sockaddr_qrtr sq = {};
    struct qrtr_packet pkt = {};
    socklen_t sl = 0;
    char buf[4096];
    int32_t recvBuf = 0;
    int32_t fd = -1;
    int32_t retryCount = 0;

    if (*sockFd < 0) {
        fd = qrtr_open(0);
        if (fd < 0) {
            LOG_ERR("qrtr_open failed: fd = %d, errno %d\n", fd, errno);
            return -1;
        }
        if (NULL == address) {
          LOG_ERR("address is NULL, failed to use atoi()\n");
          vm_osal_socket_close(fd);
          return -1;
        }
        addrValue = atoi(address);
        if (qrtr_new_lookup(fd, addrValue, MINK_QRTR_LA_SERVICE_VERSION,
                            MINK_QRTR_LA_SERVICE_INSTANCE) == -1) {
            LOG_ERR("lookup failed\n");
            vm_osal_socket_close(fd);
            return -1;
        }

        while (1) {
            sl = sizeof(sq);
            recvBuf = recvfrom(fd, buf, sizeof(buf), 0, (void *)&sq, &sl);
            if (recvBuf < 0) {
                retryCount++;
                if (retryCount < MAX_RETRY_COUNT) {
                    LOG_ERR("Didn't recv control pkt from server: errno = %d\n. Retrying", errno);
                    usleep(10 * 1000); // 10ms
                    continue;
                } else {
                    LOG_ERR("Always failed recv control pkt: errno = %d\n. Exiting", errno);
                    vm_osal_socket_close(fd);
                    return -1;
                }
            }

            qrtr_decode(&pkt, buf, recvBuf, &sq);
            if (pkt.node == 0 && pkt.port == 0) {
                break;
            }

            sockAddr->sq_family = AF_QIPCRTR;
            sockAddr->sq_node = pkt.node;
            sockAddr->sq_port = pkt.port;
            *node = sockAddr->sq_node;
            *port = sockAddr->sq_port;
        }
        *sockFd = fd;
    }

    return 0;
}

int32_t SockQRTR_validate(int sockFd)
{
    struct sockaddr_qrtr sqServer;
    socklen_t sockLen = sizeof(struct sockaddr_qrtr);

    return getsockname(sockFd, (void *)&sqServer, &sockLen);
}

int SockQRTR_recvfrom(int sockFd, void *buf, size_t sizeBufMax, int flags,
                      struct sockaddr_qrtr **srcAddr, socklen_t *addrLen, int exitFd)
{
    vm_osal_pollfd pfd[2];
    int timeout = -1;
    int rc = -1;
    int recvLen = -1;

    pfd[0].fd = sockFd;
    pfd[0].events = POLLIN;
    pfd[1].fd = exitFd;
    pfd[1].events = POLLIN;

    while (1) {
        rc = vm_osal_poll(pfd, 2, timeout);
        if (rc < 0) {
            if (EINTR == errno) {
                LOG_ERR("event occurs before poll(), try again\n");
                continue;
            } else {
                LOG_ERR("poll() failed with %s, connection shutdown\n", strerror(errno));
                return -1;
            }
        }

        if ((pfd[1].revents & POLLHUP) || (pfd[1].revents & POLLNVAL)
            || (pfd[1].revents & POLLERR)) {
            LOG_ERR("%s: poll pfd[1] error, tid: %lx, sockFd: %d, revents: %d\n",
                    __func__, vm_osal_thread_self(), sockFd, pfd[1].revents);
            return -1;
        }

        if (pfd[1].revents & POLLIN) {
            LOG_ERR("%s: received exit message, sockFd: %d, exitFd: %d\n",
                    __func__, sockFd, exitFd);
            return -1;
        }

        if ((pfd[0].revents & POLLHUP) || (pfd[0].revents & POLLNVAL)
             || (pfd[0].revents & POLLERR)) {
            LOG_ERR("%s: poll pfd[0], error tid: %lx, sockFd: %d, revents: %d\n",
                    __func__, vm_osal_thread_self(), sockFd, pfd[0].revents);
            return -1;
        }

        if (pfd[0].revents & POLLIN) {
            recvLen = recvfrom(sockFd, buf, sizeBufMax, flags, (void *)*srcAddr, addrLen);
            if (recvLen < 0) {
                /* EINTR is thread was signaled, ignore the signal and retry */
                rc = errno;
                if (rc == EINTR || rc == EAGAIN) {
                    continue;
                }
                LOG_ERR("%s: tid: %lx, sock: %d, returning errno: %d\n",
                        __func__, vm_osal_thread_self(), sockFd, errno);
                return -1;
            } else {
                break;
            }
        }
    }

    return recvLen;
}

int32_t SockQRTR_preProcess(void *buf, size_t len, struct sockaddr_qrtr *srcAddr,
                            uint32_t *pktNode, uint32_t *pktPort)
{
    struct qrtr_packet pkt = {0};
    PREPROCESS_STATE status = 0;

    qrtr_decode(&pkt, buf, len, srcAddr);

    *pktNode = pkt.node;
    *pktPort = pkt.port;

    if (pkt.type == QRTR_TYPE_NEW_SERVER || pkt.type == QRTR_TYPE_DEL_SERVER) {
        if (pkt.node == 0 && pkt.port ==0) {
            status = DISCARD;
        }
    }

    if (pkt.type != QRTR_TYPE_DATA) {
        status = DISCARD;
    }

    if (pkt.type == QRTR_TYPE_DEL_CLIENT) {
        status = CLIENTDOWN;
    }

    if (pkt.type == QRTR_TYPE_DEL_SERVER) {
        status = SERVERDOWN;
    }

    if (pkt.type == QRTR_TYPE_BYE) {
        if (pkt.node) {
            LOG_TRACE("NOT a modem SSR, skip\n");
            status = DISCARD;
        } else {
            status = SUBSYSDOWN;
        }
    }

    return status;
}

int32_t SockQRTR_triggerAuxPipe(int fd)
{
    if (fd < 0) {
        LOG_ERR("Fd is invalid\n");
        return -1;
    }

    if (write(fd, "\0", 1) <= 0) {
        LOG_ERR("write to exitFd failed\n");
        return -1;
    }

    return 0;
}

int SockQRTR_connect(int sockFd, struct sockaddr_qrtr *addr, socklen_t addrLen)
{
    return connect(sockFd, (struct sockaddr *)addr, addrLen);
}

int32_t SockQRTR_sendMsg(int sockFd, uint32_t node, uint32_t port,
                         const void *data, size_t size)
{
    return qrtr_sendto(sockFd, node, port, data, size);
}

int32_t SockQRTR_sendVec(int sockFd, void *data, size_t dataSize, int *fds, int numFds,
                         uint32_t node, uint32_t port)
{
    size_t loadMax = MAX_QRTR_PAYLOAD;
    int32_t ret = 0;

    (void)fds;
    (void)numFds;
    if (loadMax < dataSize) {
        LOG_ERR("Data size exceeds limitation of\n");
        return -1;
    }

    ret = SockQRTR_sendMsg(sockFd, node, port, data, dataSize);
    if (ret < 0) {
        LOG_ERR("SockQRTR_sendMsg failed: errno = %d, sockFd = %d, node = %d\n",
                errno, sockFd, node);
    }

    return ret;
}
