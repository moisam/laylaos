/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: cal.c
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
 *  \file cal.c
 *
 *  A simple calendar program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

char *ver = "1.0";

static char *months[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static char *long_months[] =
{
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
};

static char *long_months_padded[] =
{
    "          January           ",
    "          February          ",
    "           March            ",
    "           April            ",
    "            May             ",
    "           June             ",
    "           July             ",
    "           August           ",
    "         September          ",
    "          October           ",
    "          November          ",
    "          December          ",
};

char lines[8][128];

time_t now = 0;
struct tm *tmnow = NULL;
int year, month;
int show_year = 0;
int show_three = 0;


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


void print_cal_for_month(int month, int year)
{
	// Index of the day from 0 to 6
	int current = day_number(1, 1, year);
	//int days = days_of_month(month, year);
	int days;
	int i, j, k;
	int print = 0;
	
	/*
	k = (28 - strlen(long_months[month])) / 2;
	
	for(j = 0; j < k; j++)
	{
	    printf(" ");
	}

	printf("%s %d\n", long_months[month], year);

	// Print the columns
	printf(" Sun Mon Tue Wed Thu Fri Sat\n");
	*/


	for(i = 0; i <= month; i++)
	{
		days = days_of_month(i, year);
		
		if(i == month)
		{
		    print = 1;

        	k = (28 - strlen(long_months[month]) - 5) / 2;
	
        	for(j = 0; j < k; j++)
        	{
        	    printf(" ");
        	}

        	printf("%s %d\n", long_months[month], year);

        	// Print the columns
        	printf(" Sun Mon Tue Wed Thu Fri Sat\n");
		}
		
		// Print appropriate spaces
		for(k = 0; k < current; k++)
		{
		    if(print)
		    {
			    printf("    ");
			}
		}

		for(j = 1; j <= days; j++)
		{
		    if(print)
		    {
			    printf("%4d", j);
			}

			if(++k > 6)
			{
				k = 0;

    		    if(print)
    		    {
				    printf("\n");
				}
			}
		}

		if(k && print)
		{
			printf("\n");
		}

		current = k;
	}


    /*
	for(i = 0; i <= month; i++)
	{
		days = days_of_month(i, year);
		k = current;

		for(j = 1; j <= days; j++)
		{
			if(++k > 6)
			{
				k = 0;
			}
		}

		current = k;
	}

	// Print appropriate spaces
	for(k = 0; k < current; k++)
	{
		printf("    ");
	}

	for(j = 1; j <= days; j++)
	{
		printf("%4d", j);

		if(++k > 6)
		{
			k = 0;
			printf("\n");
		}
	}

	if(k)
	{
		printf("\n");
	}
	*/
}


void print_cal_for_year(int year)
{
	int days;
	int i, j;
	//int header_col[7];
	int line_col[8];

	printf("                                          %d\n", year);

	// Index of the day from 0 to 6
	int current = day_number(1, 1, year);
	
	for(i = 0; i < 8; i++)
	{
	    lines[i][0] = '\0';
	    //header_col[i] = 0;
	    line_col[i] = 0;
	}

	// i for Iterate through months
	// j for Iterate through days
	// of the month - i
	for(i = 0; i < 12; i++)
	{
		days = days_of_month(i, year);
		
		if(i && (i % 3) == 0)
		{
		    /*
		    lines[0][line_col[0]] = '\n';
		    lines[0][line_col[0] + 1] = '\0';
		    lines[1][line_col[1]] = '\n';
		    lines[1][line_col[1] + 1] = '\0';
		    */

		    for(j = 0; j < 8; j++)
		    {
		        if(lines[j][0])
		        {
		            printf("%s\n", lines[j]);
		        }
		        
		        line_col[j] = 0;
		    }

		    printf("\n");
		}

		// Print the current month name
		//printf("\n ------------%s-------------\n", months[i]);
		sprintf(&lines[0][line_col[0]], "%s", long_months_padded[i]);

		// Print the columns
		//printf(" Sun Mon Tue Wed Thu Fri Sat\n");
		sprintf(&lines[1][line_col[1]], "%s", " Sun Mon Tue Wed Thu Fri Sat");

		// Print appropriate spaces
		int k;
		int cur_col;
		int cur_line;

		cur_col = line_col[2];
		cur_line = 2;

		for(k = 0; k < current; k++, cur_col += 4)
		{
			//printf("    ");
			sprintf(&lines[cur_line][cur_col], "    ");
		}

		for(j = 1; j <= days; j++)
		{
			//printf("%4d", j);
			sprintf(&lines[cur_line][cur_col], "%4d", j);
			cur_col += 4;

			if(++k > 6)
			{
				k = 0;
				//printf("\n");
				cur_col = line_col[++cur_line];
			}
		}

        /*
		if(k)
		{
			printf("\n");
		}
		*/

		current = k;
		
		//header_col += 28;
		
		for(j = 0; j < 8; j++)
		{
		    /*
			sprintf(&lines[j][28], "  ");
		    line_col[j] += 30;
		    */
		    lines[j][line_col[j] + 28] = ' ';
		    lines[j][line_col[j] + 29] = ' ';
		    //lines[j][line_col[j] + 30] = ' ';
		    //lines[j][line_col[j] + 31] = ' ';
		    lines[j][line_col[j] + 30] = '\0';
		    line_col[j] += 30;
		}
	}

    for(j = 0; j < 8; j++)
    {
        if(lines[j][0])
        {
            printf("%s\n", lines[j]);
        }
    }
}


void parse_line_args(int argc, char **argv) 
{
    int c, i;
    static struct option long_options[] =
    {
        {"help",    no_argument,       0, 'h'},
        {"month",   required_argument, 0, 'm'},
        {"version", no_argument,       0, 'v'},
        {"year",    required_argument, 0, 'y'},
        {"one",     no_argument,       0, '1'},
        {"three",   no_argument,       0, '3'},
        {0, 0, 0, 0}
    };
  
    while((c = getopt_long(argc, argv, "hm:y:v13", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 0:
                break;

            case '1':
                month = tmnow->tm_mon;
                break;

            case '3':
                show_three = 1;
                break;

            case 'm':
                if(optarg == NULL)
                {
                    fprintf(stderr, "%s: missing option arg: -%c\n", argv[0], c);
                    exit(EXIT_FAILURE);
                }
                
                for(i = 0; i < 12; i++)
                {
                    // try matching short and long month names
                    if(strcasecmp(optarg, long_months[i]) == 0 ||
                       strcasecmp(optarg, months[i]) == 0)
                    {
                        month = i;
                        break;
                    }
                }
                
                if(i >= 12)
                {
                    // try matching months numbers 1-12
                    i = atoi(optarg);
                    
                    if(i >= 1 && i <= 12)
                    {
                        month = i - 1;
                        break;
                    }
                }

                fprintf(stderr, "%s: invalid option arg: %s\n", argv[0], optarg);
                exit(EXIT_FAILURE);

            case 'y':
                if(optarg == NULL)
                {
                    year = tmnow->tm_year + 1900;
                    show_year = 1;
                    break;
                }

                i = atoi(optarg);
                
                if(i >= 1900 && i <= 3000)
                {
                    year = i;
                    show_year = 1;
                    break;
                }

                fprintf(stderr, "%s: invalid option arg: %s\n", argv[0], optarg);
                exit(EXIT_FAILURE);

            case 'v':
                printf("%s\n", ver);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                printf("cal utility for LaylaOS, Version %s\n\n", ver);
                printf("Usage: %s [options]\n\n"
                       "Options:\n"
                       "  -1, --one         Display the current month\n"
                       "  -3, --three       Display the previous, current and next month\n"
                       "  -h, --help        Show this help and exit\n"
                       "  -m, --month       Display the given month\n"
                       "  -v, --version     Print version and exit\n"
                       "  -y, --year        Display the given year\n"
                       "\n", argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case '?':
                exit(EXIT_FAILURE);
                break;

            default:
                abort();
        }
    }
}


int main(int argc, char **argv)
{
    time(&now);
    tmnow = gmtime(&now);
    year = tmnow->tm_year + 1900;
    month = tmnow->tm_mon;

    parse_line_args(argc, argv);
    
    if(show_year)
    {
	    // show the given year
	    print_cal_for_year(year);
	}
	else if(show_three)
	{
	    // show the month before
	    if(month == 0)
	    {
	        print_cal_for_month(11, year - 1);
	    }
	    else
	    {
	        print_cal_for_month(month - 1, year);
	    }
	    
	    printf("\n");

	    // show the month itself
	    print_cal_for_month(month, year);

	    printf("\n");

	    // show the month after
	    if(month == 11)
	    {
	        print_cal_for_month(0, year + 1);
	    }
	    else
	    {
	        print_cal_for_month(month + 1, year);
	    }
	}
	else
	{
	    // show the given month
	    print_cal_for_month(month, year);
	}
}

