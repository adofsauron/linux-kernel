/*
 *  SSP control code for Sharp Corgi devices
 *
 *  Copyright (c) 2004-2005 Richard Purdie
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>

#include <asm/arch/ssp.h>
#include <asm/arch/pxa-regs.h>
#include "sharpsl.h"

static DEFINE_SPINLOCK(corgi_ssp_lock);
static struct ssp_dev corgi_ssp_dev;
static struct ssp_state corgi_ssp_state;
static struct corgissp_machinfo *ssp_machinfo;

/*
 * There are three devices connected to the SSP interface:
 *   1. A touchscreen controller (TI ADS7846 compatible)
 *   2. An LCD contoller (with some Backlight functionality)
 *   3. A battery moinitoring IC (Maxim MAX1111)
 *
 * Each device uses a different speed/mode of communication.
 *
 * The touchscreen is very sensitive and the most frequently used
 * so the port is left configured for this.
 *
 * Devices are selected using Chip Selects on GPIOs.
 */

/*
 *  ADS7846 Routines
 */
unsigned long corgi_ssp_ads7846_putget(ulong data)
{
	unsigned long ret,flag;

	spin_lock_irqsave(&corgi_ssp_lock, flag);
	GPCR(ssp_machinfo->cs_ads7846) = GPIO_bit(ssp_machinfo->cs_ads7846);

	ssp_write_word(&corgi_ssp_dev,data);
	ret = ssp_read_word(&corgi_ssp_dev);

	GPSR(ssp_machinfo->cs_ads7846) = GPIO_bit(ssp_machinfo->cs_ads7846);
	spin_unlock_irqrestore(&corgi_ssp_lock, flag);

	return ret;
}

/*
 * NOTE: These functions should always be called in interrupt context
 * and use the _lock and _unlock functions. They are very time sensitive.
 */
void corgi_ssp_ads7846_lock(void)
{
	spin_lock(&corgi_ssp_lock);
	GPCR(ssp_machinfo->cs_ads7846) = GPIO_bit(ssp_machinfo->cs_ads7846);
}

void corgi_ssp_ads7846_unlock(void)
{
	GPSR(ssp_machinfo->cs_ads7846) = GPIO_bit(ssp_machinfo->cs_ads7846);
	spin_unlock(&corgi_ssp_lock);
}

void corgi_ssp_ads7846_put(ulong data)
{
	ssp_write_word(&corgi_ssp_dev,data);
}

unsigned long corgi_ssp_ads7846_get(void)
{
	return ssp_read_word(&corgi_ssp_dev);
}

EXPORT_SYMBOL(corgi_ssp_ads7846_putget);
EXPORT_SYMBOL(corgi_ssp_ads7846_lock);
EXPORT_SYMBOL(corgi_ssp_ads7846_unlock);
EXPORT_SYMBOL(corgi_ssp_ads7846_put);
EXPORT_SYMBOL(corgi_ssp_ads7846_get);


/*
 *  LCD/Backlight Routines
 */
unsigned long corgi_ssp_dac_put(ulong data)
{
	unsigned long flag, sscr1 = SSCR1_SPH;

	spin_lock_irqsave(&corgi_ssp_lock, flag);

	if (machine_is_spitz() || machine_is_akita() || machine_is_borzoi())
		sscr1 = 0;

	ssp_disable(&corgi_ssp_dev);
	ssp_config(&corgi_ssp_dev, (SSCR0_Motorola | (SSCR0_DSS & 0x07 )), sscr1, 0, SSCR0_SerClkDiv(ssp_machinfo->clk_lcdcon));
	ssp_enable(&corgi_ssp_dev);

	GPCR(ssp_machinfo->cs_lcdcon) = GPIO_bit(ssp_machinfo->cs_lcdcon);
	ssp_write_word(&corgi_ssp_dev,data);
	/* Read null data back from device to prevent SSP overflow */
	ssp_read_word(&corgi_ssp_dev);
	GPSR(ssp_machinfo->cs_lcdcon) = GPIO_bit(ssp_machinfo->cs_lcdcon);

	ssp_disable(&corgi_ssp_dev);
	ssp_config(&corgi_ssp_dev, (SSCR0_National | (SSCR0_DSS & 0x0b )), 0, 0, SSCR0_SerClkDiv(ssp_machinfo->clk_ads7846));
	ssp_enable(&corgi_ssp_dev);

	spin_unlock_irqrestore(&corgi_ssp_lock, flag);

	return 0;
}

void corgi_ssp_lcdtg_send(u8 adrs, u8 data)
{
	corgi_ssp_dac_put(((adrs & 0x07) << 5) | (data & 0x1f));
}

void corgi_ssp_blduty_set(int duty)
{
	corgi_ssp_lcdtg_send(0x02,duty);
}

EXPORT_SYMBOL(corgi_ssp_lcdtg_send);
EXPORT_SYMBOL(corgi_ssp_blduty_set);

/*
 *  Max1111 Routines
 */
int corgi_ssp_max1111_get(ulong data)
{
	unsigned long flag;
	int voltage,voltage1,voltage2;

	spin_lock_irqsave(&corgi_ssp_lock, flag);
	GPCR(ssp_machinfo->cs_max1111) = GPIO_bit(ssp_machinfo->cs_max1111);
	ssp_disable(&corgi_ssp_dev);
	ssp_config(&corgi_ssp_dev, (SSCR0_Motorola | (SSCR0_DSS & 0x07 )), 0, 0, SSCR0_SerClkDiv(ssp_machinfo->clk_max1111));
	ssp_enable(&corgi_ssp_dev);

	udelay(1);

	/* TB1/RB1 */
	ssp_write_word(&corgi_ssp_dev,data);
	ssp_read_word(&corgi_ssp_dev); /* null read */

	/* TB12/RB2 */
	ssp_write_word(&corgi_ssp_dev,0);
	voltage1=ssp_read_word(&corgi_ssp_dev);

	/* TB13/RB3*/
	ssp_write_word(&corgi_ssp_dev,0);
	voltage2=ssp_read_word(&corgi_ssp_dev);

	ssp_disable(&corgi_ssp_dev);
	ssp_config(&corgi_ssp_dev, (SSCR0_National | (SSCR0_DSS & 0x0b )), 0, 0, SSCR0_SerClkDiv(ssp_machinfo->clk_ads7846));
	ssp_enable(&corgi_ssp_dev);
	GPSR(ssp_machinfo->cs_max1111) = GPIO_bit(ssp_machinfo->cs_max1111);
	spin_unlock_irqrestore(&corgi_ssp_lock, flag);

	if (voltage1 & 0xc0 || voltage2 & 0x3f)
		voltage = -1;
	else
		voltage = ((voltage1 << 2) & 0xfc) | ((voltage2 >> 6) & 0x03);

	return voltage;
}

EXPORT_SYMBOL(corgi_ssp_max1111_get);

/*
 *  Support Routines
 */

void __init corgi_ssp_set_machinfo(struct corgissp_machinfo *machinfo)
{
	ssp_machinfo = machinfo;
}

static int __init corgi_ssp_probe(struct device *dev)
{
	int ret;

	/* Chip Select - Disable All */
	GPDR(ssp_machinfo->cs_lcdcon) |= GPIO_bit(ssp_machinfo->cs_lcdcon); /* output */
	GPSR(ssp_machinfo->cs_lcdcon) = GPIO_bit(ssp_machinfo->cs_lcdcon);  /* High - Disable LCD Control/Timing Gen */
	GPDR(ssp_machinfo->cs_max1111) |= GPIO_bit(ssp_machinfo->cs_max1111); /* output */
	GPSR(ssp_machinfo->cs_max1111) = GPIO_bit(ssp_machinfo->cs_max1111);  /* High - Disable MAX1111*/
	GPDR(ssp_machinfo->cs_ads7846) |= GPIO_bit(ssp_machinfo->cs_ads7846);  /* output */
	GPSR(ssp_machinfo->cs_ads7846) = GPIO_bit(ssp_machinfo->cs_ads7846);   /* High - Disable ADS7846*/

	ret = ssp_init(&corgi_ssp_dev,ssp_machinfo->port);

	if (ret)
		printk(KERN_ERR "Unable to register SSP handler!\n");
	else {
		ssp_disable(&corgi_ssp_dev);
		ssp_config(&corgi_ssp_dev, (SSCR0_National | (SSCR0_DSS & 0x0b )), 0, 0, SSCR0_SerClkDiv(ssp_machinfo->clk_ads7846));
		ssp_enable(&corgi_ssp_dev);
	}

	return ret;
}

static int corgi_ssp_remove(struct device *dev)
{
	ssp_exit(&corgi_ssp_dev);
	return 0;
}

static int corgi_ssp_suspend(struct device *dev, pm_message_t state, u32 level)
{
	if (level == SUSPEND_POWER_DOWN) {
		ssp_flush(&corgi_ssp_dev);
		ssp_save_state(&corgi_ssp_dev,&corgi_ssp_state);
	}
	return 0;
}

static int corgi_ssp_resume(struct device *dev, u32 level)
{
	if (level == RESUME_POWER_ON) {
		GPSR(ssp_machinfo->cs_lcdcon) = GPIO_bit(ssp_machinfo->cs_lcdcon);  /* High - Disable LCD Control/Timing Gen */
		GPSR(ssp_machinfo->cs_max1111) = GPIO_bit(ssp_machinfo->cs_max1111); /* High - Disable MAX1111*/
		GPSR(ssp_machinfo->cs_ads7846) = GPIO_bit(ssp_machinfo->cs_ads7846); /* High - Disable ADS7846*/
		ssp_restore_state(&corgi_ssp_dev,&corgi_ssp_state);
		ssp_enable(&corgi_ssp_dev);
	}
	return 0;
}

static struct device_driver corgissp_driver = {
	.name		= "corgi-ssp",
	.bus		= &platform_bus_type,
	.probe		= corgi_ssp_probe,
	.remove		= corgi_ssp_remove,
	.suspend	= corgi_ssp_suspend,
	.resume		= corgi_ssp_resume,
};

int __init corgi_ssp_init(void)
{
	return driver_register(&corgissp_driver);
}

arch_initcall(corgi_ssp_init);
