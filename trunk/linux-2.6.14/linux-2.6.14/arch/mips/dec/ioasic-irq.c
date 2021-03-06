/*
 *	linux/arch/mips/dec/ioasic-irq.c
 *
 *	DEC I/O ASIC interrupts.
 *
 *	Copyright (c) 2002, 2003  Maciej W. Rozycki
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <asm/dec/ioasic.h>
#include <asm/dec/ioasic_addrs.h>
#include <asm/dec/ioasic_ints.h>


static DEFINE_SPINLOCK(ioasic_lock);

static int ioasic_irq_base;


static inline void unmask_ioasic_irq(unsigned int irq)
{
	u32 simr;

	simr = ioasic_read(IO_REG_SIMR);
	simr |= (1 << (irq - ioasic_irq_base));
	ioasic_write(IO_REG_SIMR, simr);
}

static inline void mask_ioasic_irq(unsigned int irq)
{
	u32 simr;

	simr = ioasic_read(IO_REG_SIMR);
	simr &= ~(1 << (irq - ioasic_irq_base));
	ioasic_write(IO_REG_SIMR, simr);
}

static inline void clear_ioasic_irq(unsigned int irq)
{
	u32 sir;

	sir = ~(1 << (irq - ioasic_irq_base));
	ioasic_write(IO_REG_SIR, sir);
}

static inline void enable_ioasic_irq(unsigned int irq)
{
	unsigned long flags;

	spin_lock_irqsave(&ioasic_lock, flags);
	unmask_ioasic_irq(irq);
	spin_unlock_irqrestore(&ioasic_lock, flags);
}

static inline void disable_ioasic_irq(unsigned int irq)
{
	unsigned long flags;

	spin_lock_irqsave(&ioasic_lock, flags);
	mask_ioasic_irq(irq);
	spin_unlock_irqrestore(&ioasic_lock, flags);
}


static inline unsigned int startup_ioasic_irq(unsigned int irq)
{
	enable_ioasic_irq(irq);
	return 0;
}

#define shutdown_ioasic_irq disable_ioasic_irq

static inline void ack_ioasic_irq(unsigned int irq)
{
	spin_lock(&ioasic_lock);
	mask_ioasic_irq(irq);
	spin_unlock(&ioasic_lock);
	fast_iob();
}

static inline void end_ioasic_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_ioasic_irq(irq);
}

static struct hw_interrupt_type ioasic_irq_type = {
	.typename = "IO-ASIC",
	.startup = startup_ioasic_irq,
	.shutdown = shutdown_ioasic_irq,
	.enable = enable_ioasic_irq,
	.disable = disable_ioasic_irq,
	.ack = ack_ioasic_irq,
	.end = end_ioasic_irq,
};


#define startup_ioasic_dma_irq startup_ioasic_irq

#define shutdown_ioasic_dma_irq shutdown_ioasic_irq

#define enable_ioasic_dma_irq enable_ioasic_irq

#define disable_ioasic_dma_irq disable_ioasic_irq

#define ack_ioasic_dma_irq ack_ioasic_irq

static inline void end_ioasic_dma_irq(unsigned int irq)
{
	clear_ioasic_irq(irq);
	fast_iob();
	end_ioasic_irq(irq);
}

static struct hw_interrupt_type ioasic_dma_irq_type = {
	.typename = "IO-ASIC-DMA",
	.startup = startup_ioasic_dma_irq,
	.shutdown = shutdown_ioasic_dma_irq,
	.enable = enable_ioasic_dma_irq,
	.disable = disable_ioasic_dma_irq,
	.ack = ack_ioasic_dma_irq,
	.end = end_ioasic_dma_irq,
};


void __init init_ioasic_irqs(int base)
{
	int i;

	/* Mask interrupts. */
	ioasic_write(IO_REG_SIMR, 0);
	fast_iob();

	for (i = base; i < base + IO_INR_DMA; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = 0;
		irq_desc[i].depth = 1;
		irq_desc[i].handler = &ioasic_irq_type;
	}
	for (; i < base + IO_IRQ_LINES; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = 0;
		irq_desc[i].depth = 1;
		irq_desc[i].handler = &ioasic_dma_irq_type;
	}

	ioasic_irq_base = base;
}
