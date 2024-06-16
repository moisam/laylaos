/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: rtc.h
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
 *  \file rtc.h
 *
 *  Functions to work with the Real-Time Clock (RTC).
 */

#ifndef __RTC_H__
#define __RTC_H__

/**
 * @struct systime_t
 * @brief The systime_t structure.
 *
 * A structure to represent time retrieved from the RTC.
 */
typedef struct systime_t
{
    unsigned char t_second;     /**< seconds */
    unsigned char t_minute;     /**< minutes */
    unsigned char t_hour;       /**< hours */
    unsigned char t_day;        /**< day */
    unsigned char t_month;      /**< month */
    unsigned int  t_year;       /**< year */
} systime_t;


/**
 * \enum CMOS_regs
 *
 * CMOS registers
 */
enum CMOS_regs
{
    CMOS_address    = 0x70,     /**< CMOS address register */
    CMOS_data       = 0x71      /**< CMOS data register */
};


/***********************
 * Function prototypes
 ***********************/

/**
 * @brief Read system clock.
 *
 * Read the Real-Time Clock (RTC) and return the system clock.
 *
 * @param   t       system clock is returned here
 *
 * @return  nothing.
 */
void kget_sys_clock(systime_t *t);

/**
 * @brief Read system clock.
 *
 * Read the Real-Time Clock (RTC) and return the system clock.
 *
 * @return  system clock.
 */
systime_t get_system_clock();

#include <time.h>

/**
 * @brief System time to POSIX time.
 *
 * Convert our system time to Unix Epoch time.
 *
 * @return  POSIX time.
 */
time_t systime_to_posix(systime_t *time);

#endif      /* __RTC_H__ */
