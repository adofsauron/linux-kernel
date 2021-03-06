/*
 *   ALSA driver for VT1720/VT1724 (Envy24PT/Envy24HT)
 *
 *   Lowlevel functions for VT1720-based motherboards
 *
 *	Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */      

#include <sound/driver.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <sound/core.h>

#include "ice1712.h"
#include "vt1720_mobo.h"


static int __devinit k8x800_init(ice1712_t *ice)
{
	ice->vt1720 = 1;

	/* VT1616 codec */
	ice->num_total_dacs = 6;
	ice->num_total_adcs = 2;

	/* WM8728 codec */
	/* FIXME: TODO */

	return 0;
}

static int __devinit k8x800_add_controls(ice1712_t *ice)
{
	/* FIXME: needs some quirks for VT1616? */
	return 0;
}

/* EEPROM image */

static unsigned char k8x800_eeprom[] __devinitdata = {
	0x01,	/* SYSCONF: clock 256, 1ADC, 2DACs */
	0x02,	/* ACLINK: ACLINK, packed */
	0x00,	/* I2S: - */
	0x00,	/* SPDIF: - */
	0xff,	/* GPIO_DIR */
	0xff,	/* GPIO_DIR1 */
	0x00,	/* - */
	0xff,	/* GPIO_MASK */
	0xff,	/* GPIO_MASK1 */
	0x00,	/* - */
	0x00,	/* GPIO_STATE */
	0x00,	/* GPIO_STATE1 */
	0x00,	/* - */
};


/* entry point */
struct snd_ice1712_card_info snd_vt1720_mobo_cards[] __devinitdata = {
	{
		.subvendor = VT1720_SUBDEVICE_K8X800,
		.name = "Albatron K8X800 Pro II",
		.model = "k8x800",
		.chip_init = k8x800_init,
		.build_controls = k8x800_add_controls,
		.eeprom_size = sizeof(k8x800_eeprom),
		.eeprom_data = k8x800_eeprom,
	},
	{
		.subvendor = VT1720_SUBDEVICE_ZNF3_150,
		.name = "Chaintech ZNF3-150",
		/* identical with k8x800 */
		.chip_init = k8x800_init,
		.build_controls = k8x800_add_controls,
		.eeprom_size = sizeof(k8x800_eeprom),
		.eeprom_data = k8x800_eeprom,
	},
	{
		.subvendor = VT1720_SUBDEVICE_ZNF3_250,
		.name = "Chaintech ZNF3-250",
		/* identical with k8x800 */
		.chip_init = k8x800_init,
		.build_controls = k8x800_add_controls,
		.eeprom_size = sizeof(k8x800_eeprom),
		.eeprom_data = k8x800_eeprom,
	},
	{
		.subvendor = VT1720_SUBDEVICE_9CJS,
		.name = "Chaintech 9CJS",
		/* identical with k8x800 */
		.chip_init = k8x800_init,
		.build_controls = k8x800_add_controls,
		.eeprom_size = sizeof(k8x800_eeprom),
		.eeprom_data = k8x800_eeprom,
	},
	{
		.subvendor = VT1720_SUBDEVICE_SN25P,
		.name = "Shuttle SN25P",
		/* identical with k8x800 */
		.chip_init = k8x800_init,
		.build_controls = k8x800_add_controls,
		.eeprom_size = sizeof(k8x800_eeprom),
		.eeprom_data = k8x800_eeprom,
	},
	{ } /* terminator */
};

