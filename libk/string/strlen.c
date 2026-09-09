#include <stdint.h>
#include <string.h>

#ifdef __x86_64__

size_t strlen(const char *str)
{
    const char *char_ptr = str;
    
    // Process unaligned bytes until we hit an 8-byte memory boundary
    while((uintptr_t)char_ptr & 7)
    {
        if(*char_ptr == '\0')
        {
            return char_ptr - str;
        }

        char_ptr++;
    }

    // Main alignment look-ahead chunk loop (Processes 8 bytes per loop iteration)
    const uint64_t *word_ptr = (const uint64_t *)char_ptr;
    
    // Magic masks to isolate zero bytes across a 64-bit register integer
    const uint64_t high_bits = 0x8080808080808080ULL;
    const uint64_t low_bits  = 0x0101010101010101ULL;

    while(1)
    {
        uint64_t word = *word_ptr;
        
        // This bitwise formula isolates if any byte slot inside the 64-bit word is 0x00
        // It subtracts 1 from all bytes, checks if high bits flipped, and masks out non-zero bits
        uint64_t has_zero = (word - low_bits) & ~word & high_bits;
        
        if(has_zero != 0)
        {
            // Null terminator detected inside this 8-byte chunk!
            // __builtin_ctzll counts trailing zero bits in a single CPU clock cycle (TZCNT/BSF assembly)
            int trailing_zeros = __builtin_ctzll(has_zero);
            
            // Divide by 8 bits to convert bit index to the exact byte offset inside this word
            return ((const char *)word_ptr - str) + (trailing_zeros >> 3);
        }

        word_ptr++;
    }
}

#else       /* __x86_64__ */

size_t strlen(const char *str)
{
  size_t res = 0;
  while(str[res]) res++;
  return res;
}

#endif      /* __x86_64__ */
