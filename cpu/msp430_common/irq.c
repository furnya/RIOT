/*
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu
* @{
 *
 * @file
 * @brief       ISR related functions
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 *
 * @}
 */

#include "irq.h"
#include "cpu.h"

volatile int __irq_is_in = 0;

char __isr_stack[ISR_STACKSIZE];

unsigned int irq_disable(void)
{
    int state = irq_is_enabled();

    if (state) {
        __disable_irq();
    }

    return (unsigned)state;
}

unsigned int irq_enable(void)
{
    int state = irq_is_enabled();

    if (!state) {
        __enable_irq();
    }

    return (unsigned)state;
}

void irq_restore(unsigned int state)
{
    if (state) {
        __enable_irq();
    }
}

int irq_is_enabled(void)
{
    unsigned int state;
    __asm__("mov.w r2,%0" : "=r"(state));
    return (state & GIE);
}

int irq_is_in(void)
{
    return __irq_is_in;
}
