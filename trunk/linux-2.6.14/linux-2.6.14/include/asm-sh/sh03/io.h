/*
 * include/asm-sh/sh03/io.h
 *
 * Copyright 2004 Interface Co.,Ltd. Saito.K
 *
 * IO functions for an Interface CTP/PCI-SH03
 */

#ifndef _ASM_SH_IO_SH03_H
#define _ASM_SH_IO_SH03_H

#include <linux/time.h>

#define INTC_IPRD	0xffd00010UL

#define IRL0_IRQ	2
#define IRL0_IPR_ADDR	INTC_IPRD
#define IRL0_IPR_POS	3
#define IRL0_PRIORITY	13

#define IRL1_IRQ	5
#define IRL1_IPR_ADDR	INTC_IPRD
#define IRL1_IPR_POS	2
#define IRL1_PRIORITY	10

#define IRL2_IRQ	8
#define IRL2_IPR_ADDR	INTC_IPRD
#define IRL2_IPR_POS	1
#define IRL2_PRIORITY	7

#define IRL3_IRQ	11
#define IRL3_IPR_ADDR	INTC_IPRD
#define IRL3_IPR_POS	0
#define IRL3_PRIORITY	4


extern unsigned long sh03_isa_port2addr(unsigned long offset);

extern void setup_sh03(void);
extern void init_sh03_IRQ(void);
extern void heartbeat_sh03(void);

extern void sh03_rtc_gettimeofday(struct timeval *tv);
extern int sh03_rtc_settimeofday(const struct timeval *tv);

#endif /* _ASM_SH_IO_SH03_H */
