#include <string.h>

int strcmp(const char *str1, const char *str2)
{
  //printf("strcmp(): %x[%d], %x[%d]\n", str1, str1[0], str2, str2[0]);
  //printf("strcmp(): %x, %x\n", (unsigned int)str1, (unsigned int)str2);
  int res = 0;
  while(!(res = *(unsigned char*)str1 - *(unsigned char*)str2) && *str2)
    ++str1, ++str2;

  if(res < 0)
    return -1;
  else if (res > 0)
      return 1;
    
  return 0;
}
