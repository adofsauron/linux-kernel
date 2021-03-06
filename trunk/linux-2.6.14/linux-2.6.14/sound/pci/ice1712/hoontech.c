/*
 *   ALSA driver for ICEnsemble ICE1712 (Envy24)
 *
 *   Lowlevel functions for Hoontech STDSP24
 *
 *	Copyright (c) 2000 Jaroslav Kysela <perex@suse.cz>
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
#include "hoontech.h"


static void __devinit snd_ice1712_stdsp24_gpio_write(ice1712_t *ice, unsigned char byte)
{
	byte |= ICE1712_STDSP24_CLOCK_BIT;
	udelay(100);
	snd_ice1712_write(ice, ICE1712_IREG_GPIO_DATA, byte);
	byte &= ~ICE1712_STDSP24_CLOCK_BIT;
	udelay(100);
	snd_ice1712_write(ice, ICE1712_IREG_GPIO_DATA, byte);
	byte |= ICE1712_STDSP24_CLOCK_BIT;
	udelay(100);
	snd_ice1712_write(ice, ICE1712_IREG_GPIO_DATA, byte);
}

static void __devinit snd_ice1712_stdsp24_darear(ice1712_t *ice, int activate)
{
	down(&ice->gpio_mutex);
	ICE1712_STDSP24_0_DAREAR(ice->spec.hoontech.boxbits, activate);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[0]);
	up(&ice->gpio_mutex);
}

static void __devinit snd_ice1712_stdsp24_mute(ice1712_t *ice, int activate)
{
	down(&ice->gpio_mutex);
	ICE1712_STDSP24_3_MUTE(ice->spec.hoontech.boxbits, activate);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[3]);
	up(&ice->gpio_mutex);
}

static void __devinit snd_ice1712_stdsp24_insel(ice1712_t *ice, int activate)
{
	down(&ice->gpio_mutex);
	ICE1712_STDSP24_3_INSEL(ice->spec.hoontech.boxbits, activate);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[3]);
	up(&ice->gpio_mutex);
}

static void __devinit snd_ice1712_stdsp24_box_channel(ice1712_t *ice, int box, int chn, int activate)
{
	down(&ice->gpio_mutex);

	/* select box */
	ICE1712_STDSP24_0_BOX(ice->spec.hoontech.boxbits, box);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[0]);

	/* prepare for write */
	if (chn == 3)
		ICE1712_STDSP24_2_CHN4(ice->spec.hoontech.boxbits, 0);
	ICE1712_STDSP24_2_MIDI1(ice->spec.hoontech.boxbits, activate);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[2]);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[3]);

	ICE1712_STDSP24_1_CHN1(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_1_CHN2(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_1_CHN3(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_2_CHN4(ice->spec.hoontech.boxbits, 1);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[1]);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[2]);
	udelay(100);
	if (chn == 3) {
		ICE1712_STDSP24_2_CHN4(ice->spec.hoontech.boxbits, 0);
		snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[2]);
	} else {
		switch (chn) {
		case 0:	ICE1712_STDSP24_1_CHN1(ice->spec.hoontech.boxbits, 0); break;
		case 1:	ICE1712_STDSP24_1_CHN2(ice->spec.hoontech.boxbits, 0); break;
		case 2:	ICE1712_STDSP24_1_CHN3(ice->spec.hoontech.boxbits, 0); break;
		}
		snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[1]);
	}
	udelay(100);
	ICE1712_STDSP24_1_CHN1(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_1_CHN2(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_1_CHN3(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_2_CHN4(ice->spec.hoontech.boxbits, 1);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[1]);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[2]);
	udelay(100);

	ICE1712_STDSP24_2_MIDI1(ice->spec.hoontech.boxbits, 0);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[2]);

	up(&ice->gpio_mutex);
}

static void __devinit snd_ice1712_stdsp24_box_midi(ice1712_t *ice, int box, int master)
{
	down(&ice->gpio_mutex);

	/* select box */
	ICE1712_STDSP24_0_BOX(ice->spec.hoontech.boxbits, box);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[0]);

	ICE1712_STDSP24_2_MIDIIN(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_2_MIDI1(ice->spec.hoontech.boxbits, master);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[2]);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[3]);

	udelay(100);
	
	ICE1712_STDSP24_2_MIDIIN(ice->spec.hoontech.boxbits, 0);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[2]);
	
	mdelay(10);
	
	ICE1712_STDSP24_2_MIDIIN(ice->spec.hoontech.boxbits, 1);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[2]);

	up(&ice->gpio_mutex);
}

static void __devinit snd_ice1712_stdsp24_midi2(ice1712_t *ice, int activate)
{
	down(&ice->gpio_mutex);
	ICE1712_STDSP24_3_MIDI2(ice->spec.hoontech.boxbits, activate);
	snd_ice1712_stdsp24_gpio_write(ice, ice->spec.hoontech.boxbits[3]);
	up(&ice->gpio_mutex);
}

static int __devinit snd_ice1712_hoontech_init(ice1712_t *ice)
{
	int box, chn;

	ice->num_total_dacs = 8;
	ice->num_total_adcs = 8;

	ice->spec.hoontech.boxbits[0] = 
	ice->spec.hoontech.boxbits[1] = 
	ice->spec.hoontech.boxbits[2] = 
	ice->spec.hoontech.boxbits[3] = 0;	/* should be already */

	ICE1712_STDSP24_SET_ADDR(ice->spec.hoontech.boxbits, 0);
	ICE1712_STDSP24_CLOCK(ice->spec.hoontech.boxbits, 0, 1);
	ICE1712_STDSP24_0_BOX(ice->spec.hoontech.boxbits, 0);
	ICE1712_STDSP24_0_DAREAR(ice->spec.hoontech.boxbits, 0);

	ICE1712_STDSP24_SET_ADDR(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_CLOCK(ice->spec.hoontech.boxbits, 1, 1);
	ICE1712_STDSP24_1_CHN1(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_1_CHN2(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_1_CHN3(ice->spec.hoontech.boxbits, 1);
	
	ICE1712_STDSP24_SET_ADDR(ice->spec.hoontech.boxbits, 2);
	ICE1712_STDSP24_CLOCK(ice->spec.hoontech.boxbits, 2, 1);
	ICE1712_STDSP24_2_CHN4(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_2_MIDIIN(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_2_MIDI1(ice->spec.hoontech.boxbits, 0);

	ICE1712_STDSP24_SET_ADDR(ice->spec.hoontech.boxbits, 3);
	ICE1712_STDSP24_CLOCK(ice->spec.hoontech.boxbits, 3, 1);
	ICE1712_STDSP24_3_MIDI2(ice->spec.hoontech.boxbits, 0);
	ICE1712_STDSP24_3_MUTE(ice->spec.hoontech.boxbits, 1);
	ICE1712_STDSP24_3_INSEL(ice->spec.hoontech.boxbits, 0);

	/* let's go - activate only functions in first box */
	ice->spec.hoontech.config = 0;
			    /* ICE1712_STDSP24_MUTE |
			       ICE1712_STDSP24_INSEL |
			       ICE1712_STDSP24_DAREAR; */
	ice->spec.hoontech.boxconfig[0] = ICE1712_STDSP24_BOX_CHN1 |
				     ICE1712_STDSP24_BOX_CHN2 |
				     ICE1712_STDSP24_BOX_CHN3 |
				     ICE1712_STDSP24_BOX_CHN4 |
				     ICE1712_STDSP24_BOX_MIDI1 |
				     ICE1712_STDSP24_BOX_MIDI2;
	ice->spec.hoontech.boxconfig[1] = 
	ice->spec.hoontech.boxconfig[2] = 
	ice->spec.hoontech.boxconfig[3] = 0;
	snd_ice1712_stdsp24_darear(ice, (ice->spec.hoontech.config & ICE1712_STDSP24_DAREAR) ? 1 : 0);
	snd_ice1712_stdsp24_mute(ice, (ice->spec.hoontech.config & ICE1712_STDSP24_MUTE) ? 1 : 0);
	snd_ice1712_stdsp24_insel(ice, (ice->spec.hoontech.config & ICE1712_STDSP24_INSEL) ? 1 : 0);
	for (box = 0; box < 4; box++) {
		for (chn = 0; chn < 4; chn++)
			snd_ice1712_stdsp24_box_channel(ice, box, chn, (ice->spec.hoontech.boxconfig[box] & (1 << chn)) ? 1 : 0);
		snd_ice1712_stdsp24_box_midi(ice, box,
				(ice->spec.hoontech.boxconfig[box] & ICE1712_STDSP24_BOX_MIDI1) ? 1 : 0);
		if (ice->spec.hoontech.boxconfig[box] & ICE1712_STDSP24_BOX_MIDI2)
			snd_ice1712_stdsp24_midi2(ice, 1);
	}

	return 0;
}

/*
 * AK4524 access
 */

/* start callback for STDSP24 with modified hardware */
static void stdsp24_ak4524_lock(akm4xxx_t *ak, int chip)
{
	ice1712_t *ice = ak->private_data[0];
	unsigned char tmp;
	snd_ice1712_save_gpio_status(ice);
	tmp =	ICE1712_STDSP24_SERIAL_DATA |
		ICE1712_STDSP24_SERIAL_CLOCK |
		ICE1712_STDSP24_AK4524_CS;
	snd_ice1712_write(ice, ICE1712_IREG_GPIO_DIRECTION,
			  ice->gpio.direction | tmp);
	snd_ice1712_write(ice, ICE1712_IREG_GPIO_WRITE_MASK, ~tmp);
}

static int __devinit snd_ice1712_value_init(ice1712_t *ice)
{
	/* Hoontech STDSP24 with modified hardware */
	static akm4xxx_t akm_stdsp24_mv __devinitdata = {
		.num_adcs = 2,
		.num_dacs = 2,
		.type = SND_AK4524,
		.ops = {
			.lock = stdsp24_ak4524_lock
		}
	};

	static struct snd_ak4xxx_private akm_stdsp24_mv_priv __devinitdata = {
		.caddr = 2,
		.cif = 1, /* CIF high */
		.data_mask = ICE1712_STDSP24_SERIAL_DATA,
		.clk_mask = ICE1712_STDSP24_SERIAL_CLOCK,
		.cs_mask = ICE1712_STDSP24_AK4524_CS,
		.cs_addr = ICE1712_STDSP24_AK4524_CS,
		.cs_none = 0,
		.add_flags = 0,
	};

	int err;
	akm4xxx_t *ak;

	/* set the analog DACs */
	ice->num_total_dacs = 2;

	/* set the analog ADCs */
	ice->num_total_adcs = 2;
	
	/* analog section */
	ak = ice->akm = kmalloc(sizeof(akm4xxx_t), GFP_KERNEL);
	if (! ak)
		return -ENOMEM;
	ice->akm_codecs = 1;

	err = snd_ice1712_akm4xxx_init(ak, &akm_stdsp24_mv, &akm_stdsp24_mv_priv, ice);
	if (err < 0)
		return err;

	/* ak4524 controls */
	err = snd_ice1712_akm4xxx_build_controls(ice);
	if (err < 0)
		return err;

	return 0;
}

static int __devinit snd_ice1712_ez8_init(ice1712_t *ice)
{
	ice->gpio.write_mask = ice->eeprom.gpiomask;
	ice->gpio.direction = ice->eeprom.gpiodir;
	snd_ice1712_write(ice, ICE1712_IREG_GPIO_WRITE_MASK, ice->eeprom.gpiomask);
	snd_ice1712_write(ice, ICE1712_IREG_GPIO_DIRECTION, ice->eeprom.gpiodir);
	snd_ice1712_write(ice, ICE1712_IREG_GPIO_DATA, ice->eeprom.gpiostate);
	return 0;
}


/* entry point */
struct snd_ice1712_card_info snd_ice1712_hoontech_cards[] __devinitdata = {
	{
		.subvendor = ICE1712_SUBDEVICE_STDSP24,
		.name = "Hoontech SoundTrack Audio DSP24",
		.model = "dsp24",
		.chip_init = snd_ice1712_hoontech_init,
	},
	{
		.subvendor = ICE1712_SUBDEVICE_STDSP24_VALUE,	/* a dummy id */
		.name = "Hoontech SoundTrack Audio DSP24 Value",
		.model = "dsp24_value",
		.chip_init = snd_ice1712_value_init,
	},
	{
		.subvendor = ICE1712_SUBDEVICE_STDSP24_MEDIA7_1,
		.name = "Hoontech STA DSP24 Media 7.1",
		.model = "dsp24_71",
		.chip_init = snd_ice1712_hoontech_init,
	},
	{
		.subvendor = ICE1712_SUBDEVICE_EVENT_EZ8,	/* a dummy id */
		.name = "Event Electronics EZ8",
		.model = "ez8",
		.chip_init = snd_ice1712_ez8_init,
	},
	{ } /* terminator */
};

