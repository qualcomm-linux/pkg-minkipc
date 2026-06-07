// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

//-------------------------------------------------------------------------
// This is the "XX" template for ElfFile.c.  The following template
// "variables" must be defined as preprocessor symbols before inclusion:
//
//    XX = "32" or "64"   (selects ELF32 or ELF64 file formats)
//
//-------------------------------------------------------------------------

// #include "stringl.h"

#include "MemSCpy.h"

#ifndef XX
#error This file requires template variable XX.
#endif

// Template instance-specific function names:

#define ElfFileXX_getSegmentInfo CAT3(ElfFile, XX, _getSegmentInfo)
#define ElfFileXX_getSectionInfo CAT3(ElfFile, XX, _getSectionInfo)
#define ElfFileXX_compare CAT3(ElfFile, XX, _compare)
#define ElfFileXX_ctor CAT3(ElfFile, XX, _ctor)
#define ElfImageXX_getRelInfo CAT3(ElfImage, XX, _getRelInfo)
#define ElfImageXX_getSymbol CAT3(ElfImage, XX, _getSymbol)
#define ElfImageXX_getLibraryName CAT3(ElfImage, XX, _getLibraryName)
#define ElfImageXX_updateRel CAT3(ElfImage, XX, _updateRel)
#define ElfImageXX_ctor CAT3(ElfImage, XX, _ctor)
#define computeRelocXX CAT(computeReloc, XX)
#define popRelocInfoXX CAT(popRelocInfo, XX)

// Template instance-specific types:

#define intXX_t CAT3(int, XX, _t)
#define uintXX_t CAT3(uint, XX, _t)
#define ElfXX_Ehdr CAT3(Elf, XX, _Ehdr)
#define ElfXX_Phdr CAT3(Elf, XX, _Phdr)
#define ElfXX_Shdr CAT3(Elf, XX, _Shdr)
#define ElfXX_Dyn CAT3(Elf, XX, _Dyn)
#define ElfXX_Sym CAT3(Elf, XX, _Sym)
#define ElfXX_Rel CAT3(Elf, XX, _Rel)
#define ElfXX_Rela CAT3(Elf, XX, _Rela)
#define ELFXX_R_TYPE CAT3(ELF, XX, _R_TYPE)
#define ELFXX_R_SYM CAT3(ELF, XX, _R_SYM)

static ElfStatus ElfFileXX_getSegmentInfo(const ElfFile *me, size_t ndx, ElfSegment *seg)
{
    ElfStatus ret = ELFFILE_SUCCESS;

    T_CHECK(ndx < me->phNum);

    const ElfXX_Phdr *phdr = &((const ElfXX_Phdr *)me->pht)[ndx];

    seg->type = phdr->p_type;
    seg->offset = phdr->p_offset;
    seg->vaddr = phdr->p_vaddr;
    seg->paddr = phdr->p_paddr;
    seg->filesz = phdr->p_filesz;
    seg->memsz = phdr->p_memsz;
    seg->flags = phdr->p_flags;
    seg->align = phdr->p_align;

exit:
    return ret;
}

static ElfStatus ElfFileXX_getSectionInfo(const ElfFile *me, size_t ndx, ElfSection *sec)
{
    ElfStatus ret = ELFFILE_SUCCESS;

    T_CHECK(ndx < me->shNum);

    const ElfXX_Shdr *shdr = &((const ElfXX_Shdr *)me->sht)[ndx];

    sec->name_offset = shdr->sh_name;
    sec->type = shdr->sh_type;
    sec->flags = shdr->sh_flags;
    sec->offset = shdr->sh_offset;
    sec->size = shdr->sh_size;

    // Find the address of the string name of the above segment
    shdr = &((const ElfXX_Shdr *)me->sht)[me->shstrndx];
    sec->name_addr = shdr->sh_offset + sec->name_offset;

exit:
    return ret;
}

static ElfStatus ElfFileXX_compare(const ElfFile *me, const ElfFile *other)
{
    ElfStatus ret = ELFFILE_SUCCESS;

    T_CHECK(me->is64 == other->is64);
    T_CHECK(me->phNum == other->phNum);

    size_t phtSize = me->phNum * sizeof(ElfXX_Phdr);
    if (0 != memcmp(me->pht, other->pht, phtSize)) {
        return ELFFILE_ERROR;
    }

exit:
    return ret;
}

static ElfStatus ElfFileXX_ctor(ElfFile *me, ElfSource *source)
{
    ElfXX_Ehdr ehdr;
    ElfStatus ret = ELFFILE_SUCCESS;

    // Read the ELF header, copy it out, and then free it.
    {
        const ElfXX_Ehdr *pehdr = NULL;
        T_GUARD(ElfSource_getData((void **)(&pehdr), source, 0, sizeof(ElfXX_Ehdr)));

        T_CHECK(pehdr != NULL);
        ehdr = *pehdr;
        ElfSource_freeData(me->source, pehdr);
    }

    T_CHECK(ehdr.e_version == ELF_VERSION_CURRENT);
    T_CHECK(ehdr.e_ehsize == sizeof(ElfXX_Ehdr));
    T_CHECK(ehdr.e_phentsize == sizeof(ElfXX_Phdr));

    me->entry = ehdr.e_entry;
    me->machine = ehdr.e_machine;
    me->flags = ehdr.e_flags;
    me->ehSize = sizeof(ElfXX_Ehdr);
    me->phentSize = sizeof(ElfXX_Phdr);
    me->phOff = ehdr.e_phoff;
    me->shstrndx = ehdr.e_shstrndx;

    size_t phtSize = satMul(ehdr.e_phnum, sizeof(ElfXX_Phdr));
    T_GUARD(ElfSource_getData((void **)(&me->pht), source, ehdr.e_phoff, phtSize));

    T_CHECK(me->pht != NULL);
    me->phNum = ehdr.e_phnum;

    size_t shtSize = satMul(ehdr.e_shnum, ehdr.e_shentsize);
    T_GUARD(ElfSource_getData((void **)(&me->sht), source, ehdr.e_shoff, shtSize));

    T_CHECK(me->sht != NULL);
    me->shNum = ehdr.e_shnum;

exit:
    return ret;
}

// Return the address of a relocation entry.
//
// On entry:
//   globalIndex     = a number that identifies a relocation entry from among
//                     all REL and RELA entries.  Values start at 1 for the
//                     first REL entry.
//
// On exit:
//   return value    = pointer to entry, or NULL if index is out of range.
//   infoOut->isRela = true if the entry is of type ElfXX_Rela,
//                     false if the entry is of type ElfXX_Rel
//
static ElfStatus ElfImageXX_getRelInfo(ElfImage *me, size_t globalIndex, ElfRelInfo *infoOut)
{
    /* It is assumed that the order of the Table entries is rel, rela, pltrel */
    size_t index = globalIndex - 1;

    do {
        /* If the incoming index falls within the rel indices, which are assumed
         * to
         * be first, then treat this as a Rel info.
         */
        if (index < me->relLen) {
            infoOut->ptr = (char *)me->relTable + index * sizeof(ElfXX_Rel);
            infoOut->isRela = false;
            break;
        }

        /* The Rela entries are assumed to follow the Rel entries, so adjust our
         * index to account for any Rel entries we might have skipped.
         */
        index -= me->relLen;

        /* If the adjusted index falls within the Rela indices, which are
         * assumed to
         * follow the Rel entries, then treat this as a Rela info.
         */
        if (index < me->relaLen) {
            infoOut->ptr = (char *)me->relaTable + index * sizeof(ElfXX_Rela);
            infoOut->isRela = true;
            break;
        }

        /* The Plt entries are assumed to follow the Rela entries, so adjust our
         * index to account for any Rel entries we might have skipped.
         */
        index -= me->relaLen;

        /* If the adjusted index falls within the Plt indices, which are assumed
         * to
         * follow the Rela entries, then treat this as a Plt info.
         */
        if (index < me->pltrelLen) {
            infoOut->ptr = (char *)me->pltrelTable;
            infoOut->ptr += index * ((me->isPltRela) ? sizeof(ElfXX_Rela) : sizeof(ElfXX_Rel));
            break;
        }

        /* If we got here then the incoming index is outside the known
         * index ranges.
         */
        infoOut->ptr = NULL;
        return ELFFILE_RELOC_OUT_OF_RANGE;
    } while (0);

    infoOut->symIndex = ELFXX_R_SYM(((ElfXX_Rel *)infoOut->ptr)->r_info);

    return ELFFILE_SUCCESS;
}

// Get information describing the symbol.
//
static ElfStatus ElfImageXX_getSymbol(ElfImage *me, size_t symIndex, ElfSymbol *symOut)
{
    ElfStatus ret = ELFFILE_SUCCESS;

    T_CHECK(symIndex < me->symLen);

    ElfXX_Sym *sym = ((ElfXX_Sym *)me->symTable) + symIndex;

    T_CHECK(sym->st_name < me->stringLen);

    const char *name = me->stringTable + sym->st_name;

    symOut->name = name;
    if (SHN_UNDEF != sym->st_shndx) {
        symOut->value = sym->st_value + (sym->st_shndx == SHN_ABS ? 0 : me->base);
        symOut->needsLookup = false;
    }
    /* ELF weak reference symbol
     * Per the ELF spec, lookups are not performed on weak references. The
     * expected symbol value for weak references are:
     *   Absolute REL types: value is 0
     *   PC relative REL types: value address of the place.
     *   Base relative REL types: value of the base.
     */
    else if (STB_WEAK == ELF_ST_BIND(sym->st_info)) {
        symOut->value = sym->st_value;
        symOut->needsLookup = false;
    } else {
        symOut->needsLookup = true;
    }

    symOut->type = ELF_ST_TYPE(sym->st_info);
    symOut->binding = ELF_ST_BIND(sym->st_info);

    if (symOut->type == STT_OBJECT) {
        symOut->size = sym->st_size;
    } else {
        symOut->size = 0;
    }

exit:
    return ret;
}

static ElfStatus ElfImageXX_getLibraryName(ElfImage *me, int ndx, const char **name)
{
    ElfXX_Dyn *dyns = (ElfXX_Dyn *)me->dynTable;
    int cur_ndx = 0;
    ElfStatus ret = ELFFILE_SUCCESS;

    for (uint32_t i = 0; i < me->dynLen; ++i) {
        if (dyns[i].d_tag == DT_NEEDED) {
            if (ndx == cur_ndx) {
                uintXX_t offset = dyns[i].d_un.d_val;
                T_CHECK(offset < me->stringLen);

                *name = me->stringTable + offset;
                return ELFFILE_SUCCESS;
            }

            cur_ndx++;
        }
    }

exit:
    return ret;
}

static ElfStatus ElfImageXX_updateRel(ElfImage *me, uintptr_t base, ElfRelInfo *relInfo,
                                      ElfSymbol *sym)
{
    ElfXX_Rel *rel = (ElfXX_Rel *)relInfo->ptr;
    uintXX_t place_val;

    uint32_t rtype = ELFXX_R_TYPE(rel->r_info);

    uintXX_t S = (uintXX_t)sym->value;
    uintXX_t T = 0;
    ElfStatus ret = ELFFILE_SUCCESS;
    uintXX_t *place = (uintXX_t *)ElfImage_getAddr(me, rel->r_offset, sizeof(uintXX_t));
    T_CHECK(NULL != place);

    memscpy(&place_val, sizeof(place_val), place, sizeof(*place));
    uintXX_t A = relInfo->isRela ? ((ElfXX_Rela *)rel)->r_addend : place_val;
    uintXX_t B = (uintXX_t)base;

    if (sym->type == STT_FUNC && (S & 1) != 0) {
        S &= ~1u;
        T = 1;
    }

    if (XX == 32) {
        switch (rtype) {
            case R_ARM_RELATIVE:
                // Note: we assume segment being relocated and segment containing the
                // symbol are both in this file, and therefore have the same offset.
                place_val = B + A;
                break;

            case R_ARM_ABS32:
                place_val = S + A;
                break;

            case R_ARM_JUMP_SLOT:
            case R_ARM_GLOB_DAT:
                place_val = (S + A) | T;
                break;

            default:
                // CHECK_LOG(); // TODO: uncomment
                return ELFFILE_ERROR;
        }

    } else if (XX == 64) {
        switch (rtype) {
            case R_AARCH64_RELATIVE:
                // Note: we assume segment being relocated and segment containing the
                // symbol are both in this file, and therefore have the same offset.
                place_val = B + A;
                break;

            case R_AARCH64_ABS64:
                place_val = S + A;
                break;

            case R_AARCH64_GLOB_DAT:
            case R_AARCH64_JUMP_SLOT:
                place_val = (S + A) | T;
                break;

            default:
                // CHECK_LOG(); // TODO: uncomment
                return ELFFILE_ERROR;
                break;
        }
    }

    memscpy(place, sizeof(*place), &place_val, sizeof(place_val));

exit:
    return ret;
}

static ElfStatus popRelocInfoXX(ElfImage *me)
{
    uintXX_t rel = 0, rela = 0, strtab = 0, symtab = 0, hash = 0;
    uintXX_t relsz = 0, relasz = 0, strsz = 0;
    uintXX_t relent = 0, relaent = 0;
    uintXX_t pltrel = 0, pltrelsz = 0;
    bool isPltRela = false;
    ElfStatus ret = ELFFILE_SUCCESS;

    ElfXX_Dyn *dyns = (ElfXX_Dyn *)me->dynTable;
    // TODO: switch to size_t?
    for (uintXX_t i = 0; i < me->dynLen; ++i) {
        uintXX_t val = dyns[i].d_un.d_val;

        switch (dyns[i].d_tag) {
            case DT_NEEDED:
                me->libnum++;
                break;
            case DT_STRTAB:
                strtab = val;
                break;
            case DT_STRSZ:
                strsz = val;
                break;
            case DT_SYMTAB:
                symtab = val;
                break;
            case DT_HASH:
                hash = val;
                break;
            case DT_RELA:
                rela = val;
                break;
            case DT_RELASZ:
                relasz = val;
                break;
            case DT_RELAENT:
                relaent = val;
                break;
            case DT_REL:
                rel = val;
                break;
            case DT_RELSZ:
                relsz = val;
                break;
            case DT_RELENT:
                relent = val;
                break;
            case DT_JMPREL:
                pltrel = val;
                break;
            case DT_PLTRELSZ:
                pltrelsz = val;
                break;
            case DT_PLTREL:
                isPltRela = (val == DT_RELA);
                break;
        }
    }

    // Store each size only after its memory range has been validated.
    if (relsz) {
        T_CHECK(relent == sizeof(ElfXX_Rel));
        me->relTable = ElfImage_getAddr(me, rel, relsz);
        T_CHECK(NULL != me->relTable);
        me->relLen = relsz / relent;
    }

    if (relasz) {
        T_CHECK(NULL != me->relTable);
        me->relaTable = ElfImage_getAddr(me, rela, relasz);
        T_CHECK(NULL != me->relaTable);
        me->relaLen = relasz / relaent;
    }

    if (pltrelsz) {
        me->isPltRela = isPltRela;
        me->pltrelTable = ElfImage_getAddr(me, pltrel, pltrelsz);
        T_CHECK(NULL != me->pltrelTable);
        if (isPltRela) {
            me->pltrelLen = pltrelsz / sizeof(ElfXX_Rela);
        } else {
            me->pltrelLen = pltrelsz / sizeof(ElfXX_Rel);
        }
    }

    me->stringTable = (const char *)ElfImage_getAddr(me, strtab, strsz);
    T_CHECK(NULL != me->stringTable);
    // A zero-terminated string table (as per spec) guarantees that any index
    // within the table points to a zero-terminated string.
    T_CHECK(strsz > 0 && me->stringTable[strsz - 1] == '\0');
    me->stringLen = strsz;

    // The hash table contains the following (all 32-bit values):
    //    nbucket
    //    nchain
    //    buckets[0...nbucket-1]
    //    chains[0...nchain-1]
    // TODO: should be uintXX_t?
    uint32_t *hashAddr = (uint32_t *)ElfImage_getAddr(me, hash, sizeof(uint32_t) * 2);
    T_CHECK(NULL != hashAddr);
    size_t nbucket = hashAddr[0];
    size_t nchain = hashAddr[1];
    size_t hashLen = satAdd(satAdd(nbucket, nchain), 2);

    // TODO: should be uintXX_t?
    me->hashTable = (uint32_t *)ElfImage_getAddr(me, hash, satMul(hashLen, sizeof(uint32_t)));
    T_CHECK(NULL != me->hashTable);
    me->hashLen = hashLen;
    me->nbucket = nbucket;

    // nchain also gives us the symbol table size
    me->symTable = ElfImage_getAddr(me, symtab, satMul(nchain, sizeof(ElfXX_Sym)));
    T_CHECK(NULL != me->symTable);
    me->symLen = nchain;

exit:
    return ret;
}

static ElfStatus ElfImageXX_ctor(ElfImage *me, ElfFile *ef, uintptr_t base)
{
    ElfStatus ret = ELFFILE_SUCCESS;
#ifdef OFFTARGET
    T_CHECK(ef->machine == EM_X86_64);
#else
    T_CHECK((ef->machine == EM_ARM) || (ef->machine == EM_AARCH64));
#endif

    me->base = base;

    const ElfXX_Phdr *dynHdr = NULL;
    // TODO: should be ElfXX_Phdr?
    const ElfXX_Phdr *pht = (ElfXX_Phdr *)ef->pht;
    for (size_t i = 0; i < ef->phNum; ++i) {
        if (pht[i].p_type == PT_LOAD) {
            // Note: Overflow of `end` is harmless. When `end < start` the range will match nothing.
            uintptr_t start = pht[i].p_vaddr;
            uintptr_t end = start + pht[i].p_memsz;
            T_CHECK(me->rangeLen < (sizeof(me->ranges) / sizeof(me->ranges[0])));
            me->ranges[me->rangeLen].start = start;
            me->ranges[me->rangeLen].end = end;
            ++me->rangeLen;
        } else if (pht[i].p_type == PT_DYNAMIC) {
            dynHdr = pht + i;
        } else if (pht[i].p_type == PT_GNU_RELRO) {
            me->relroSegmentStart = ElfImage_getAddr(me, pht[i].p_vaddr, pht[i].p_memsz);
            me->relroSegmentLen = pht[i].p_memsz;
        }
    }

    // Get dynamic segment address & size
    // Not having a dynamic program header is considered an error in
    // the case of our TAs since all TAs should be compiled to be relocatable

    if (dynHdr) {
        uintXX_t dynSize = dynHdr->p_filesz;
        me->dynTable = ElfImage_getAddr(me, dynHdr->p_vaddr, dynSize);
        T_CHECK(me->dynTable != NULL);
        me->dynLen = dynSize / sizeof(ElfXX_Dyn);
    } else {
        return ELFFILE_ERROR;
    }

    // Scan dynamic entries
    return popRelocInfoXX(me);

exit:
    return ret;
}

// These definitions should not be used outside of this template file:

#undef ElfFileXX_getSegmentInfo
#undef ElfFileXX_getSectionInfo
#undef ElfFileXX_compare
#undef ElfFileXX_ctor
#undef ElfImageXX_getRelInfo
#undef ElfImageXX_getSymbol
#undef ElfImageXX_getLibraryName
#undef ElfImageXX_updateRel
#undef ElfImageXX_ctor
#undef computeRelocXX
#undef popRelocInfoXX
#undef intXX_t
#undef uintXX_t
#undef ElfXX_Ehdr
#undef ElfXX_Phdr
#undef ElfXX_Shdr
#undef ElfXX_Dyn
#undef ElfXX_Sym
#undef ElfXX_Rel
#undef ElfXX_Rela
#undef ELFXX_R_TYPE
#undef ELFXX_R_SYM
