// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __ELFFILE_H
#define __ELFFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "object.h"

#include "TUtils.h"

//----------------------------------------------------------------
// ElfSource: Abstract data source for data consumed by ELF parser.
//----------------------------------------------------------------

typedef struct ElfSource ElfSource;

struct ElfSource {
    int32_t (*getData)(void **meOut, ElfSource *cxt, size_t offset, size_t size);
    void (*freeData)(ElfSource *cxt, const void *ptr);
};

enum ElfStatus {
    ELFFILE_SUCCESS,
    ELFFILE_ERROR,
    ELFFILE_LOOKUP,
    ELFFILE_RELOC_OUT_OF_RANGE,
    ELFFILE_MALLOC_FAILURE,
};
typedef enum ElfStatus ElfStatus;

// Get data from the ELF file.  On success, a pointer to
// `size` bytes of memory will be assigned to meOut reference, along with
// success as return.  The caller must call ElfSource_freeData() (using
// the same ElfSource instance) to free the memory.  On failure,  is
// returned.
//
static inline ElfStatus ElfSource_getData(void **meOut, ElfSource *me, size_t offset, size_t size)
{
    int32_t ret = me->getData(meOut, me, offset, size);
    if (ret) {
        if (ret == Object_ERROR_MEM) {
            return ELFFILE_MALLOC_FAILURE;
        }

        return ELFFILE_ERROR;
    }

    return ELFFILE_SUCCESS;
}

// Free a pointer previously returned by ElfSource_getData().  Freeing NULL
// is always safe (and does nothing).
//
static inline void ElfSource_freeData(ElfSource *me, const void *ptr)
{
    if (me != NULL) {
        me->freeData(me, ptr);
    }
}

//----------------------------------------------------------------
// ElfFile
//
// ElfFile facilitates loading of ELF files by parsing the ELF header and
// program header table.  ElfFile reads data from an ELF file and provides
// information necessary for construction of the ELF memory image.  Clients
// of ElfFile do not need to be concerned with most details of the ELF file
// format, or differences between 32-bit and 64-bit ELF formats.
//
// ElfFile is intended to be used by a loader that executes in a different
// domain from the one in which the ELF will execute.  It must guard against
// maliciously constructed ELF files.  Bad ELF files may of course adversely
// affect any domain in which the ELF file's code is executed, but the
// caller of ElfFile shall not be vulnerable.
//
//----------------------------------------------------------------

typedef struct ElfFile {
    // Public members

    bool is64;
    size_t ehSize;     // size of the ELF header (depends on 64/32)
    uint32_t machine;  // ELF Machine type
    uint64_t entry;    // program entry point
    uint32_t flags;    // flags (from ELF header)

    size_t phentSize;  // size of one program header table entry (depends on 64/32)
    uint64_t phOff;    // the offset to the program header table

    size_t shstrndx;  // index of Section header string table

    // Private members (for internal use only)

    const void *pht;  // array of program headers (ElfXX_Phdr[])
    size_t phNum;     // number of entries in pht

    const void *sht;  // array of section headers (ElfXX_Shdr[])
    size_t shNum;     // number of entries in sht

    ElfSource *source;  // data source

} ElfFile;

// Note that this is our internal represenation of the ElfXX_Phdr structure and
// is used to go between code that is code that is not Elf32_ and Elf64_ aware
// and the ELF loader which is.
//
typedef struct ElfSegment {
    uint64_t offset;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t flags;
    uint64_t align;
    uint32_t type;
} ElfSegment;

#define ELFSEGMENT_PT_LOAD 1

// Note that this is our internal represenation of the ElfXX_Phdr structure and
// is used to go between code that is code that is not Elf32_ and Elf64_ aware
// and the ELF loader which is.
//
typedef struct ElfSection {
    uint64_t flags;
    uint64_t offset;
    uint64_t size;
    uint32_t type;
    uint32_t name_offset;
    uint64_t name_addr;
} ElfSection;

// Construct an ElfFile instance.
//
// On entry:
//   data = an ElfSource instance that will be used to read the ELF file contents.
//
ElfStatus ElfFile_ctor(ElfFile *ef, ElfSource *data);

// Return the number of segments in the ELF file.
//
size_t ElfFile_getSegmentCount(const ElfFile *ef);

// Return information on a segment, given its index.
//
ElfStatus ElfFile_getSegmentInfo(const ElfFile *ef, size_t ndx, ElfSegment *seg);

// Return the number of sections in the ELF file.
//
size_t ElfFile_getSectionCount(const ElfFile *ef);

// Return information on a section, given its index.
//
ElfStatus ElfFile_getSectionInfo(const ElfFile *ef, size_t ndx, ElfSection *seg);

// Compare two ElfFiles
// Returns ELFFILE_SUCCESS if identical.
// Any other return value indicates the two ElfFiles differ.
//
ElfStatus ElfFile_compare(const ElfFile *ef, const ElfFile *other);

// Destroy an ElfFile instance.
//
// Note: it shall be safe to call dtor() passing a zero-initialized (never
// constructed) structure.
//
void ElfFile_dtor(ElfFile *ef);

//----------------------------------------------------------------
// ElfImage
//
// ElfImage manages symbol lookup and relocation.  ElfImage operates on an
// ELF memory image after the loadable ("allocable") segments have been
// copied or mapped into memory.
//
// Like ElfFile, ElfImage must protect against maliciously-contructed ELF
// files.
//
// ----------------------------------------------------------------

#define ELFIMAGE_MAX_RANGES 8

// Note: ElfRange values are in terms of "vaddr" values that appear in the
// ELF image, and are not actual memory addresses.  The memory address is
// given by `base + vaddr`.
//
// When end <= start, the range is empty.
//
typedef struct {
    uintptr_t start;
    uintptr_t end;
} ElfRange;

typedef struct {
    char *ptr;        // pointer to ElfXX_Rel or ElfXX_Rela
    bool isRela;      // true => ElfXX_Rela
    size_t symIndex;  // symbol index referenced by this rel entry
} ElfRelInfo;

typedef struct ElfImage {
    bool is64;  // ELF64 vs. ELF32

    // Private members (for internal use only)

    uint64_t libnum;                       // number of dependent libs
    void *dynTable;                        // array of ElfXX_Dynamic entries
    size_t dynLen;                         // number of entries in dynTable
    const char *stringTable;               // string table
    size_t stringLen;                      // number of bytes in the stringTable
    void *symTable;                        // array of ElfXX_Sym entries
    size_t symLen;                         // number of entries in symTable
    uint32_t *hashTable;                   // hash table of symbols in symTable
    size_t hashLen;                        // validated size of hashTable
    size_t nbucket;                        // number of buckets in hash table
    void *relTable;                        // array of ElfXX_Rel entries
    size_t relLen;                         // number of entries in relTable
    void *relaTable;                       // array of ElfXX_Rela entries
    size_t relaLen;                        // number of entries in relaTable
    void *pltrelTable;                     // array of PLT Rel or Rela entries
    size_t pltrelLen;                      // number of entries in PLT pltrelTable
    bool isPltRela;                        // Boolean indicating type of PLT entries
    uintptr_t base;                        // offset applied to vaddrs
    ElfRange ranges[ELFIMAGE_MAX_RANGES];  // validated ranges
    size_t rangeLen;                       // number of entries populated in ranges[]
    size_t relIndex;                       // pending relocation index (0 => not started)
    ElfRelInfo relInfo;                    // description of pending relocation
    void *relroSegmentStart;               // Virtual address for start of GNU_RELRO segment
                                           // (NULL if not present)
    size_t relroSegmentLen;                // Length of GNU_RELRO segment (zero if not present)
} ElfImage;

typedef struct ElfSymbol {
    const char *name;  // symbol name
    uint64_t value;    // symbol value (st_value)
    uint32_t type;     // symbol type
    uint32_t binding;  // symbol binding
    bool needsLookup;
    size_t size;
} ElfSymbol;

// Construct an ElfImage instance from an Elf file.
//
// ElfImage may be constructed only after all loadable segments have been
// mapped or copied into memory in the current address space.  The `base`
// parameter provides an offset applied to the `vaddr` memory of each
// loadable segment to give the actual location in memory.  For each
// loadable segment, the segment image shall reside at `base + seg.vaddr`,
// and shall extend for at least a`seg.memsz` bytes.
//
// To maintain safety, the caller must ensure that the following conditions
// hold:
//
//  1. During any call to ElfSyms_relocate(), all segment data shall be writable.
//
//  2. During calls to any other member function, all segment data shall be readable.
//
// After ctor() has been called -- regardless of its error status -- it
// shall be safe to call the destructor or any other member functions.
//
// On entry:
//   ef = a previously constructed ElfFile object.

//   base = an offset applied to segment `vaddr` fields to obtain their location in memory.
//
// On exit:
//   ELFFILE_SUCCESS => success.
//   Any other value => an error condition was detected.
//
ElfStatus ElfImage_ctor(ElfImage *es, ElfFile *ef, uintptr_t base);

// Destroy an ElfImage instance.
//
// Note: it shall be safe to call dtor() passing a zero-initialized
// structure.
//
void ElfImage_dtor(ElfImage *es);

// Return the number of external libraries that are referenced by this ELF file.
//
uint64_t ElfImage_getLibraryCount(ElfImage *es);

// Return the name of the external library at index `ndx`, or NULL on error.
//
ElfStatus ElfImage_getLibraryName(ElfImage *es, int ndx, const char **nameOut);

// Locate a symbol, given its name, and return its description.
//
// On entry:
//   name: name of the symbol to look up.
//   symOut: pointer to structure to be filled.
//
// On exit:
//   ELFFILE_SUCCESS: All fields in *symOut are valid. symOut->value will
//                    contain the absolute address of the symbol.
//   Otherwise      : An error was encountered.  *symOut is garbage.
//
ElfStatus ElfImage_findSymbol(ElfImage *es, const char *name, ElfSymbol *symOut);

// Perform all relocations in the ELF file.  When an external symbol lookup
// is required, this function returns ELFFILE_LOOKUP and sets symInOut->name
// to the name of the symbol.  The client is expected to initialize the
// other fields of *symInOut and call this function again to resume
// relocation.  When relocation is complete, ELFFILE_SUCCESS is returned.
//
// There is no way to restart or repeat relocation.  An ELF image can only
// be relocated once because the operation may read locations that it later modifies.
//
// On entry:
//
//   *base     = Address used for relocation
//   *symInOut = On first entry, unused.  Afer an ELFFILE_LOOKUP return, its
//               `name` field holds the symbol to be resolved.  On subsequent
//               calls its other fields should describe the named symbol.
//
// Return value:
//   ELFFILE_SUCCESS   => iteration completed normally.
//   ELFFILE_LOOKUP    => symInOut->name holds the name of the symbol.
//   other             => an error occurred
//
ElfStatus ElfImage_relocate(ElfImage *me, uintptr_t base, ElfSymbol *symInOut);

#endif /* ELFFILE_H */
