/*
 *  linux/include/asm-arm/arch-ebsa285/io.h
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Modifications:
 *   06-12-1997	RMK	Created.
 *   07-04-1999	RMK	Major cleanup
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffff

/*
 * Translation of various region addresses to virtual addresses
 */
#define __io(a)			((void __iomem *)(PCIO_BASE + (a)))
#if 1
#define __mem_pci(a)		(a)
#define __mem_isa(a)		((a) + PCIMEM_BASE)
#else

static inline void __iomem *___mem_pci(void __iomem *p)
{
	unsigned long a = (unsigned long)p;
	BUG_ON(a <= 0xc0000000 || a >= 0xe0000000);
	return p;
}

static inline void __iomem *___mem_isa(void __iomem *p)
{
	unsigned long a = (unsigned long)p;
	BUG_ON(a >= 16*1048576);
	return p + PCIMEM_BASE;
}
#define __mem_pci(a)		___mem_pci(a)
#define __mem_isa(a)		___mem_isa(a)
#endif

#endif
