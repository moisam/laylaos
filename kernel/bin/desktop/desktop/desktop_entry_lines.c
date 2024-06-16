/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: desktop_entry_lines.c
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
 *  \file desktop_entry_lines.c
 *
 *  A helper function that splits the name of a desktop entry into two lines.
 *  This is then cached and used when painting desktop entries. It is also
 *  used by the file-selector widget.
 */

static void split_two_lines(struct font_t *font, char *line,
                            size_t *line_start, size_t *line_end,
                            size_t *line_pixels,
                            int maxw)
{
    char *p = line;
    int w = 0, charw;

    while(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
    {
        p++;
    }

    line_start[0] = p - line;
    line_end[0] = 0;
    line_pixels[0] = 0;
    line_start[1] = 0;
    line_end[1] = 0;
    line_pixels[1] = 0;

    // first line
    while(*p)
    {
        if(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
        {
            if(w >= maxw)
            {
                break;
            }

            line_end[0] = p - line;
            line_pixels[0] = w;
        }

        charw = char_width(font, *p);

        if(w + charw >= maxw)
        {
            if(line_pixels[0] == 0)
            {
                line_end[0] = p - line;
                line_pixels[0] = w;
            }

            break;
        }

        w += charw;
        p++;
    }

    // reached the end of the string, no second line
    if(line_pixels[0] == 0)
    {
        line_end[0] = p - line;
        line_pixels[0] = w;
        return;
    }

    // second line
    p = line + line_end[0];

    while(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
    {
        p++;
    }

    if(!*p)
    {
        return;
    }

    line_start[1] = p - line;
    w = 0;

    // account for the possible two dots at the end if the line is long
    maxw -= char_width(font, '.') * 2;

    while(*p)
    {
        charw = char_width(font, *p);

        if(w + charw >= maxw)
        {
            break;
        }

        w += charw;
        p++;
    }

    line_end[1] = p - line;
    line_pixels[1] = w;
}

