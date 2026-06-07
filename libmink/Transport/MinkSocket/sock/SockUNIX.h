// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __SOCK_UNIX_H
#define __SOCK_UNIX_H

#include <sys/un.h>
#include <sys/socket.h>
#include "VmOsal.h"

#if defined (__cplusplus)
extern "C" {
#endif

int32_t SockUNIX_new(const char *address, struct sockaddr_un *sockAddr, int32_t *sock,
                     uint32_t *node, uint32_t *port);
int SockUNIX_listen(int sockFd, int32_t backlog);
int SockUNIX_bind(int sockFd, struct sockaddr_un *sockAddr, socklen_t addrLen);
int SockUNIX_accept(int sockFd, struct sockaddr_un *addr, socklen_t *addrLen);
int SockUNIX_connect(int sockFd, struct sockaddr_un *addr, socklen_t addrLen);
int SockUNIX_recv(int sockFd, void **msg, size_t *sizeMsg, int fds[], size_t *numFds);
int32_t SockUNIX_sendMsg(int sockFd, const void *buf, size_t len);
int32_t SockUNIX_sendVec(int sockFd, void *data, size_t sizeData,
                         int *fds, int numFds);

#if defined (__cplusplus)
}
#endif

#endif //__SOCK_UNIX_H
