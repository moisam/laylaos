#include <stdio.h>
#include <stdlib.h>
#include "../include/bitmap.h"


int main(int argc, char **argv)
{
    struct bitmap32_t bitmap;
    unsigned int i, j, k;

    if(argc != 2)
    {
        fprintf(stderr, "%s: usage: %s png-filename\n", argv[0], argv[0]);
        exit(1);
    }

    if(!png_load(argv[1], &bitmap))
    {
        fprintf(stderr, "%s: failed to load PNG image\n", argv[0]);
        exit(1);
    }
    
    k = 0;

    for(i = 0; i < bitmap.height; i++)
    {
        printf("[%d] ", i);

        for(j = 0; j < bitmap.width; j++, k++)
        {
            printf("%08x ", bitmap.data[k]);
        }
        
        printf("\n");
    }
    
    exit(0);
}

