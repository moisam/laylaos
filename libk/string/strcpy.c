#include <string.h>

char *strcpy(char *s1, const char *s2)
{
  char *s3 = (char *)s1;
  while((*s1++ = *s2++)) ;
  return s3;
}
