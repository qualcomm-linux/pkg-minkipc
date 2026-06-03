// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TICRYPTO_H__
#define __TICRYPTO_H__

#include <stdbool.h>
#include <stdint.h>
#include "TIMessage.h"

/* Values chosen for AES-128 */
#define TI_KEY_LEN_BYTES 16
#define TI_IV_LEN_BYTES 12
#define TI_TAG_LEN_BYTES 16

typedef struct TIPacketHeader {
    uint64_t counter;
    uint8_t iv[TI_IV_LEN_BYTES];
} TIPacketHeader;

typedef struct TIPacket {
    TIPacketHeader header;
    uint8_t tag[TI_TAG_LEN_BYTES];
    TIMessage message;  // Variable length
} TIPacket;

/* Allocate and construct a new TIPacket given a plaintext TIMessage as input.
   Updates message counter, encrypts message, generates a tag.
   Also allocates the corresponding output packet. */
int32_t TICrypto_newPackets(uint8_t* key, size_t keyLen, uint64_t* counterSession, bool isHost,
                            const TIMessage* msgIn, size_t msgInLen, size_t msgOutLen,
                            TIPacket** packetIn, size_t* packetInLen, TIPacket** packetOut,
                            size_t* packetOutLen);

/* Allocate and construct a new plaintext TIMessage given a TIPacket.
   Updates message counter, decrypts message, validates tag. */
int32_t TICrypto_newMessage(uint8_t* key, size_t keyLen, uint64_t* counterSession,
                            const TIPacket* input, size_t inputLen, bool untrustedPacket,
                            TIMessage** output, size_t* outputLen);

/* Construct a TIPacket given a plaintext TIMessage as input.
   Updates message counter, encrypts message, generates a tag.
   TIPacket buffer is given by the caller. */
int32_t TICrypto_encrypt(uint8_t* key, size_t keyLen, uint64_t* counterSession, bool isHost,
                         const TIMessage* input, size_t inputLen, TIPacket* output,
                         size_t outputLen, size_t* outputLenOut, bool untrustedPacket);

/* Port implemented functions:
   (these call into a crypto library such as UCLIB or Open SSL) */

/* Do authenticated encryption with AES-128-GCM.
   'encrypt' should be set to true for encryption, false for decryption.
   'aad' is additional authenticated data (used for mac, not for cipher). */
int32_t TICrypto_port_doGCM(bool encrypt, const uint8_t* key, size_t keyLen, const uint8_t* iv,
                            size_t ivLen, const uint8_t* tagIn, size_t tagInLen, uint8_t* tagOut,
                            size_t tagOutLen, const uint8_t* in, size_t inLen, const uint8_t* aad,
                            size_t aadLen, uint8_t* out, size_t outLen);

#endif  // __TICRYPTO_H__
