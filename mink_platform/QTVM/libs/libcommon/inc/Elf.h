// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#if defined(ELF_HEADER)
/*
 * Defined as either local elf.h, /pkg/ssg/toolchain's elf.h, or Android NDK toolchain's elf.h,
 * depending on build options.
 *
 * Unfortunately mink/elflib/inc comes first in the include path list so this file will always be
 * included first.
 */
#include ELF_HEADER
#else

#ifndef __ELF_H
#define __ELF_H

#include <stdint.h>

#define ELFINFO_MAGIC_SIZE (16)

#define CHECK_OVERFLOW(val0, val1) ((uintptr_t)(val0) > (uintptr_t)(val0) + (uintptr_t)(val1))

/*
 * These types are defined in the following documentation:
 *
 *   http://infocenter.arm.com/help/topic/com.arm.doc.espc0003/ARMELF.pdf
 *   https://uclibc.org/docs/elf-64-gen.pdf
 *
 * We specifically use these types to make it easier to
 * compare the documentation against the source.
 */

typedef uint16_t Elf32_Half;
typedef uint16_t Elf64_Half;

typedef uint32_t Elf32_Word;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;

typedef uint64_t Elf32_Xword;
typedef int64_t Elf32_Sxword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

typedef uint32_t Elf32_Addr;
typedef uint64_t Elf64_Addr;

typedef uint32_t Elf32_Off;
typedef uint64_t Elf64_Off;

typedef struct {
    uint8_t e_ident[ELFINFO_MAGIC_SIZE]; /** Magic number and other info   */
    Elf32_Half e_type;                   /** Object file type                    */
    Elf32_Half e_machine;                /** Architecture                        */
    Elf32_Word e_version;                /** Object file version                 */
    Elf32_Addr e_entry;                  /** Entry point virtual address         */
    Elf32_Off e_phoff;                   /** Program header table file offset    */
    Elf32_Off e_shoff;                   /** Section header table file offset    */
    Elf32_Word e_flags;                  /** Processor-specific flags            */
    Elf32_Half e_ehsize;                 /** ELF header size in bytes            */
    Elf32_Half e_phentsize;              /** Program header table entry size     */
    Elf32_Half e_phnum;                  /** Program header table entry count    */
    Elf32_Half e_shentsize;              /** Section header table entry size     */
    Elf32_Half e_shnum;                  /** Section header table entry count    */
    Elf32_Half e_shstrndx;               /** Section header string table index   */
} Elf32_Ehdr;

typedef struct {
    uint8_t e_ident[ELFINFO_MAGIC_SIZE]; /** Magic number and other info   */
    Elf64_Half e_type;                   /** Object file type                    */
    Elf64_Half e_machine;                /** Architecture                        */
    Elf64_Word e_version;                /** Object file version                 */
    Elf64_Addr e_entry;                  /** Entry point virtual address         */
    Elf64_Off e_phoff;                   /** Program header table file offset    */
    Elf64_Off e_shoff;                   /** Section header table file offset    */
    Elf64_Word e_flags;                  /** Processor-specific flags            */
    Elf64_Half e_ehsize;                 /** ELF header size in bytes            */
    Elf64_Half e_phentsize;              /** Program header table entry size     */
    Elf64_Half e_phnum;                  /** Program header table entry count    */
    Elf64_Half e_shentsize;              /** Section header table entry size     */
    Elf64_Half e_shnum;                  /** Section header table entry count    */
    Elf64_Half e_shstrndx;               /** Section header string table index   */
} Elf64_Ehdr;

/* Fields in the e_ident array.  The ELFINFO_*_INDEX macros are
 * indices into the array.  The macros under each ELFINFO_* macro
 * is the values the byte may have.
 */

#define ELFINFO_MAG0_INDEX 0 /* File identification byte 0 index */
#define ELFINFO_MAG0 0x7f    /* Magic number byte 0              */

#define ELFINFO_MAG1_INDEX 1 /* File identification byte 1 index */
#define ELFINFO_MAG1 'E'     /* Magic number byte 1              */

#define ELFINFO_MAG2_INDEX 2 /* File identification byte 2 index */
#define ELFINFO_MAG2 'L'     /* Magic number byte 2              */

#define ELFINFO_MAG3_INDEX 3 /* File identification byte 3 index */
#define ELFINFO_MAG3 'F'     /* Magic number byte 3              */

#define ELFINFO_CLASS_INDEX 4   /* File class byte index            */

/* ELF Object Type */
#define ELF_CLASS_32 1          /* 32-bit objects                   */
#define ELF_CLASS_64 2          /* 64-bit objects                   */

/* Version information */
#define ELFINFO_VERSION_INDEX 6 /* File version byte index          */
#define ELF_VERSION_CURRENT 1   /* Current version                  */

#define EM_ARM 40      /* Ehdr.e_machine                   */
#define EM_X86_64 62   /* Ehdr.e_machine                   */
#define EM_DSP6 164    /* Qualcomm DSP6                    */
#define EM_AARCH64 183 /* ARM AArch64                      */
#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3

typedef struct {
    Elf32_Word p_type;   /** Segment type */
    Elf32_Off p_offset;  /** Segment file offset */
    Elf32_Addr p_vaddr;  /** Segment virtual address */
    Elf32_Addr p_paddr;  /** Segment physical address */
    Elf32_Word p_filesz; /** Segment size in file */
    Elf32_Word p_memsz;  /** Segment size in memory */
    Elf32_Word p_flags;  /** Segment flags */
    Elf32_Word p_align;  /** Segment alignment */
} Elf32_Phdr;

typedef struct {
    Elf64_Word p_type;    /** Segment type */
    Elf64_Word p_flags;   /** Segment attributes */
    Elf64_Off p_offset;   /** Segment file offset */
    Elf64_Addr p_vaddr;   /** Segment virtual address */
    Elf64_Addr p_paddr;   /** Segment physical address */
    Elf64_Xword p_filesz; /** Segment size in file */
    Elf64_Xword p_memsz;  /** Segment size in memory */
    Elf64_Xword p_align;  /** Segment alignment */
} Elf64_Phdr;

typedef struct {
    Elf32_Word sh_name;      /** Section name index in String Table */
    Elf32_Word sh_type;      /** Section type */
    Elf32_Word sh_flags;     /** Section flags */
    Elf32_Addr sh_addr;      /** Section virtual address */
    Elf32_Off sh_offset;     /** Section file offset */
    Elf32_Word sh_size;      /** Section size in file */
    Elf32_Word sh_link;      /** Index of associated section */
    Elf32_Word sh_info;      /** Extra info */
    Elf32_Word sh_addralign; /** Section alignment */
    Elf32_Word sh_entsize;   /** Section entry size */
} Elf32_Shdr;

typedef struct {
    Elf64_Word sh_name;       /** Section name index in String Table */
    Elf64_Word sh_type;       /** Section type */
    Elf64_Xword sh_flags;     /** Section flags */
    Elf64_Addr sh_addr;       /** Section virtual address */
    Elf64_Off sh_offset;      /** Section file offset */
    Elf64_Xword sh_size;      /** Section size in file */
    Elf64_Word sh_link;       /** Index of associated section */
    Elf64_Word sh_info;       /** Extra info */
    Elf64_Xword sh_addralign; /** Section alignment */
    Elf64_Xword sh_entsize;   /** Section entry size */
} Elf64_Shdr;

typedef struct {
    Elf32_Addr r_offset; /** Address of reference */
    Elf32_Word r_info;   /** Relocation type and symbol index */
} Elf32_Rel;

typedef struct {
    Elf64_Addr r_offset; /** Address of reference */
    Elf64_Xword r_info;  /** Relocation type and symbol index */
} Elf64_Rel;

typedef struct {
    Elf32_Addr r_offset;  /** Address of reference */
    Elf32_Word r_info;    /** Relocation type and symbol index */
    Elf32_Sword r_addend; /** Addend */
} Elf32_Rela;

typedef struct {
    Elf64_Addr r_offset;   /** Address of reference */
    Elf64_Xword r_info;    /** Relocation type and symbol index */
    Elf64_Sxword r_addend; /** Addend */
} Elf64_Rela;

typedef struct {
    Elf32_Word st_name;  /* Symbol name (string tbl index) */
    Elf32_Addr st_value; /* Symbol value */
    Elf32_Word st_size;  /* Symbol size */
    uint8_t st_info;     /* Symbol type and binding */
    uint8_t st_other;    /* Symbol visibility */
    Elf32_Half st_shndx; /* Section index */
} Elf32_Sym;

typedef struct {
    Elf64_Word st_name;  /* Symbol name (string tbl index) */
    uint8_t st_info;     /* Symbol type and binding */
    uint8_t st_other;    /* Symbol visibility */
    Elf64_Half st_shndx; /* Section index */
    Elf64_Addr st_value; /* Symbol value */
    Elf64_Xword st_size; /* Symbol size */
} Elf64_Sym;

typedef struct {
    Elf32_Sword d_tag; /* Dynamic entry type */
    union {
        Elf32_Word d_val; /* Integer value */
        Elf32_Addr d_ptr; /* Address value */
    } d_un;
} Elf32_Dyn;

typedef struct {
    Elf64_Sxword d_tag; /* Dynamic entry type */
    union {
        Elf64_Xword d_val; /* Integer value */
        Elf64_Addr d_ptr;  /* Address value */
    } d_un;
} Elf64_Dyn;

#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_PLTGOT 3
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_INIT 12
#define DT_FINI 13
#define DT_SONAME 14
#define DT_RPATH 15
#define DT_SYMBOLIC 16
#define DT_REL 17
#define DT_RELSZ 18
#define DT_RELENT 19
#define DT_PLTREL 20
#define DT_DEBUG 21
#define DT_TEXTREL 22
#define DT_JMPREL 23
#define DT_BIND_NOW 24
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH 29
#define DT_FLAGS 30
#define DT_ENCODING 32
#define DT_PREINIT_ARRAY 32
#define DT_PREINIT_ARRAYSZ 33
#define DT_NUM 34
#define DT_GNU_HASH 0x6ffffef5 /* GNU-style dynamic symbol hash table */

#define R_ARM_NONE 0
#define R_ARM_PC24 1
#define R_ARM_ABS32 2
#define R_ARM_REL32 3
#define R_ARM_PC13 4
#define R_ARM_ABS16 5
#define R_ARM_ABS12 6
#define R_ARM_THM_ABS5 7
#define R_ARM_ABS8 8
#define R_ARM_SBREL32 9
#define R_ARM_THM_PC22 10
#define R_ARM_THM_PC8 11
#define R_ARM_AMP_VCALL9 12
#define R_ARM_TLS_DESC 13
#define R_ARM_THM_SWI8 14
#define R_ARM_XPC25 15
#define R_ARM_THM_XPC22 16
#define R_ARM_TLS_DTPMOD32 17
#define R_ARM_TLS_DTPOFF32 18
#define R_ARM_TLS_TPOFF32 19
#define R_ARM_COPY 20
#define R_ARM_GLOB_DAT 21
#define R_ARM_JUMP_SLOT 22
#define R_ARM_RELATIVE 23

#define R_AARCH64_NONE 0            /* No relocation. */
#define R_AARCH64_ABS64 257         /* Direct 64 bit. */
#define R_AARCH64_ABS32 258         /* Direct 32 bit. */
#define R_AARCH64_ABS16 259         /* Direct 16-bit. */
#define R_AARCH64_PREL64 260        /* PC-relative 64-bit. */
#define R_AARCH64_PREL32 261        /* PC-relative 32-bit. */
#define R_AARCH64_PREL16 262        /* PC-relative 16-bit. */
#define R_AARCH64_COPY 1024         /* Copy symbol at runtime. */
#define R_AARCH64_GLOB_DAT 1025     /* Create GOT entry. */
#define R_AARCH64_JUMP_SLOT 1026    /* Create PLT entry. */
#define R_AARCH64_RELATIVE 1027     /* Adjust by program base. */
#define R_AARCH64_TLS_DTPMOD64 1028 /* Module number, 64 bit. */
#define R_AARCH64_TLS_DTPREL64 1029 /* Module-relative offset, 64 bit. */
#define R_AARCH64_TLS_TPREL64 1030  /* TP-relative offset, 64 bit. */
#define R_AARCH64_TLSDESC 1031      /* TLS Descriptor. */
#define R_AARCH64_IRELATIVE 1032    /* Indirect(Delta(S)+ A) */

#define PT_NULL 0               /** NULL Segment */
#define PT_LOAD 1               /** Load Segment */
#define PT_DYNAMIC 2            /** Dynamic Segment */
#define PT_INTERP 3             /** Interpreter Segment */
#define PT_GNU_RELRO 0x6474e552 /** RELRO Segment */

#define PT_FLAG_HASH_TYPE_MASK 0x2000000
#define PT_FLAG_MANIFEST_TYPE_MASK 0x4000000
#define PT_FLAG_TPROCESS_TYPE_MASK 0x3000000
#define PT_FLAG_TYPE_MASK 0x7000000

/* Fields in the e_ident array.  The ELFINFO_*_INDEX macros are
 * indices into the array.
 */
#define ELFINFO_CLASS_INDEX 4 /* File class byte index            */

/* Special section indices */
#define SHN_UNDEF 0
#define SHN_ABS 0xfff1

/* End of a chain in the hash table  */
#define STN_UNDEF 0

/* Special symbol table index. */
/* clang-format off */
#define ELF_ST_BIND(i) ((i) >> 4)
#define ELF_ST_TYPE(i) ((i) & 0xF)
#define ELF_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_LOPROC 13
#define STB_HIPROC 15

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_LOPROC 13
#define STT_HIPROC 15

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((i) & 0xff)
#define ELF32_R_INFO(s, t) (((s) << 8) + ((t) & 0xff))

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)
#define ELF64_R_INFO(s, t) ((((uint64_t)(s)) << 32) + (t))
/* clang-format on */

#endif /* __ELF_H */
#endif /* NOT ELF_HEADER */
