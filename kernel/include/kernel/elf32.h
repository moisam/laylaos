/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2022, 2023, 2024 (c)
 * 
 *    file: elf32.h
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
 *  \file elf32.h
 *
 *  Functions and macro definitions for working with files in the Executable
 *  and Linkable Format (ELF), 32-bit version. Do not include this file in 
 *  your project. Instead, you need to include elf.h, which will in turn
 *  include either elf32.h or elf64.h as appropriate.
 */

#ifndef __ELF32_H__
#define __ELF32_H__

/*
 * TYPE DEFS 
 */
typedef uint16_t elf32_half;    /**< ELF 16-bit half word */
typedef uint32_t elf32_off;     /**< ELF 32-bit offset */
typedef uint32_t elf32_addr;    /**< ELF 32-bit address */
typedef uint32_t elf32_word;    /**< ELF 32-bit word */
typedef int32_t  elf32_sword;   /**< ELF 32-bit signed word */


/**
 * @struct elf32_ehdr
 * @brief 32-bit ELF header.
 *
 * A structure containing the fields of a 32-bit ELF header.
 * Most fields have a set of valid values, which are defined as macros
 * in elf.h.
 */
typedef struct
{
    uint8_t	    e_ident[ELF_NIDENT];    /**< identification */
    elf32_half  e_type;                 /**< file type */
    elf32_half  e_machine;              /**< machine type */
    elf32_word  e_version;              /**< file version */
    elf32_addr  e_entry;                /**< entry point */
    elf32_off   e_phoff;                /**< offset of program headers */
    elf32_off   e_shoff;                /**< offset of section headers */
    elf32_word  e_flags;                /**< flags */
    elf32_half  e_ehsize;               /**< size of executable header */
    elf32_half  e_phentsize;            /**< size of program header entry */
    elf32_half  e_phnum;                /**< count of program headers */
    elf32_half  e_shentsize;            /**< size of section header entry */
    elf32_half  e_shnum;                /**< count of section headers */
    elf32_half  e_shstrndx;             /**< section header table index of
                                             name string table */
} elf32_ehdr;


/**
 * @struct elf32_shdr
 * @brief 32-bit ELF section header.
 *
 * A structure containing the fields of a 32-bit ELF section header.
 * Most fields have a set of valid values, which are defined as macros
 * in elf.h.
 */
typedef struct
{
    elf32_word  sh_name;        /**< section name */
    elf32_word  sh_type;        /**< section type */
    elf32_word  sh_flags;       /**< section flags */
    elf32_addr  sh_addr;        /**< section address in memory */
    elf32_off   sh_offset;      /**< section offset in file */
    elf32_word  sh_size;        /**< section size */
    elf32_word  sh_link;        /**< section header table index link */
    elf32_word  sh_info;        /**< section info */
    elf32_word  sh_addralign;   /**< section address alignment */
    elf32_word  sh_entsize;     /**< section entry size */
} elf32_shdr;


/**
 * @struct elf32_sym
 * @brief 32-bit ELF symbol table entry.
 *
 * A structure containing the fields of a 32-bit ELF symbol table.
 * The elf.h header file contains macro definitions for extracting information
 * from the \a st_info and \a st_other fields.
 */
typedef struct
{
    elf32_word  st_name;    /**< string table index to symbol name */
    elf32_addr  st_value;   /**< symbol value */
    elf32_word  st_size;    /**< symbol size */
    uint8_t     st_info;    /**< symbol type and binding attributes */
    uint8_t     st_other;   /**< symbol visibility */
    elf32_half  st_shndx;   /**< section header table index for the section
                                 in which this symbol is defined */
} elf32_sym;


/**
 * @struct elf32_rel
 * @brief 32-bit ELF relocation entry.
 *
 * A structure containing the fields of a 32-bit ELF relocation entry.
 * Macros for extracting information the \a r_info field are defined below.
 */
typedef struct
{
    elf32_addr  r_offset;   /**< relocation entry offset */
    elf32_word  r_info;     /**< symbol table index and relocation type */
} elf32_rel;


/**
 * @struct elf32_rela
 * @brief 32-bit ELF relocation entry with addend.
 *
 * A structure containing the fields of a 32-bit ELF relocation entry with
 * a constant value to addend.
 * Macros for extracting information the \a r_info field are defined below.
 */
typedef struct
{
    elf32_addr  r_offset;   /**< relocation entry offset */
    elf32_word  r_info;     /**< symbol table index and relocation type */
    elf32_sword r_addend;   /**< constant value to addend */
} elf32_rela;


#define ELF32_R_SYM(INFO)	((INFO) >> 8)     /**< get the relocation entry's
                                                   symbol table index */

#define ELF32_R_TYPE(INFO)	((INFO) & 0xFF)   /**< get the relocation entry's
                                                   type */

/*
 * RELOCATION TYPES
 */
#define R_386_NONE		    0
#define R_386_32		    1
#define R_386_PC32		    2
#define R_386_GOT32		    3
#define R_386_PLT32		    4
#define R_386_COPY		    5
#define R_386_GLOB_DAT	    6
#define R_386_JMP_SLOT	    7
#define R_386_RELATIVE	    8
#define R_386_TLS_TPOFF	    14


/**
 * @struct elf32_dyn
 * @brief 32-bit ELF dynamic entry.
 *
 * A structure containing the fields of a 32-bit ELF dynamic entry.
 * Valid dynamic entry tags are defined as macros in elf.h.
 */
typedef struct
{
    elf32_sword d_tag;      /**< dynamic entry tag */
    
    union
    {
        elf32_word d_val;   /**< dynamic entry value */
        elf32_addr d_ptr;   /**< dynamic entry virtual address */
    } d_un;
} elf32_dyn;


/**
 * @struct elf32_phdr
 * @brief 32-bit ELF program header.
 *
 * A structure containing the fields of a 32-bit ELF program header.
 * Valid header types are defined as macros in elf.h.
 */
typedef struct
{
    elf32_word  p_type;     /**< segment type */
    elf32_off   p_offset;   /**< segment offset in file */
    elf32_addr  p_vaddr;    /**< segment's virtual address in memory */
    elf32_addr  p_paddr;    /**< segment's physical address in memory */
    elf32_word  p_filesz;   /**< segment's size in file */
    elf32_word  p_memsz;    /**< segment's size in memory */
    elf32_word  p_flags;    /**< segment's flags */
    elf32_word  p_align;    /**< segment's alignment in memory */
} elf32_phdr;

#endif      /* __ELF32_H__ */
