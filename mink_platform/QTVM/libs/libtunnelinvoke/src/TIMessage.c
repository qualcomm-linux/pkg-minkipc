// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "TIMessage.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "ObjectTable.h"
#include "TIErrno.h"
#include "TunnelInvoke.h"
#include "bbuf.h"
#include "heap.h"
#include "memscpy.h"
#include "object.h"

#define FOR_ARGS(ndxvar, counts, section)                                                    \
    for (ndxvar = ObjectCounts_index##section(counts);                                       \
         ndxvar < (ObjectCounts_index##section(counts) + ObjectCounts_num##section(counts)); \
         ++ndxvar)

// Need a portable util.h...
/* clang-format off */
#define IS_ALIGNED(ptr, aln) (0 == ((uintptr_t)(ptr) & (aln - 1)))
#define ALIGN_UP(val, size) (((val) + (size) - 1) & ~((size) - 1))
/* clang-format on */

/* A note about function names:
   marshalIn = Marshal into the destination domain
   marshalOut = Marshal out of the destination domain. */

/* Object handles can be marked as "remote in destination domain" during
   marshalling so the destination domain can interpret them correctly,
   ie, the destination domain will interpret those handles as remote
   and others as local.

   Note: This property is defined relatively and changes depending on
   the direction of the invocation. There is no fixed definition of
   domains like "non-secure" or "secure" in SMC Invoke. */
#define REMOTE_IN_DEST_DOMAIN (UINT32_C(1) << 31)
#define isRemoteInDestDomain(obj) (obj & REMOTE_IN_DEST_DOMAIN)
#define isLocalInDestDomain(obj) (!isRemoteInDestDomain(obj))
#define markRemoteInDestDomain(obj) (obj | REMOTE_IN_DEST_DOMAIN)
#define markLocalInDestDomain(obj) (obj & ~REMOTE_IN_DEST_DOMAIN)

/* We use handle 0 to identify Object_NULL (gNullTIMessageObj).
   Shifting by 1 to accomodate this discrepancy happens during marshalling
   when converting between handles and OT indexes. */
#define HANDLE_TO_INDEX(h) (markLocalInDestDomain(h) - 1)
#define INDEX_TO_HANDLE(i) (i + 1)

#define INVALID_POINTER ((void*)(uintptr_t)16)

/** Serialized buffer data must be 8 byte aligned */
#define TI_MESSAGE_ALIGN_BYTES 8
#define ARGS_ALIGN(x) ALIGN_UP(x, TI_MESSAGE_ALIGN_BYTES)

/* Return the size of a message header plus args array */
#define GET_HEADER_LEN(k) \
    ({ ARGS_ALIGN(sizeof(TIMessage) + sizeof(TIMessageArg) * ObjectCounts_total(k)); })

/* Returns the total message length required for either an
   input message or output message (use 'input' to distinguish) */
#define GET_MESSAGE_LEN(k, args, input)                   \
    ({                                                    \
        size_t iii, argsSize = GET_HEADER_LEN(k);         \
        if (input) {                                      \
            FOR_ARGS(iii, k, BI)                          \
            {                                             \
                argsSize += ARGS_ALIGN(args[iii].b.size); \
            }                                             \
        } else {                                          \
            FOR_ARGS(iii, k, BO)                          \
            {                                             \
                argsSize += ARGS_ALIGN(args[iii].b.size); \
            }                                             \
        }                                                 \
        ARGS_ALIGN(argsSize);                             \
    })

/** Preallocated value to represent the NULL object reference
    for both local and remote objects. */
static const TIMessageObj gNullTIMessageObj = 0;

/**
 * Copy an object received from the remote domain. The object can be either
 * local or remote.
 * If the object is local, we retrieve it from the object table (which will
 * retain it).
 * If the object is remote, we create a new object wrapper around it.
 *
 * Note that we don't keep track of remote object in any structured way: we
 * give them suitable clothing in the form of an invoke function and a context,
 * and let them live their life.
 * So if we receive the same object twice, for us they are 2 separate objects,
 * each one with its independent lifecycle.
 * */
static int32_t copyObjectFromRemote(TI* ti, ObjectTable* ot, TIMessageObj rObj, Object* obj)
{
    if (gNullTIMessageObj == rObj) {
        *obj = Object_NULL;
    } else if (isRemoteInDestDomain(rObj)) {
        /* Remote object */
        int32_t ret = TI_newRemoteObject(ti, HANDLE_TO_INDEX(rObj), obj);
        if (Object_isERROR(ret)) {
            TI_LOG_ERR(TI_MSG_ERR_CPY_FROM_REMOTE_ROBJ, rObj, ret);
            return ret;
        }
    } else {
        /* Local object */
        *obj = ObjectTable_recoverObject(ot, HANDLE_TO_INDEX(rObj));
        if (Object_isNull(*obj)) {
            TI_LOG_ERR(TI_MSG_ERR_CPY_FROM_REMOTE, rObj);
            return Object_ERROR_BADOBJ;
        }
    }

    return Object_OK;
}

/**
 * Copy an object to the remote domain. The object can be either local or remote.
 * If the object is remote, we simply pass the remote handle and selectively
 * retain it (see below the usage of outbound).
 * If the object is local, we simply add it to the OT (which retains it) and pass
 * the resulting handle.
 *
 * When going outbound (i.e. outbound_marshalIn), this is an OI, which therefore
 * we want to retain as part of the invocation (even if it's a remote obj).
 * On the other end, when returning from inbound (i.e. inbound_marshalOut),
 * we're simply copying the handle into the message, and the operation of
 * "copying" in and by itself creates a new reference. Hence we don't need
 * to retain the remote object further.
 * As for local objects, in order to copy them into a message, we need to obtain
 * a handle for them, i.e. add them to the OT. The operation of adding them to
 * the table generates a handle and correspondingly retain them. The handle is
 * then inserted into the message and IS the new reference (to which the retain
 * done on OT insertion corresponds, for bookkeeping).
 * I.e. local objects are always retained (OT), while remote objects are retained
 * only when exiting in an outbound invocation.
 * In both cases, this function adds a new reference to the object.
 * */
static int32_t copyObjectToRemote(ObjectTable* ot, Object obj, TIMessageObj* rObj, bool outbound)
{
    size_t index;

    if (Object_isNull(obj)) {
        *rObj = gNullTIMessageObj;
    } else if (TI_isRemoteObj(obj, &index)) {
        /* Remote object */
        if (outbound) {
            Object_retain(obj);
        }

        *rObj = markLocalInDestDomain(INDEX_TO_HANDLE(index));
    } else {
        /* Local object, add it to the object table */
        int32_t retval = ObjectTable_addObject(ot, obj, &index);
        if (Object_isERROR(retval)) {
            TI_LOG_ERR(TI_MSG_ERR_CPY_TO_REMOTE, retval);
            return retval;
        }

        *rObj = markRemoteInDestDomain(INDEX_TO_HANDLE(index));
    }

    return Object_OK;
}

/**
 * Get the buffer associated with an argument.
 * Buffers are in the message after the message header and the array of arguments.
 *
 * 0-length buffers are mapped onto an INVALID_POINTER.
 * In this case the buffer is valid, but cannot be accessed (the callee has to check
 * the buffer size).
 * See the comments in kprocess.c for a more complete explanation of this
 * behaviour and its assumptions.
 */
static void* getTIMessageBuffer(const TIMessageBuf* msgArg, ObjectCounts k, const void* msg,
                                size_t msgLen)
{
    size_t headerLen = GET_HEADER_LEN(k);

    if (msgArg->size == 0) {
        return INVALID_POINTER;
    }

    if ((msgArg->offset > msgLen) || (msgArg->offset < headerLen)) {
        TI_LOG_ERR(TI_MSG_ERR_MALFORMED_MGS_0, msgArg->offset, msgLen, headerLen);
        return NULL;
    }

    if (msgArg->size > msgLen - msgArg->offset) {
        TI_LOG_ERR(TI_MSG_ERR_MALFORMED_MGS_1, msgArg->size, msgLen, msgArg->offset);
        return NULL;
    }

    void* data = (void*)((uintptr_t)msg + msgArg->offset);

    if (!IS_ALIGNED(data, TI_MESSAGE_ALIGN_BYTES)) {
        TI_LOG_ERR(TI_MSG_ERR_UNALIGNED_BUF, data);
        return NULL;
    }

    return data;
}

/**
 * Construct a BBuf starting at the buffer portion of a TIMessage.
 * (ie, past the headers and args array)
 *
 * */
static void newBbufForMessageBuffers(BBuf* bbufOut, TIMessage* msg, size_t msgLen, ObjectCounts k)
{
    size_t headerLen = GET_HEADER_LEN(k);
    size_t bufRegionLen = msgLen - headerLen;

    /* Start of the buffer portion, following args array */
    uintptr_t bufRegion = ((uintptr_t)msg + headerLen);

    BBuf_construct(bbufOut, (void*)bufRegion, bufRegionLen);
}

/**
 * Marshal objects and buffers in from the remote domain for an inbound invocation.
 *
 * A remote object is created for any new remote objects received, while local objects
 * are recovered from our OT.
 *
 * Input buffer arguments are made to point to the respective buffers in the
 * received input message. Similarly, output buffers point to the output message.
 * */
static int32_t inbound_marshalIn(TI* ti, ObjectTable* ot, ObjectCounts k, TIMessageArg* msgArgs,
                                 const TIMessage* inputMsg, size_t inputMsgLen,
                                 TIMessage* outputMsg, size_t outputMsgLen, ObjectArg* args)
{
    size_t ii;
    int32_t ret = Object_OK;

    /* Ensure casts to uint32_t are safe */
    if (outputMsgLen > UINT32_MAX) {
        TI_LOG_ERR(TI_MSG_ERR_SIZE_OVERFLOW_4);
        return Object_ERROR_MAXDATA;
    }

    BBuf outMsgBbuf;
    newBbufForMessageBuffers(&outMsgBbuf, outputMsg, outputMsgLen, k);

    /* Locate input buffers from input message */
    FOR_ARGS(ii, k, BI)
    {
        void* data = getTIMessageBuffer(&msgArgs[ii].b, k, inputMsg, inputMsgLen);

        if (NULL == data) {
            return Object_ERROR_UNAVAIL;
        }

        args[ii].b = (ObjectBuf){data, msgArgs[ii].b.size};
    }

    /* Locate output buffers from output message */
    FOR_ARGS(ii, k, BO)
    {
        void* dst = BBuf_alloc(&outMsgBbuf, msgArgs[ii].b.size);

        if (NULL == dst) {
            return Object_ERROR_MAXDATA;
        }

        args[ii].b = (ObjectBuf){dst, msgArgs[ii].b.size};

        /* Update msgArgs with BO offset info.
           This is used later for the output message. */
        msgArgs[ii].b.offset = (uint32_t)((uintptr_t)dst - (uintptr_t)outputMsg);
    }

    FOR_ARGS(ii, k, OI)
    {
        ret = copyObjectFromRemote(ti, ot, msgArgs[ii].o, &args[ii].o);
        if (Object_isERROR(ret)) {
            break;
        }
    }

    return ret;
}

/**
 * Marshal objects and data back to the remote domain after the inbound invocation.
 *
 * Remote objects are replaced by their handle, local objects
 * are added to the OT and replaced by their handle in the table, output buffers
 * are already in place, they only need to be updated with their effective returned
 * size.
 *
 * OO references are "copied" from args into msgArgs in case of success, so
 * args need to be released afterwards.
 * In case of failure, no reference is copied.
 * The caller is therefore always supposed to release OO args, in case of success
 * since the reference has been copied to msgArgs, in case of failure since the
 * reference cannot be transferred to the other domain.
 * */
static int32_t inbound_marshalOut(ObjectTable* ot, ObjectCounts k, TIMessageArg* msgArgs,
                                  const ObjectArg* args)
{
    size_t ii = 0;
    int32_t retval = Object_OK;

    FOR_ARGS(ii, k, BO)
    {
        /* Ensure casts to uint32_t are safe.
           Note: The remote caller will see this error,
           even if the local callee is the one who caused it.
           Note: This error should never be hit if Mink IPC
           is doing its job (as mentioned just below).
        */
        if (args[ii].b.size > UINT32_MAX) {
            TI_LOG_ERR(TI_MSG_ERR_SIZE_OVERFLOW_1);
            return Object_ERROR_MAXDATA;
        }

        /* Mink guarantees a BO returned size is valid, i.e. <= the buffer
         * actual size */
        if ((uint32_t)args[ii].b.size < msgArgs[ii].b.size) {
            msgArgs[ii].b.size = (uint32_t)args[ii].b.size;
        }
    }

    FOR_ARGS(ii, k, OO)
    {
        retval = copyObjectToRemote(ot, args[ii].o, &msgArgs[ii].o, false);
        if (Object_isERROR(retval)) {
            break;
        }
    }

    if (Object_isERROR(retval)) {
        /* Error, release all OO added so far */
        for (size_t jj = ObjectCounts_indexOO(k); jj < ii; jj++) {
            if (Object_isNull(args[jj].o)) {
                continue;
            }

            if (isLocalInDestDomain(msgArgs[jj].o)) {
                /* Nothing to do, "destroying" the message automatically removes
                 * the
                 * logical reference. */
            } else {
                /* Local object, release from table to account for the msgArg
                 * being ditched */
                ObjectTable_releaseHandle(ot, HANDLE_TO_INDEX(msgArgs[jj].o));
            }
        }
    }

    return retval;
}

/**
 * Marshal objects and buffers to the remote domain during an outbound
 * invocation.
 *
 * Serializes objects and buffers into a single shared buffer.
 *
 * Converts local invokable objects into remote objects.
 * Deep copy input buffers on the buffer portion of the shared message.
 */
static int outbound_marshalIn(ObjectTable* ot, ObjectCounts k, const ObjectArg* args,
                              TIMessageArg* msgArgs, TIMessage* msg, size_t msgLen)
{
    int retval = Object_OK;
    size_t ii = 0;

    BBuf msgBbuf;
    newBbufForMessageBuffers(&msgBbuf, msg, msgLen, k);

    /* Deep copy content for BI */
    FOR_ARGS(ii, k, BI)
    {
        /* Ensure casts to uint32_t are safe */
        if (args[ii].b.size > UINT32_MAX) {
            TI_LOG_ERR(TI_MSG_ERR_SIZE_OVERFLOW_2);
            return Object_ERROR_MAXDATA;
        }

        void* dst = BBuf_alloc(&msgBbuf, args[ii].b.size);

        if (NULL == dst) {
            return Object_ERROR_MAXDATA;
        }

        if (args[ii].b.size && !args[ii].b.ptr) {
            return Object_ERROR;
        }

        memscpy(dst, args[ii].b.size, args[ii].b.ptr, args[ii].b.size);

        msgArgs[ii].b.size = (uint32_t)args[ii].b.size;
        msgArgs[ii].b.offset = (uint32_t)((uintptr_t)dst - (uintptr_t)msg);
    }

    /* Only give size information for BO */
    FOR_ARGS(ii, k, BO)
    {
        /* Ensure casts to uint32_t are safe */
        if (args[ii].b.size > UINT32_MAX) {
            TI_LOG_ERR(TI_MSG_ERR_SIZE_OVERFLOW_3);
            return Object_ERROR_MAXDATA;
        }

        msgArgs[ii].b.size = (uint32_t)args[ii].b.size;
        msgArgs[ii].b.offset = 0;
    }

    FOR_ARGS(ii, k, OI)
    {
        retval = copyObjectToRemote(ot, args[ii].o, &msgArgs[ii].o, true);
        if (Object_isERROR(retval)) {
            /* Error already logged */
            break;
        }
    }

    if (Object_isERROR(retval)) {
        /* cleanup after ourselves */
        size_t jj = 0;
        for (jj = ObjectCounts_indexOI(k); jj < ii; jj++) {
            size_t index;
            if (TI_isRemoteObj(args[jj].o, &index)) {
                Object_release(args[jj].o);
            } else if (!Object_isNull(args[jj].o)) {
                /* Object has been inserted in the OT, but the corresponding reference
                 * has not been passed to HLOS, hence remove it from the OT. */
                ObjectTable_releaseHandle(ot, HANDLE_TO_INDEX(msgArgs[jj].o));
            }
        }
    }

    return retval;
}

/**
 * Marshal objects and buffers back into the local domain.
 *
 * Deserializes objects and buffers from a single shared buffer back
 * into multiple output buffers.
 *
 * Converts remote objects into invokable callback objects and
 * recover our own objects from the OT.
 */
static int outbound_marshalOut(TI* ti, ObjectTable* ot, ObjectCounts k, ObjectArg* args,
                               const TIMessageArg* msgArgs, const TIMessage* msg, size_t msgLen)
{
    size_t ii = 0;
    int32_t retval = Object_OK;

    FOR_ARGS(ii, k, BO)
    {
        void* data = getTIMessageBuffer(&msgArgs[ii].b, k, msg, msgLen);

        if (NULL == data) {
            return Object_ERROR_UNAVAIL;
        }

        if (msgArgs[ii].b.size > args[ii].b.size) {
            return Object_ERROR_MAXDATA;
        }

        args[ii].b.size = memscpy(args[ii].b.ptr, args[ii].b.size, data, msgArgs[ii].b.size);
    }

    FOR_ARGS(ii, k, OO)
    {
        retval = copyObjectFromRemote(ti, ot, msgArgs[ii].o, &args[ii].o);
        if (Object_isERROR(retval)) {
            break;
        }
    }

    if (Object_isERROR(retval)) {
        /* Cleanup after ourselves. */
        for (size_t jj = ObjectCounts_indexOO(k); jj < ii; jj++) {
            Object_RELEASE_IF(args[jj].o);
        }
    }

    return retval;
}

/**
 * Prepare an invocation into the remote domain.
 * Allocates and constructs input (outbound) message.
 * Gives the length of the corresponding output message.
 * */
int32_t TIMessage_newOutbound(uint32_t h, ObjectOp op, ObjectArg* args, ObjectCounts k, TI* ti,
                              ObjectTable* ot, TIMessage** inputMsg_p, size_t* inputMsgLen,
                              size_t* outputMsgLen)
{
    *inputMsgLen = GET_MESSAGE_LEN(k, args, true);
    *outputMsgLen = GET_MESSAGE_LEN(k, args, false);

    TIMessage* msgIn = (TIMessage*)heap_zalloc(*inputMsgLen);
    if (!msgIn) {
        TI_LOG_ERR(TI_MSG_ERR_NEWOUTB_MALLOC, *inputMsgLen);
        return Object_ERROR_MEM;
    }

    msgIn->result = Object_ERROR;
    msgIn->cxt = INDEX_TO_HANDLE(h);
    msgIn->op = op;
    msgIn->counts = k;

    int32_t rv = outbound_marshalIn(ot, k, args, msgIn->args, msgIn, *inputMsgLen);

    if (Object_isERROR(rv)) {
        HEAP_FREE_PTR(msgIn);
    } else {
        *inputMsg_p = msgIn;
    }

    return rv;
}

/**
 * Process the response of an outbound invocation.
 * */
int32_t TIMessage_processResponse(const TIMessage* msg, size_t msgLen, ObjectArg* args,
                                  ObjectCounts k, TI* ti, ObjectTable* ot)
{
    /* Was the invocation successful? */
    if (Object_isERROR(msg->result)) {
        return msg->result;
    }

    return outbound_marshalOut(ti, ot, k, args, msg->args, msg, msgLen);
}

/**
 * Release local input objects.
 * */
void TIMessage_releaseLocalOI(const TIMessage* msg, size_t msgLen, ObjectTable* ot)
{
    size_t ii = 0;

    FOR_ARGS(ii, msg->counts, OI)
    {
        TIMessageObj handle = msg->args[ii].o;
        if (isRemoteInDestDomain(handle)) {
            ObjectTable_releaseHandle(ot, HANDLE_TO_INDEX(handle));
        }
    }
}

/**
 * Release remote input objects.
 * */
void TIMessage_releaseRemoteOI(ObjectArg* args, ObjectCounts k)
{
    size_t ii = 0;

    FOR_ARGS(ii, k, OI)
    {
        size_t index;
        if (TI_isRemoteObj(args[ii].o, &index)) {
            Object_release(args[ii].o);
        }
    }
}

/**
 * Inbound invocation handler.
 * Allocates and constructs the output message.
 * */
int32_t TIMessage_handleInbound(const TIMessage* inputMsg, size_t inputMsgLen, TI* ti,
                                ObjectTable* ot, TIMessage** outputMsg_p, size_t* outputMsg_lenout)
{
    size_t ii = 0;
    int32_t retval = Object_OK;
    Object o = Object_NULL;

    const ObjectOp op = inputMsg->op;
    const ObjectCounts k = inputMsg->counts;
    const TIMessageObj cxt = inputMsg->cxt;

    ObjectArg* args = NULL;
    TIMessageArg* msgArgs = NULL;

    TIMessage* outputMsg = NULL;
    size_t outputMsgLen = 0;

    /* Is inputMsg large enough to contain at least
       the message header and args array? */
    if (inputMsgLen < GET_HEADER_LEN(k)) {
        TI_LOG_ERR(TI_MSG_ERR_MGSLEN_TOO_SHORT, inputMsgLen, k);
        return Object_ERROR_UNAVAIL;
    }

    if (ObjectCounts_total(k)) {
        args = HEAP_ZALLOC_ARRAY(ObjectArg, ObjectCounts_total(k));
        msgArgs = HEAP_ZALLOC_ARRAY(TIMessageArg, ObjectCounts_total(k));
        if (!args || !msgArgs) {
            TI_LOG_ERR(TI_MSG_ERR_NO_ARGS_MEM, k);
            retval = Object_ERROR_MEM;
            goto exit;
        }

        memscpy(msgArgs, sizeof(TIMessageArg) * ObjectCounts_total(k), inputMsg->args,
                inputMsgLen - offsetof(struct TIMessage, args));
    }

    /* Construct output buffer
       (GET_MESSAGE_LEN is fine with k=0 and msgArgs=NULL) */
    outputMsgLen = GET_MESSAGE_LEN(k, msgArgs, false);
    outputMsg = (TIMessage*)heap_zalloc(outputMsgLen);
    if (!outputMsg) {
        TI_LOG_ERR(TI_MSG_ERR_INBOUND_OUTMSG_ALLOC, outputMsgLen);
        retval = Object_ERROR_MEM;
        goto exit;
    }

    *outputMsg_p = outputMsg;

    /* Minimum output size, will be updated later. */
    *outputMsg_lenout = GET_HEADER_LEN(k);

    if (ObjectOp_isLocal(op)) {
        /* ObjectTable will forward release to object when appropriate. */
        if (Object_OP_release == op) {
            ObjectTable_releaseHandle(ot, HANDLE_TO_INDEX(cxt));
            retval = outputMsg->result = Object_OK;
            goto exit;
        } else {
            /* Remote domain is expected to maintain local reference counting.
             * We do not process cross-domain retain requests.
             * Nor do we process any other local ops */
            TI_LOG_ERR(TI_MSG_ERR_ERROR_INBOUND_RETAIN);
            retval = Object_ERROR_BADOBJ;
            goto exit;
        }
    }

    o = ObjectTable_recoverObject(ot, HANDLE_TO_INDEX(cxt));

    if (Object_isNull(o)) {
        /* If we fail here, args will contain only a string of Object_NULL.
         * So it's safe to release them (no-op). */
        TI_LOG_ERR(TI_MSG_ERR_ERROR_RECOVER_TARGET, cxt);
        retval = Object_ERROR_BADOBJ;
        goto bail;
    }

    /* This check is redundant, but stops the compiler firing a warning when expanding FOR_ARGS */
    if (args && msgArgs) {
        retval = inbound_marshalIn(ti, ot, k, msgArgs, inputMsg, inputMsgLen, outputMsg,
                                   outputMsgLen, args);
        if (Object_isERROR(retval)) {
            /* If we fail here, args will contain only valid OI from our OT, valid OI from
             * the remote OT or Object_NULL.
             * So it's safe to release them. */
            goto bail;
        }
    }

    /* At this point, both args and msgArgs contain pointers
       into inputMsg for BIs, and pointers into outputMsg for BOs. */

    if (Object_OK != (outputMsg->result = Object_invoke(o, op, args, k))) {
        /* If we bail here, the content of OO in args is unreliable and must NOT
         * be released. OI are safe to release.
         * Invocation failed, but transport still okay, so we return Object_OK.
         * At this point, outputMsg_lenout only contains the TIMessage header.
         */
        retval = Object_OK;
        goto bail;
    }

    /* Output message now contains data past the header */
    *outputMsg_lenout = outputMsgLen;

    /* From this moment on, we can assume the whole content of args is ok
     * so we need to release on exit both OO and OI */

    /* This check is redundant, but stops the compiler firing a warning when expanding FOR_ARGS */
    if (args && msgArgs) {
        retval = inbound_marshalOut(ot, k, msgArgs, args);

        /* Copy back the args array */
        memscpy(outputMsg->args, outputMsgLen - offsetof(struct TIMessage, args), msgArgs,
                sizeof(TIMessageArg) * ObjectCounts_total(k));

        /* If inbound_marshalOut() succeeded, a new reference for all OO has
         * been inserted in msgArgs[]. We therefore need to release them in args[],
         * to signify that the reference has indeed been transferred from args[] to msgArgs[].
         *
         * If inbound_marshalOut() failed, though, we STILL need to release all
         * OO. inbound_marshalOut() failed, but the operation still succeeded on
         * the target of invocation, which passed out all OO properly referenced.
         * But this result will not reach the original caller (marshalling failed).
         * So the best we can do to revert the operation carried out by the target
         * of invocation is to releae the objects resulting from such invocation.
         * */
        FOR_ARGS(ii, k, OO)
        {
            Object_RELEASE_IF(args[ii].o);
        }
    }

bail:

    if (args) {
        /* Object lifetime should be the duration of a call. */
        FOR_ARGS(ii, k, OI)
        {
            Object_RELEASE_IF(args[ii].o);
        }
    }

    Object_RELEASE_IF(o);

exit:
    HEAP_FREE_PTR_IF(args);
    HEAP_FREE_PTR_IF(msgArgs);

    if (Object_isERROR(retval)) {
        HEAP_FREE_PTR_IF(outputMsg);
    }

    return retval;
}
