#include <time.h>
#include <stdint.h>

/*
 * Code for this function & file is adopted from Sortix libc:
 * https://gitorious.org/sortix/sortix/source/
 * Which is released under the LGPL.
 */
static const int DAYS_JANUARY	= 31;
static const int DAYS_FEBRUARY	= 28;
static const int DAYS_MARCH	= 31;
static const int DAYS_APRIL	= 30;
static const int DAYS_MAY	= 31;
static const int DAYS_JUNE	= 30;
static const int DAYS_JULY	= 31;
static const int DAYS_AUGUST	= 31;
static const int DAYS_SEPTEMBER	= 30;
static const int DAYS_OCTOBER	= 31;
static const int DAYS_NOVEMBER	= 30;
static const int DAYS_DECEMBER	= 31;

#define DECL_LEAP_SECONDS(yr, jun, dec)	\
{0, 0, 0, 0, 0, jun, 0, 0, 0, 0, 0, dec}

static int8_t leap_seconds[][12] =
{
  DECL_LEAP_SECONDS(1970, 0, 0),
  DECL_LEAP_SECONDS(1971, 0, 0),
  DECL_LEAP_SECONDS(1972, 0, 0),
  DECL_LEAP_SECONDS(1973, 1, 1),
  DECL_LEAP_SECONDS(1974, 0, 1),
  DECL_LEAP_SECONDS(1975, 0, 1),
  DECL_LEAP_SECONDS(1976, 0, 1),
  DECL_LEAP_SECONDS(1977, 0, 1),
  DECL_LEAP_SECONDS(1978, 0, 1),
  DECL_LEAP_SECONDS(1979, 0, 1),
  DECL_LEAP_SECONDS(1980, 0, 0),
  DECL_LEAP_SECONDS(1981, 1, 0),
  DECL_LEAP_SECONDS(1982, 1, 0),
  DECL_LEAP_SECONDS(1983, 1, 0),
  DECL_LEAP_SECONDS(1984, 0, 0),
  DECL_LEAP_SECONDS(1985, 1, 0),
  DECL_LEAP_SECONDS(1986, 0, 0),
  DECL_LEAP_SECONDS(1987, 0, 1),
  DECL_LEAP_SECONDS(1988, 0, 0),
  DECL_LEAP_SECONDS(1989, 0, 1),
  DECL_LEAP_SECONDS(1990, 0, 1),
  DECL_LEAP_SECONDS(1991, 0, 0),
  DECL_LEAP_SECONDS(1992, 1, 0),
  DECL_LEAP_SECONDS(1993, 1, 0),
  DECL_LEAP_SECONDS(1994, 1, 0),
  DECL_LEAP_SECONDS(1995, 0, 1),
  DECL_LEAP_SECONDS(1996, 0, 0),
  DECL_LEAP_SECONDS(1997, 1, 0),
  DECL_LEAP_SECONDS(1998, 0, 1),
  DECL_LEAP_SECONDS(1999, 0, 0),
  DECL_LEAP_SECONDS(2000, 0, 0),
  DECL_LEAP_SECONDS(2001, 0, 0),
  DECL_LEAP_SECONDS(2002, 0, 0),
  DECL_LEAP_SECONDS(2003, 0, 0),
  DECL_LEAP_SECONDS(2004, 0, 0),
  DECL_LEAP_SECONDS(2005, 0, 1),
  DECL_LEAP_SECONDS(2006, 0, 0),
  DECL_LEAP_SECONDS(2007, 0, 0),
  DECL_LEAP_SECONDS(2008, 0, 1),
  DECL_LEAP_SECONDS(2009, 0, 0),
  DECL_LEAP_SECONDS(2010, 0, 0),
  DECL_LEAP_SECONDS(2011, 0, 0),
  DECL_LEAP_SECONDS(2012, 1, 0),
  DECL_LEAP_SECONDS(2013, 0, 0),
  DECL_LEAP_SECONDS(2014, 0, 0),
  DECL_LEAP_SECONDS(2015, 1, 0),
};

static time_t get_leap_second(int yr, int month)
{
  const time_t num_years = sizeof(leap_seconds)/sizeof(leap_seconds[0]);
  if(yr < 1970)
    return 0;
  if(yr-1970 >= num_years)
    return 0;
  return leap_seconds[yr-1970][month];
}

static time_t leap_seconds_in_yr(int yr)
{
  time_t ret = 0;
  for(int i = 0; i < 12; i++)
    ret += get_leap_second(yr, i);
  return ret;
}

static int is_leap_yr(int yr)
{
  return (yr%4 == 0 && yr%100 != 0) || yr%400 == 0;
}

static time_t days_in_yr(int yr)
{
  return DAYS_JANUARY +
	 DAYS_FEBRUARY + (is_leap_yr(yr) ? 1 : 0) +
	 DAYS_MARCH +
	 DAYS_APRIL +
	 DAYS_MAY +
	 DAYS_JUNE +
	 DAYS_JULY +
	 DAYS_AUGUST +
	 DAYS_SEPTEMBER +
	 DAYS_OCTOBER +
	 DAYS_NOVEMBER +
	 DAYS_DECEMBER;
}


struct tm* _gmtime(const time_t* time_ptr, struct tm* __tm)
{
  time_t left = *time_ptr;
  __tm->tm_year = 1970;
  __tm->tm_wday = 4;
  
  /* if timestamp is after the Epoch */
  while(left > 0)
  {
    time_t year_leaps = leap_seconds_in_yr(__tm->tm_year);
    time_t year_days = days_in_yr(__tm->tm_year);
    time_t year_seconds = year_days*24*60*60 + year_leaps;
    if(year_seconds <= left)
    {
      left -= year_seconds;
      __tm->tm_wday = (__tm->tm_wday + year_days) % 7;
      __tm->tm_year++;
      continue;
    }
    break;
  }

  /* if timestamp is before the Epoch */
  while(left < 0)
  {
    __tm->tm_year--;
    time_t year_leaps = leap_seconds_in_yr(__tm->tm_year);
    time_t year_days = days_in_yr(__tm->tm_year);
    time_t year_seconds = year_days*24*60*60 + year_leaps;
    left += year_seconds;
    __tm->tm_wday = (__tm->tm_wday + year_days + 7*7*7*7) % 7;
  }

  int month_days_list[12] =
  {
    DAYS_JANUARY,
    DAYS_FEBRUARY + (is_leap_yr(__tm->tm_year) ? 1 : 0),
    DAYS_MARCH,
    DAYS_APRIL,
    DAYS_MAY,
    DAYS_JUNE,
    DAYS_JULY,
    DAYS_AUGUST,
    DAYS_SEPTEMBER,
    DAYS_OCTOBER,
    DAYS_NOVEMBER,
    DAYS_DECEMBER,
  };
  
  /* find the month */
  __tm->tm_mon = 0;
  __tm->tm_yday = 0;
  while(1)
  {
    int month_leaps = get_leap_second(__tm->tm_year, __tm->tm_mon);
    int month_days = month_days_list[__tm->tm_mon];
    int month_seconds = month_days*24*60*60 + month_leaps;
    if(month_seconds <= left)
    {
      left -= month_seconds;
      __tm->tm_mon++;
      __tm->tm_yday = month_days;
      __tm->tm_wday = (__tm->tm_wday + month_days) % 7;
      continue;
    }
    break;
  }
  __tm->tm_mday = left/(24*60*60);
  left = left%(24*60*60);
  
  /* if this is a regular timestamp */
  if(__tm->tm_mday < month_days_list[__tm->tm_mon])
  {
    __tm->tm_yday += __tm->tm_mday;
    __tm->tm_hour = left/(60*60);
    left = left%(60*60);
    __tm->tm_min = left/60;
    left = left%60;
    __tm->tm_sec = left;
  }
  else
  {
    __tm->tm_mday--;
    __tm->tm_yday += __tm->tm_mday;
    __tm->tm_hour = 23;
    __tm->tm_min = 59;
    __tm->tm_sec = 60;
  }
  __tm->tm_wday = (__tm->tm_wday + __tm->tm_mday) % 7;
  __tm->tm_isdst = -1;
  __tm->tm_mday += 1;
  __tm->tm_year -= 1900;
  return __tm;
}


struct tm* gmtime(const time_t* time_ptr)
{
  static struct tm __tm;
  return _gmtime(time_ptr, &__tm);
}


time_t timegm(struct tm* tm)
{
  time_t yr = tm->tm_year+1900;
  time_t month = tm->tm_mon;
  time_t day = tm->tm_mday - 1;
  time_t hour = tm->tm_hour;
  time_t minute = tm->tm_min;
  time_t second = tm->tm_sec;
  time_t ret = 0;
  
  for(time_t y = 1970; y < yr; y++)
  {
    time_t year_leaps = leap_seconds_in_yr(y);
    time_t year_days = days_in_yr(y);
    time_t year_seconds = year_days*24*60*60 + year_leaps;
    ret += year_seconds;
  }
  
  int month_days_list[12] = 
  {
    DAYS_JANUARY,
    DAYS_FEBRUARY + (is_leap_yr(yr) ? 1 : 0),
    DAYS_MARCH,
    DAYS_APRIL,
    DAYS_MAY,
    DAYS_JUNE,
    DAYS_JULY,
    DAYS_AUGUST,
    DAYS_SEPTEMBER,
    DAYS_OCTOBER,
    DAYS_NOVEMBER,
    DAYS_DECEMBER,
  };
  
  for(uint8_t m = 0; m < month; m++)
  {
    int month_leaps = get_leap_second(yr, m);
    int month_days = month_days_list[m];
    int month_seconds = month_days*24*60*60 + month_leaps;
    ret += month_seconds;
  }
  
  ret += (time_t) day*24*60*60;
  ret += (time_t) hour*60*60;
  ret += (time_t) minute*60;
  ret += (time_t) second*1;
  
  _gmtime(&ret, tm);		/* NOTE: I need to figure out why are we doing this step??? */
  
  return ret;
}
