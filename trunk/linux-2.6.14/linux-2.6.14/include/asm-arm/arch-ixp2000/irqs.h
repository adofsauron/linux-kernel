/*
 * linux/include/asm-arm/arch-ixp2000/irqs.h
 *
 * Original Author: Naeem Afzal <naeem.m.afzal@intel.com>
 * Maintainer: Deepak Saxena <dsaxena@plexity.net>
 *
 * Copyright (C) 2002 Intel Corp.
 * Copyright (C) 2003-2004 MontaVista Software, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IRQS_H
#define _IRQS_H

/*
 * Do NOT add #ifdef MACHINE_FOO in here.
 * Simpy add your machine IRQs here and increase NR_IRQS if needed to
 * hold your machine's IRQ table.
 */

/*
 * Some interrupt numbers go unused b/c the IRQ mask/ummask/status
 * register has those bit reserved. We just mark those interrupts
 * as invalid and this allows us to do mask/unmask with a single
 * shift operation instead of having to map the IRQ number to
 * a HW IRQ number.
 */
#define	IRQ_IXP2000_SOFT_INT		0 /* soft interrupt */
#define	IRQ_IXP2000_ERRSUM		1 /* OR of all bits in ErrorStatus reg*/
#define	IRQ_IXP2000_UART		2
#define	IRQ_IXP2000_GPIO		3
#define	IRQ_IXP2000_TIMER1     		4
#define	IRQ_IXP2000_TIMER2     		5
#define	IRQ_IXP2000_TIMER3     		6
#define	IRQ_IXP2000_TIMER4     		7
#define	IRQ_IXP2000_PMU        		8               
#define	IRQ_IXP2000_SPF        		9  /* Slow port framer IRQ */
#define	IRQ_IXP2000_DMA1      		10
#define	IRQ_IXP2000_DMA2      		11
#define	IRQ_IXP2000_DMA3      		12
#define	IRQ_IXP2000_PCI_DOORBELL	13
#define	IRQ_IXP2000_ME_ATTN       	14 
#define	IRQ_IXP2000_PCI   		15 /* PCI INTA or INTB */
#define	IRQ_IXP2000_THDA0   		16 /* thread 0-31A */
#define	IRQ_IXP2000_THDA1  		17 /* thread 32-63A, IXP2800 only */
#define	IRQ_IXP2000_THDA2		18 /* thread 64-95A */
#define	IRQ_IXP2000_THDA3 		19 /* thread 96-127A, IXP2800 only */
#define	IRQ_IXP2000_THDB0		24 /* thread 0-31B */
#define	IRQ_IXP2000_THDB1		25 /* thread 32-63B, IXP2800 only */
#define	IRQ_IXP2000_THDB2		26 /* thread 64-95B */
#define	IRQ_IXP2000_THDB3		27 /* thread 96-127B, IXP2800 only */

/* define generic GPIOs */
#define IRQ_IXP2000_GPIO0		32
#define IRQ_IXP2000_GPIO1		33
#define IRQ_IXP2000_GPIO2		34
#define IRQ_IXP2000_GPIO3		35
#define IRQ_IXP2000_GPIO4		36
#define IRQ_IXP2000_GPIO5		37
#define IRQ_IXP2000_GPIO6		38
#define IRQ_IXP2000_GPIO7		39

/* split off the 2 PCI sources */
#define IRQ_IXP2000_PCIA		40
#define IRQ_IXP2000_PCIB		41

#define NR_IXP2000_IRQS                 42

#define	IXP2000_BOARD_IRQ(x)		(NR_IXP2000_IRQS + (x))

#define	IXP2000_BOARD_IRQ_MASK(irq)	(1 << (irq - NR_IXP2000_IRQS))	

/*
 * This allows for all the on-chip sources plus up to 32 CPLD based
 * IRQs. Should be more than enough.
 */
#define	IXP2000_BOARD_IRQS		32
#define NR_IRQS				(NR_IXP2000_IRQS + IXP2000_BOARD_IRQS)


/* 
 * IXDP2400 specific IRQs
 */
#define	IRQ_IXDP2400_INGRESS_NPU	IXP2000_BOARD_IRQ(0) 
#define	IRQ_IXDP2400_ENET		IXP2000_BOARD_IRQ(1) 
#define	IRQ_IXDP2400_MEDIA_PCI		IXP2000_BOARD_IRQ(2) 
#define	IRQ_IXDP2400_MEDIA_SP		IXP2000_BOARD_IRQ(3) 
#define	IRQ_IXDP2400_SF_PCI		IXP2000_BOARD_IRQ(4) 
#define	IRQ_IXDP2400_SF_SP		IXP2000_BOARD_IRQ(5) 
#define	IRQ_IXDP2400_PMC		IXP2000_BOARD_IRQ(6) 
#define	IRQ_IXDP2400_TVM		IXP2000_BOARD_IRQ(7) 

#define	NR_IXDP2400_IRQS		((IRQ_IXDP2400_TVM)+1)  
#define	IXDP2400_NR_IRQS		NR_IXDP2400_IRQS - NR_IXP2000_IRQS

/* IXDP2800 specific IRQs */
#define IRQ_IXDP2800_EGRESS_ENET	IXP2000_BOARD_IRQ(0)
#define IRQ_IXDP2800_INGRESS_NPU	IXP2000_BOARD_IRQ(1)
#define IRQ_IXDP2800_PMC		IXP2000_BOARD_IRQ(2)
#define IRQ_IXDP2800_FABRIC_PCI		IXP2000_BOARD_IRQ(3)
#define IRQ_IXDP2800_FABRIC		IXP2000_BOARD_IRQ(4)
#define IRQ_IXDP2800_MEDIA		IXP2000_BOARD_IRQ(5)

#define	NR_IXDP2800_IRQS		((IRQ_IXDP2800_MEDIA)+1)
#define	IXDP2800_NR_IRQS		NR_IXDP2800_IRQS - NR_IXP2000_IRQS

/* 
 * IRQs on both IXDP2x01 boards
 */
#define IRQ_IXDP2X01_SPCI_DB_0		IXP2000_BOARD_IRQ(2)
#define IRQ_IXDP2X01_SPCI_DB_1		IXP2000_BOARD_IRQ(3)
#define IRQ_IXDP2X01_SPCI_PMC_INTA	IXP2000_BOARD_IRQ(4)
#define IRQ_IXDP2X01_SPCI_PMC_INTB	IXP2000_BOARD_IRQ(5)
#define IRQ_IXDP2X01_SPCI_PMC_INTC	IXP2000_BOARD_IRQ(6)
#define IRQ_IXDP2X01_SPCI_PMC_INTD	IXP2000_BOARD_IRQ(7)
#define IRQ_IXDP2X01_SPCI_FIC_INT	IXP2000_BOARD_IRQ(8)
#define IRQ_IXDP2X01_IPMI_FROM		IXP2000_BOARD_IRQ(16)
#define IRQ_IXDP2X01_125US		IXP2000_BOARD_IRQ(17)
#define IRQ_IXDP2X01_DB_0_ADD		IXP2000_BOARD_IRQ(18)
#define IRQ_IXDP2X01_DB_1_ADD		IXP2000_BOARD_IRQ(19)
#define IRQ_IXDP2X01_UART1		IXP2000_BOARD_IRQ(21)
#define IRQ_IXDP2X01_UART2		IXP2000_BOARD_IRQ(22)
#define IRQ_IXDP2X01_FIC_ADD_INT	IXP2000_BOARD_IRQ(24)
#define IRQ_IXDP2X01_CS8900		IXP2000_BOARD_IRQ(25)
#define IRQ_IXDP2X01_BBSRAM		IXP2000_BOARD_IRQ(26)

#define IXDP2X01_VALID_IRQ_MASK ( \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_SPCI_DB_0) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_SPCI_DB_1) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_SPCI_PMC_INTA) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_SPCI_PMC_INTB) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_SPCI_PMC_INTC) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_SPCI_PMC_INTD) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_SPCI_FIC_INT) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_IPMI_FROM) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_125US) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_DB_0_ADD) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_DB_1_ADD) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_UART1) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_UART2) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_FIC_ADD_INT) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_CS8900) | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2X01_BBSRAM) )

/* 
 * IXDP2401 specific IRQs
 */
#define IRQ_IXDP2401_INTA_82546		IXP2000_BOARD_IRQ(0)
#define IRQ_IXDP2401_INTB_82546		IXP2000_BOARD_IRQ(1)

#define	IXDP2401_VALID_IRQ_MASK ( \
		IXDP2X01_VALID_IRQ_MASK | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2401_INTA_82546) |\
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2401_INTB_82546))

/*
 * IXDP2801-specific IRQs
 */
#define IRQ_IXDP2801_RIV		IXP2000_BOARD_IRQ(0)
#define IRQ_IXDP2801_CNFG_MEDIA		IXP2000_BOARD_IRQ(27)
#define IRQ_IXDP2801_CLOCK_REF		IXP2000_BOARD_IRQ(28)

#define	IXDP2801_VALID_IRQ_MASK ( \
		IXDP2X01_VALID_IRQ_MASK | \
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2801_RIV) |\
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2801_CNFG_MEDIA) |\
		IXP2000_BOARD_IRQ_MASK(IRQ_IXDP2801_CLOCK_REF))

#define	NR_IXDP2X01_IRQS		((IRQ_IXDP2801_CLOCK_REF) + 1)

#endif /*_IRQS_H*/
