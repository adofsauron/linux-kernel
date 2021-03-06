/*
 * include/asm-arm/arch-ixp2000/ixdp2x01.h
 *
 * Platform definitions for IXDP2X01 && IXDP2801 systems
 *
 * Author: Deepak Saxena <dsaxena@plexity.net>
 *
 * Copyright 2004 (c) MontaVista Software, Inc. 
 *
 * Based on original code Copyright (c) 2002-2003 Intel Corporation
 * 
 * This file is licensed under  the terms of the GNU General Public 
 * License version 2. This program is licensed "as is" without any 
 * warranty of any kind, whether express or implied.
 */

#ifndef __IXDP2X01_H__
#define __IXDP2X01_H__

#define	IXDP2X01_PHYS_CPLD_BASE		0xc6024000
#define	IXDP2X01_VIRT_CPLD_BASE		0xfe000000
#define	IXDP2X01_CPLD_REGION_SIZE	0x00100000

#define IXDP2X01_CPLD_VIRT_REG(reg) (volatile unsigned long*)(IXDP2X01_VIRT_CPLD_BASE | reg)
#define IXDP2X01_CPLD_PHYS_REG(reg) (volatile u32*)(IXDP2X01_PHYS_CPLD_BASE | reg)

#define IXDP2X01_UART1_VIRT_BASE	IXDP2X01_CPLD_VIRT_REG(0x40)
#define IXDP2X01_UART1_PHYS_BASE	IXDP2X01_CPLD_PHYS_REG(0x40)

#define IXDP2X01_UART2_VIRT_BASE	IXDP2X01_CPLD_VIRT_REG(0x60)
#define IXDP2X01_UART2_PHYS_BASE	IXDP2X01_CPLD_PHYS_REG(0x60)

#define IXDP2X01_CS8900_VIRT_BASE	IXDP2X01_CPLD_VIRT_REG(0x80)
#define IXDP2X01_CS8900_VIRT_END	(IXDP2X01_CS8900_VIRT_BASE + 16)

#define IXDP2X01_CPLD_RESET_REG         IXDP2X01_CPLD_VIRT_REG(0x00)
#define IXDP2X01_INT_MASK_SET_REG	IXDP2X01_CPLD_VIRT_REG(0x08)
#define IXDP2X01_INT_STAT_REG		IXDP2X01_CPLD_VIRT_REG(0x0C)
#define IXDP2X01_INT_RAW_REG		IXDP2X01_CPLD_VIRT_REG(0x10) 
#define IXDP2X01_INT_MASK_CLR_REG	IXDP2X01_INT_RAW_REG
#define IXDP2X01_INT_SIM_REG		IXDP2X01_CPLD_VIRT_REG(0x14)

#define IXDP2X01_CPLD_FLASH_REG		IXDP2X01_CPLD_VIRT_REG(0x20)

#define IXDP2X01_CPLD_FLASH_INTERN 	0x8000
#define IXDP2X01_CPLD_FLASH_BANK_MASK 	0xF
#define IXDP2X01_FLASH_WINDOW_BITS 	25
#define IXDP2X01_FLASH_WINDOW_SIZE 	(1 << IXDP2X01_FLASH_WINDOW_BITS)
#define IXDP2X01_FLASH_WINDOW_MASK 	(IXDP2X01_FLASH_WINDOW_SIZE - 1)

#define	IXDP2X01_UART_CLK		1843200

#define	IXDP2X01_GPIO_I2C_ENABLE	0x02
#define	IXDP2X01_GPIO_SCL		0x07
#define	IXDP2X01_GPIO_SDA		0x06

#endif /* __IXDP2x01_H__ */
