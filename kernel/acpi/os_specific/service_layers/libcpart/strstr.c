/* Optimized strstr function.
   Copyright (c) 2018 Arm Ltd.  All rights reserved.

   SPDX-License-Identifier: BSD-3-Clause

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. The name of the company may not be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY ARM LTD ``AS IS'' AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
   IN NO EVENT SHALL ARM LTD BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/*
FUNCTION
	<<strstr>>---find string segment

INDEX
	strstr

SYNOPSIS
	#include <string.h>
	char *strstr(const char *<[s1]>, const char *<[s2]>);

DESCRIPTION
	Locates the first occurrence in the string pointed to by <[s1]> of
	the sequence of characters in the string pointed to by <[s2]>
	(excluding the terminating null character).

RETURNS
	Returns a pointer to the located string segment, or a null
	pointer if the string <[s2]> is not found. If <[s2]> points to
	a string with zero length, <[s1]> is returned.

PORTABILITY
<<strstr>> is ANSI C.

<<strstr>> requires no supporting OS subroutines.

QUICKREF
	strstr ansi pure
*/

#include <string.h>


/* Small and efficient strstr implementation.  */
char *
strstr (const char *hs, const char *ne)
{
  size_t i;
  int c = ne[0];

  if (c == 0)
    return (char*)hs;

  for ( ; hs[0] != '\0'; hs++)
    {
      if (hs[0] != c)
	continue;
      for (i = 1; ne[i] != 0; i++)
	if (hs[i] != ne[i])
	  break;
      if (ne[i] == '\0')
	return (char*)hs;
    }

  return NULL;
}

