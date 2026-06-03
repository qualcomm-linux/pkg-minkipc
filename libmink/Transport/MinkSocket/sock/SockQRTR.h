// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __SOCK_QRTR_H
#define __SOCK_QRTR_H

#include "VmOsal.h"
#include "libqrtr.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define MAX_QRTR_PAYLOAD    (64 * 1024)
#define MINK_QRTR_LA_SERVICE_VERSION 1
#define MINK_QRTR_LA_SERVICE_INSTANCE 1

int32_t SockQRTR_new(const char *address, struct sockaddr_qrtr *sockAddr, int *sockFd,
                     uint32_t *node, uint32_t *port);
int32_t SockQRTR_validate(int sockFd);
int32_t SockQRTR_recvfrom(int sockFd, void *buf, size_t len, int32_t flags,
                          struct sockaddr_qrtr **srcAddr, socklen_t *addrLen, int exitFd);
int32_t SockQRTR_preProcess(void *buf, size_t len, struct sockaddr_qrtr *src_addr,
                            uint32_t *pktNode, uint32_t *pktPort);
int32_t SockQRTR_triggerAuxPipe(int exitFd);
int SockQRTR_connect(int sockfd, struct sockaddr_qrtr *addr, socklen_t addrlen);
int32_t SockQRTR_sendMsg(int sockFd, uint32_t node, uint32_t port,
                         const void *data, size_t size);
int32_t SockQRTR_sendVec(int sockFd, void *data, size_t dataSize, int *fds, int numFds,
                         uint32_t node, uint32_t port);

#if defined (__cplusplus)
}
#endif

#endif //__SOCK_QRTR_H
