#include <string.h>

void* memsetw(void* bufptr, int value, size_t size)
{
	unsigned short* buf = (unsigned short*) bufptr;
	for ( size_t i = 0; i < size; i++ )
		buf[i] = (unsigned short) value;
	return bufptr;
}
