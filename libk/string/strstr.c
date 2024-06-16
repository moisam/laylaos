#include <string.h>

char *strstr(const char *str1, const char *str2)
{
  const char *mystr1 = (const char *)str1;
  const char *mystr2 = (const char *)str2;
  do
  {
    if(*mystr1 == *mystr2)
    {
      const char *str3 = (const char *)mystr1;
      while(*mystr1 == *mystr2 && *mystr2)
      {
	mystr2++; mystr1++;
      }
      if(!*mystr2)
	return (char *)str3;
    }
    mystr1++;
  } while(*mystr1 && *mystr2);
  return 0;
}
