/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2023, 2024 (c)
 * 
 *    file: keys.c
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
 *  \file keys.c
 *
 *  Functions to work with keys and their bindings on the server side.
 */

#define GUI_SERVER
#include <pthread.h>
#include "../include/gc.h"
#include "../include/server/window.h"
#include "../include/server/event.h"
#include "../include/server/server.h"
#include "../include/keys.h"
#include "../include/menu.h"
//#include <kernel/keycodes.h>
#include <kernel/reboot.h>

/*
 * Mask of the CTRL, ALT and SHIFT modifier keys.
 */
char modifiers = 0;

/*
 * Key state for every key on the keyboard:
 * 0 -> key is released
 * 1 -> key is pressed
 */
char key_state[256] = { 0, };


extern int reboot(int cmd);


struct keybinding_t
{
	//int keymod;		// key | (modifiers << 8)
	char modifiers;

	// see keys.h for KEYBINDING_* definitions
	int action;

	// depending on the action, we EITHER use the watchers list OR
	// the func index

	// see keys.h for KEYBINDING_FUNCTION_* definitions
	int func;

	List *watchers;

	struct keybinding_t *next;
};

struct keybinding_t *keybindings[256] = { 0, };


static inline void toggle_modifier(char which)
{
    if((modifiers & which))
    {
        modifiers &= ~which;
    }
    else
    {
        modifiers |= which;
    }
}


void server_process_key(struct gc_t *gc, char *key)
{
	// test if this is a break code
    volatile char brk = (key[0] & KEYCODE_BREAK_MASK) ? 1 : 0;
	struct keybinding_t *binding;
	int i = (int)key[1] & 0xff;

	key_state[i] = !brk;
    
    switch(key[1])
    {
        case KEYCODE_LCTRL:
        case KEYCODE_RCTRL:
			if(brk)
			{
			    modifiers &= ~MODIFIER_MASK_CTRL;
			}
			else
			{
			    modifiers |= MODIFIER_MASK_CTRL;
			}

			break;

        case KEYCODE_LSHIFT:
        case KEYCODE_RSHIFT:
			if(brk)
			{
			    modifiers &= ~MODIFIER_MASK_SHIFT;
			}
			else
			{
			    modifiers |= MODIFIER_MASK_SHIFT;
			}

			break;

        case KEYCODE_LALT:
        case KEYCODE_RALT:
			if(brk)
			{
			    modifiers &= ~MODIFIER_MASK_ALT;
			}
			else
			{
			    modifiers |= MODIFIER_MASK_ALT;
			}

			break;

		case KEYCODE_CAPS:
			if(!brk)
			{
			    toggle_modifier(MODIFIER_MASK_CAPS);
				//kbd_set_leds(_numlock, _capslock, _scrolllock);
			}

			return;

		case KEYCODE_NUM:
			if(!brk)
			{
			    toggle_modifier(MODIFIER_MASK_NUM);
				//kbd_set_leds(_numlock, _capslock, _scrolllock);
			}

			return;

		case KEYCODE_SCROLL:
			if(!brk)
			{
			    toggle_modifier(MODIFIER_MASK_SCROLL);
				//kbd_set_leds(_numlock, _capslock, _scrolllock);
			}

			return;

        default:
			break;
    }


    for(binding = keybindings[i]; binding != NULL; binding = binding->next)
	{
		if(binding->modifiers == modifiers)
		{
			if(binding->action == KEYBINDING_NOTIFY)
			{
				// notify all interested clients
				ListNode *current_node = binding->watchers->root_node;

				for( ; current_node != NULL; current_node = current_node->next)
				{
					winid_t winid = (winid_t)current_node->payload;
					struct server_window_t *win = server_window_by_winid(winid);

					if(win)
					{
						send_key_event(win, key[1], key[0], modifiers);
					}
				}
			}
			else if(binding->action == KEYBINDING_NOTIFY_ONCE)
			{
				// notify the first client we find
				ListNode *current_node = binding->watchers->root_node;

				for( ; current_node != NULL; current_node = current_node->next)
				{
					winid_t winid = (winid_t)current_node->payload;
					struct server_window_t *win = server_window_by_winid(winid);

					if(win)
					{
						send_key_event(win, key[1], key[0], modifiers);
						break;
					}
				}

    			return;
			}
			else if(binding->action == KEYBINDING_FUNCTION)
			{
				switch(binding->func)
				{
					case KEYBINDING_FUNCTION_REBOOT:
						reboot(KERNEL_REBOOT_RESTART);
						break;
				}

    			return;
			}

			break;
		}
	}


    if(grabbed_keyboard_window)
    {
		send_key_event(grabbed_keyboard_window, key[1], key[0], modifiers);
	}
	else if(root_window && root_window->focused_child)
	{
		send_key_event(root_window->focused_child, key[1], key[0], modifiers);
	}
}


void server_key_bind(char key, char modifiers, int action, winid_t winid)
{
	int i = (int)key & 0xff;
	struct keybinding_t *new_binding, *binding;

	if(!(new_binding = malloc(sizeof(struct keybinding_t))))
	{
		return;
	}

	new_binding->watchers = List_new();
	new_binding->modifiers = modifiers;
	new_binding->action = action;
	new_binding->func = 0;
	new_binding->next = NULL;

    for(binding = keybindings[i]; binding != NULL; binding = binding->next)
	{
		if(binding->modifiers != modifiers)
		{
			continue;
		}

		if(action == KEYBINDING_NOTIFY ||
		   action == KEYBINDING_NOTIFY_ONCE)
		{
			List_add(binding->watchers, (void *)winid);
			binding->action = action;
		}

		List_free(new_binding->watchers);
		free(new_binding);
		return;
	}

	List_add(new_binding->watchers, (void *)winid);
	new_binding->next = keybindings[i];
	keybindings[i] = new_binding;
}


void server_key_unbind(char key, char modifiers, winid_t winid)
{
	int i = (int)key & 0xff;
	struct keybinding_t *binding;

    for(binding = keybindings[i]; binding != NULL; binding = binding->next)
	{
		if(binding->modifiers != modifiers)
		{
			continue;
		}

		while(binding->watchers->root_node)
		{
			ListNode *current_node = binding->watchers->root_node;

			binding->watchers->root_node = current_node->next;
			Listnode_free(current_node);
		}

		binding->watchers->last_node = NULL;
		binding->watchers->count = 0;

		break;
	}
}


/*
 * Return the key states as a compressed bitmap (should be 32 bytes long, to
 * accommodate the 256 different entries). This function is called when a client
 * application requests to know the key states.
 */
void key_state_bitmap(char *bitmap)
{
    int i;
    
    A_memset(bitmap, 0, 32);
    
    for(i = 0; i < 256; i++)
    {
        if(key_state[i])
        {
            bitmap[i / 8] |= (1 << (i % 8));
        }
    }
}

