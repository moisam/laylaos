/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: rtc.c
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
 *  \file rtc.c
 *
 *  Code to read the Real-Time Clock (RTC).
 */

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <kernel/io.h>
#include <kernel/rtc.h>

#define CURRENT_YEAR    COMPILE_YEAR

int century_reg;

extern time_t timegm(struct tm* tm);


static int get_update_inprogress_flag(void)
{
    outb(CMOS_address, 0x0A);
    return (inb(CMOS_data) & 0x80);
}


static unsigned char get_RTC_reg(int reg)
{
    outb(CMOS_address, reg);
    return inb(CMOS_data);
}


/*
 * Read system clock.
 */
void kget_sys_clock(systime_t *t)
{
    if(!t)
    {
        return;
    }

    systime_t st = get_system_clock();
    memcpy((void *)t, (void *)&st, sizeof(systime_t));
}


systime_t get_system_clock(void)
{
    systime_t st;

    century_reg = 0x00;

    unsigned char century = 0;
    unsigned char last_second;
    unsigned char last_minute;
    unsigned char last_hour;
    unsigned char last_day;
    unsigned char last_month;
    unsigned char last_year;
    unsigned char last_century;
    unsigned char register_B;

    /*
     * Reads registers until same value is got twice to avoid
     * dodgy/inconsistent values due to RTC updates.
     */
    while(get_update_inprogress_flag()) 
    {
        ;
    }

    st.t_second = get_RTC_reg(0x00);
    st.t_minute = get_RTC_reg(0x02);
    st.t_hour   = get_RTC_reg(0x04);
    st.t_day    = get_RTC_reg(0x07);
    st.t_month  = get_RTC_reg(0x08);
    st.t_year   = get_RTC_reg(0x09);

    if(century_reg != 0)
    {
        century = get_RTC_reg(century_reg);
    }

    do
    {
        last_second = st.t_second;
        last_minute = st.t_minute;
        last_hour   = st.t_hour;
        last_day    = st.t_day;
        last_month  = st.t_month;
        last_year   = st.t_year;
        last_century= century;
        
        while(get_update_inprogress_flag())
        {
            ;
        }

        st.t_second = get_RTC_reg(0x00);
        st.t_minute = get_RTC_reg(0x02);
        st.t_hour   = get_RTC_reg(0x04);
        st.t_day    = get_RTC_reg(0x07);
        st.t_month  = get_RTC_reg(0x08);
        st.t_year   = get_RTC_reg(0x09);

        if(century_reg != 0)
        {
            century = get_RTC_reg(century_reg);
        }
    } while((last_second != st.t_second) || 
            (last_minute != st.t_minute) ||
	        (last_hour != st.t_hour) || 
	        (last_day != st.t_day) || 
	        (last_month != st.t_month) ||
	        (last_year != st.t_year) || 
	        (last_century != century));

    register_B = get_RTC_reg(0x0B);

    /* Convert BCD to binary if necessary */
    if(!(register_B & 0x04))
    {
        st.t_second = (st.t_second & 0x0F) + ((st.t_second/16)*10);
        st.t_minute = (st.t_minute & 0x0F) + ((st.t_minute/16)*10);
        st.t_hour   = ((st.t_hour  & 0x0F) + (((st.t_hour & 0x70)/16)*10)) | 
                                (st.t_hour & 0x80);
        st.t_day    = (st.t_day    & 0x0F) + ((st.t_day/16)*10);
        st.t_month  = (st.t_month  & 0x0F) + ((st.t_month/16)*10);
        st.t_year   = (st.t_year   & 0x0F) + ((st.t_year/16)*10);

        if(century_reg != 0)
        {
            century = (century & 0x0F) + ((century/16)*10);
        }
    }

    /* Convert 12hr clock to 24hr clock if necessary */
    if(!(register_B & 0x02) && (st.t_hour & 0x80))
    {
        st.t_hour = ((st.t_hour & 0x7F) + 12) % 24;
    }

    /* calculate full 4-digit year */
    if(century_reg != 0)
    {
        st.t_year += century * 100;
    }
    else
    {
        st.t_year += (CURRENT_YEAR/100)*100;

        if(st.t_year < CURRENT_YEAR) 
        {
            st.t_year += 100;
        }
    }

    return st;
}


/*
 * Convert our system time to Unix Epoch time.
 */
time_t systime_to_posix(systime_t *time)
{
  struct tm t;

  t.tm_year = time->t_year-1900;
  t.tm_mon  = time->t_month-1;
  t.tm_mday = time->t_day;
  t.tm_hour = time->t_hour;
  t.tm_min  = time->t_minute;
  t.tm_sec  = time->t_second;
  
  return timegm(&t);
}

