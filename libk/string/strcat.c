#include <string.h>

char *strcat(char *__restrict s1, const char *__restrict s2)
//char *strcat(char *s1, char *s2)
{
  //if(!s1 || !s2)
  //  return NULL;

  while(*s1++) ;
  s1--;

  while(*s2)
    *s1++ = *s2++;
  *s1 = '\0';
  return s1;
}
