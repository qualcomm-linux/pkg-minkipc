// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __SOCK_QMSGQ_H
#define __SOCK_QMSGQ_H

#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include "VmOsal.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifndef AF_QMSGQ
#define AF_QMSGQ          27
#endif

// QMSGQ protocol max payload is 64KB minus header size
#define MAX_QMSGQ_PAYLOAD   (0x00010000 - 0x00000020)

int SockQMSGQ_constructFd(void);
int SockQMSGQ_validate(int sockFd);
int SockQMSGQ_new(const char *address, struct sockaddr_vm *sockAddr, int *sockFd,
                  uint32_t *node, uint32_t *port);
int SockQMSGQ_bind(int sockFd, struct sockaddr_vm *sockAddr, socklen_t addrLen);
int SockQMSGQ_listen(int sockFd, int32_t backlog);
int SockQMSGQ_accept(int sockFd, struct sockaddr *addr, socklen_t *addrLen);
int SockQMSGQ_connect(int sockFd, struct sockaddr_vm *addr, socklen_t addrLen);
int SockQMSGQ_recv(int sockFd, void *data, size_t size);
int SockQMSGQ_sendMsg(int sockFd, const void *buf, size_t len);
int SockQMSGQ_sendVec(int sockFd, void *data, size_t dataSize, int32_t *fds, int32_t numFds,
                      uint32_t node, uint32_t port);
#if defined (__cplusplus)
}
#endif

#endif //__SOCK_QMSGQ_H
