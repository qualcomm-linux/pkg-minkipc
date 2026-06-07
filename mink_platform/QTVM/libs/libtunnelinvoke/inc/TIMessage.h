// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TI_MESSAGE_H__
#define __TI_MESSAGE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ObjectTable.h"
#include "TunnelInvoke.h"
#include "object.h"

/** A reference to an object in a different domain. */
typedef uint32_t TIMessageObj;

/** A structure that describes the location and size of a buffer
    argument in a buffer contained in an TIMessage. */
typedef struct TIMessageBuf {
    /** The offset in bytes from the containing TIMessage where
        the data for a buffer type invoke parameter exists. The result
        of the offset added to the base of the TIMessage must be
        aligned to TI_MESSAGE_ALIGN_BYTES. */
    uint32_t offset;

    /** The size of the buffer argument. */
    uint32_t size;
} TIMessageBuf;

/** A representation of arguments from object invoke adapted for use
    in this implementation of invoke across domains. */
typedef union TIMessageArg {
    TIMessageBuf b;
    TIMessageObj o;
} TIMessageArg;

/** A data structure used to convey object invocation across
    domains. */
typedef struct TIMessage {
    /** Result of the invocation */
    uint32_t result;

    /** The object being invoked. */
    TIMessageObj cxt;

    /** Format is as described by object invoke documentation. */
    ObjectOp op;

    /** Format is as described by object invoke documentation. */
    ObjectCounts counts;

    /** This array's length is determined by the counts element
        above. */
    TIMessageArg args[0];
} TIMessage;

/**
 * Inbound invocation handler.
 * A new TIMessage is allocated for the output message.
 * */
int32_t TIMessage_handleInbound(const TIMessage* inputMsg, size_t inputMsgLen, TI* ti,
                                ObjectTable* ot, TIMessage** outputMsg_p, size_t* outputMsg_lenout);

/**
 * Prepare an invocation into the remote domain.
 * A new TIMessage is allocated.
 * The corresponding output message length is calculated.
 * */
int32_t TIMessage_newOutbound(uint32_t h, ObjectOp op, ObjectArg* args, ObjectCounts k, TI* ti,
                              ObjectTable* ot, TIMessage** inputMsg_p, size_t* inputMsgLen,
                              size_t* outputMsgLen);

/**
 * Process the response of an outbound invocation.
 * */
int32_t TIMessage_processResponse(const TIMessage* msg, size_t msgLen, ObjectArg* args,
                                  ObjectCounts k, TI* ti, ObjectTable* ot);

/**
 * Release local input objects.
 * */
void TIMessage_releaseLocalOI(const TIMessage* msg, size_t msgLen, ObjectTable* ot);

/**
 * Release remote input objects.
 * */
void TIMessage_releaseRemoteOI(ObjectArg* args, ObjectCounts k);

#endif  // __TI_MESSAGE_H__
