/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024, 2025 (c)
 * 
 *    file: elf.h
 *    This file is part of LaylaOS.
 *
 *    LaylaOS is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    LaylaOS is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LaylaOS.  If not, see <http://www.gnu.org/licenses/>.
 */    

/**
 *  \file elf.h
 *
 *  Functions and macro definitions for working with files in the Executable
 *  and Linkable Format (ELF). This file includes either elf32.h or elf64.h,
 *  depending on the architecture.
 */

#ifndef __ELF_H__
#define __ELF_H__

#include <stdint.h>
#include <kernel/vfs.h>

#define ELF_NIDENT                16

/**
 * \enum elf_ident
 *
 * Values for the identification field (e_ident)
 */
enum elf_ident
{
    EI_MAG0         = 0,    /**< 0x7F */
    EI_MAG1         = 1,    /**< 'E' */
    EI_MAG2         = 2,    /**< 'L' */
    EI_MAG3         = 3,    /**< 'F' */
    EI_CLASS        = 4,    /**< ARCH (32/64) */
    EI_DATA         = 5,    /**< BYTE ORDER */
    EI_VERSION      = 6,    /**< ELF VER. */
    EI_OSABI        = 7,    /**< OS SPECIFIC */
    EI_ABIVERSION   = 8,    /**< OS SPECIFIC */
    EI_PAD          = 9     /**< PADDING */
};

#define ELFMAG0                   0x7F
#define ELFMAG1                   'E'
#define ELFMAG2                   'L'
#define ELFMAG3                   'F'

/*
 * Values for the EI_DATA member of the identification field
 */
#define ELFDATANONE               0    /**< INVALID ENCODING */
#define ELFDATA2LSB               1    /**< LITTLE ENDIAN */
#define ELFDATA2MSB               2    /**< BIG ENDIAN */

/*
 * Values for the EI_CLASS member of the identification field
 */
#define ELFCLASSNONE              0    /**< INVALID CLASS */
#define ELFCLASS32                1    /**< 32-BIT ARCH */
#define ELFCLASS64                2    /**< 64-BIT ARCH */

/**
 * \enum elf_type
 *
 * Values for the file type field (e_type)
 */
enum elf_type
{
    ET_NONE         = 0,    /**< UNKNOWN TYPE */
    ET_REL          = 1,    /**< RELOCATABLE FILE */
    ET_EXEC         = 2,    /**< EXECUTABLE FILE */
    ET_DYN          = 3,    /**< SHARED OBJECT FILE */
};

/*
 * Values for the e_machine field
 */
#define EM_NONE                 0    /**< NO MACHINE */
#define EM_M32                  1    /**< AT&T WE 32100 MACHINE */
#define EM_SPARC                2    /**< SPARC MACHINE */
#define EM_386                  3    /**< INTEL 80386 MACHINE */
#define EM_68K                  4    /**< MOTOROLA 68000 MACHINE */
#define EM_88K                  5    /**< MOTOROLA 88000 MACHINE */
#define EM_860                  7    /**< INTEL 80860 MACHINE */
#define EM_MIPS                 8    /**< MIPS RS3000 MACHINE */
#define EM_X86_64               62  /**< AMD x86-64 MACHINE */

/*
 * Values for the e_version field
 */
#define EV_NONE                 0    /**< INVALID VERSION */
#define EV_CURRENT              1    /**< ELF CURRENT VERSION */


/*
 * SPECIAL SECTION INDEXES
 */
#define SHN_UNDEF               (0x00)        //undefined/not present
#define SHN_LORESERVE           (0xFF00)
#define SHN_LOPROC              (0xFF00)
#define SHN_HIPROC              (0xFF1F)
#define SHN_ABS                 (0xFFF1)
#define SHN_COMMON              (0xFFF2)
#define SHN_HIRESERVE           (0xFFFF)

/*
 * SECTION TYPES
 */
#define SHT_NULL                0   /**< NULL SECTION */
#define SHT_PROGBITS            1   /**< PROGRAM INFORMATION */
#define SHT_SYMTAB              2   /**< SYMBOL TABLE */
#define SHT_STRTAB              3   /**< STRING TABLE */
#define SHT_RELA                4   /**< RELOCATION (W/ ADDEND) */
#define SHT_HASH                5   /**< HASH TABLE */
#define SHT_DYNAMIC             6   /**< DYNAMIC SECTION */
#define SHT_NOTE                7   /**< NOTE SECTION */
#define SHT_NOBITS              8   /**< SECTION NOT PRESENT IN FILE */
#define SHT_REL                 9   /**< RELOCATION (NO ADDEND) */
#define SHT_SHLIB               10
#define SHT_DYNSYM              11  /**< DYNAMIC SYMTAB */

#define SHT_NUM                 12

#define SHT_LOPROC              0x70000000
#define SHT_HIPROC              0x7FFFFFFF
#define SHT_LOUSER              0x80000000
#define SHT_HIUSER              0xFFFFFFFF

/*
 * SECTION ATTRIBUTES
 */
#define SHF_WRITE               0x01        // WRITEABLE SECTION
#define SHF_ALLOC               0x02        // EXISTS IN MEMORY
#define SHF_EXECINSTR           0x04        //
#define SHF_MASKPROC            0xF0000000  //

/*
 * Binding types (st_info field)
 */
#define STB_LOCAL               0
#define STB_GLOBAL              1
#define STB_WEAK                2

/*
 * Symbol types (st_info field)
 */
#define STT_NOTYPE              0
#define STT_OBJECT              1
#define STT_FUNC                2
#define STT_SECTION             3
#define STT_FILE                4
#define STT_COMMON              5
#define STT_TLS                 6

/*
 * DYNAMIC ARRAY TAGS
 */
#define DT_NULL                 0
#define DT_NEEDED               1
#define DT_PLTRELSZ             2
#define DT_PLTGOT               3
#define DT_HASH                 4
#define DT_STRTAB               5
#define DT_SYMTAB               6
#define DT_RELA                 7
#define DT_RELASZ               8
#define DT_RELAENT              9
#define DT_STRSZ                10
#define DT_SYMENT               11
#define DT_INIT                 12
#define DT_FINI                 13
#define DT_SONAME               14
#define DT_RPATH                15
#define DT_SYMBOLIC             16
#define DT_REL                  17
#define DT_RELSZ                18
#define DT_RELENT               19
#define DT_PLTREL               20
#define DT_DEBUG                21
#define DT_TEXTREL              22
#define DT_JMPREL               23

#define DT_INIT_ARRAY           25
#define DT_INIT_ARRAYSZ         27
#define DT_ENCODING             32

#define DT_LOPROC               0x70000000
#define DT_HIPROC               0x7FFFFFFF

/*
 * PROGRAM HEADER TYPES
 */
#define PT_NULL                 0
#define PT_LOAD                 1
#define PT_DYNAMIC              2
#define PT_INTERP               3
#define PT_NOTE                 4
#define PT_SHLIB                5
#define PT_PHDR                 6
#define PT_LOPROC               0x70000000
#define PT_HIPROC               0x7FFFFFFF

/*
 * PROGRAM HEADER FLAGS
 */
#define PT_R                    4
#define PT_W                    2
#define PT_X                    1

/*
 * AUXILIARY VECTOR INDICES
 */
#define AT_NULL                 0
#define AT_IGNORE               1
#define AT_EXECFD               2
#define AT_PHDR                 3
#define AT_PHENT                4
#define AT_PHNUM                5
#define AT_PAGESZ               6
#define AT_BASE                 7
#define AT_FLAGS                8
#define AT_ENTRY                9
#define AT_NOTELF               10
#define AT_UID                  11
#define AT_EUID                 12
#define AT_GID                  13
#define AT_EGID                 14
#define AT_PLATFORM             15
#define AT_HWCAP                16
#define AT_CLKTCK               17
#define AT_FPUCW                18
#define AT_DCACHEBSIZE          19
#define AT_ICACHEBSIZE          20
#define AT_UCACHEBSIZE          21
#define AT_IGNOREPPC            22
#define AT_SECURE               23
#define AT_BASE_PLATFORM        24
#define AT_RANDOM               25
#define AT_HWCAP2               26

#define AT_EXECFN               31
#define AT_SYSINFO              32
#define AT_SYSINFO_EHDR         33

#define AUXV_SIZE               16


#ifdef __x86_64__

#include <kernel/elf64.h>

#define elf_half                elf64_half
#define elf_off                 elf64_off
#define elf_addr                elf64_addr
#define elf_word                elf64_word
#define elf_sword               elf64_sword

#define elf_ehdr                elf64_ehdr
#define elf_shdr                elf64_shdr
#define elf_phdr                elf64_phdr
#define elf_sym                 elf64_sym
#define elf_rel                 elf64_rel
#define elf_rela                elf64_rela
#define elf_dyn                 elf64_dyn

#else       /* !__x86_64__ */

#include <kernel/elf32.h>

#define elf_half                elf32_half
#define elf_off                 elf32_off
#define elf_addr                elf32_addr
#define elf_word                elf32_word
#define elf_sword               elf32_sword

#define elf_ehdr                elf32_ehdr
#define elf_shdr                elf32_shdr
#define elf_phdr                elf32_phdr
#define elf_sym                 elf32_sym
#define elf_rel                 elf32_rel
#define elf_rela                elf32_rela
#define elf_dyn                 elf32_dyn

#endif      /* !__x86_64__ */


#ifdef KERNEL

/*
 * Flags to elf_load_file().
 */
#define ELF_FLAG_LOAD_NOW       0x01    /**< load all loadable ELF sections
                                             to memory now */
#define ELF_FLAG_NONE           0x00


/**
 * @brief Load ELF file.
 *
 * Read ELF file sections and reserve the required memory. If \a flags include
 * \ref ELF_FLAG_LOAD_NOW, the sections are loaded to memory immediately.
 *
 * @param   node    file node referring to the ELF file to be loaded
 * @param   block0  buffer containing the first disk block from the ELF file
 * @param   auxv    the auxiliary vector in which several bits of info, 
 *                    including the ELF's entry point, is stored
 * @param   flags   zero or \ref ELF_FLAG_LOAD_NOW
 *
 * @return  zero on success, -(errno) on failure.
 */
long elf_load_file(struct fs_node_t *node, struct cached_page_t *block0,
                   size_t *auxv, int flags);

/**
 * @brief Load the dynamic loader (ldso).
 *
 * Find the dynamic loader (ldso) executable and load it into memory.
 * The dynamic loader loads dynamically-linked ELF executables.
 *
 * @param   auxv    the auxiliary vector in which several bits of info, 
 *                    including the dynamic loader's entry point, is stored
 *
 * @return  zero on success, -(errno) on failure.
 */
long ldso_load(size_t *auxv);

#endif      /* KERNEL */


/**
 * @brief Check ELF header.
 *
 * Check if the given ELF file header contains the values we expect (depends
 * on the architecture we are running on).
 *
 * @param   caller      short string identifying the calling function (for
 *                        error messages)
 * @param   hdr         pointer to the loaded ELF header
 * @param   print_err   non-zero to print error messages
 *
 * @return  1 on success, 0 on failure.
 */
int check_elf_hdr(char *caller, elf_ehdr *hdr, int print_err);

/**
 * @brief Calculate ELF memory size.
 *
 * Read the executable header \a hdr and the program headers in \a phdr
 * to calculate the memory needed to load ELF segments to memory.
 *
 * @param   hdr         pointer to the ELF executable header
 * @param   phdr        pointer to the ELF program headers
 * @param   membase     the ELF's base memory address is returned here
 * @param   memsz       the ELF's total memory size is returned here
 *
 * @return  0 on success, errno on failure.
 */
int object_calc_memsz(elf_ehdr *hdr, elf_phdr *phdr,
                      uintptr_t *membase, uintptr_t *memsz);

/**
 * @brief Check ELF program header.
 *
 * Check if the given ELF file program  header address and size are valid
 * (depends on the architecture we are running on).
 *
 * @param   phdr        pointer to the ELF program header
 *
 * @return  1 on success, 0 on failure.
 */
int check_phdr_sizes(elf_phdr *phdr);

/**
 * @brief Get next path entry.
 *
 * Create a pathname by concatenating the next colon entry and the given 
 * \a filename. If \a use_dot is non-zero, we prepend './' to the pathname if 
 * the next colon entry is an empty string.
 * 
 * @param   colon_list  colon-separated list of paths
 * @param   filename    file name
 * @param   use_dot     use ./ if the next colon entry is empty
 * 
 * @return  malloc'd pathname, or NULL if we reached the end of the string,
 *          or an error occurs.
 */
char *next_path_entry(char **colon_list, char *filename, int use_dot);

#endif      /* __ELF_H__ */
