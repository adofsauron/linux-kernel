/* arch/arm/mach-lh7a40x/irq-lh7a404.c
 *
 *  Copyright (C) 2004 Logic Product Development
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/irq.h>
#include <asm/arch/irqs.h>

#define USE_PRIORITIES

/* See Documentation/arm/Sharp-LH/VectoredInterruptController for more
 * information on using the vectored interrupt controller's
 * prioritizing feature. */

static unsigned char irq_pri_vic1[] = {
#if defined (USE_PRIORITIES)
IRQ_GPIO3INTR,
#endif
};
static unsigned char irq_pri_vic2[] = {
#if defined (USE_PRIORITIES)
	IRQ_T3UI, IRQ_GPIO7INTR,
	IRQ_UART1INTR, IRQ_UART2INTR, IRQ_UART3INTR,
#endif
};

  /* CPU IRQ handling */

static void lh7a404_vic1_mask_irq (u32 irq)
{
	VIC1_INTENCLR = (1 << irq);
}

static void lh7a404_vic1_unmask_irq (u32 irq)
{
	VIC1_INTEN = (1 << irq);
}

static void lh7a404_vic2_mask_irq (u32 irq)
{
	VIC2_INTENCLR = (1 << (irq - 32));
}

static void lh7a404_vic2_unmask_irq (u32 irq)
{
	VIC2_INTEN = (1 << (irq - 32));
}

static void lh7a404_vic1_ack_gpio_irq (u32 irq)
{
	GPIO_GPIOFEOI = (1 << IRQ_TO_GPIO (irq));
	VIC1_INTENCLR = (1 << irq);
}

static void lh7a404_vic2_ack_gpio_irq (u32 irq)
{
	GPIO_GPIOFEOI = (1 << IRQ_TO_GPIO (irq));
	VIC2_INTENCLR = (1 << irq);
}

static struct irqchip lh7a404_vic1_chip = {
	.ack	= lh7a404_vic1_mask_irq, /* Because level-triggered */
	.mask	= lh7a404_vic1_mask_irq,
	.unmask	= lh7a404_vic1_unmask_irq,
};

static struct irqchip lh7a404_vic2_chip = {
	.ack	= lh7a404_vic2_mask_irq, /* Because level-triggered */
	.mask	= lh7a404_vic2_mask_irq,
	.unmask	= lh7a404_vic2_unmask_irq,
};

static struct irqchip lh7a404_gpio_vic1_chip = {
	.ack	= lh7a404_vic1_ack_gpio_irq,
	.mask	= lh7a404_vic1_mask_irq,
	.unmask	= lh7a404_vic1_unmask_irq,
};

static struct irqchip lh7a404_gpio_vic2_chip = {
	.ack	= lh7a404_vic2_ack_gpio_irq,
	.mask	= lh7a404_vic2_mask_irq,
	.unmask	= lh7a404_vic2_unmask_irq,
};

  /* IRQ initialization */

void __init lh7a404_init_irq (void)
{
	int irq;

	VIC1_INTENCLR = 0xffffffff;
	VIC2_INTENCLR = 0xffffffff;
	VIC1_INTSEL = 0;		/* All IRQs */
	VIC2_INTSEL = 0;		/* All IRQs */
	VIC1_NVADDR = VA_VIC1DEFAULT;
	VIC2_NVADDR = VA_VIC2DEFAULT;
	VIC1_VECTADDR = 0;
	VIC2_VECTADDR = 0;

	GPIO_GPIOFINTEN = 0x00;		/* Disable all GPIOF interrupts */
	barrier ();

		/* Install prioritized interrupts, if there are any. */
		/* The | 0x20*/
	for (irq = 0; irq < 16; ++irq) {
		(&VIC1_VAD0)[irq]
			= (irq < ARRAY_SIZE (irq_pri_vic1))
			? (irq_pri_vic1[irq] | VA_VECTORED) : 0;
		(&VIC1_VECTCNTL0)[irq]
			= (irq < ARRAY_SIZE (irq_pri_vic1))
			? (irq_pri_vic1[irq] | VIC_CNTL_ENABLE) : 0;
		(&VIC2_VAD0)[irq]
			= (irq < ARRAY_SIZE (irq_pri_vic2))
			? (irq_pri_vic2[irq] | VA_VECTORED) : 0;
		(&VIC2_VECTCNTL0)[irq]
			= (irq < ARRAY_SIZE (irq_pri_vic2))
			? (irq_pri_vic2[irq] | VIC_CNTL_ENABLE) : 0;
	}

	for (irq = 0; irq < NR_IRQS; ++irq) {
		switch (irq) {
		case IRQ_GPIO0INTR:
		case IRQ_GPIO1INTR:
		case IRQ_GPIO2INTR:
		case IRQ_GPIO3INTR:
		case IRQ_GPIO4INTR:
		case IRQ_GPIO5INTR:
		case IRQ_GPIO6INTR:
		case IRQ_GPIO7INTR:
			set_irq_chip (irq, irq < 32
				      ? &lh7a404_gpio_vic1_chip
				      : &lh7a404_gpio_vic2_chip);
			set_irq_handler (irq, do_level_IRQ); /* OK default */
			break;
		default:
			set_irq_chip (irq, irq < 32
				      ? &lh7a404_vic1_chip
				      : &lh7a404_vic2_chip);
			set_irq_handler (irq, do_level_IRQ);
		}
		set_irq_flags (irq, IRQF_VALID);
	}

	lh7a40x_init_board_irq ();
}
