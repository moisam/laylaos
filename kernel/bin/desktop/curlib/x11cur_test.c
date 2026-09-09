#include <stdio.h>
#include <stdlib.h>
#include "../include/cursor.h"
#include "../include/cursor-struct-alloc.h"


int main(int argc, char **argv)
{
    struct cursor_t *cursor;
    struct cursor_bitmap_t *bitmap;
    int i, j, k, n;

    if(argc != 2)
    {
        fprintf(stderr, "%s: usage: %s x11cur-filename\n", argv[0], argv[0]);
        exit(1);
    }

    if(!(cursor = x11_cursor_load(argv[1])))
    {
        fprintf(stderr, "%s: failed to load X11 cursor\n", argv[0]);
        exit(1);
    }
    

    for(n = 0; n < cursor->count; n++)
    {
        k = 0;
        bitmap = &cursor->bitmaps[n];
        printf("Cursor [%d] - w %d, h %d, hotx %d, hoty %d, delay %d\n",
               n, bitmap->w, bitmap->h, bitmap->hotx, bitmap->hoty, bitmap->delay);

        for(i = 0; i < bitmap->h; i++)
        {
            printf("[%d] ", i);

            for(j = 0; j < bitmap->w; j++, k++)
            {
                printf("%08x ", bitmap->data[k]);
            }
        
            printf("\n");
        }
    }
    
    exit(0);
}

