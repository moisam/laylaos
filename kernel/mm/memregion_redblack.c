/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: memregion_redblack.c
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
 *  \file memregion_redblack.c
 *
 *  Functions for working with Red-Black memory region trees.
 */

#include <mm/mmap.h>
#include <mm/memregion.h>

static inline void rb_update_subtree_max(volatile struct memregion_t *node)
{
    if(!node)
    {
        return;
    }
    
    virtual_addr max_val = REGION_END(node);
    
    if(node->rb_left && node->rb_left->subtree_max_high > max_val)
    {
        max_val = node->rb_left->subtree_max_high;
    }

    if(node->rb_right && node->rb_right->subtree_max_high > max_val)
    {
        max_val = node->rb_right->subtree_max_high;
    }
    
    node->subtree_max_high = max_val;
}


static void rb_rotate_left(struct task_vm_t *mm, volatile struct memregion_t *x)
{
    volatile struct memregion_t *y = x->rb_right;

    x->rb_right = y->rb_left;

    if(y->rb_left != NULL)
    {
        y->rb_left->rb_parent = x;
    }

    y->rb_parent = x->rb_parent;

    if(x->rb_parent == NULL)
    {
        mm->rb_root = y;
    }
    else if(x == x->rb_parent->rb_left)
    {
        x->rb_parent->rb_left = y;
    }
    else
    {
        x->rb_parent->rb_right = y;
    }

    y->rb_left = x;
    x->rb_parent = y;

    rb_update_subtree_max(x);
    rb_update_subtree_max(y);
}


static void rb_rotate_right(struct task_vm_t *mm, volatile struct memregion_t *x)
{
    volatile struct memregion_t *y = x->rb_left;

    x->rb_left = y->rb_right;

    if(y->rb_right != NULL)
    {
        y->rb_right->rb_parent = x;
    }

    y->rb_parent = x->rb_parent;

    if(x->rb_parent == NULL)
    {
        mm->rb_root = y;
    }
    else if(x == x->rb_parent->rb_right)
    {
        x->rb_parent->rb_right = y;
    }
    else
    {
        x->rb_parent->rb_left = y;
    }

    y->rb_right = x;
    x->rb_parent = y;

    rb_update_subtree_max(x);
    rb_update_subtree_max(y);
}


static void rb_insert_fixup(struct task_vm_t *mm, volatile struct memregion_t *z)
{
    while(z->rb_parent && z->rb_parent->rb_color == RB_RED)
    {
        if(z->rb_parent == z->rb_parent->rb_parent->rb_left)
        {
            volatile struct memregion_t *y = z->rb_parent->rb_parent->rb_right;

            if(y && y->rb_color == RB_RED)
            {
                z->rb_parent->rb_color = RB_BLACK;
                y->rb_color = RB_BLACK;
                z->rb_parent->rb_parent->rb_color = RB_RED;
                z = z->rb_parent->rb_parent;
            }
            else
            {
                if(z == z->rb_parent->rb_right)
                {
                    z = z->rb_parent;
                    rb_rotate_left(mm, z);
                }

                z->rb_parent->rb_color = RB_BLACK;
                z->rb_parent->rb_parent->rb_color = RB_RED;
                rb_rotate_right(mm, z->rb_parent->rb_parent);
            }
        }
        else
        {
            volatile struct memregion_t *y = z->rb_parent->rb_parent->rb_left;

            if(y && y->rb_color == RB_RED)
            {
                z->rb_parent->rb_color = RB_BLACK;
                y->rb_color = RB_BLACK;
                z->rb_parent->rb_parent->rb_color = RB_RED;
                z = z->rb_parent->rb_parent;
            }
            else
            {
                if(z == z->rb_parent->rb_left)
                {
                    z = z->rb_parent;
                    rb_rotate_right(mm, z);
                }

                z->rb_parent->rb_color = RB_BLACK;
                z->rb_parent->rb_parent->rb_color = RB_RED;
                rb_rotate_left(mm, z->rb_parent->rb_parent);
            }
        }
    }

    mm->rb_root->rb_color = RB_BLACK;
}


void rb_insert(struct task_vm_t *mm, volatile struct memregion_t *new_region)
{
    new_region->rb_left = NULL;
    new_region->rb_right = NULL;
    new_region->rb_parent = NULL;
    new_region->rb_color = RB_RED;

    volatile struct memregion_t *x = mm->rb_root;
    volatile struct memregion_t *y = NULL;

    while(x != NULL)
    {
        y = x;

        if(new_region->addr < x->addr)
        {
            x = x->rb_left;
        }
        else
        {
            x = x->rb_right;
        }
    }

    new_region->rb_parent = y;

    if(y == NULL)
    {
        mm->rb_root = new_region;
    }
    else if(new_region->addr < y->addr)
    {
        y->rb_left = new_region;
    }
    else
    {
        y->rb_right = new_region;
    }

    rb_insert_fixup(mm, new_region);

    // Start at the immediate parent node and walk all the way to the top root node.
    // Because rotations already update x and y internally, this handles the remaining
    // ancestral branch paths.
    volatile struct memregion_t *p = new_region->rb_parent;

    while(p != NULL)
    {
        rb_update_subtree_max(p);
        p = p->rb_parent;
    }
}


static void rb_delete_fixup(struct task_vm_t *mm, volatile struct memregion_t *x, volatile struct memregion_t *x_parent)
{
    while(x != mm->rb_root && (x == NULL || x->rb_color == RB_BLACK))
    {
        if(x == x_parent->rb_left)
        {
            volatile struct memregion_t *w = x_parent->rb_right;

            if(w && w->rb_color == RB_RED)
            {
                w->rb_color = RB_BLACK;
                x_parent->rb_color = RB_RED;
                rb_rotate_left(mm, x_parent);
                w = x_parent->rb_right;
            }

            if((w == NULL || w->rb_left == NULL || w->rb_left->rb_color == RB_BLACK) &&
               (w == NULL || w->rb_right == NULL || w->rb_right->rb_color == RB_BLACK))
            {
                if(w)
                {
                    w->rb_color = RB_RED;
                }

                x = x_parent;
                x_parent = x->rb_parent;
            }
            else
            {
                if(w && (w->rb_right == NULL || w->rb_right->rb_color == RB_BLACK))
                {
                    if(w->rb_left)
                    {
                        w->rb_left->rb_color = RB_BLACK;
                    }

                    w->rb_color = RB_RED;
                    rb_rotate_right(mm, w);
                    w = x_parent->rb_right;
                }

                if(w)
                {
                    w->rb_color = x_parent->rb_color;

                    if(w->rb_right)
                    {
                        w->rb_right->rb_color = RB_BLACK;
                    }
                }

                x_parent->rb_color = RB_BLACK;
                rb_rotate_left(mm, x_parent);
                x = mm->rb_root;
                break;
            }
        }
        else
        {
            volatile struct memregion_t *w = x_parent->rb_left;

            if(w && w->rb_color == RB_RED)
            {
                w->rb_color = RB_BLACK;
                x_parent->rb_color = RB_RED;
                rb_rotate_right(mm, x_parent);
                w = x_parent->rb_left;
            }

            if((w == NULL || w->rb_right == NULL || w->rb_right->rb_color == RB_BLACK) &&
               (w == NULL || w->rb_left == NULL || w->rb_left->rb_color == RB_BLACK))
            {
                if(w)
                {
                    w->rb_color = RB_RED;
                }

                x = x_parent;
                x_parent = x->rb_parent;
            }
            else
            {
                if(w && (w->rb_left == NULL || w->rb_left->rb_color == RB_BLACK))
                {
                    if(w->rb_right)
                    {
                        w->rb_right->rb_color = RB_BLACK;
                    }

                    w->rb_color = RB_RED;
                    rb_rotate_left(mm, w);
                    w = x_parent->rb_left;
                }

                if(w)
                {
                    w->rb_color = x_parent->rb_color;

                    if(w->rb_left)
                    {
                        w->rb_left->rb_color = RB_BLACK;
                    }
                }

                x_parent->rb_color = RB_BLACK;
                rb_rotate_right(mm, x_parent);
                x = mm->rb_root;
                break;
            }
        }
    }

    if(x)
    {
        x->rb_color = RB_BLACK;
    }
}


void rb_remove(struct task_vm_t *mm, volatile struct memregion_t *z)
{
    volatile struct memregion_t *y = z;
    volatile struct memregion_t *x = NULL;
    volatile struct memregion_t *x_parent = NULL;
    volatile rb_color_t y_original_color = y->rb_color;

    if(z->rb_left == NULL)
    {
        x = z->rb_right;
        x_parent = z->rb_parent;

        if(z->rb_parent == NULL)
        {
            mm->rb_root = x;
        }
        else if(z == z->rb_parent->rb_left)
        {
            z->rb_parent->rb_left = x;
        }
        else
        {
            z->rb_parent->rb_right = x;
        }

        if(x)
        {
            x->rb_parent = z->rb_parent;
        }
    } 
    else if(z->rb_right == NULL)
    {
        x = z->rb_left;
        x_parent = z->rb_parent;

        if(z->rb_parent == NULL)
        {
            mm->rb_root = x;
        }
        else if(z == z->rb_parent->rb_left)
        {
            z->rb_parent->rb_left = x;
        }
        else
        {
            z->rb_parent->rb_right = x;
        }

        if(x)
        {
            x->rb_parent = z->rb_parent;
        }
    } 
    else
    {
        // Find node successor (minimum node in the right branch)
        y = z->rb_right;

        while(y->rb_left != NULL)
        {
            y = y->rb_left;
        }

        y_original_color = y->rb_color;
        x = y->rb_right;
        
        if(y->rb_parent == z)
        {
            x_parent = y;
        }
        else
        {
            x_parent = y->rb_parent;

            if(x)
            {
                x->rb_parent = y->rb_parent;
            }

            y->rb_parent->rb_left = x;
            y->rb_right = z->rb_right;

            if(y->rb_right)
            {
                y->rb_right->rb_parent = y;
            }
        }

        if(z->rb_parent == NULL)
        {
            mm->rb_root = y;
        }
        else if(z == z->rb_parent->rb_left)
        {
            z->rb_parent->rb_left = y;
        }
        else
        {
            z->rb_parent->rb_right = y;
        }

        y->rb_parent = z->rb_parent;
        y->rb_left = z->rb_left;
        y->rb_left->rb_parent = y;
        y->rb_color = z->rb_color;
    }

    if(y_original_color == RB_BLACK)
    {
        rb_delete_fixup(mm, x, x_parent);
    }

    // Start at the parent node that was directly affected by the deletion pass,
    // and recalculate the maximum bounds all the way up to the root!
    volatile struct memregion_t *p = x_parent;

    while(p != NULL)
    {
        rb_update_subtree_max(p);
        p = p->rb_parent;
    }
}

