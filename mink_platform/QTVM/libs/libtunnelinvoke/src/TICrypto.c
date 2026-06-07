// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "TICrypto.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../inc/object.h"
#include "TIErrno.h"
#include "TIMessage.h"
#include "heap.h"

static size_t TICrypto_getPacketLen(size_t messageLen)
{
    return messageLen + offsetof(struct TIPacket, message);
}

static size_t TICrypto_getMessageLen(size_t packetLen)
{
    if (packetLen < sizeof(TIPacket)) {
        return 0;
    }

    return packetLen - offsetof(struct TIPacket, message);
}

static int32_t constructIV(uint64_t counter, bool isHost, uint8_t* iv, size_t ivLen)
{
    // Note: We are using a concatenation of the message counter
    //   and the host/client designation as the IV.
    //   This is sufficient as it is always unique (non-repeating).

    uint32_t id = (isHost == true) ? 1 : 0;

    if (sizeof(counter) + sizeof(id) > ivLen) {
        return Object_ERROR;
    }

    memcpy(iv, &counter, sizeof(counter));
    memcpy(iv + sizeof(counter), &id, sizeof(id));
    return Object_OK;
}

int32_t TICrypto_encrypt(uint8_t* key, size_t keyLen, uint64_t* counterSession, bool isHost,
                         const TIMessage* message, size_t messageLen, TIPacket* packet,
                         size_t packetLen, size_t* packetLenOut, bool untrustedPacket)
{
    int32_t rv;

    // Check output buffer length is sufficient
    size_t lenRequired = TICrypto_getPacketLen(messageLen);
    if (packetLen < lenRequired) {
        TI_LOG_ERR(TI_CRYPTO_ERR_ENCRYPT_BAD_INPUT, packetLen, lenRequired);
        return Object_ERROR;
    }

    *packetLenOut = lenRequired;

    // Allocate bounce buffer for non-secure output packet.
    //   (mitigates against TOCTOU race conditions)
    TIPacket* bounceBuf = NULL;
    TIPacket* packetSecure = NULL;
    if (untrustedPacket) {
        bounceBuf = (TIPacket*)heap_zalloc(lenRequired);
        if (!bounceBuf) {
            TI_LOG_ERR(TI_CRYPTO_ERR_ALLOC_BOUNCEBUF_1, lenRequired);
            return Object_ERROR_MEM;
        }

        packetSecure = bounceBuf;
    } else {
        packetSecure = packet;
    }

    // Choose a new counter value
    uint64_t counterNew = ++(*counterSession);
    if (counterNew == 0) {
        TI_LOG_ERR(TI_CRYPTO_ERR_MAX_COUNTER, *counterSession);
        rv = Object_ERROR_MAXREPLAY;
        goto bail;
    }

    packetSecure->header.counter = counterNew;

    // Populate IV
    rv = constructIV(counterNew, isHost, &packetSecure->header.iv[0], TI_IV_LEN_BYTES);
    if (Object_isERROR(rv)) {
        TI_LOG_ERR(TI_CRYPTO_ERR_IV_GEN, rv);
        goto bail;
    }

    // Encrypt message and generate tag
    rv = TICrypto_port_doGCM(true, key, keyLen, &packetSecure->header.iv[0], TI_IV_LEN_BYTES, NULL,
                             0, &packetSecure->tag[0], TI_TAG_LEN_BYTES, (const uint8_t*)message,
                             messageLen, (const uint8_t*)&packetSecure->header,
                             sizeof(TIPacketHeader), (uint8_t*)&packetSecure->message, messageLen);
    if (Object_isERROR(rv)) {
        TI_LOG_ERR(TI_CRYPTO_ERR_ENCRYPT_GCM_FAIL, rv);
        goto bail;
    }

    if (untrustedPacket) {
        memcpy(packet, packetSecure, lenRequired);
    }

bail:
    HEAP_FREE_PTR_IF(bounceBuf);
    return rv;
}

int32_t TICrypto_newPackets(uint8_t* key, size_t keyLen, uint64_t* counterSession, bool isHost,
                            const TIMessage* message, size_t messageLen, size_t msgOutLen,
                            TIPacket** inPacket_p, size_t* inPacketLen_p, TIPacket** outPacket_p,
                            size_t* outPacketLen_p)
{
    int32_t rv;

    // Allocate two new packets
    size_t inPacketLen = TICrypto_getPacketLen(messageLen);
    TIPacket* inPacket = (TIPacket*)heap_zalloc(inPacketLen);

    size_t outPacketLen = TICrypto_getPacketLen(msgOutLen);
    TIPacket* outPacket = (TIPacket*)heap_zalloc(outPacketLen);

    if (!outPacket || !inPacket) {
        TI_LOG_ERR(TI_CRYPTO_ERR_NEWPACKETS_ALLOC, outPacketLen, inPacketLen);
        rv = Object_ERROR_MEM;
        goto bail;
    }

    // Encrypt message into inPacket
    rv = TICrypto_encrypt(key, keyLen, counterSession, isHost, message, messageLen, inPacket,
                          inPacketLen, &inPacketLen, false);

bail:
    if (Object_isERROR(rv)) {
        HEAP_FREE_PTR_IF(inPacket);
        HEAP_FREE_PTR_IF(outPacket);
        return rv;
    }

    // All OK, assign outputs
    *inPacket_p = inPacket;
    *inPacketLen_p = inPacketLen;
    *outPacket_p = outPacket;
    *outPacketLen_p = outPacketLen;

    return Object_OK;
}

int32_t TICrypto_newMessage(uint8_t* key, size_t keyLen, uint64_t* counterSession,
                            const TIPacket* packet, size_t packetLen, bool untrustedPacket,
                            TIMessage** message_p, size_t* messageLen_p)
{
    int32_t rv;
    uint64_t counterMessage = 0;

    // Check input length
    if (packetLen < sizeof(TIPacket)) {
        TI_LOG_ERR(TI_CRYPTO_ERR_NEWMSG_BAD_INPUT, packetLen, sizeof(TIPacket));
        return Object_ERROR;
    }

    // Allocate output TIMessage
    size_t messageLen = TICrypto_getMessageLen(packetLen);
    TIMessage* message = (TIMessage*)heap_zalloc(messageLen);
    if (!message) {
        TI_LOG_ERR(TI_CRYPTO_ERR_NEWMSG_ALLOC, messageLen);
        return Object_ERROR_MEM;
    }

    // Allocate a bounce buffer for non-secure input packet.
    //   (mitigates against TOCTOU race conditions)
    TIPacket* bounceBuf = NULL;
    const TIPacket* packetSecure = NULL;
    if (untrustedPacket) {
        bounceBuf = (TIPacket*)heap_zalloc(packetLen);
        if (!bounceBuf) {
            TI_LOG_ERR(TI_CRYPTO_ERR_ALLOC_BOUNCEBUF_2, packetLen);
            return Object_ERROR_MEM;
        }
        memcpy(bounceBuf, packet, packetLen);
        packetSecure = bounceBuf;
    } else {
        packetSecure = packet;
    }

    // Decrypt message, authenticate tag
    rv = TICrypto_port_doGCM(false, key, keyLen, &packetSecure->header.iv[0], TI_IV_LEN_BYTES,
                             &packetSecure->tag[0], TI_TAG_LEN_BYTES, NULL, 0,
                             (const uint8_t*)&packetSecure->message, messageLen,
                             (const uint8_t*)&packetSecure->header, sizeof(TIPacketHeader),
                             (uint8_t*)message, messageLen);
    if (Object_isERROR(rv)) {
        TI_LOG_ERR(TI_CRYPTO_ERR_NEWMSG_GCM_FAIL, rv);
        goto bail;
    }

    // Check counter values for replay protection
    counterMessage = packetSecure->header.counter;
    if (!(counterMessage > *counterSession)) {
        TI_LOG_ERR(TI_CRYPTO_ERR_REPLAY_CHECK, *counterSession, counterMessage);
        rv = Object_ERROR_REPLAY;
        goto bail;
    }

    // Update session counter
    *counterSession = counterMessage;

bail:
    HEAP_FREE_PTR_IF(bounceBuf);

    if (Object_isERROR(rv)) {
        HEAP_FREE_PTR_IF(message);
        return rv;
    }

    // All OK, populate outputs:
    *message_p = message;
    *messageLen_p = messageLen;

    return Object_OK;
}
