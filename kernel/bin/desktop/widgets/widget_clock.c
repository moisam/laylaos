/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: widget_clock.c
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
 *  \file widget_clock.c
 *
 *  The clock widget. This is shown in the right corner of the top panel.
 */

#include <string.h>
#include "../include/panels/widget.h"


static char *weekdays[] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

static char *months[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", 
    "Aug", "Sep", "Oct", "Nov", "Dec",
};

static char *long_months[] =
{
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
};


static int last_min = 0;

time_t t = 0;
struct tm *tm = NULL;


/*
 * Repaint the widget itself.
 */
void widget_repaint_clock(struct window_t *widget_win, int is_active_child)
{
    struct widget_t *widget = (struct widget_t *)widget_win;
    size_t len;
    static char buf[32] = { 0, };
    int x, y;

    widget_fill_background(widget);

    sprintf(buf, "%s %d %s %02u:%02u", weekdays[tm->tm_wday], tm->tm_mday,
                                       months[tm->tm_mon], tm->tm_hour, 
                                       tm->tm_min);
    len = widget_string_width(buf);

    x = (widget_win->w / 2) - (len / 2);
    y = (widget_win->h / 2) - (widget_char_height() / 2);

    if(x < 0)
    {
        x = 0;
    }

    // Draw the title centered within the widget
    widget_draw_text(widget, buf, x, y, widget_fg_color(widget));
}


int widget_periodic_clock(struct widget_t *widget)
{
    struct window_t *widget_win = (struct window_t *)widget;
    time(&t);
    tm = gmtime(&t);

    if(last_min == tm->tm_min)
    {
        return 0;
    }

    last_min = tm->tm_min;
    widget_repaint_clock(widget_win, 
                            widget_win == widget_win->parent->active_child);

    return 1;
}


// Function that returns the index of the
// day for date DD/MM/YYYY
int day_number(int day, int month, int year)
{
	static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

	year -= month < 3;

	return (year + year / 4
			- year / 100
			+ year / 400
			+ t[month - 1] + day) % 7;
}


// Function to return the number of days
// in a month
int days_of_month(int month, int year)
{
    static int mon[] = { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	// February
	if(month == 1)
	{
		// If the year is leap then Feb has 29 days
		if(year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))
		{
			return 29;
		}
		else
		{
			return 28;
		}
	}
	
	return mon[month];
}


/*
 * Repaint the widget's menu frame.
 */
void widget_menu_repaint_calendar(struct window_t *frame, int unused)
{
    (void)unused;

    int year = tm->tm_year + 1900;
	// Index of the day from 0 to 6
	int current = day_number(1, 1, year);
	int days;
	int i, j, k;
    int x = 0, y = 0;
	int print = 0;
	int charh = widget_char_height();
	size_t len;
	uint32_t textcolor = widget_menu_fg_color();
	uint32_t hicolor = widget_menu_hi_color();
    static char buf[32] = { 0, };
    
    widget_menu_fill_background(frame);

	for(i = 0; i <= tm->tm_mon; i++)
	{
		days = days_of_month(i, tm->tm_year);
		
		if(i == tm->tm_mon)
		{
		    print = 1;

            sprintf(buf, "%s %d %s %d", weekdays[tm->tm_wday], tm->tm_mday,
                                        long_months[tm->tm_mon], 
                                        tm->tm_year + 1900);
            len = widget_string_width(buf);
            x = (frame->w / 2) - (len / 2);
            y = charh;
            widget_menu_draw_text(frame, buf, x, y, textcolor);

        	y += (charh * 2);
        	x = 8 /* charw */;

        	if(widget_is_monospace_font())
        	{
            	sprintf(buf, " Sun Mon Tue Wed Thu Fri Sat");
                widget_menu_draw_text(frame, buf, x, y, textcolor);
            }
            else
            {
                for(k = 0; k < 7; k++)
                {
                    sprintf(buf, " %s", weekdays[k]);
                    widget_menu_draw_text(frame, buf, x, y, textcolor);
                    x += 8 * 4;
                }

            	x = 8 /* charw */;
            }

        	y += (charh * 2);
		}
		
		// Print appropriate spaces
		for(k = 0; k < current; k++)
		{
		    if(print)
		    {
		        x += (8 /* charw */ * 4);
			}
		}

		for(j = 1; j <= days; j++)
		{
		    if(print)
		    {
		        // highlight the current day
		        if(j == tm->tm_mday)
		        {
                    widget_menu_fill_rect(frame, x + 8 /* charw */, 
                                          y - (charh >> 1), 
                                          3 * 8 /* charw */, 
                                          charh * 2, hicolor);
		        }
		        
			    sprintf(buf, "%d", j);
			    
			    if(j < 10)
			    {
                    widget_menu_draw_text(frame, buf,
                                          x + (8 /* charw */ << 1), y,
                                          textcolor);
			    }
			    else
			    {
                    widget_menu_draw_text(frame, buf,
                                          x + 8 /* charw */ + (8 >> 1), y,
                                          textcolor);
			    }

            	x += (8 /* charw */ * 4);
			}

			if(++k > 6)
			{
				k = 0;

    		    if(print)
    		    {
                	x = 8 /* charw */;
                	y += (charh * 2);
				}
			}
		}

		current = k;
	}
}


/*
 * Handle mouse up events on the widget itself.
 * This toggles showing/hiding the widget's menu frame.
 */
void widget_mouseup_clock(struct widget_t *widget, int mouse_x, int mouse_y)
{
    // toggle showing/hiding the panel menu
    if(widget->menu)
    {
        if((widget->menu->flags & WINDOW_HIDDEN))
        {
            widget_menu_show(widget);
        }
        else
        {
            widget_menu_hide(widget);
        }
    }
    else
    {
        struct window_t *menu;
        int charh = widget_char_height();
        int w = (4 * 7 * 8 /* charw */) + (8 /* charw */ * 4);
        int h = (16 * charh);
        
        if((menu = widget_menu_create(w, h)))
        {
            menu->repaint = widget_menu_repaint_calendar;
            widget->menu = menu;
            widget_menu_repaint_calendar(menu, 0);
            widget_menu_show(widget);
        }
    }
}


int widget_init_clock(void)
{
    struct widget_t *widget;

    time(&t);
    tm = gmtime(&t);
    last_min = tm->tm_min;

    if(!(widget = widget_create()))
    {
        return 0;
    }
    
    widget->win.w = 200;
    widget->win.repaint = widget_repaint_clock;
    widget->periodic = widget_periodic_clock;
    widget->button_click_callback = widget_mouseup_clock;
    widget->flags |= WIDGET_FLAG_INITIALIZED;
    
    return 1;
}

