// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include "TICrypto.h"
#include "TIErrno.h"
#include "object.h"
//#include "uclib.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include "ssgtzd_logging.h"

int32_t TICrypto_port_doGCM(bool encrypt, const uint8_t* key, size_t keyLen, const uint8_t* iv,
                            size_t ivLen, const uint8_t* tagIn, size_t tagInLen, uint8_t* tagOut,
                            size_t tagOutLen, const uint8_t* in, size_t inLen, const uint8_t* aad,
                            size_t aadLen, uint8_t* out, size_t outLen)
{
    EVP_CIPHER_CTX* ctx;
    int len = 0;
    int err = Object_ERROR;
    unsigned char *specifyAAD = NULL;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        LOGE("Error getting cipher context");
        return Object_ERROR;
    }

    /* Initialise the encryption operation. */
    if (1 != EVP_CipherInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL, encrypt)) {
        LOGE("Error initialising cipher context for EVP_aes_256_gcm");
        goto bail;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, ivLen, NULL)) {
        LOGE("Error setting cipher context for IV Len");
        goto bail;
    }

    /* Initialise key and IV */
    if (1 != EVP_CipherInit_ex(ctx, NULL, NULL, key, iv, encrypt)) {
        LOGE("Error initialising cipher context with keys and IVs");
        goto bail;
    }

    /*
     * Provide any AAD data. This can be called zero or more times as
     * required. (To specify additional authenticated data (AAD), set
     * out parameter to NULL in call to EVP_CipherUpdate()
     */
    if (1 != EVP_CipherUpdate(ctx, specifyAAD, &len, aad, aadLen)) {
        LOGE("Error updating cipher context for AAD");
        goto bail;
    }

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if (1 != EVP_CipherUpdate(ctx, out, &len, in, inLen)) {
        LOGE("Error updating cipher context with input data");
        goto bail;
    }

    outLen = len;

    if (!encrypt) {
        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tagInLen, (void*)tagIn)) {
            LOGE("Error setting final tag for cipher context");
            goto bail;
        }
    }

    /*
     * Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if (1 != EVP_CipherFinal_ex(ctx, out + len, &len)) {
        LOGE("Error finalizing cipher context for crypto");
        goto bail;
    }

    outLen += len;

    if (encrypt) {
        /* Get the tag */
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tagOutLen, tagOut)) {
            LOGE("Error getting final tag for cipher context");
            goto bail;
        }
    }

    err = Object_OK;
bail:
    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return err;
}
