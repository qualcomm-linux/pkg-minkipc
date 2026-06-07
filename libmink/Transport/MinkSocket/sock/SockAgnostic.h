// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __SOCK_AGNOSTIC_H
#define __SOCK_AGNOSTIC_H

#include "VmOsal.h"
#include "LinkCredentials.h"
#include "lxcom_sock.h"
#include "Marshalling.h"
#include "Types.h"

#ifdef VNDR_UNIXSOCK
#include <sys/un.h>
#include <sys/socket.h>
#endif
#ifdef VNDR_SMCINVOKE_QRTR
#include "libqrtr.h"
#endif
#if (defined(VNDR_QMSGQ) || defined(VNDR_VSOCK))
#include <linux/vm_sockets.h>
#endif

#if defined (__cplusplus)
extern "C" {
#endif

#define MAX_QUEUE_LENGTH     5
#define MAX_SOCKADDR_LEN     108

typedef union {
#ifdef VNDR_UNIXSOCK
    struct sockaddr_un addr;
#endif
#ifdef VNDR_SMCINVOKE_QRTR
    struct sockaddr_qrtr qaddr;
#endif
#ifdef VNDR_VSOCK
    struct sockaddr_vm vaddr;
#endif
#ifdef VNDR_QMSGQ
    struct sockaddr_vm maddr;
#endif
    char spaceholder[256];
} sockaddr_t;

typedef struct SockAgnostic {
    sockaddr_t sockaddr;
    IPC_TYPE sockType;
    int sockFd;
    uint32_t node;
    uint32_t port;
    int auxPipe[2];
    bool bServer;
} SockAgnostic;

static inline bool SockAgnostic_isAlive(SockAgnostic *me)
{
    return (-1 != me->sockFd);
}
static inline bool SockAgnostic_isConnless(SockAgnostic *me)
{
    return ((QIPCRTR == me->sockType) || (VSOCK == me->sockType));
}
static inline uint32_t SockAgnostic_getNode(SockAgnostic *me)
{
    return me->node;
}
static inline int SockAgnostic_getSockType(SockAgnostic *me)
{
    return me->sockType;
}

static inline int SockAgnostic_getSockFd(SockAgnostic *me)
{
    return me->sockFd;
}
size_t SockAgnostic_getPayloadSize(SockAgnostic *me);
size_t SockAgnostic_getSockAddrSize(SockAgnostic *me);
int SockAgnostic_getPeerIdentity(SockAgnostic *me, CredInfo *credInfo,
                                 uint8_t **vmuuid, size_t *vmuuidlen);
int32_t SockAgnostic_new(const char *address, int sockFd, int32_t sockType, bool bServer,
                         SockAgnostic *objOut);
void SockAgnostic_populate(SockAgnostic *me, int dupSocdFd, uint32_t node, uint32_t port,
                           bool bServer, int32_t sockType);
int SockAgnostic_adaptAndCopy(SockAgnostic *dstSockAgn, SockAgnostic *srcSockAgn);
int SockAgnostic_shutdown(SockAgnostic *me, int how);
int SockAgnostic_close(SockAgnostic *me);
bool SockAgnostic_fullMatched(SockAgnostic *sockAgn1, SockAgnostic *sockAgn2);
bool SockAgnostic_nodeMatched(SockAgnostic *sockAgn1, SockAgnostic *sockAgn2);
int SockAgnostic_validate(SockAgnostic *me);
int SockAgnostic_listen(SockAgnostic *me, int backlog);
int SockAgnostic_bind(SockAgnostic *me);
int SockAgnostic_poll(SockAgnostic *me, int extraFds[], int extraNum, int timeout);
int SockAgnostic_accept(SockAgnostic *me, SockAgnostic *newSockAgn);
int SockAgnostic_connect(SockAgnostic *me);
int SockAgnostic_preProcess(SockAgnostic *me, void *buf, size_t sizeBuf, void *srcAddr,
                            SockAgnostic *newSockAgn);
int SockAgnostic_triggerAuxPipe(SockAgnostic *me);
int SockAgnostic_sendMsg(SockAgnostic *me, const void *prt, size_t size);
int SockAgnostic_sendVec(SockAgnostic *me, void *data, size_t dataSize,int *fds, int num_fds);

int SockAgnostic_prepareBuffer(SockAgnostic *me, void **msg, size_t *sizeMsg);
int SockAgnostic_recvfrom(SockAgnostic *me, void *buf, size_t len, int flags, void **src_addr);
int SockAgnostic_recv(SockAgnostic *me, void **msg, size_t *sizeMsg, int fds[], size_t *numFds);

#if defined (__cplusplus)
}
#endif

#endif //__SOCK_AGNOSTIC_H
