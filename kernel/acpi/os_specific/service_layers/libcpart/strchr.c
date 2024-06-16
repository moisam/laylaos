#include <string.h>

char *strchr(const char *str, int chr)
//char *strchr(char *str, int chr)
{
  do
  {
    if(*str == chr)
      return (char *)str;
  } while(*str++);
  return 0;
}
