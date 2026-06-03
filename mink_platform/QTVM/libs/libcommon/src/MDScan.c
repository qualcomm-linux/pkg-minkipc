// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "MDScan.h"
#include <string.h>

//----------------------------------------------------------------
// Utility functions
//----------------------------------------------------------------

#define ZERO_REC(obj) memset(&(obj), 0, sizeof(obj))

#define ZERO_AT(ptr) memset((ptr), 0, sizeof(*(ptr)))

static inline uint32_t _u32SatMul(uint32_t a, uint32_t b)
{
    if (a >= UINT32_MAX / b) {
        return UINT32_MAX;
    }

    return a * b;
}

static inline uint32_t _u32SatAdd(uint32_t a, uint32_t b)
{
    if (a >= UINT32_MAX - b) {
        return UINT32_MAX;
    }

    return a + b;
}

// Evaluate to `true` when val is in the range [smallest ... largest],
// inclusive.
//
#define WITHIN(val, smallest, largest) \
    (((unsigned)(val) >= (unsigned)(smallest)) && ((unsigned)(val) <= (unsigned)(largest)))

// If ch is an upper-case or lower-case ASCII letter, return the lower-case
// form.
//
#define charToLower(ch) ((ch) | 32)

// Note: value not defined for non-hex-digits
//
static int _hexValue(char ch)
{
    return (WITHIN(ch, '0', '9')
                ? ch - '0'
                : WITHIN(charToLower(ch), 'a', 'f') ? charToLower(ch) - 'a' + 10 : -1);
}

static inline int _decValue(char ch)
{
    unsigned u = ((unsigned char)ch) - (unsigned)'0';
    return u <= 9 ? (int)u : -1;
}

// Search for `ch` in start[0..end-1].
//
// Return pointer to occurrence of `ch`, or `end` if not found.
//
static const char *_findCharEnd(const char *start, const char *end, char ch)
{
    while (start < end && *start != ch) {
        ++start;
    }

    return start;
}

//----------------------------------------------------------------
// WBuf is a bounded writable buffer.
//----------------------------------------------------------------

typedef struct {
    char *ptr;
    size_t len;
} WBuf;

// Overwrite a character in the buffer (but never write out of bounds).
static inline void _wBuf_set(WBuf *me, size_t index, char ch)
{
    if (index < me->len) {
        me->ptr[index] = ch;
    }
}

//----------------------------------------------------------------
// MDScan: Metadata parser
//
// Metadata syntax:
//
//  MData :=  ( ";"? Name "="? Value )*
//  Name  :=  [^=;]*
//  Value :=  [^;]*
//
//----------------------------------------------------------------

void MDScan_init(MDScan *me, const char *start, size_t len)
{
    ZERO_AT(me);
    me->pos = start;
    me->end = start + len;
}

bool MDScan_next(MDScan *me)
{
    while (me->pos < me->end) {
        const char *valueEnd = _findCharEnd(me->pos, me->end, ';');
        const char *nameEnd = _findCharEnd(me->pos, valueEnd, '=');
        const char *valueStart = nameEnd;
        if (valueStart < valueEnd) {
            // step over '='
            ++valueStart;
        }

        me->name = (RBuf){me->pos, nameEnd - me->pos};
        me->value = (RBuf){valueStart, valueEnd - valueStart};

        // advance to next entry
        me->pos = (valueEnd < me->end ? valueEnd + 1 : valueEnd);

        // ignore entries with empty names
        if (me->name.len) {
            return true;
        }
    }

    return false;
}

// Write a string value into destPtr[].
//
// Return the number of bytes required for the string (including the null
// terminating character).  Note: this result is independent of the size of
// the destination array.  One can pass a zero-length buffer to measure the
// string.
//
// Zero-termination of the resulting buffer is guaranteed (as long as the
// buffer size is non-zero) by truncating the string to the maximum length.
//
// Note that there might other zero characters might occur *before* the
// final terminating zero.
//
size_t MDScan_readString(MDScan *me, char *destPtr, size_t destLen)
{
    WBuf wb = {destPtr, destLen};
    size_t readIndex = 0;
    size_t writeIndex = 0;

    while (readIndex < me->value.len) {
        char ch = RBuf_get(&me->value, readIndex);
        ++readIndex;
        if (ch == '%') {
            int a = _hexValue(RBuf_get(&me->value, readIndex));
            int b = _hexValue(RBuf_get(&me->value, readIndex + 1));
            if (a >= 0 && b >= 0) {
                ch = (char)(a * 16 + b);
                readIndex += 2;
            }
        }

        _wBuf_set(&wb, writeIndex, ch);
        ++writeIndex;
    }

    _wBuf_set(&wb, writeIndex, '\0');
    ++writeIndex;

    // ensure zero-termination
    _wBuf_set(&wb, destLen - 1, '\0');
    return writeIndex;
}

// Read hex binary value data into destPtr[].
//
// Return the number of bytes required for the binary data. Note: this
// result is independent of the size of the destination array.  One can pass
// a zero-length buffer to measure the string.
//
size_t MDScan_readBytes(MDScan *me, void *destPtr, size_t destSize)
{
    WBuf wb = {(char *)destPtr, destSize};

    size_t writeIndex = 0;
    size_t readIndex = 0;

    for (;;) {
        int a = _hexValue(RBuf_get(&me->value, readIndex++));
        int b = _hexValue(RBuf_get(&me->value, readIndex++));
        if (a < 0 || b < 0) {
            break;
        }

        _wBuf_set(&wb, writeIndex++, (char)(a * 16 + b));
    }

    return writeIndex;
}

// Read uint32_t from value, saturating on overflow.  The number is assumed
// to be decimal.
//
// Returns `false` if a negative number or if an invalid character is seen.
//
bool MDScan_readU32(MDScan *me, uint32_t *numOut)
{
    uint32_t u = 0;
    size_t readIndex;

    for (readIndex = 0; readIndex < me->value.len; ++readIndex) {
        int n = _decValue(RBuf_get(&me->value, readIndex));
        if (n < 0) {
            *numOut = 0;
            return false;
        }

        u = _u32SatMul(u, 10);
        u = _u32SatAdd(u, n);
    }

    *numOut = u;
    return true;
}

//----------------------------------------------------------------
// IDScan: Metadata ID set parser
//----------------------------------------------------------------
//
// Syntax:
//   IDSet   := ( ","? IDEntry )*
//   IDEntry := ID ( ":" Mask )
//   ID      := HexDigit+              An MSB-first numeric vvalue
//   Mask    := HexDigit+              An LSB-first bit mask
//
// ID is a service ID that is present in the set.  It is represented in
// MSB-first hex form.  (Only the least-significant 32-bits are used.)
//
// Mask describes some number of additional IDs that follow ID.  The first
// hex digit represents the next 4 UID values following ID, the next digit
// the next 4, and so on.  Within each digit, the least-significant bits
// represent the lowest IDs.  When a bit is 1, it indicates the
// corresponding ID is present in the set.
//
// Examples:    "10,20,30"   =>   [16, 32, 48]
//              "10:A5"      =>   [16, 18, 20, 21, 23]

void IDScan_init(IDScan *me, MDScan *mdata)
{
    ZERO_AT(me);
    me->prb = &mdata->value;
}

// Get next ID in the ID set.
//
// Return value:
//    true  =>  me->id holds the next ID in the set
//    false =>
//
// In each case where we return `true`, we must make sure that our internal
// state has been updated to reflect "progress" (to avoid looping forever).
//
bool IDScan_next(IDScan *me)
{
    // enumerate bits in mask, if reading a mask

    while (me->maskBits) {
        if (me->maskBits > 1) {
            // examine bits previously obtained from a hex digit

            bool isInSet = me->maskBits & 1;
            me->maskBits >>= 1;
            ++me->id;
            if (isInSet) {
                return true;
            }
        } else {
            // get next mask digit (or end)
            int digit = _hexValue(RBuf_get(me->prb, me->readIndex));
            if (digit >= 0) {
                me->maskBits = digit | 16;
                ++me->readIndex;
            } else {
                me->maskBits = 0;
                break;
            }
        }
    }

    if (RBuf_get(me->prb, me->readIndex) == ',') {
        ++me->readIndex;
    }

    // get next ID (or end)

    size_t index = me->readIndex;
    int digit;
    uint32_t id = 0;
    for (; (digit = _hexValue(RBuf_get(me->prb, index))) >= 0; ++index) {
        id = (id << 4) | digit;
    }

    if (index == me->readIndex) {
        // no ID found
        return false;
    }

    me->id = id;
    if (RBuf_get(me->prb, index) == ':') {
        // ":" initiates mask portion
        ++index;
        me->maskBits = 1;
    }

    me->readIndex = index;
    return true;
}
