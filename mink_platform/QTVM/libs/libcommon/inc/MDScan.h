
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __MDSCAN_H
#define __MDSCAN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------
// RBuf is a bounded readable buffer.
//----------------------------------------------------------------

typedef struct {
    const char *ptr;
    size_t len;
} RBuf;

static inline char RBuf_get(RBuf *me, size_t index)
{
    if (index < me->len) {
        return me->ptr[index];
    }

    return 0;
}

//----------------------------------------------------------------
// MDScan: Metadata parser
//----------------------------------------------------------------

typedef struct MDScan MDScan;

struct MDScan {
    RBuf name;
    RBuf value;
    const char *pos;
    const char *end;
};

// Initialize MDScan.  start[0...len-1] = the metadata string.
//
void MDScan_init(MDScan *me, const char *start, size_t len);

// Advance to next name/value pair in the metadata string.  Return `true` if
// a new name/value pair is found, `false` if at the end of the metadata.
//
bool MDScan_next(MDScan *me);

// Interpret the metadata value as a string and put that value in
// destPtr[0...destLen-1].
//
size_t MDScan_readString(MDScan *me, char *destPtr, size_t destLen);

// Interpret the metadata value as a raw binary blob (hex-encoded) and put
// that value in destPtr[0...destSize-1].
//
size_t MDScan_readBytes(MDScan *me, void *destPtr, size_t destSize);

// Interpret the metadata value as decimal unsigned integer and put that
// value in *numOut.  Return `true` if a well-formed numeric value was
// found, `false` otherwise.
//
bool MDScan_readU32(MDScan *me, uint32_t *numOut);

bool MDScan_getAppName(const char *md, size_t mdLen, char *namePtr, size_t namePtrLen);

//----------------------------------------------------------------
// IDScan: Metadata ID set parser
//----------------------------------------------------------------

typedef struct IDScan IDScan;

struct IDScan {
    // value of current ID; valid when `next` returns true
    uint32_t id;

    // internal state
    RBuf *prb;
    size_t readIndex;
    unsigned maskBits;
};

// Intialize IDScan to enumerate a set of UIDs from the `value` portion of
// MData.
//
void IDScan_init(IDScan *me, MDScan *mdata);

// Obtain the next UID in the set.
//
// On success, `true` is returned and the `id` field of IDScan contains the
// UID value.
//
// On failure (i.e. end of the set), `false` is returned.
//
bool IDScan_next(IDScan *me);

#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif /* __MDSCAN_H */
