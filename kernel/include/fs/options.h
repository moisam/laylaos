/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: options.h
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
 *  \file options.h
 *
 *  Functions and macros for parsing filesystem mounting options.
 */

#ifndef __KERNEL_FS_OPTIONS_H__
#define __KERNEL_FS_OPTIONS_H__

/**
 * \def OPS_FLAG_REPORT_ERRORS
 *
 * flag to tell the kernel to report errors in mounting option strings
 */
#define OPS_FLAG_REPORT_ERRORS          0x1

/**
 * \def OPS_FLAG_IGNORE_UNKNOWN
 *
 * flag to tell the kernel to ignore unknown mounting options
 */
#define OPS_FLAG_IGNORE_UNKNOWN         0x2


/**
 * @struct ops_t
 * @brief The ops_t structure.
 *
 * A structure to represent a mounting option inside the kernel.
 */
struct ops_t
{
    char *name;         /**< option name */
    
    union
    {
        size_t i;       /**< the numeric value that is parsed from the supplied
                             options string */
        char *s;        /**< the string value that is parsed from the supplied
                             options string */
    } val;
    
    char is_required,   /**< non-zero if this option is required */
         is_int,        /**< non-zero if the value is numeric */
         is_present;    /**< non-zero if the option is present in the
                             supplied options string */
};


/**
 * @brief Parse mount options.
 *
 * Parse the options string passed by the user when mounting a filesystem.
 * The options string is supplied in \a str. Valid options are supplied by
 * the filesystem driver in \a ops (and their count in \a ops_count). Flags
 * can be zero or a combination of \ref OPS_FLAG_REPORT_ERRORS and
 * \ref OPS_FLAG_IGNORE_UNKNOWN.
 *
 * @param   module      name of the calling module (used when printing error
 *                        messages)
 * @param   str         options string to be parsed
 * @param   ops         valid options
 * @param   ops_count   valid option count
 * @param   flags       parsing flags
 *
 * @return  zero on success, -(errno) on failure.
 */
int parse_options(char *module, char *str, struct ops_t *ops,
                  int ops_count, int flags);

/**
 * @brief Free mount options.
 *
 * Free the strings pointed to by the given options array.
 *
 * @param   ops         options
 * @param   ops_count   option count
 *
 * @return  nothing.
 */
void free_option_strings(struct ops_t *ops, int ops_count);

#endif      /* __KERNEL_FS_OPTIONS_H__ */
