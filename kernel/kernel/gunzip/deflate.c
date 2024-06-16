/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: deflate.c
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
 *  \file deflate.c
 *
 *  This file implements the kernel GZip decompression function, ingeniously
 *  called deflate(). It performs the low-level hardwork of actually deflating
 *  the archive. The high-level end is done by read_member() (defined in
 *  member.c). This implementation is not intended to be pretty or complete,
 *  just functional enough to allow us to have a GZipped initial ramdisk that
 *  we can load and unzip early during the boot process, although in reality,
 *  we are using this function in our user programs (until we port something
 *  more complete, e.g. zlib).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define __HUFFMAN_DEFS

#ifdef KERNEL

#include <kernel/laylaos.h>
#include <gunzip/deflate.h>
#include <gunzip/member.h>
#include <mm/kheap.h>

// pointer format specifier we use with printk()
#ifdef __x86_64__
# define _X                 "%lx"
#else
# define _X                 "%x"
#endif

#define RETURN_GZIP_INVALID_BLOCKDATA() \
    { printk("\b\n"); return GZIP_INVALID_BLOCKDATA; }

#else       /* !KERNEL */

#include "../../include/gunzip/deflate.h"
#include "../../include/gunzip/member.h"

#define RETURN_GZIP_INVALID_BLOCKDATA() \
    { return GZIP_INVALID_BLOCKDATA; }

#endif      /* KERNEL */


#define ASSERT_VALID_SYMBOL(type, x)    \
    if(x == (type)-1) RETURN_GZIP_INVALID_BLOCKDATA();


/*
 * For details on GZIP format, see:
 *          https://datatracker.ietf.org/doc/html/rfc1952
 *
 * For details on DEFLATE format, see:
 *          https://www.ietf.org/rfc/rfc1951.txt
 */

uint8_t *deflate_data_in;
uint32_t input_len;
uint32_t byte_pos;
uint32_t bit_pos;
uint8_t *output_stream;
uint32_t output_len;
uint32_t output_pos;

static HTREE literal[288];
static HTREE huffman[19];
static HTREE distance[32];
static unsigned tree2d_huff[19*2*sizeof(unsigned)];
static unsigned tree2d_lit[288*2*sizeof(unsigned)];
static unsigned tree2d_dist[32*2*sizeof(unsigned)];

static inline void add_bytes(int len, int dist);
static inline void add_byte(uint8_t value);
static void build_huffman_tree(HTREE tree[], int max_bits, int max_code);
static unsigned make_2d_tree(HTREE tree[], unsigned *tree2d, int max_code);
static unsigned huffman_decode_symbol(unsigned *tree2d, unsigned max_code,
                                      uint32_t data_length);
static uint16_t get_bits(int how_many);
static uint16_t get_reversed_bits(int how_many);


//check endianness of the system
static inline int is_big_endian(void)
{
    int x = 1;
    return ((((char *)&x)[0]) == 0);
}


int deflate_in_memory(char *datain, uint32_t data_length,
                      uint32_t *inbitpos, uint32_t *inbytepos,
                      char *dataout, long outlen, uint32_t *outpos)
{

#ifdef KERNEL
    printk("    Deflating to 0x" _X " (data length %s).. -",
           dataout, get_mbs(data_length));
#endif

    deflate_data_in = (uint8_t *)datain;
    byte_pos        = 0;
    bit_pos         = 0;
    output_stream   = (uint8_t *)dataout;
    output_len      = outlen;
    output_pos      = *outpos;
    input_len       = data_length;
    int i, j;
    int big_endian = is_big_endian();
    
#ifdef KERNEL
    // visual indicator index & chars
    int vii = 1;
    static char vic[] = { '-', '\\', '|', '/', '-', '\\', '|', '/' };
#endif

    while(byte_pos < input_len)
    {
        char    BFINAL = 0;
        uint8_t BTYPE;

        while(!BFINAL)
        {

#ifdef KERNEL
            printk("\b%c", vic[vii]);
            vii = (vii + 1) & ((~8) & 0x0f);
#endif
            
            BFINAL = (char   )get_bits(1);
            BTYPE  = (uint8_t)get_bits(1);
            BTYPE += (uint8_t)get_bits(1) << 1;
            
            if(BTYPE == 0)
            {
                ////////////////////////////////////////////
                //no compression for the data
                ////////////////////////////////////////////
                /* skip to the next byte boundary */
                if(bit_pos != 0)
                {
                    byte_pos++;
                }
                
                bit_pos = 0;
                uint16_t LEN;
                int16_t NLEN;
                
                if(big_endian)
                {
                    LEN = (deflate_data_in[byte_pos  ]*256)
                            +deflate_data_in[byte_pos+1];
                }
                else
                {
                    LEN = (deflate_data_in[byte_pos+1]*256)
                            +deflate_data_in[byte_pos  ];
                }
                
                //i++;
                byte_pos += 2;

                if(big_endian)
                {
                    NLEN = (deflate_data_in[byte_pos  ]*256)
                            +deflate_data_in[byte_pos+1];
                }
                else
                {
                    NLEN = (deflate_data_in[byte_pos+1]*256)
                            +deflate_data_in[byte_pos  ];
                }
                
                byte_pos += 2;
                
                if(LEN + NLEN != 65535)
                {
#ifdef KERNEL
                    printk("\b\n");
#endif
                    return GZIP_INVALID_BLOCKLEN;
                }
                
                j = 0;
                
                while(j < LEN)
                {
                    add_byte(deflate_data_in[byte_pos]);
                    byte_pos++;
                    j++;
                }
            }
            else if(BTYPE == 1)
            {
                ////////////////////////////////////////////
                //compressed with fixed Huffman codes
                ////////////////////////////////////////////
                
                for(i = 0; i < 288; i++)
                {
                    literal[i].len = (i <= 143) ? 8 :
                                     (i <= 255) ? 9 :
                                     (i <= 279) ? 7 : 8;
                }
                
                /* build the fixed Huffman table */
                build_huffman_tree(literal, 15, 288);
                /*
                tree2d_lit = (unsigned*)kmalloc(288*2*sizeof(unsigned));

                if(!tree2d_lit)
                {
                    return GZIP_INSUFFICIENT_MEMORY;
                }
                */

                make_2d_tree(literal, tree2d_lit, 288);
          
                for(i = 0; i < 32; i++)
                {
                    distance[i].len = 5;
                }

                //build the fixed Huffman table
                build_huffman_tree(distance, 15, 32);
                /*
                tree2d_dist = (unsigned*)kmalloc(32*2*sizeof(unsigned));

                if(!tree2d_dist)
                {
                    //kfree(tree2d_lit );
                    return GZIP_INSUFFICIENT_MEMORY;
                }
                */

                make_2d_tree(distance, tree2d_dist, 32);

                /////////////////////////////////////////////
                //read the Huffman-encoded data
                /////////////////////////////////////////////
                char finish_block = 0;

                while(byte_pos < input_len)
                {
                    unsigned x = huffman_decode_symbol(tree2d_lit, 288,
                                                       input_len);

                    ASSERT_VALID_SYMBOL(unsigned, x);

                    if(x < 256)
                    {
                        add_byte(x);
                    }
                    else if(x == 256)
                    {
                        finish_block = 1;
                    }
                    else
                    {
                        unsigned int x2 = x - 257;
                        int extra_bits = LEN_EXTRA_BITS[x2];
                        
                        ASSERT_VALID_SYMBOL(int, extra_bits);
            
                        int len = LEN_BASE_VAL[x2] +
                                  get_reversed_bits(extra_bits);
                        unsigned int dist_code;
                        unsigned y2;

                        y2 = huffman_decode_symbol(tree2d_dist, 32, input_len);
                        ASSERT_VALID_SYMBOL(unsigned, y2);
                        dist_code  = y2;
                        extra_bits = DIST_EXTRA_BITS[dist_code];
                        ASSERT_VALID_SYMBOL(int, extra_bits);

                        int dist = DIST_BASE_VAL[dist_code] +
                                   get_reversed_bits(extra_bits);
                        add_bytes(len, dist);
                    }
                    
                    if(finish_block)
                    {
                        break;
                    }
                }       //end reading data
                
                //kfree(tree2d_lit );
                //kfree(tree2d_dist);
            }
            else if(BTYPE == 2)
            {
                ////////////////////////////////////////////
                //compressed with dynamic Huffman codes
                ////////////////////////////////////////////
                int HLIT  = get_reversed_bits(5) + 257;
                int HDIST = get_reversed_bits(5) + 1;
                int HCLEN = get_reversed_bits(4) + 4;
                unsigned prev = 0;
                
                for(i = 0; i < HCLEN; i++)
                {
                    huffman[CODE_LENGTHS_POS[i]].len = get_reversed_bits(3);
                }
                
                for(i = HCLEN; i < 19; i++)
                {
                    huffman[CODE_LENGTHS_POS[i]].len = 0;
                }
                
                //build the Huffman table used to encode the lengths
                build_huffman_tree(huffman, 7, 19);
                /*
                tree2d_huff = (unsigned*)kmalloc(19*2*sizeof(unsigned));

                if(!tree2d_huff)
                {
                    return GZIP_INSUFFICIENT_MEMORY;
                }
                */

                make_2d_tree(huffman, tree2d_huff, 19);

                /////////////////////////////////////////////
                //read the Huffman-encoded 'literal' lengths
                /////////////////////////////////////////////
                for(i = 0; i < HLIT; i++)
                {
                    unsigned z = huffman_decode_symbol(tree2d_huff, 19,
                                                       input_len);
                    ASSERT_VALID_SYMBOL(unsigned, z);

                    int end;

                    switch(z)
                    {
                        case(0):
                        case(1):
                        case(2):
                        case(3):
                        case(4):
                        case(5):
                        case(6):
                        case(7):
                        case(8):
                        case(9):
                        case(10):
                        case(11):
                        case(12):
                        case(13):
                        case(14):
                        case(15):
                            prev = z;
                            literal[i].len = z;
                            break;

#define ASSERT_LESS_THAN_HLIT(x)    \
    if(x > HLIT) RETURN_GZIP_INVALID_BLOCKDATA();

#define FILL_WITH_LITERAL(i, val)   \
    while(i < end) { literal[i].len = val; i++; } i--;
                        
                        case(16):
                            end = i + 3 + get_reversed_bits(2);
                            ASSERT_LESS_THAN_HLIT(end);
                            FILL_WITH_LITERAL(i, prev);
                            break;
                        
                        case 17:
                            end = i + 3 + get_reversed_bits(3);
                            ASSERT_LESS_THAN_HLIT(end);
                            FILL_WITH_LITERAL(i, 0);
                            break;
                        
                        case 18:
                            end = i + 11 + get_reversed_bits(7);
                            ASSERT_LESS_THAN_HLIT(end);
                            FILL_WITH_LITERAL(i, 0);
                            break;
                        
                        default:
                            RETURN_GZIP_INVALID_BLOCKDATA();

#undef ASSERT_LESS_THAN_HLIT
#undef FILL_WITH_LITERAL

                    }   // end switch
                }

                for(i = HLIT; i < 288; i++)
                {
                    literal[i].len = 0;
                }
                
                build_huffman_tree(literal, 15, 288);
                /*
                tree2d_lit = (unsigned*)kmalloc(288*2*sizeof(unsigned));

                if(!tree2d_lit)
                {
                    kfree(tree2d_huff);
                    return GZIP_INSUFFICIENT_MEMORY;
                }
                */

                make_2d_tree(literal, tree2d_lit, 288);

                /////////////////////////////////////////////
                //read the Huffman-encoded 'distance' lengths
                /////////////////////////////////////////////
                prev = 0;

                for(i = 0; i < HDIST; i++)
                {
                    unsigned z = huffman_decode_symbol(tree2d_huff, 19,
                                                       input_len);
                    ASSERT_VALID_SYMBOL(unsigned, z);

                    int end;

                    switch(z)
                    {
                        case(0):
                        case(1):
                        case(2):
                        case(3):
                        case(4):
                        case(5):
                        case(6):
                        case(7):
                        case(8):
                        case(9):
                        case(10):
                        case(11):
                        case(12):
                        case(13):
                        case(14):
                        case(15):
                            prev = z;
                            distance[i].len = z;
                            break;

#define ASSERT_LESS_THAN_HDIST(x)   \
    if(x > HDIST) RETURN_GZIP_INVALID_BLOCKDATA();

#define FILL_WITH_DISTANCE(i, val)  \
    while(i < end) { distance[i].len = val; i++; } i--;
                        
                        case(16):
                            end = i + 3 + get_reversed_bits(2);
                            ASSERT_LESS_THAN_HDIST(end);
                            FILL_WITH_DISTANCE(i, prev);
                            break;
                        
                        case 17:
                            end = i + 3 + get_reversed_bits(3);
                            ASSERT_LESS_THAN_HDIST(end);
                            FILL_WITH_DISTANCE(i, 0);
                            break;
                        
                        case 18:
                            end = i + 11 + get_reversed_bits(7);
                            ASSERT_LESS_THAN_HDIST(end);
                            FILL_WITH_DISTANCE(i, 0);
                            break;
                        
                        default:
                            RETURN_GZIP_INVALID_BLOCKDATA();

#undef ASSERT_LESS_THAN_HLIT
#undef FILL_WITH_DISTANCE

                    }   // end switch
                }

                for(i = HDIST; i < 32; i++)
                {
                    distance[i].len = 0;
                }

                build_huffman_tree(distance, 15, 32);
                /*
                tree2d_dist = (unsigned*)kmalloc(32*2*sizeof(unsigned));

                if(!tree2d_dist)
                {
                    kfree(tree2d_huff);
                    //kfree(tree2d_lit );
                    return GZIP_INSUFFICIENT_MEMORY;
                }
                */

                make_2d_tree(distance, tree2d_dist, 32);

                /////////////////////////////////////////////
                //read the Huffman-encoded data
                /////////////////////////////////////////////
                char finish_block = 0;

                while(byte_pos < input_len)
                {
                    unsigned x = huffman_decode_symbol(tree2d_lit, 288,
                                                       input_len);
                    ASSERT_VALID_SYMBOL(unsigned, x);

                    if(x < 256)
                    {
                        add_byte(x);
                    }
                    else if(x == 256)
                    {
                        finish_block = 1;
                    }
                    else
                    {
                        int x2 = x - 257;
                        int extra_bits = LEN_EXTRA_BITS[x2];
                        ASSERT_VALID_SYMBOL(int, extra_bits);
            
                        unsigned len = LEN_BASE_VAL[x2] +
                                       get_reversed_bits(extra_bits);
                        unsigned dist_code =
                                    huffman_decode_symbol(tree2d_dist, 32,
                                                          input_len);
                        ASSERT_VALID_SYMBOL(unsigned, dist_code);
                        extra_bits = DIST_EXTRA_BITS[dist_code];
                        ASSERT_VALID_SYMBOL(int, extra_bits);
                        
                        unsigned d2 = get_reversed_bits(extra_bits);
                        unsigned dist = DIST_BASE_VAL[dist_code]+d2;
                        add_bytes(len, dist);
                    }

                    if(finish_block)
                    {
                        break;
                    }
                }       // end reading data
                
                //kfree(tree2d_huff);
                //kfree(tree2d_lit );
                //kfree(tree2d_dist);
            }
            else    // if(BTYPE == 3)
            { // error
#ifdef KERNEL
                printk("\b\n");
#endif
                return GZIP_INVALID_ENCODING;
            } // end if
        } // end while
        
        if(BFINAL)
        {
            break;
        }
    }

    *outpos = output_pos;
    
    if(inbitpos && inbytepos)
    {
        *inbitpos = bit_pos;
        *inbytepos = byte_pos;
    }

    //fprintf(stderr, "%s(): *outpos = %u\n", __func__, *outpos);

#ifdef KERNEL
    printk("\bDone\n");
#endif

    return GZIP_VALID_ARCHIVE;
}


/*
 * Add bytes to the output stream, as defined by length of bytes
 * and distance backwards from current position.
 */
inline void add_bytes(int len, int dist)
{
    int i;
    int start = output_pos-dist;
    int j = 0;

    for(i = 0; i < len; i++)
    {
        add_byte(output_stream[start+j]);
        j++;
        if(j >= dist) j = 0;
    }
}


/*
 * Add byte with given value to the output stream.
 */
inline void add_byte(uint8_t value)
{
    if(output_pos >= output_len)
    {
        output_pos++;
        return;
    }

    output_stream[output_pos] = value;
    output_pos++;
}


/*
 * Build Huffman code tree. Based on the code examples from RFC 1951,
 * see: https://www.ietf.org/rfc/rfc1951.txt
 */
static unsigned int bl_count[16];
static unsigned int next_code[16];

static void build_huffman_tree(HTREE tree[], int max_bits, int max_code)
{
    //count the number of codes for each code length
    int i;

    for(i = 0; i <= max_bits; i++)
    {
        bl_count[i] = 0;
    }
    
    for(i = 0; i < max_code; i++)
    {
        bl_count[tree[i].len]++;
    }
    
    //Find the numerical value of the smallest code for each code length
    unsigned int code = 0;
    bl_count[0] = 0;
    int bits;
    unsigned int len;

    for(bits = 1; bits <= max_bits; bits++)
    {
        code = (code + bl_count[bits-1]) << 1;
        next_code[bits] = code;
    }

    //Assign numerical values to all codes
    for(i = 0; i < max_code; i++)
    {
        len = tree[i].len;

        if(len != 0)
        {
            tree[i].code = next_code[len];
            next_code[len]++;
        }
    }
}


/*
 * Make two dimensional Huffman tree from one dimensional tree.
 * Based largely on the great work of Lode Vandevenne (2005-2012)
 * with only minor modifications. The original code is part of his
 * LodePNG codec, the source of which can be found on the website:
 *     http://lodev.org/lodepng/
 * It is released under the ZLib license, which is GPL-compatible.
 * Here we reserved the original comments as they are quite self
 * explanatory.
 */
static unsigned make_2d_tree(HTREE tree[], unsigned *tree2d, int max_code)
{
    unsigned nodefilled = 0; /* up to which node it is filled */
    unsigned i;
    int treepos = 0; /* position in the tree (1 of the numcodes columns) */
    int n;

    /*
     * convert tree1d[] to tree2d[][]. In the 2D array, a value of 32767 means
     * uninited, a value >= numcodes is an address to another bit, a
     * value < numcodes is a code. The 2 rows are the 2 possible bit values
     * (0 or 1), there are as many columns as codes - 1.
     *
     * A good huffmann tree has N * 2 - 1 nodes, of which N - 1 are internal 
     * nodes. Here, the internal nodes are stored (what their 0 and 1 option 
     * point to). There is only memory for such good tree currently, if there
     * are more nodes (due to too long length codes), error 55 will happen.
     */
    for(n = 0; n < max_code * 2; n++)
    {
        tree2d[n] = 32767;      /* 32767 here means the tree2d isn't filled
                                   there yet */
    }

    for(n = 0; n < max_code; n++)   /* the codes */
    {
        for(i = 0; i < tree[n].len; i++)    /* the bits for this code */
        {
            unsigned char bit = (unsigned char)((tree[n].code >>
                                                (tree[n].len - i - 1)) & 1);

            if(treepos > max_code-2)
            {
                return 55;      /* oversubscribed */
            }
            
            if(tree2d[2 * treepos + bit] == 32767)  /* not yet filled in */
            {
                if(i + 1 == tree[n].len)    /* last bit */
                {
                    tree2d[2 * treepos + bit] = n;  /* put the current code
                                                       in it */
                    treepos = 0;
                }
                else
                {
                    /*
                     * put address of the next step in here, first that address
                     * has to be found of course (it's just nodefilled + 1)...
                     */
                    nodefilled++;

                    /* addresses encoded with numcodes added to it */
                    tree2d[2 * treepos + bit] = nodefilled + max_code;
                    treepos = nodefilled;
                }
            }
            else
            {
                treepos = tree2d[2 * treepos + bit] - max_code;
            }
        }
    }

    for(n = 0; n < max_code * 2; n++)
    {
        if(tree2d[n] == 32767)
        {
            tree2d[n] = 0;  /* remove possible remaining 32767's */
        }
    }

    return 0;
}


/*
 * Decode the next Huffman encoded symbol from the in stream.
 * Based largely on the great work of Lode Vandevenne (2005-2012)
 * with only minor modifications. The original code is part of his
 * LodePNG codec, the source of which can be found on the website:
 *     http://lodev.org/lodepng/
 * It is released under the ZLib license, which is GPL-compatible.
 * Here we reserved the original comments as they are quite self
 * explanatory.
 */
static unsigned huffman_decode_symbol(unsigned *tree2d, unsigned max_code,
                                      uint32_t data_length)
{
    unsigned treepos = 0, ct;

    for(;;)
    {
        if(bit_pos > 7 || byte_pos >= data_length)
        {
            return (unsigned)(-1);      /* error: end of input memory reached
                                           without endcode */
        }
        
        /*
         * decode the symbol from the tree. The "readBitFromStream" code is
         * inlined in the expression below because this is the biggest 
         * bottleneck while decoding
         */
        ct = tree2d[(treepos << 1) + get_bits(1)];
        
        if(ct < max_code)
        {
            return ct;  /* the symbol is decoded, return it */
        }
        else
        {
            treepos = ct-max_code;  /* symbol not yet decoded, instead move
                                       tree position */
        }

        if(treepos >= max_code) 
        {
#ifdef KERNEL
            printk("huffman_decode_symbol: error %d, %d\n",
                   treepos, max_code);
#else
            fprintf(stderr, "huffman_decode_symbol: error %d, %d\n",
                            treepos, max_code);
#endif
            return (unsigned)(-1);      /* error: it appeared outside 
                                                  the codetree */
        }
    }
}


/*
 * Get a number of bits (passed as 'how_many') from the input stream.
 * This function returns them in the order of LSB first.
 */
static uint16_t get_reversed_bits(int how_many)
{
    if(how_many == 0)
    {
        return (uint16_t)0;
    }
    
    uint16_t res = 0;
    uint8_t  j = 0;

    while(how_many)
    {
        res |= ((deflate_data_in[byte_pos] & (1 << bit_pos)) >> bit_pos) << j;
        bit_pos++;

        if(bit_pos > 7)
        {
            bit_pos = 0;
            byte_pos++;
        }
        
        how_many--;
        j++;
    }

    return res;
}


/*
 * Get a number of bits (passed as 'how_many') from the input stream.
 * This function returns them in the order of MSB first.
 */
static uint16_t get_bits(int how_many)
{
    if(how_many == 0)
    {
        return (uint16_t)0;
    }
    
    uint16_t res = 0;

    while(how_many)
    {
        res |= ((deflate_data_in[byte_pos] & (1 << bit_pos)) >>
                            bit_pos) << (how_many - 1);
        bit_pos++;

        if(bit_pos > 7)
        {
            bit_pos = 0;
            byte_pos++;
        }
        
        how_many--;
    }

    return res;
}

