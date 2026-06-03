// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// TI (TunnelInvoke): TI provides a Mink-over-Mink transport with end-to-end
// encryption, similar to a VPN tunnel.
//
// Each instance of TI is one endpoint (of two) for a transport.
// One endpoint is the "client" and one the "host".  They use different sets of TI functions.

#include "TunnelInvoke.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "IMinkPortal_invoke.h"
#include "ITunnelInvokeMgr.h"
#include "ObjectTable.h"
#include "TICrypto.h"
#include "TIErrno.h"
#include "TIMessage.h"
#include "TIMutex_port.h"
#include "cdefs.h"
#include "check.h"
#include "heap.h"
#include "memscpy.h"
#include "object.h"

struct TI {
    int refs;
    uint32_t version;
    uint64_t counter;
    bool isHost;
    uint8_t key[TI_KEY_LEN_BYTES];
    ObjectTable objTable;
    Object peer;  // IMinkPortal
    TIMutex counterMutex;
};

static void TI_delete(TI* me)
{
    ObjectTable_destruct(&me->objTable);
    Object_ASSIGN_NULL(me->peer);

    HEAP_FREE_PTR(me);
}

/**
 * Invoke an object in the peer domain
 */
static int32_t TI_invokeRemoteObject(TI* me, size_t remoteIndex, ObjectOp op, ObjectArg* args,
                                     ObjectCounts k)
{
    // Need to init to error, due to how bail works
    int32_t errPack = Object_ERROR;
    int32_t errLock = Object_ERROR;
    int32_t errEncrypt = Object_ERROR;
    int32_t errPortal = Object_ERROR;
    int32_t errDecrypt = Object_ERROR;
    int32_t errUnpack = Object_ERROR;

    TIMessage *messageIn = NULL, *messageOut = NULL;
    size_t messageInLen, messageOutLen;

    TIPacket *packetIn = NULL, *packetOut = NULL;
    size_t packetInLen, packetOutLen, packetOutLenOut;

    if (Object_isNull(me->peer)) {
        TI_LOG_ERR(TI_ERR_NO_PEER);
        return Object_ERROR_DEFUNCT;
    }

    // Construct new TIMessage 'messageIn'
    errPack = TIMessage_newOutbound(remoteIndex, op, args, k, me, &me->objTable, &messageIn,
                                    &messageInLen, &messageOutLen);
    if (Object_isERROR(errPack)) {
        TI_LOG_ERR(TI_ERR_PREPARE_OUTB_FAIL, errPack);
        messageIn = NULL;
        goto bail;
    }

    // Take the counter mutex.
    //
    //   Due to the replay protection counters, tunnel invoke must be synchronous.
    //   When one side authors a packet, it must wait for its peer to respond before
    //   authoring any new packets. Otherwise, the counter values become interleaved
    //   and failures result.
    //
    //   Each session's counter mutex represents the ability to author new packets.
    //   When the mutex is taken, we are waiting for the peer to respond,
    //   and no new counter values can be generated.
    errLock = TIMutex_lock(&me->counterMutex);
    if (Object_isERROR(errLock)) {
        TI_LOG_ERR(TI_ERR_LOCK_INV_REMOBJ, errLock);
        goto bail;
    }

    // Encrypt 'messageIn' into a TIPacket 'packetIn' Allocates both packetIn and packetOut
    errEncrypt = TICrypto_newPackets(me->key, TI_KEY_LEN_BYTES, &me->counter, me->isHost, messageIn,
                                     messageInLen, messageOutLen, &packetIn, &packetInLen,
                                     &packetOut, &packetOutLen);
    if (Object_isERROR(errEncrypt)) {
        TI_LOG_ERR(TI_ERR_NEWPACKETS_FAIL, errEncrypt);
        packetIn = NULL;
        packetOut = NULL;
        TIMutex_unlock(&me->counterMutex);
        goto bail;
    }

    // Transact to other domain
    errPortal = IMinkPortal_transact(me->peer, packetIn, packetInLen, packetOut, packetOutLen,
                                     &packetOutLenOut);
    if (Object_isERROR(errPortal)) {
        TI_LOG_ERR(TI_ERR_PORTAL_ERROR, errPortal);
        TIMutex_unlock(&me->counterMutex);
        goto bail;
    }

    // Decrypt 'packetOut' into a new TIMessage 'messageOut'
    // Allocates messageOut
    errDecrypt = TICrypto_newMessage(me->key, TI_KEY_LEN_BYTES, &me->counter, packetOut,
                                     packetOutLenOut, false, &messageOut, &messageOutLen);
    if (Object_isERROR(errDecrypt)) {
        TI_LOG_ERR(TI_ERR_NEWMESSAGE_FAIL_1, errDecrypt);
        messageOut = NULL;
        TIMutex_unlock(&me->counterMutex);
        goto bail;
    }

    // Release the counter mutex.
    // We've consumed the message counter from the peer, and we're free to author a new one.
    TIMutex_unlock(&me->counterMutex);

    // Process TIMessage 'messageOut'
    // Note: errUnpack will contain the result from the invocation
    //       in the remote domain, or an error from marshalling out.
    errUnpack = TIMessage_processResponse(messageOut, messageOutLen, args, k, me, &me->objTable);
    if (Object_isERROR(errUnpack)) {
        // Following log clutters the log buffer, can remove
        TI_LOG_ERR(TI_ERR_PROCESS_RSP_FAIL, errUnpack);
        goto bail;
    }

bail:
    if (Object_isOK(errPack)) {
        // TIMessage_newOutbound() has retained remote OIs,
        // and they need to be released.
        TIMessage_releaseRemoteOI(args, k);

        if (Object_isERROR(errPortal)) {
            // Local OIs have been inserted in the OT, but the corresponding references
            // have not been passed to remote domain, remove them from the OT.
            TIMessage_releaseLocalOI(messageIn, messageInLen, &me->objTable);
        }
    }

    HEAP_FREE_PTR(messageIn);
    HEAP_FREE_PTR(messageOut);
    HEAP_FREE_PTR(packetIn);
    HEAP_FREE_PTR(packetOut);

    // Return only generic or transport error codes from object.h,
    // or the result of the invocation in the remote domain (errUnpack).

    GUARD(errPack);
    GUARD(errLock);
    GUARD(errEncrypt);
    GUARD(errPortal);
    GUARD(errDecrypt);
    GUARD(errUnpack);

    return Object_OK;
}

static int32_t TI_handleInboundInvoke(TI* me, const void* inputs, size_t inputsLen, void* outputs,
                                      size_t outputsLen, size_t* outputsLenOut)
{
    int32_t errDecrypt = Object_ERROR;
    int32_t errInvoke = Object_ERROR;
    int32_t errLock = Object_ERROR;
    int32_t errEncrypt = Object_ERROR;

    TIMessage *messageIn = NULL, *messageOut = NULL;
    size_t messageInLen, messageOutLen = 0;

    // Decrypt into plaintext buffer, validate inputs
    errDecrypt =
        TICrypto_newMessage(me->key, TI_KEY_LEN_BYTES, &me->counter, (const TIPacket*)inputs,
                            inputsLen, true, &messageIn, &messageInLen);
    if (Object_isERROR(errDecrypt)) {
        TI_LOG_ERR(TI_ERR_NEWMESSAGE_FAIL_2, errDecrypt);
        messageIn = NULL;
        goto bail;
    }

    TIMutex_unlock(&me->counterMutex);

    // Allocates messageOut
    errInvoke = TIMessage_handleInbound(messageIn, messageInLen, me, &me->objTable, &messageOut,
                                        &messageOutLen);
    if (Object_isERROR(errInvoke)) {
        TI_LOG_ERR(TI_ERR_HANDLE_INB_FAIL, errInvoke);
        messageOut = NULL;
        TIMutex_lock(&me->counterMutex);
        goto bail;
    }

    errLock = TIMutex_lock(&me->counterMutex);
    if (Object_isERROR(errLock)) {
        TI_LOG_ERR(TI_ERR_LOCK_HNDL_INBOUND, errLock);
        goto bail;
    }

    // Encrypt response data directly into transport buffer
    errEncrypt =
        TICrypto_encrypt(me->key, TI_KEY_LEN_BYTES, &me->counter, me->isHost, messageOut,
                         messageOutLen, (TIPacket*)outputs, outputsLen, outputsLenOut, true);
    if (Object_isERROR(errEncrypt)) {
        TI_LOG_ERR(TI_ERR_ENCRYPT_FAIL, errEncrypt);
        goto bail;
    }

bail:

    HEAP_FREE_PTR(messageOut);
    HEAP_FREE_PTR(messageIn);

    GUARD(errDecrypt);
    GUARD(errInvoke);
    GUARD(errLock);
    GUARD(errEncrypt);

    return Object_OK;
}

//----------------------------------------------------------------
// IMinkPortal
//----------------------------------------------------------------

static int32_t TI_IMinkPortal_retain(TI* me)
{
    TI_retain(me);
    return Object_OK;
}

static int32_t TI_IMinkPortal_release(TI* me)
{
    TI_release(me);
    return Object_OK;
}

static int32_t TI_IMinkPortal_setPeer(TI* me, Object peer)
{
    return TI_setPeer(me, peer);
}

/**
 * Inbound invocation
 */
static int32_t TI_IMinkPortal_transact(TI* me, const void* inputs, size_t inputsLen, void* outputs,
                                       size_t outputsLen, size_t* outputsLenOut)
{
    return TI_handleInboundInvoke(me, inputs, inputsLen, outputs, outputsLen, outputsLenOut);
}

static IMinkPortal_DEFINE_INVOKE(TI_IMinkPortal_invoke, TI_IMinkPortal_, TI*)

    static inline Object TI_asIMinkPortal(TI* me)
{
    return (Object){TI_IMinkPortal_invoke, (void*)me};
}

//----------------------------------------------------------------
// TIF: TI Forwarder
//----------------------------------------------------------------

typedef struct {
    int refs;
    size_t remoteObjectIndex;
    TI* parent;
} TIF;

static int32_t TIF_delete(TIF* me)
{
    (void)TI_invokeRemoteObject(me->parent, me->remoteObjectIndex, Object_OP_release, NULL, 0);
    (void)TI_release(me->parent);
    HEAP_FREE_PTR(me);
    return Object_OK;
}

static int32_t TIF_retain(TIF* me)
{
    ++me->refs;
    return Object_OK;
}

static int32_t TIF_release(TIF* me)
{
    if (--me->refs == 0) {
        TIF_delete(me);
    }

    return Object_OK;
}

static int32_t TIF_invoke(ObjectCxt cxt, ObjectOp op, ObjectArg* args, ObjectCounts k)
{
    TIF* me = (TIF*)cxt;
    if (ObjectOp_isLocal(op)) {
        switch (ObjectOp_methodID(op)) {
            case Object_OP_release:
                return TIF_release(me);

            case Object_OP_retain:
                return TIF_retain(me);

            default:
                // Do not forward local methods to remote domain
                return Object_ERROR_REMOTE;
        }
    }

    TIF_retain(me);

    int32_t rv = TI_invokeRemoteObject(me->parent, me->remoteObjectIndex, op, args, k);

    TIF_release(me);

    return rv;
}

static int32_t TIF_new(TI* parent, size_t index, Object* objOut)
{
    TIF* me = HEAP_ZALLOC_TYPE(TIF);
    if (!me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    me->remoteObjectIndex = index;
    me->parent = parent;
    (void)TI_retain(parent);

    *objOut = (Object){TIF_invoke, (void*)me};
    return Object_OK;
}

//----------------------------------------------------------------
// Client & Host APIs
//----------------------------------------------------------------

int32_t TI_new(uint32_t version, bool isHost, uint8_t* key, size_t keyLen, size_t objTableLen,
               TI** meOut)
{
    if (version != TI_LATEST_VERSION) {
        TI_LOG_ERR(TI_ERR_BAD_VERSION, version, TI_LATEST_VERSION);
        return ITunnelInvokeMgr_ERROR_BAD_VERSION;
    }

    TI* me = HEAP_ZALLOC_TYPE(TI);
    if (!me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    me->version = version;
    me->counter = 0;
    me->isHost = isHost;

    if (Object_isERROR(TIMutex_init(&me->counterMutex))) {
        goto bail;
    }

    // Host side starts waiting for a packet.
    if (isHost) {
        if (Object_isERROR(TIMutex_lock(&me->counterMutex))) {
            goto bail;
        }
    }

    if (keyLen != TI_KEY_LEN_BYTES) {
        TI_LOG_ERR(TI_ERR_BAD_KEY_LEN, keyLen, TI_KEY_LEN_BYTES);
        goto bail;
    }

    memscpy(me->key, TI_KEY_LEN_BYTES, key, keyLen);

    if (Object_isERROR(ObjectTable_construct(&me->objTable, objTableLen))) {
        goto bail;
    }

    *meOut = me;
    return Object_OK;

bail:
    TI_release(me);
    return Object_ERROR;
}

int32_t TI_retain(TI* me)
{
    ++me->refs;
    return Object_OK;
}

int32_t TI_release(TI* me)
{
    if (--me->refs == 0) {
        TI_delete(me);
    }

    return Object_OK;
}

//----------------------------------------------------------------
// Host APIs
//----------------------------------------------------------------

// Make `obj` available for invocation by the peer at index `n`.
int32_t TI_addLocalObject(TI* me, Object obj, size_t* n)
{
    return ObjectTable_addObject(&me->objTable, obj, n);
}

// Obtain the portal interface that we need to provide to our peer endpoint.
Object TI_getIMinkPortal(TI* me)
{
    ++me->refs;
    return TI_asIMinkPortal(me);
}

//----------------------------------------------------------------
// Client APIs
//----------------------------------------------------------------

// Obtain an object reference that will invoke an object at index `n` in the peer.
int32_t TI_newRemoteObject(TI* me, size_t index, Object* objOut)
{
    return TIF_new(me, index, objOut);
}

// Set the peer endpoint.
int32_t TI_setPeer(TI* me, Object peer)
{
    Object_ASSIGN(me->peer, peer);
    return Object_OK;
}

int32_t TI_releasePeer(TI* me)
{
    Object_ASSIGN_NULL(me->peer);
    return Object_OK;
}

// Register this endpoint with the peer so that it can call back into this domain.
int32_t TI_registerForCallbacks(TI* me)
{
    if (Object_isNull(me->peer)) {
        TI_LOG_ERR(TI_ERR_REG_CBACKS_NO_PEER);
        return Object_ERROR;
    }

    Object minkPortal = TI_getIMinkPortal(me);

    // setPeer() retains object
    int rv = IMinkPortal_setPeer(me->peer, minkPortal);

    Object_ASSIGN_NULL(minkPortal);

    return rv;
}

// Determine if 'obj' is a TIF, and give its remote index Used internally (by TIMessage)
bool TI_isRemoteObj(Object obj, size_t* index)
{
    // Ideally we should check if obj belongs to *this* tunnel (if there are multiple).
    // This can be a future improvement.
    if (obj.invoke == TIF_invoke) {
        TIF* me = (TIF*)obj.context;
        *index = me->remoteObjectIndex;
        return true;
    }

    return false;
}
