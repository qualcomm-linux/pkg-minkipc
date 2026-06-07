// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
// elffile.c
//
// This implementation makes use of C macros to achieve something akin to
// C++ templates.  The template file (ElfFileXX.tmpl.c) is included multiple
// times to define multiple functions.  For example:
//
//    ElfFileXX_func()   This is how function names are defined and called
//                       within the template source file (ElfFileXX.tmpl.c).
//
//    ElfFile32_func()   This is what the name expands to when the template
//    source
//                       is included with "XX" defined as "32".
//
// Outside of template code, ElfFileXX_func(ef, ...) can be used to call the
// template instance that corresponds to the "bitness" of `ef`.
//
#include "ElfFile.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Elf.h"
#include "TUtils.h"

#define satAdd(a, b) ((a) + (b))
#define satMul(a, b) ((a) * (b))

/**
 * Zero a value (set all its bytes to zero).
 */
#define C_ZERO(value) memset(&(value), 0, sizeof(value))

// Convert a ELF image VA value to a memory address after validating that
// the specified range is accessible.
//
// Each me->ranges[] entry describes a valid memory range in terms of
// unadjusted virtual addresses, which correspond to the `vaddr` values
// present in the ELF data structures.  These are adjusted to absolute
// addresses by adding me->base.
//
// On entry:
//    offset = ELF image vaddr (NOT adjusted for base)
//    size = the number of bytes that must be accessible
// On exit:
//    NULL => the range was not valid
//    other => pointer to memory
//
static void *ElfImage_getAddr(ElfImage *me, uintptr_t offset, size_t size)
{
    uintptr_t end = satAdd(offset, size);
    for (size_t i = 0; i < me->rangeLen; ++i) {
        if (offset >= me->ranges[i].start && end <= me->ranges[i].end) {
            return (void *)(me->base + offset);
        }
    }

    return NULL;
}

// Macros for expanding and concatenating tokens.  Used in templating.
#define XCAT(A, B) A##B
#define CAT(A, B) XCAT(A, B)
#define CAT3(A, B, C) CAT(XCAT(A, B), C)

#define XX 32
#include "ElfFileXX.tmpl.h"
#undef XX

#define XX 64
#include "ElfFileXX.tmpl.h"
#undef XX

// These macros call into a template instance from non-templated code:

#define CALLXX(me, c, m) ((me)->is64 ? CAT3(c, 64_, m) : CAT3(c, 32_, m))

#define ElfFileXX_ctor(e, s) CALLXX(e, ElfFile, ctor)(e, s)
#define ElfFileXX_getSegmentInfo(e, n, p) CALLXX(e, ElfFile, getSegmentInfo)(e, n, p)
#define ElfFileXX_getSectionInfo(e, n, p) CALLXX(e, ElfFile, getSectionInfo)(e, n, p)
#define ElfFileXX_compare(e, o) CALLXX(e, ElfFile, compare)(e, o)
#define ElfImageXX_ctor(e, f, b) CALLXX(e, ElfImage, ctor)(e, f, b)
#define ElfImageXX_getLibraryName(e, n, o) CALLXX(e, ElfImage, getLibraryName)(e, n, o)
#define ElfImageXX_updateRel(e, b, n, s) CALLXX(e, ElfImage, updateRel)(e, b, n, s)
#define ElfImageXX_getSymbol(e, n, i) CALLXX(e, ElfImage, getSymbol)(e, n, i)
#define ElfImageXX_getRelInfo(e, i, o) CALLXX(e, ElfImage, getRelInfo)(e, i, o)

typedef struct {
    char ident[ELFINFO_MAGIC_SIZE];
} ElfMagic;

// "Trusted" members are values that we rely on to avoid undefined behavior.
// These are:
//
//   - Members ending in `Len` specify the length of arrays.
//   - Array pointers (WHEN their `Len` is non-zero) identify memory.
//   - Pointers to single objects (not arrays) identify memory.
//   - `is64` dictates (along with `Len` values) array sizes.
//
// All other values are untrusted.  No matter what they hold, this code must
// avoid undefined behavior.
//
ElfStatus ElfFile_ctor(ElfFile *me, ElfSource *source)
{
    ElfMagic magic;
    ElfStatus retval = ELFFILE_SUCCESS;
    int32_t ret = Object_OK;

    C_ZERO(*me);
    me->source = source;

    // Read magic cookie, copy it out, and then free the data.
    {
        const ElfMagic *pm = NULL;
        retval = ElfSource_getData((void **)(&pm), source, 0, sizeof(ElfMagic));
        if (retval) return retval;
        T_CHECK(pm != NULL);
        magic = *pm;
        ElfSource_freeData(source, pm);
    }

    uint8_t cls = magic.ident[4];
    T_CHECK(0 == strncmp(magic.ident, "\177ELF", 4) &&
            (cls == ELF_CLASS_32 || cls == ELF_CLASS_64));

    me->is64 = (cls == ELF_CLASS_64);
    return ElfFileXX_ctor(me, source);

exit:
    return ret;
}

size_t ElfFile_getSegmentCount(const ElfFile *me)
{
    return me->phNum;
}

ElfStatus ElfFile_getSegmentInfo(const ElfFile *me, size_t ndx, ElfSegment *seg)
{
    return ElfFileXX_getSegmentInfo(me, ndx, seg);
}

size_t ElfFile_getSectionCount(const ElfFile *me)
{
    return me->shNum;
}

ElfStatus ElfFile_getSectionInfo(const ElfFile *me, size_t ndx, ElfSection *sec)
{
    return ElfFileXX_getSectionInfo(me, ndx, sec);
}

ElfStatus ElfFile_compare(const ElfFile *me, const ElfFile *other)
{
    int32_t ret = Object_OK;

    T_CHECK(me->ehSize == other->ehSize);
    T_CHECK(me->entry == other->entry);
    T_CHECK(me->flags == other->flags);
    T_CHECK(me->phOff == other->phOff);
    T_CHECK(me->phentSize == other->phentSize);

    return ElfFileXX_compare(me, other);

exit:
    return ret;
}

void ElfFile_dtor(ElfFile *me)
{
    ElfSource_freeData(me->source, me->pht);
}

// ElfImage follows the rules for trusted/untrusted members as documented in
// ElfFile_ctor(), with the following additions:
//
//  - `base` is trusted because it is used to compute addresses within the
//    ELF image.
//
ElfStatus ElfImage_ctor(ElfImage *me, ElfFile *ef, uintptr_t base)
{
    C_ZERO(*me);
    me->is64 = ef->is64;
    return ElfImageXX_ctor(me, ef, base);
}

void ElfImage_dtor(ElfImage *me)
{
    memset(me, 0, sizeof(ElfImage));
}

uint64_t ElfImage_getLibraryCount(ElfImage *me)
{
    return me->libnum;
}

ElfStatus ElfImage_getLibraryName(ElfImage *me, int ndx, const char **nameOut)
{
    return ElfImageXX_getLibraryName(me, ndx, nameOut);
}

ElfStatus ElfImage_relocate(ElfImage *me, uintptr_t base, ElfSymbol *symInOut)
{
    ElfStatus status = ELFFILE_SUCCESS;
    int32_t ret = Object_OK;

    // Note: me->relIndex is 1-based and indexes into both REL and RELA
    // tables.  See getRelInfo() for more.
    while (1) {
        if (me->relIndex > 0) {
            // Relocation in progress...
            T_GUARD(ElfImageXX_updateRel(me, base, &me->relInfo, symInOut));
        }

        size_t ndx = ++me->relIndex;
        status = ElfImageXX_getRelInfo(me, ndx, &me->relInfo);
        if (ELFFILE_RELOC_OUT_OF_RANGE == status) {
            // Relocation out of range is returned when the end of the
            // relocation information has
            // been reached. Once this occurs the relocation is deemed complete.
            return ELFFILE_SUCCESS;
        }

        T_GUARD(status);

        T_GUARD(ElfImageXX_getSymbol(me, me->relInfo.symIndex, symInOut));

        if (me->relInfo.symIndex && symInOut->needsLookup) {
            return ELFFILE_LOOKUP;
        }
    }

exit:
    return ret;
}

static uint32_t elfHash(const char *name)
{
    uint32_t h = 0, g;

    while (*name != '\0') {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        if (g) {
            h ^= g >> 24;
        }
        h &= (~g);
    }

    return h;
}

ElfStatus ElfImage_findSymbol(ElfImage *me, const char *name, ElfSymbol *symOut)
{
    int32_t ret = Object_OK;

    T_CHECK(me->nbucket != 0);

    // For more detailed information see here:
    //
    //    https://uclibc.org/docs/elf-64-gen.pdf
    //
    // In summary, the dynamic symbol table can be accessed through the
    // use of a hash table, typically in the .hash section. The hash table is
    // an array of Elf64_Word objects as below:
    //
    //    |---------------------|
    //    |       nbucket       |
    //    |---------------------|
    //    |       nchain        |
    //    |---------------------|
    //    |       bucket[0]     |
    //    |         ...         |
    //    | bucket[nbucket - 1] |
    //    |---------------------|
    //    |       chain[0]      |
    //    |         ...         |
    //    |  chain[nchain - 1]  |
    //    |---------------------|
    //
    // The bucket array forms the hash table. The number of entries in the
    // hash table is nbucket
    //
    // The entries in the chain array parallel the symbol table, there is one
    // chain entry for each symbol in the symbol table (nchain equals the number
    // of symbol table entries).
    //
    // Symbols in the hash table are organized in to hash chains, one chain per
    // bucket.
    //
    // size_t hashIndex = 2 + elfHash(name) % me->nbucket;
    uint32_t elf_hash = elfHash(name);
    size_t hashIndex = (2 + elf_hash) % me->nbucket;

    // Limit looping in case there is a cycle in the chain.
    for (size_t limit = me->symLen; limit; --limit) {
        T_CHECK_ERR(hashIndex < me->hashLen, ELFFILE_ERROR);

        uint32_t symIndex = me->hashTable[hashIndex];
        T_CHECK_ERR(symIndex == 0, ELFFILE_ERROR);

        T_GUARD(ElfImageXX_getSymbol(me, symIndex, symOut));
        if (strcmp(symOut->name, name) == 0 && !symOut->needsLookup &&
            symOut->binding != STB_LOCAL) {
            return ELFFILE_SUCCESS;
        }

        // If symbol is not found, go to the chain table to find the next symbol
        // with the same hash value. The index of this symbol is stored in
        // chain[symIndex].
        hashIndex = 2 + me->nbucket + symIndex;
    }

    return ELFFILE_ERROR;

exit:
    return ret;
}
