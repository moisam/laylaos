#include <string.h>

size_t strlen(const char *str)
{
  size_t res = 0;
  while(str[res]) res++;
  return res;
}
