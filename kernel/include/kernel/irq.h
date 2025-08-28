/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2021, 2022, 2023, 2024 (c)
 * 
 *    file: irq.h
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
 *  \file irq.h
 *
 *  Functions and macro definitions for working with IRQs.
 */

#ifndef __IRQ_H__
#define __IRQ_H__

#include <kernel/laylaos.h>

#define IRQ_TIMER               0       /**< Timer IRQ */
#define IRQ_KBD                 1       /**< PS/2 Keyboard IRQ */
#define IRQ_MOUSE               12      /**< PS/2 Mouse IRQ */


/**
 * @struct handler_t
 * @brief The handler_t structure.
 *
 * A structure to represent an IRQ handler and its callback function.
 */
struct handler_t
{
    int (*handler)(struct regs *r, int arg);  /**< handler function */
    int handler_arg;                          /**< handler function argument */
    char short_name[16];           /**< what this IRQ is used for */
    uint64_t hits;                 /**< how many times this IRQ happened */
    uint64_t ticks;                /**< total ticks used to serve this IRQ */
    struct handler_t *next;        /**< next handler (for shared IRQs) */
};

/**
 * @var interrupt_handlers
 * @brief IRQ handler table.
 *
 * This variable points to the master IRQ handler table.
 */
extern struct handler_t *interrupt_handlers[];

struct irq_redir_t
{
    uint32_t gsi;
    uint16_t flags;
};

extern struct irq_redir_t irq_redir[];


/*
 * IRQ entry functions, defined in arch/xx/interrupt.S.
 */
void irq0(void);
void irq1(void);
void irq2(void);
void irq3(void);
void irq4(void);
void irq5(void);
void irq6(void);
void irq7(void);
void irq8(void);
void irq9(void);
void irq10(void);
void irq11(void);
void irq12(void);
void irq13(void);
void irq14(void);
void irq15(void);


/**
 * @brief Register IRQ handler.
 *
 * More than one handler can be associated with an IRQ, i.e. shared IRQs.
 *
 * @param   n           IRQ number
 * @param   handler     IRQ handler
 *
 * @return  nothing.
 */
void register_irq_handler(int n, struct handler_t *handler);

/**
 * @brief Register ISR handler.
 *
 * More than one handler can be associated with an ISR, i.e. shared ISRs.
 *
 * @param   n           ISR number
 * @param   handler     ISR handler
 *
 * @return  nothing.
 */
void register_isr_handler(int n, struct handler_t *handler);

/**
 * @brief Unregister IRQ handler.
 *
 * Unregister the given IRQ handler.
 *
 * @param   n           IRQ number
 * @param   handler     IRQ handler
 *
 * @return  nothing.
 */
void unregister_irq_handler(int n, struct handler_t *handler);

/**
 * @brief Unregister ISR handler.
 *
 * Unregister the given ISR handler.
 *
 * @param   n           ISR number
 * @param   handler     ISR handler
 *
 * @return  nothing.
 */
void unregister_isr_handler(int n, struct handler_t *handler);

/**
 * @brief Allocate an IRQ handler.
 *
 * Allocate a new handler_t struct and assign its handler and argument.
 *
 * @param   func        handler function
 * @param   arg         handler function argument
 * @param   name        short IRQ name (less than 16 chars)
 *
 * @return  new handler_t struct.
 */
struct handler_t *irq_handler_alloc(int (*func)(struct regs *, int), 
                                    int arg, char *name);

/**
 * @brief Initialise IRQs.
 *
 * Initialise system IRQs.
 *
 * @return  nothing.
 */
void irq_init(void);


//uint32_t irq_remap(uint32_t irq);

#endif      /* __IRQ_H__ */
