#ifndef __ASM_SH_HP6XX_H
#define __ASM_SH_HP6XX_H

/*
 * Copyright (C) 2003  Andriy Skulysh
 */

#define HP680_TS_IRQ IRQ3_IRQ

#define DAC_LCD_BRIGHTNESS	0
#define DAC_SPEAKER_VOLUME	1

#define PHDR_TS_PEN_DOWN	0x08

#define SCPDR_TS_SCAN_ENABLE	0x20
#define SCPDR_TS_SCAN_Y		0x02
#define SCPDR_TS_SCAN_X		0x01

#define SCPCR_TS_ENABLE		0x405
#define SCPCR_TS_MASK		0xc0f

#define ADC_CHANNEL_TS_Y	1
#define ADC_CHANNEL_TS_X	2

#define HD64461_GPADR_SPEAKER	0x01
#define HD64461_GPADR_PCMCIA0	(0x02|0x08)
#define HD64461_GPBDR_LCDOFF	0x01
#define HD64461_GPBDR_LED_RED	0x80


#endif /* __ASM_SH_HP6XX_H */
