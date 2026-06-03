// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __SOCK_VSOCK_H
#define __SOCK_VSOCK_H

#include <linux/vm_sockets.h>
#include "VmOsal.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define AF_QMSGQ        27

#define MAX_VSOCK_PAYLOAD   (64 * 1024)
#define VSOCK_PORT_NO       0x5555

int32_t SockVSOCK_constructFd(void);
int32_t SockVSOCK_new(const char *address, struct sockaddr_vm *sockAddr, int *sockFd,
                      uint32_t *node, uint32_t *port);
int SockVSOCK_bind(int sockFd, struct sockaddr_vm *sockAddr, socklen_t addrLen);
int SockVSOCK_validate(int sockFd);
int32_t SockVSOCK_recvfrom(int sockFd, void *buf, size_t len, int32_t flags,
                           struct sockaddr_vm **srcAddr, socklen_t *addrLen);
int SockVSOCK_preProcess(void *buf, size_t len, struct sockaddr_vm *srcAddr,
                         uint32_t *pktNode, uint32_t *pktPort);
int SockVSOCK_connect(int sockFd, struct sockaddr_vm *addr, socklen_t addrLen);
int32_t SockVSOCK_sendMsg(int sockFd, uint32_t node, uint32_t port,
                          const void *data, uint32_t size);
int32_t SockVSOCK_sendVec(int sockFd, void *data, size_t dataSize, int32_t *fds,
                          int32_t numFds, uint32_t node, uint32_t port);

#if defined (__cplusplus)
}
#endif

#endif //__SOCK_VSOCK_H
