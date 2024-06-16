/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: elf64.h
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
 *  \file elf64.h
 *
 *  Functions and macro definitions for working with files in the Executable
 *  and Linkable Format (ELF), 64-bit version. Do not include this file in 
 *  your project. Instead, you need to include elf.h, which will in turn
 *  include either elf32.h or elf64.h as appropriate.
 */

#ifndef __ELF64_H__
#define __ELF64_H__

/*
 * TYPE DEFS 
 */
typedef uint16_t elf64_half;    /**< ELF 16-bit half word */
typedef uint64_t elf64_off;     /**< ELF 64-bit offset */
typedef uint64_t elf64_addr;    /**< ELF 64-bit address */
typedef uint32_t elf64_word;    /**< ELF 32-bit word */
typedef int32_t  elf64_sword;   /**< ELF 32-bit signed word */
typedef uint64_t elf64_xword;   /**< ELF 64-bit word */
typedef int64_t  elf64_sxword;  /**< ELF 64-bit signed word */


/**
 * @struct elf64_ehdr
 * @brief 64-bit ELF header.
 *
 * A structure containing the fields of a 64-bit ELF header.
 * Most fields have a set of valid values, which are defined as macros
 * in elf.h.
 */
typedef struct
{
    uint8_t	        e_ident[ELF_NIDENT];  /**< identification */
    elf64_half      e_type;               /**< file type */
    elf64_half      e_machine;            /**< machine type */
    elf64_word      e_version;            /**< file version */
    elf64_addr      e_entry;              /**< entry point */
    elf64_off       e_phoff;              /**< offset of program headers */
    elf64_off       e_shoff;              /**< offset of section headers */
    elf64_word      e_flags;              /**< flags */
    elf64_half      e_ehsize;             /**< size of executable header */
    elf64_half      e_phentsize;          /**< size of program header entry */
    elf64_half      e_phnum;              /**< count of program headers */
    elf64_half      e_shentsize;          /**< size of section header entry */
    elf64_half      e_shnum;              /**< count of section headers */
    elf64_half      e_shstrndx;           /**< section header table index of
                                               name string table */
} elf64_ehdr;


/**
 * @struct elf64_shdr
 * @brief 64-bit ELF section header.
 *
 * A structure containing the fields of a 64-bit ELF section header.
 * Most fields have a set of valid values, which are defined as macros
 * in elf.h.
 */
typedef struct
{
    elf64_word      sh_name;        /**< section name */
    elf64_word      sh_type;        /**< section type */
    elf64_xword     sh_flags;       /**< section flags */
    elf64_addr      sh_addr;        /**< section address in memory */
    elf64_off       sh_offset;      /**< section offset in file */
    elf64_xword     sh_size;        /**< section size */
    elf64_word      sh_link;        /**< section header table index link */
    elf64_word      sh_info;        /**< section info */
    elf64_xword     sh_addralign;   /**< section address alignment */
    elf64_xword     sh_entsize;     /**< section entry size */
} elf64_shdr;


/**
 * @struct elf64_sym
 * @brief 64-bit ELF symbol table entry.
 *
 * A structure containing the fields of a 64-bit ELF symbol table.
 * The elf.h header file contains macro definitions for extracting information
 * from the \a st_info and \a st_other fields.
 */
typedef struct
{
    elf64_word      st_name;    /**< string table index to symbol name */
    uint8_t         st_info;    /**< symbol type and binding attributes */
    uint8_t         st_other;   /**< symbol visibility */
    elf64_half      st_shndx;   /**< section header table index for the section
                                     in which this symbol is defined */
    elf64_addr      st_value;   /**< symbol value */
    elf64_xword     st_size;    /**< symbol size */
} elf64_sym;


/**
 * @struct elf64_rel
 * @brief 64-bit ELF relocation entry.
 *
 * A structure containing the fields of a 64-bit ELF relocation entry.
 * Macros for extracting information the \a r_info field are defined below.
 */
typedef struct
{
    elf64_addr      r_offset;   /**< relocation entry offset */
    elf64_xword     r_info;     /**< symbol table index and relocation type */
} elf64_rel;


/**
 * @struct elf64_rela
 * @brief 64-bit ELF relocation entry with addend.
 *
 * A structure containing the fields of a 64-bit ELF relocation entry with
 * a constant value to addend.
 * Macros for extracting information the \a r_info field are defined below.
 */
typedef struct
{
    elf64_addr      r_offset;   /**< relocation entry offset */
    elf64_xword     r_info;     /**< symbol table index and relocation type */
    elf64_sxword    r_addend;   /**< constant value to addend */
} elf64_rela;

#define ELF64_R_SYM(INFO)	((INFO) >> 32)          /**< get the relocation
                                                         entry's symbol table
                                                         index */

#define ELF64_R_TYPE(INFO)	((INFO) & 0xFFFFFFFFL)  /**< get the relocation
                                                         entry's type */

/*
 * RELOCATION TYPES
 */
#define R_x86_64_NONE           0
#define R_x86_64_64             1
#define R_x86_64_PC32           2
#define R_x86_64_GOT32          3
#define R_x86_64_PLT32          4
#define R_x86_64_COPY           5
#define R_x86_64_GLOB_DAT       6
#define R_x86_64_JMP_SLOT       7
#define R_x86_64_RELATIVE       8
#define R_x86_64_GOTPCREL       9
#define R_x86_64_32             10
#define R_x86_64_32S            11
#define R_x86_64_8              14
#define R_x86_64_TPOFF64        18
//#define R_x86_64_PC64           24


/**
 * @struct elf64_dyn
 * @brief 64-bit ELF dynamic entry.
 *
 * A structure containing the fields of a 64-bit ELF dynamic entry.
 * Valid dynamic entry tags are defined as macros in elf.h.
 */
typedef struct
{
    elf64_sxword d_tag;     /**< dynamic entry tag */
    
    union
    {
        elf64_xword d_val;  /**< dynamic entry value */
        elf64_addr  d_ptr;  /**< dynamic entry virtual address */
    } d_un;
} elf64_dyn;


/**
 * @struct elf64_phdr
 * @brief 64-bit ELF program header.
 *
 * A structure containing the fields of a 64-bit ELF program header.
 * Valid header types are defined as macros in elf.h.
 */
typedef struct
{
    elf64_word  p_type;     /**< segment type */
    elf64_word  p_flags;    /**< segment's flags */
    elf64_off   p_offset;   /**< segment offset in file */
    elf64_addr  p_vaddr;    /**< segment's virtual address in memory */
    elf64_addr  p_paddr;    /**< segment's physical address in memory */
    elf64_xword p_filesz;   /**< segment's size in file */
    elf64_xword p_memsz;    /**< segment's size in memory */
    elf64_xword p_align;    /**< segment's alignment in memory */
} elf64_phdr;

#endif      /* __ELF64_H__ */
