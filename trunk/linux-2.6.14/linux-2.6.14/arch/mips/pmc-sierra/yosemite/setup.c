/*
 *  Copyright (C) 2003 PMC-Sierra Inc.
 *  Author: Manish Lachwani (lachwani@pmc-sierra.com)
 *
 * Copyright (C) 2004 by Ralf Baechle (ralf@linux-mips.org)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/bcd.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/swap.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/termios.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/serial_core.h>

#include <asm/time.h>
#include <asm/bootinfo.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/reboot.h>
#include <asm/serial.h>
#include <asm/titan_dep.h>
#include <asm/m48t37.h>

#include "setup.h"

unsigned char titan_ge_mac_addr_base[6] = {
	// 0x00, 0x03, 0xcc, 0x1d, 0x22, 0x00
	0x00, 0xe0, 0x04, 0x00, 0x00, 0x21
};

unsigned long cpu_clock;
unsigned long yosemite_base;

static struct m48t37_rtc *m48t37_base;

void __init bus_error_init(void)
{
	/* Do nothing */
}


unsigned long m48t37y_get_time(void)
{
	unsigned int year, month, day, hour, min, sec;

	/* Stop the update to the time */
	m48t37_base->control = 0x40;

	year = BCD2BIN(m48t37_base->year);
	year += BCD2BIN(m48t37_base->century) * 100;

	month = BCD2BIN(m48t37_base->month);
	day = BCD2BIN(m48t37_base->date);
	hour = BCD2BIN(m48t37_base->hour);
	min = BCD2BIN(m48t37_base->min);
	sec = BCD2BIN(m48t37_base->sec);

	/* Start the update to the time again */
	m48t37_base->control = 0x00;

	return mktime(year, month, day, hour, min, sec);
}

int m48t37y_set_time(unsigned long sec)
{
	struct rtc_time tm;

	/* convert to a more useful format -- note months count from 0 */
	to_tm(sec, &tm);
	tm.tm_mon += 1;

	/* enable writing */
	m48t37_base->control = 0x80;

	/* year */
	m48t37_base->year = BIN2BCD(tm.tm_year % 100);
	m48t37_base->century = BIN2BCD(tm.tm_year / 100);

	/* month */
	m48t37_base->month = BIN2BCD(tm.tm_mon);

	/* day */
	m48t37_base->date = BIN2BCD(tm.tm_mday);

	/* hour/min/sec */
	m48t37_base->hour = BIN2BCD(tm.tm_hour);
	m48t37_base->min = BIN2BCD(tm.tm_min);
	m48t37_base->sec = BIN2BCD(tm.tm_sec);

	/* day of week -- not really used, but let's keep it up-to-date */
	m48t37_base->day = BIN2BCD(tm.tm_wday + 1);

	/* disable writing */
	m48t37_base->control = 0x00;

	return 0;
}

void yosemite_timer_setup(struct irqaction *irq)
{
	setup_irq(7, irq);
}

void yosemite_time_init(void)
{
	board_timer_setup = yosemite_timer_setup;
	mips_hpt_frequency = cpu_clock / 2;
mips_hpt_frequency = 33000000 * 3 * 5;
}

/* No other usable initialization hook than this ...  */
extern void (*late_time_init)(void);

unsigned long ocd_base;

EXPORT_SYMBOL(ocd_base);

/*
 * Common setup before any secondaries are started
 */

#define TITAN_UART_CLK		3686400
#define TITAN_SERIAL_BASE_BAUD	(TITAN_UART_CLK / 16)
#define TITAN_SERIAL_IRQ	4
#define TITAN_SERIAL_BASE	0xfd000008UL

static void __init py_map_ocd(void)
{
	ocd_base = (unsigned long) ioremap(OCD_BASE, OCD_SIZE);
	if (!ocd_base)
		panic("Mapping OCD failed - game over.  Your score is 0.");

	/* Kludge for PMON bug ... */
	OCD_WRITE(0x0710, 0x0ffff029);
}

static void __init py_uart_setup(void)
{
	struct uart_port up;

	/*
	 * Register to interrupt zero because we share the interrupt with
	 * the serial driver which we don't properly support yet.
	 */
	memset(&up, 0, sizeof(up));
	up.membase      = (unsigned char *) ioremap(TITAN_SERIAL_BASE, 8);
	up.irq          = TITAN_SERIAL_IRQ;
	up.uartclk      = TITAN_UART_CLK;
	up.regshift     = 0;
	up.iotype       = UPIO_MEM;
	up.flags        = ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST;
	up.line         = 0;

	if (early_serial_setup(&up))
		printk(KERN_ERR "Early serial init of port 0 failed\n");
}

static void __init py_rtc_setup(void)
{
	m48t37_base = ioremap(YOSEMITE_RTC_BASE, YOSEMITE_RTC_SIZE);
	if (!m48t37_base)
		printk(KERN_ERR "Mapping the RTC failed\n");

	rtc_get_time = m48t37y_get_time;
	rtc_set_time = m48t37y_set_time;

	write_seqlock(&xtime_lock);
	xtime.tv_sec = m48t37y_get_time();
	xtime.tv_nsec = 0;

	set_normalized_timespec(&wall_to_monotonic,
	                        -xtime.tv_sec, -xtime.tv_nsec);
	write_sequnlock(&xtime_lock);
}

/* Not only time init but that's what the hook it's called through is named */
static void __init py_late_time_init(void)
{
	py_map_ocd();
	py_uart_setup();
	py_rtc_setup();
}

static int __init pmc_yosemite_setup(void)
{
	board_time_init = yosemite_time_init;
	late_time_init = py_late_time_init;

	/* Add memory regions */
	add_memory_region(0x00000000, 0x10000000, BOOT_MEM_RAM);

#if 0 /* XXX Crash ...  */
	OCD_WRITE(RM9000x2_OCD_HTSC,
	          OCD_READ(RM9000x2_OCD_HTSC) | HYPERTRANSPORT_ENABLE);

	/* Set the BAR. Shifted mode */
	OCD_WRITE(RM9000x2_OCD_HTBAR0, HYPERTRANSPORT_BAR0_ADDR);
	OCD_WRITE(RM9000x2_OCD_HTMASK0, HYPERTRANSPORT_SIZE0);
#endif

	return 0;
}

early_initcall(pmc_yosemite_setup);
