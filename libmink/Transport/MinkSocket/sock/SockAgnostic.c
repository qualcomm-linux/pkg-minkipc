// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "cdefs.h"
#include "Heap.h"
#include "memscpy.h"
#include "object.h"
#include "SockAgnostic.h"
#include "Utils.h"
#include "vmuuid.h"

#ifdef VNDR_UNIXSOCK
#include "SockUNIX.h"
#endif
#ifdef VNDR_SMCINVOKE_QRTR
#include "SockQRTR.h"
#endif
#ifdef VNDR_VSOCK
#include "SockVSOCK.h"
#endif
#ifdef VNDR_QMSGQ
#include "SockQMSGQ.h"
#endif

#define SO_PEERCRED    17

const uint8_t vmuuidLE[VMUUID_MAX_SIZE] = {CLIENT_VMUID_TUI};

const uint8_t vmuuidOEM[VMUUID_MAX_SIZE] = {CLIENT_VMUID_OEM};

size_t SockAgnostic_getPayloadSize(SockAgnostic *me)
{
    size_t size = 0;

    switch (me->sockType) {
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            size = MAX_QRTR_PAYLOAD;
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            size = MAX_VSOCK_PAYLOAD;
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            size = MAX_QMSGQ_PAYLOAD;
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            size = 0;
            break;
    }

    return size;
}

size_t SockAgnostic_getSockAddrSize(SockAgnostic *me)
{
    size_t size = 0;

    switch (me->sockType) {
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            size = sizeof(struct sockaddr_qrtr);;
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            size = sizeof(struct sockaddr_vm);
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            size = sizeof(struct sockaddr_vm);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            size = 0;
            break;
    }

    return size;
}

int SockAgnostic_getPeerIdentity(SockAgnostic *me, CredInfo *credInfo,
                                     uint8_t **vmuuid, size_t *vmuuidlen)
{
  int ret = Object_ERROR;

  if (UNIX == me->sockType) {
#ifdef VNDR_UNIXSOCK
    socklen_t sizeInfo = sizeof(CredInfo);
    ret = vm_osal_getsockopt(me->sockFd, SOL_SOCKET, SO_PEERCRED,
                              credInfo, &sizeInfo);
    if (0 != ret) {
      LOG_ERR("Failed to getsockopt, ret = %d\n", ret);
    }
#else
    LOG_ERR("Not support UnixSock to getPeerIdentity\n");
#endif
  } else {
    *vmuuid = HEAP_ZALLOC_ARRAY(uint8_t, VMUUID_MAX_SIZE);
    if (NULL != *vmuuid) {
        /* TODO: vmuuid of remote counterpart ought to be extracted from QRTR/VSOCK
         *  protocol. Now just copy static value for simulation
        */
        *vmuuidlen = memscpy(*vmuuid, VMUUID_MAX_SIZE, vmuuidOEM, VMUUID_MAX_SIZE);
        ret = 0;

    } else {
        LOG_ERR("Failed to allocate vmuuid\n");
        ret = Object_ERROR_MEM;
    }
  }

  return ret;
}

int32_t SockAgnostic_new(const char *address, int sockFd, int32_t sockType, bool bServer,
                         SockAgnostic *objOut)
{
    int32_t ret = 0;

    if (NULL == objOut) {
        LOG_ERR("invalid input\n");
        return -1;
    }
    memset(objOut, 0, sizeof(SockAgnostic));

    objOut->sockFd = sockFd;
    objOut->sockType = sockType;
    objOut->bServer = bServer;
    objOut->auxPipe[0] = -1;
    objOut->auxPipe[1] = -1;

    // auxPipe only needed for QIPCRTR protocol
    if (sockType == QIPCRTR) {
        if (vm_osal_pipe(objOut->auxPipe) < 0) {
            LOG_ERR("vm_osal_pipe filed\n");
            return -1;
        }
    }

    switch (objOut->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            ret = SockUNIX_new(address, &objOut->sockaddr.addr, &objOut->sockFd,
                               &objOut->node, &objOut->port);
            break;
#endif
#if defined(VNDR_SMCINVOKE_QRTR)
        case QIPCRTR:
            ret = SockQRTR_new(address, &objOut->sockaddr.qaddr, &objOut->sockFd,
                               &objOut->node, &objOut->port);
            break;
#endif
#if defined(VNDR_VSOCK)
        case VSOCK:
            ret = SockVSOCK_new(address, &objOut->sockaddr.vaddr, &objOut->sockFd,
                                &objOut->node, &objOut->port);
            break;
#endif
#if defined(VNDR_QMSGQ)
        case QMSGQ:
            ret = SockQMSGQ_new(address, &(objOut->sockaddr.maddr), &objOut->sockFd,
                                &objOut->node, &objOut->port);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = -1;
            break;
    }

    LOG_TRACE("constructed sockAgnostic = %p, sockFd = %d, sockType = %d\n",
              objOut, objOut->sockFd, objOut->sockType);
    return ret;
}

inline
void SockAgnostic_populate(SockAgnostic *me, int sockFd, uint32_t node, uint32_t port,
                           bool bServer, int32_t sockType)
{
    memset(me, 0, sizeof(SockAgnostic));

    me->sockFd = sockFd;
    me->node = node;
    me->port = port;
    me->bServer = bServer;
    me->sockType = sockType;
}

int SockAgnostic_adaptAndCopy(SockAgnostic *dstSockAgn, SockAgnostic *srcSockAgn)
{
    int sockFd = 0;
    uint32_t node = 0, port = 0;
    /*
    - Regarding connected protocol
        For client, MinkIPC & MinkSocket share the same sockfd, In order to decouple their sockfd
         closing behavior, fd dup() is indispensable.
        For server, MinkIPC sockfd is specified for accept() while each MinkSocket sockfd serves
         as worker, their sockfd closing behavior is independent, no fd dup() needed.
    - Regarding connectionless protocol
        For both client & server, MinkIPC & MinkSocket share the same sockfd, so fd dup() is needed.
    */
    if (UNIX == srcSockAgn->sockType || SIMULATED == srcSockAgn->sockType ||
        QMSGQ == srcSockAgn->sockType) {
        if (srcSockAgn->bServer) {
            sockFd = srcSockAgn->sockFd;
        } else {
            sockFd = vm_osal_fd_dup(srcSockAgn->sockFd);
            if (sockFd < 0) {
                LOG_ERR("Failed to duplicate client sockFd as worker sockFd, ret = %d\n", sockFd);
                return sockFd;
            }
        }
        node = -1;
        port = -1;
    } else if (QIPCRTR == srcSockAgn->sockType || VSOCK == srcSockAgn->sockType) {
        sockFd = vm_osal_fd_dup(srcSockAgn->sockFd);
        if (sockFd < 0) {
            LOG_ERR("Failed to duplicate listener sockFd as worker sockFd, ret = %d\n", sockFd);
            return sockFd;
        }
        node = srcSockAgn->node;
        port = srcSockAgn->port;

    } else {
        LOG_ERR("Not supported sockType\n");
        return -1;
    }

    SockAgnostic_populate(dstSockAgn, sockFd, node, port,
                          srcSockAgn->bServer, srcSockAgn->sockType);

    return 0;
}

int SockAgnostic_shutdown(SockAgnostic *me, int how)
{
    return vm_osal_socket_shutdown(me->sockFd, how);
}

int SockAgnostic_close(SockAgnostic *me)
{
    int ret = 0;

    SockAgnostic_triggerAuxPipe(me);
    vm_osal_socket_close(me->sockFd);
    me->sockFd = -1;

    if (me->auxPipe[0] > 0) {
        vm_osal_fd_close(me->auxPipe[0]);
    }
    if (me->auxPipe[1] > 0) {
        vm_osal_fd_close(me->auxPipe[1]);
    }

    return ret;
}

bool SockAgnostic_fullMatched(SockAgnostic *sockAgn1, SockAgnostic *sockAgn2)
{
    if (sockAgn1->sockType != sockAgn2->sockType) {
        return false;
    }

    if (UNIX == sockAgn1->sockType || SIMULATED == sockAgn1->sockType) {
        if (sockAgn1->sockFd == sockAgn2->sockFd) {
            return true;
        }
    }

    if (QIPCRTR == sockAgn1->sockType) {
        if ((sockAgn1->node == sockAgn2->node) && (sockAgn1->port == sockAgn2->port)) {
            return true;
        }
    }

    if (VSOCK == sockAgn1->sockType) {
        if (sockAgn1->port == sockAgn2->port) {
            return true;
        }
    }

    if (QMSGQ == sockAgn1->sockType) {
        if (sockAgn1->sockFd == sockAgn2->sockFd) {
            return true;
        }
    }

    return false;
}

bool SockAgnostic_nodeMatched(SockAgnostic *sockAgn1, SockAgnostic *sockAgn2)
{
    if (sockAgn1->sockType != sockAgn2->sockType) {
        return false;
    }

    if (sockAgn1->node == sockAgn2->node) {
        return true;
    }

    return false;
}

int SockAgnostic_validate(SockAgnostic *me)
{
    int ret = 0;

    switch (me->sockType) {
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            ret = SockQRTR_validate(me->sockFd);
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            ret = SockVSOCK_validate(me->sockFd);
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            ret = SockQMSGQ_validate(me->sockFd);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = Object_ERROR;
            break;
    }

    return ret;
}

int SockAgnostic_listen(SockAgnostic *me, int backlog)
{
    int ret = 0;

    switch (me->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            ret = SockUNIX_listen(me->sockFd, backlog);
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            ret = SockQMSGQ_listen(me->sockFd, backlog);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = -1;
            break;
    }

    return ret;
}

int SockAgnostic_bind(SockAgnostic *me)
{
    int ret = 0;

    switch (me->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            ret = SockUNIX_bind(me->sockFd, &(me->sockaddr.addr), sizeof(me->sockaddr.addr));
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            ret = SockVSOCK_bind(me->sockFd, &(me->sockaddr.vaddr), sizeof(me->sockaddr.vaddr));
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            ret = SockQMSGQ_bind(me->sockFd, &(me->sockaddr.maddr), sizeof(me->sockaddr.maddr));
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = -1;
            break;
    }

    return ret;
}

int SockAgnostic_poll(SockAgnostic *me, int extraFds[], int extraNum, int timeout)
{
    int status = 0;
    int ret = 0;
    int i = 0;
    struct pollfd *pbits = NULL;

    // TODO: Remove this once QMSGQ support poll()
    if(me->sockType ==  QMSGQ) {
        return ret;
    }

    pbits = HEAP_ZALLOC_ARRAY(struct pollfd, (extraNum + 1));
    if (NULL == pbits) {
        LOG_ERR("Failed to allocate pollfd for sockAgnostic %p\n", me);
        return Object_ERROR;
    }

    for(i = 0; i < extraNum; i ++) {
        pbits[i].fd = extraFds[i];
        pbits[i].events = POLLIN;
    }
    pbits[extraNum].fd = me->sockFd;
    pbits[extraNum].events = POLLIN;

    while(1) {
        status = vm_osal_poll(pbits, extraNum + 1, timeout);
        if (status < 0) {
            if (EINTR == errno) {
                LOG_ERR("Event occurs before poll(), try again\n");
                continue;
            } else {
                LOG_ERR("Failed on MinkSock_poll() with result %s. Connection shuts down\n",
                        strerror(errno));
                ret = Object_ERROR_DEFUNCT;
                goto cleanup;
            }
        } else {
            break;
        }
    }

    for (i = 0; i < extraNum; i ++) {
        /* When endpoint exits proactively, main thread tells all worker threads to
        *  stop by pipeline message
        */
        if (pbits[i].revents & POLLIN) {
            LOG_TRACE("The endpoint exits proactively, closing all worker threads\n");
            ret = Object_ERROR_DEFUNCT;
            goto cleanup;
        }
    }
    /* When sockfd closed by other worker thread, poll() will be waked up by socket
    *  events(POLLHUP|POLLIN) or POLLNVAL
    */
    if ((pbits[extraNum].revents & POLLHUP) ||
        (pbits[extraNum].revents & POLLNVAL)) {
        LOG_TRACE("A worker thread closed sockAgnostic, waking up all others\n");
        ret = Object_ERROR_DEFUNCT;
        goto cleanup;
    }

cleanup:
    if (NULL != pbits) {
        HEAP_FREE_PTR(pbits);
    }
    return ret;
}

int SockAgnostic_accept(SockAgnostic *me, SockAgnostic *newSockAgn)
{
    int newSock = -1;

    if (NULL == newSockAgn) {
        LOG_ERR("invalid input\n");
        return -1;
    }

    switch (me->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            newSock = SockUNIX_accept(me->sockFd, NULL, NULL);
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            newSock = SockQMSGQ_accept(me->sockFd, NULL, NULL);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            return -1;
    }

    if (newSock > 0) {
        SockAgnostic_populate(newSockAgn, newSock, 0, 0, true, me->sockType);
        return newSock;
    } else {
        return -1;
    }
}

int SockAgnostic_connect(SockAgnostic *me)
{
    int32_t ret = -1;

    switch (me->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            ret = SockUNIX_connect(me->sockFd, &(me->sockaddr.addr), sizeof(me->sockaddr.addr));
            break;
#endif
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            ret = SockQRTR_connect(me->sockFd, &(me->sockaddr.qaddr), sizeof(me->sockaddr.qaddr));
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            ret = SockVSOCK_connect(me->sockFd, &me->sockaddr.vaddr, sizeof(me->sockaddr.vaddr));
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            ret = SockQMSGQ_connect(me->sockFd, &(me->sockaddr.maddr), sizeof(me->sockaddr.maddr));
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = -1;
            break;
    }

    return ret;
}

int SockAgnostic_preProcess(SockAgnostic *me, void *buf, size_t sizeBuf, void *srcAddr,
                            SockAgnostic *newSockAgn)
{
    int status = 0;
    uint32_t node = 0;
    uint32_t port = 0;

    switch (me->sockType) {
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            status = SockQRTR_preProcess(buf, sizeBuf, (struct sockaddr_qrtr *)srcAddr,
                                        &node, &port);
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            status = SockVSOCK_preProcess(buf, sizeBuf, (struct sockaddr_vm *)srcAddr,
                                          &node, &port);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            status = Object_ERROR;
            break;
    }

    SockAgnostic_populate(newSockAgn, me->sockFd, node, port, me->bServer, me->sockType);

    return status;
}

int SockAgnostic_triggerAuxPipe(SockAgnostic *me)
{
    int ret = 0;

    if (me->auxPipe[1] <= 0) {
        LOG_TRACE("Only QRTR needs and supports auxPipe\n");
        return -1;
    }

    switch (me->sockType) {
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            ret = SockQRTR_triggerAuxPipe(me->auxPipe[1]);
            break;
#endif
        default:
            ret = -1;
            break;
    }

    return ret;
}

int SockAgnostic_sendMsg(SockAgnostic *me, const void *data, size_t size)
{
    int ret = -1;

    switch (me->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            ret = SockUNIX_sendMsg(me->sockFd, data, size);
            break;
#endif
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            ret = SockQRTR_sendMsg(me->sockFd, me->node, me->port, data, size);
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            ret = SockVSOCK_sendMsg(me->sockFd, me->node, me->port, data, size);
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            ret = SockQMSGQ_sendMsg(me->sockFd, data, size);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = -1;
            break;
    }

    return ret;
}

int SockAgnostic_sendVec(SockAgnostic *me, void *data, size_t dataSize,
                            int *fds, int numFds)
{
    int ret = -1;

    switch (me->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            ret = SockUNIX_sendVec(me->sockFd, data, dataSize, fds, numFds);
            break;
#endif
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            ret = SockQRTR_sendVec(me->sockFd, data, dataSize, fds, numFds,
                                   me->node, me->port);
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            ret = SockVSOCK_sendVec(me->sockFd, data, dataSize, fds, numFds,
                                    me->node, me->port);
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            ret = SockQMSGQ_sendVec(me->sockFd, data, dataSize, fds, numFds, me->node, me->port);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = -1;
            break;
    }

    return ret;
}

int SockAgnostic_prepareBuffer(SockAgnostic *me, void **msgBuf, size_t *sizeMsg)
{
    int ret = 0;

    if (me == NULL || sizeMsg == NULL) {
        LOG_ERR("input is NULL\n");
        return -1;
    }

    switch (me->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            *sizeMsg = MSG_BUFFER_MAX;
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            *msgBuf = (void *)HEAP_ZALLOC(MAX_QMSGQ_PAYLOAD);
            if (NULL == *msgBuf) {
                LOG_ERR("Failed to allocate qmsgq buffer\n");
                ret = -1;
            }
            *sizeMsg = MAX_QMSGQ_PAYLOAD;
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = -1;
            break;
    }

    return ret;
}

int SockAgnostic_recvfrom(SockAgnostic *me, void *msg, size_t sizeMsgMax,
                          int flags, void **srcAddr)
{
    int sizeRecv = 0;
#if defined(SMCINVOKE_QRTR) || defined(VNDR_VSOCK)
    socklen_t sl = 0;
#endif

    switch (me->sockType) {
#ifdef VNDR_SMCINVOKE_QRTR
        case QIPCRTR:
            sl = sizeof(struct sockaddr_qrtr);
            sizeRecv = SockQRTR_recvfrom(me->sockFd, msg, sizeMsgMax, flags,
                                        (struct sockaddr_qrtr **)srcAddr, &sl,
                                         me->auxPipe[0]);
            break;
#endif
#ifdef VNDR_VSOCK
        case VSOCK:
            sl = sizeof(struct sockaddr_vm);
            sizeRecv = SockVSOCK_recvfrom(me->sockFd, msg, sizeMsgMax, flags,
                                         (struct sockaddr_vm **)srcAddr, &sl);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            sizeRecv = -1;
            break;
    }

    if (sizeRecv <= 0) {
        LOG_ERR("Error occurs when receiving message, received size = %d\n",
                sizeRecv);
    }

    return sizeRecv;
}

int SockAgnostic_recv(SockAgnostic *me, void **msg, size_t *sizeMsg, int fds[], size_t *numFds)
{
    int ret = -1;

    if (me == NULL || msg == NULL || sizeMsg == NULL) {
        LOG_ERR("input is NULL\n");
        return ret;
    }

    switch (me->sockType) {
#ifdef VNDR_UNIXSOCK
        case UNIX:
        case SIMULATED:
            ret = SockUNIX_recv(me->sockFd, msg, sizeMsg, fds, numFds);
            break;
#endif
#ifdef VNDR_QMSGQ
        case QMSGQ:
            ret = SockQMSGQ_recv(me->sockFd, *msg, *sizeMsg);
            break;
#endif
        default:
            LOG_ERR("None protocol match\n");
            ret = -1;
            break;
    }

    return ret;
}
