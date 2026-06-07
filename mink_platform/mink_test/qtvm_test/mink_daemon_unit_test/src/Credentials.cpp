// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifdef __cplusplus
extern "C" {
#endif

#include "Credentials.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ICredentials_invoke.h"
#include "IIO_invoke.h"
#include "MDScan.h"
#include "heap.h"
#include "KMemPort.h"
#include "MemSCpy.h"
#include "object.h"
#include "TUtils.h"

#ifdef __cplusplus
} /* end extern "C" */
#endif


/* This is a simple IIO object, it only stores a buffer representing
 * the client credentials.
 * */
struct Credentials {
    int32_t refs;
    uint8_t *buffer;
    size_t bufferLen;
    int32_t retValue;
    bool copy;
    size_t callCount;
};

// Definition of supported information keys
enum {
    // Sideloaded keys
    eProcessId = ('p' << 16 | 'i' << 8 | 'd'),  // Process ID
};

Credentials *Credentials_new(void *buffer, size_t bufferLen, bool doCopy)
{
    Credentials *me = HEAP_ZALLOC_TYPE(Credentials);
    if (!me) {
        return NULL;
    }

    me->refs = 1;
    me->bufferLen = bufferLen;
    me->retValue = Object_OK;
    if (doCopy) {
        me->copy = true;
        me->buffer = (uint8_t *)heap_zalloc(bufferLen);
        if (!me->buffer) {
            HEAP_FREE_PTR(me);
            return NULL;
        }
        memscpy(me->buffer, me->bufferLen, buffer, bufferLen);
    } else {
        me->buffer = (uint8_t *)buffer;
        me->copy = false;
    }

    return me;
}

int32_t Credentials_release(Credentials *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        if (me->copy) {
            heap_free(me->buffer);
        }
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

void Credentials_setReturnValue(Credentials *me, int32_t retValue)
{
    me->retValue = retValue;
}

static int32_t Credentials_retain(Credentials *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t Credentials_readAtOffset(Credentials *me, uint32_t offset,
                                        void *value, size_t valueLen,
                                        size_t *valueLenOut)
{
    me->callCount++;
    if (Object_isERROR(me->retValue)) {
        return me->retValue;
    }

    *valueLenOut =
        memscpy(value, valueLen, me->buffer + offset, me->bufferLen - offset);

    return Object_OK;
}

static int32_t Credentials_getLength(Credentials *me, uint64_t *len_ptr)
{
    if (Object_isERROR(me->retValue)) {
        return me->retValue;
    }

    *len_ptr = me->bufferLen;
    return Object_OK;
}

static int32_t Credentials_writeAtOffset(Credentials *me, uint64_t offset_val,
                                         void const *data_ptr, size_t data_len)
{
    (void)me;
    (void)offset_val;
    (void)data_ptr;
    (void)data_len;
    return Object_ERROR_INVALID;
}

static IIO_DEFINE_INVOKE(IO_invoke, Credentials_, Credentials *)

Object Credentials_asIIO(Credentials *me)
{
    return (Object){IO_invoke, me};
}

static int32_t Credentials_getPropertyByIndex(Credentials *me,
                                              uint32_t index_val,
                                              void *name_ptr, size_t name_len,
                                              size_t *name_lenout,
                                              void *value_ptr, size_t value_len,
                                              size_t *value_lenout)
{
    /* Currently unsupported */
    return ICredentials_ERROR_NOT_FOUND;
}

/**
 * Convert a 1-4 letter string into a ID by using the character code shifted
 * into a 32bit value based on the characters position.
 *
 * @param key String to convert
 * @param length Length of key
 * @return A positivt integer or 0 on failure
 */
static inline unsigned int str_key(const char *key, size_t length)
{
    if (0 == length || sizeof(unsigned int) < length) {
        return 0;
    }

    size_t idx = 0;
    uint32_t key_val = 0;

    while (key[idx] && idx < length) {
        key_val <<= 8;

        key_val |= key[idx++];
    };

    return key_val;
}

static int32_t Credentials_getValueByName(Credentials *me, const void *name,
                                          size_t nameLen, void *value,
                                          size_t valueLen, size_t *valueLenOut)
{
    MDScan md;

    MDScan_init(&md, (char *)me->buffer, me->bufferLen);

    while (MDScan_next(&md)) {
        if (0 == tmemscmp(md.name.ptr, md.name.len, name, nameLen)) {
            // This is the requested property
            LOG_MSG("Match found! %.*s = %.*s", (int)md.name.len,
                 (char *)md.name.ptr, (int)nameLen, (char *)name);
            size_t valueTotal = 0;
            switch (str_key((char *)name, nameLen)) {
                case eProcessId:  // Process ID
                {
                    return MDScan_readU32(&md, (uint32_t *)value) ? Object_OK
                                                              : Object_ERROR;
                }
                default: {

                    // obviously, callerId should be got by readInt
                    if (0 == tmemscmp(md.name.ptr, md.name.len, "callerId", strlen("callerId"))) {
                        return MDScan_readU32(&md, (uint32_t *)value) ? Object_OK : Object_ERROR;
                    }

                    valueTotal = MDScan_readString(&md, (char *)value, valueLen);
                }
            }

            if (valueTotal > valueLen) {
                return ICredentials_ERROR_VALUE_SIZE;
            }

            *valueLenOut = valueTotal;
            return Object_OK;
        }
    }

    return ICredentials_ERROR_NOT_FOUND;
}

static ICredentials_DEFINE_INVOKE(Credentials_invoke, Credentials_,
                                  Credentials *)

Object Credentials_asICredentials(Credentials *me)
{
    return (Object){Credentials_invoke, me};
}

size_t Credentials_getCallCount(Credentials const *me)
{
    return me->callCount;
}
