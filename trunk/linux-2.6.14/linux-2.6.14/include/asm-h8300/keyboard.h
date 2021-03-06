/*
 *  linux/include/asm-h8300/keyboard.h
 *  Created 04 Dec 2001 by Khaled Hassounah <khassounah@mediumware.net>
 *  This file contains the Dragonball architecture specific keyboard definitions
 */

#ifndef _H8300_KEYBOARD_H
#define _H8300_KEYBOARD_H

#include <linux/config.h>

/* dummy i.e. no real keyboard */
#define kbd_setkeycode(x...)	(-ENOSYS)
#define kbd_getkeycode(x...)	(-ENOSYS)
#define kbd_translate(x...)	(0)
#define kbd_unexpected_up(x...)	(1)
#define kbd_leds(x...)		do {;} while (0)
#define kbd_init_hw(x...)	do {;} while (0)
#define kbd_enable_irq(x...)	do {;} while (0)
#define kbd_disable_irq(x...)	do {;} while (0)


/* needed if MAGIC_SYSRQ is enabled for serial console */
#ifndef SYSRQ_KEY
#define SYSRQ_KEY		((unsigned char)(-1))
#define kbd_sysrq_xlate         ((unsigned char *)NULL)
#endif


#endif  /* _H8300_KEYBOARD_H */



