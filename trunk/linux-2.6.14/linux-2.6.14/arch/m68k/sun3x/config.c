/*
 * Setup kernel for a Sun3x machine
 *
 * (C) 1999 Thomas Bogendoerfer (tsbogend@alpha.franken.de)
 *
 * based on code from Oliver Jowett <oliver@jowett.manawatu.gen.nz>
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/machdep.h>
#include <asm/irq.h>
#include <asm/sun3xprom.h>
#include <asm/sun3ints.h>
#include <asm/setup.h>
#include <asm/oplib.h>

#include "time.h"

volatile char *clock_va;
extern volatile unsigned char *sun3_intreg;

extern void sun3_get_model(char *model);

void sun3_leds(unsigned int i)
{

}

static int sun3x_get_hardware_list(char *buffer)
{

	int len = 0;

	len += sprintf(buffer + len, "PROM Revision:\t%s\n",
		       romvec->pv_monid);

	return len;

}

/*
 *  Setup the sun3x configuration info
 */
void __init config_sun3x(void)
{

	sun3x_prom_init();

	mach_get_irq_list	 = show_sun3_interrupts;
	mach_max_dma_address = 0xffffffff; /* we can DMA anywhere, whee */

	mach_default_handler = &sun3_default_handler;
	mach_sched_init      = sun3x_sched_init;
	mach_init_IRQ        = sun3_init_IRQ;
	enable_irq           = sun3_enable_irq;
	disable_irq          = sun3_disable_irq;
	mach_request_irq     = sun3_request_irq;
	mach_free_irq        = sun3_free_irq;
	mach_process_int     = sun3_process_int;

	mach_gettimeoffset   = sun3x_gettimeoffset;
	mach_reset           = sun3x_reboot;

	mach_hwclk           = sun3x_hwclk;
	mach_get_model       = sun3_get_model;
	mach_get_hardware_list = sun3x_get_hardware_list;

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp	     = &dummy_con;
#endif

	sun3_intreg = (unsigned char *)SUN3X_INTREG;

	/* only the serial console is known to work anyway... */
#if 0
	switch (*(unsigned char *)SUN3X_EEPROM_CONS) {
	case 0x10:
		serial_console = 1;
		conswitchp = NULL;
		break;
	case 0x11:
		serial_console = 2;
		conswitchp = NULL;
		break;
	default:
		serial_console = 0;
		conswitchp = &dummy_con;
		break;
	}
#endif

}

