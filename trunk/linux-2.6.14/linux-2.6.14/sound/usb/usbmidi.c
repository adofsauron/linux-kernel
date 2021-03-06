/*
 * usbmidi.c - ALSA USB MIDI driver
 *
 * Copyright (c) 2002-2005 Clemens Ladisch
 * All rights reserved.
 *
 * Based on the OSS usb-midi driver by NAGANO Daisuke,
 *          NetBSD's umidi driver by Takuya SHIOZAKI,
 *          the "USB Device Class Definition for MIDI Devices" by Roland
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed and/or modified under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sound/driver.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/usb.h>
#include <sound/core.h>
#include <sound/minors.h>
#include <sound/rawmidi.h>
#include "usbaudio.h"


/*
 * define this to log all USB packets
 */
/* #define DUMP_PACKETS */

/*
 * how long to wait after some USB errors, so that khubd can disconnect() us
 * without too many spurious errors
 */
#define ERROR_DELAY_JIFFIES (HZ / 10)


MODULE_AUTHOR("Clemens Ladisch <clemens@ladisch.de>");
MODULE_DESCRIPTION("USB Audio/MIDI helper module");
MODULE_LICENSE("Dual BSD/GPL");


struct usb_ms_header_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDescriptorSubtype;
	__u8  bcdMSC[2];
	__le16 wTotalLength;
} __attribute__ ((packed));

struct usb_ms_endpoint_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDescriptorSubtype;
	__u8  bNumEmbMIDIJack;
	__u8  baAssocJackID[0];
} __attribute__ ((packed));

typedef struct snd_usb_midi snd_usb_midi_t;
typedef struct snd_usb_midi_endpoint snd_usb_midi_endpoint_t;
typedef struct snd_usb_midi_out_endpoint snd_usb_midi_out_endpoint_t;
typedef struct snd_usb_midi_in_endpoint snd_usb_midi_in_endpoint_t;
typedef struct usbmidi_out_port usbmidi_out_port_t;
typedef struct usbmidi_in_port usbmidi_in_port_t;

struct usb_protocol_ops {
	void (*input)(snd_usb_midi_in_endpoint_t*, uint8_t*, int);
	void (*output)(snd_usb_midi_out_endpoint_t*);
	void (*output_packet)(struct urb*, uint8_t, uint8_t, uint8_t, uint8_t);
	void (*init_out_endpoint)(snd_usb_midi_out_endpoint_t*);
	void (*finish_out_endpoint)(snd_usb_midi_out_endpoint_t*);
};

struct snd_usb_midi {
	snd_usb_audio_t *chip;
	struct usb_interface *iface;
	const snd_usb_audio_quirk_t *quirk;
	snd_rawmidi_t* rmidi;
	struct usb_protocol_ops* usb_protocol_ops;
	struct list_head list;
	struct timer_list error_timer;

	struct snd_usb_midi_endpoint {
		snd_usb_midi_out_endpoint_t *out;
		snd_usb_midi_in_endpoint_t *in;
	} endpoints[MIDI_MAX_ENDPOINTS];
	unsigned long input_triggered;
};

struct snd_usb_midi_out_endpoint {
	snd_usb_midi_t* umidi;
	struct urb* urb;
	int urb_active;
	int max_transfer;		/* size of urb buffer */
	struct tasklet_struct tasklet;

	spinlock_t buffer_lock;

	struct usbmidi_out_port {
		snd_usb_midi_out_endpoint_t* ep;
		snd_rawmidi_substream_t* substream;
		int active;
		uint8_t cable;		/* cable number << 4 */
		uint8_t state;
#define STATE_UNKNOWN	0
#define STATE_1PARAM	1
#define STATE_2PARAM_1	2
#define STATE_2PARAM_2	3
#define STATE_SYSEX_0	4
#define STATE_SYSEX_1	5
#define STATE_SYSEX_2	6
		uint8_t data[2];
	} ports[0x10];
	int current_port;
};

struct snd_usb_midi_in_endpoint {
	snd_usb_midi_t* umidi;
	struct urb* urb;
	struct usbmidi_in_port {
		snd_rawmidi_substream_t* substream;
	} ports[0x10];
	u8 seen_f5;
	u8 error_resubmit;
	int current_port;
};

static void snd_usbmidi_do_output(snd_usb_midi_out_endpoint_t* ep);

static const uint8_t snd_usbmidi_cin_length[] = {
	0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1
};

/*
 * Submits the URB, with error handling.
 */
static int snd_usbmidi_submit_urb(struct urb* urb, int flags)
{
	int err = usb_submit_urb(urb, flags);
	if (err < 0 && err != -ENODEV)
		snd_printk(KERN_ERR "usb_submit_urb: %d\n", err);
	return err;
}

/*
 * Error handling for URB completion functions.
 */
static int snd_usbmidi_urb_error(int status)
{
	switch (status) {
	/* manually unlinked, or device gone */
	case -ENOENT:
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -ENODEV:
		return -ENODEV;
	/* errors that might occur during unplugging */
	case -EPROTO:    /* EHCI */
	case -ETIMEDOUT: /* OHCI */
	case -EILSEQ:    /* UHCI */
		return -EIO;
	default:
		snd_printk(KERN_ERR "urb status %d\n", status);
		return 0; /* continue */
	}
}

/*
 * Receives a chunk of MIDI data.
 */
static void snd_usbmidi_input_data(snd_usb_midi_in_endpoint_t* ep, int portidx,
				   uint8_t* data, int length)
{
	usbmidi_in_port_t* port = &ep->ports[portidx];

	if (!port->substream) {
		snd_printd("unexpected port %d!\n", portidx);
		return;
	}
	if (!test_bit(port->substream->number, &ep->umidi->input_triggered))
		return;
	snd_rawmidi_receive(port->substream, data, length);
}

#ifdef DUMP_PACKETS
static void dump_urb(const char *type, const u8 *data, int length)
{
	snd_printk(KERN_DEBUG "%s packet: [", type);
	for (; length > 0; ++data, --length)
		printk(" %02x", *data);
	printk(" ]\n");
}
#else
#define dump_urb(type, data, length) /* nothing */
#endif

/*
 * Processes the data read from the device.
 */
static void snd_usbmidi_in_urb_complete(struct urb* urb, struct pt_regs *regs)
{
	snd_usb_midi_in_endpoint_t* ep = urb->context;

	if (urb->status == 0) {
		dump_urb("received", urb->transfer_buffer, urb->actual_length);
		ep->umidi->usb_protocol_ops->input(ep, urb->transfer_buffer,
						   urb->actual_length);
	} else {
		int err = snd_usbmidi_urb_error(urb->status);
		if (err < 0) {
			if (err != -ENODEV) {
				ep->error_resubmit = 1;
				mod_timer(&ep->umidi->error_timer,
					  jiffies + ERROR_DELAY_JIFFIES);
			}
			return;
		}
	}

	if (usb_pipe_needs_resubmit(urb->pipe)) {
		urb->dev = ep->umidi->chip->dev;
		snd_usbmidi_submit_urb(urb, GFP_ATOMIC);
	}
}

static void snd_usbmidi_out_urb_complete(struct urb* urb, struct pt_regs *regs)
{
	snd_usb_midi_out_endpoint_t* ep = urb->context;

	spin_lock(&ep->buffer_lock);
	ep->urb_active = 0;
	spin_unlock(&ep->buffer_lock);
	if (urb->status < 0) {
		int err = snd_usbmidi_urb_error(urb->status);
		if (err < 0) {
			if (err != -ENODEV)
				mod_timer(&ep->umidi->error_timer,
					  jiffies + ERROR_DELAY_JIFFIES);
			return;
		}
	}
	snd_usbmidi_do_output(ep);
}

/*
 * This is called when some data should be transferred to the device
 * (from one or more substreams).
 */
static void snd_usbmidi_do_output(snd_usb_midi_out_endpoint_t* ep)
{
	struct urb* urb = ep->urb;
	unsigned long flags;

	spin_lock_irqsave(&ep->buffer_lock, flags);
	if (ep->urb_active || ep->umidi->chip->shutdown) {
		spin_unlock_irqrestore(&ep->buffer_lock, flags);
		return;
	}

	urb->transfer_buffer_length = 0;
	ep->umidi->usb_protocol_ops->output(ep);

	if (urb->transfer_buffer_length > 0) {
		dump_urb("sending", urb->transfer_buffer,
			 urb->transfer_buffer_length);
		urb->dev = ep->umidi->chip->dev;
		ep->urb_active = snd_usbmidi_submit_urb(urb, GFP_ATOMIC) >= 0;
	}
	spin_unlock_irqrestore(&ep->buffer_lock, flags);
}

static void snd_usbmidi_out_tasklet(unsigned long data)
{
	snd_usb_midi_out_endpoint_t* ep = (snd_usb_midi_out_endpoint_t *) data;

	snd_usbmidi_do_output(ep);
}

/* called after transfers had been interrupted due to some USB error */
static void snd_usbmidi_error_timer(unsigned long data)
{
	snd_usb_midi_t *umidi = (snd_usb_midi_t *)data;
	int i;

	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i) {
		snd_usb_midi_in_endpoint_t *in = umidi->endpoints[i].in;
		if (in && in->error_resubmit) {
			in->error_resubmit = 0;
			in->urb->dev = umidi->chip->dev;
			snd_usbmidi_submit_urb(in->urb, GFP_ATOMIC);
		}
		if (umidi->endpoints[i].out)
			snd_usbmidi_do_output(umidi->endpoints[i].out);
	}
}

/* helper function to send static data that may not DMA-able */
static int send_bulk_static_data(snd_usb_midi_out_endpoint_t* ep,
				 const void *data, int len)
{
	int err;
	void *buf = kmalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memcpy(buf, data, len);
	dump_urb("sending", buf, len);
	err = usb_bulk_msg(ep->umidi->chip->dev, ep->urb->pipe, buf, len,
			   NULL, 250);
	kfree(buf);
	return err;
}

/*
 * Standard USB MIDI protocol: see the spec.
 * Midiman protocol: like the standard protocol, but the control byte is the
 * fourth byte in each packet, and uses length instead of CIN.
 */

static void snd_usbmidi_standard_input(snd_usb_midi_in_endpoint_t* ep,
				       uint8_t* buffer, int buffer_length)
{
	int i;

	for (i = 0; i + 3 < buffer_length; i += 4)
		if (buffer[i] != 0) {
			int cable = buffer[i] >> 4;
			int length = snd_usbmidi_cin_length[buffer[i] & 0x0f];
			snd_usbmidi_input_data(ep, cable, &buffer[i + 1], length);
		}
}

static void snd_usbmidi_midiman_input(snd_usb_midi_in_endpoint_t* ep,
				      uint8_t* buffer, int buffer_length)
{
	int i;

	for (i = 0; i + 3 < buffer_length; i += 4)
		if (buffer[i + 3] != 0) {
			int port = buffer[i + 3] >> 4;
			int length = buffer[i + 3] & 3;
			snd_usbmidi_input_data(ep, port, &buffer[i], length);
		}
}

/*
 * Adds one USB MIDI packet to the output buffer.
 */
static void snd_usbmidi_output_standard_packet(struct urb* urb, uint8_t p0,
					       uint8_t p1, uint8_t p2, uint8_t p3)
{

	uint8_t* buf = (uint8_t*)urb->transfer_buffer + urb->transfer_buffer_length;
	buf[0] = p0;
	buf[1] = p1;
	buf[2] = p2;
	buf[3] = p3;
	urb->transfer_buffer_length += 4;
}

/*
 * Adds one Midiman packet to the output buffer.
 */
static void snd_usbmidi_output_midiman_packet(struct urb* urb, uint8_t p0,
					      uint8_t p1, uint8_t p2, uint8_t p3)
{

	uint8_t* buf = (uint8_t*)urb->transfer_buffer + urb->transfer_buffer_length;
	buf[0] = p1;
	buf[1] = p2;
	buf[2] = p3;
	buf[3] = (p0 & 0xf0) | snd_usbmidi_cin_length[p0 & 0x0f];
	urb->transfer_buffer_length += 4;
}

/*
 * Converts MIDI commands to USB MIDI packets.
 */
static void snd_usbmidi_transmit_byte(usbmidi_out_port_t* port,
				      uint8_t b, struct urb* urb)
{
	uint8_t p0 = port->cable;
	void (*output_packet)(struct urb*, uint8_t, uint8_t, uint8_t, uint8_t) =
		port->ep->umidi->usb_protocol_ops->output_packet;

	if (b >= 0xf8) {
		output_packet(urb, p0 | 0x0f, b, 0, 0);
	} else if (b >= 0xf0) {
		switch (b) {
		case 0xf0:
			port->data[0] = b;
			port->state = STATE_SYSEX_1;
			break;
		case 0xf1:
		case 0xf3:
			port->data[0] = b;
			port->state = STATE_1PARAM;
			break;
		case 0xf2:
			port->data[0] = b;
			port->state = STATE_2PARAM_1;
			break;
		case 0xf4:
		case 0xf5:
			port->state = STATE_UNKNOWN;
			break;
		case 0xf6:
			output_packet(urb, p0 | 0x05, 0xf6, 0, 0);
			port->state = STATE_UNKNOWN;
			break;
		case 0xf7:
			switch (port->state) {
			case STATE_SYSEX_0:
				output_packet(urb, p0 | 0x05, 0xf7, 0, 0);
				break;
			case STATE_SYSEX_1:
				output_packet(urb, p0 | 0x06, port->data[0], 0xf7, 0);
				break;
			case STATE_SYSEX_2:
				output_packet(urb, p0 | 0x07, port->data[0], port->data[1], 0xf7);
				break;
			}
			port->state = STATE_UNKNOWN;
			break;
		}
	} else if (b >= 0x80) {
		port->data[0] = b;
		if (b >= 0xc0 && b <= 0xdf)
			port->state = STATE_1PARAM;
		else
			port->state = STATE_2PARAM_1;
	} else { /* b < 0x80 */
		switch (port->state) {
		case STATE_1PARAM:
			if (port->data[0] < 0xf0) {
				p0 |= port->data[0] >> 4;
			} else {
				p0 |= 0x02;
				port->state = STATE_UNKNOWN;
			}
			output_packet(urb, p0, port->data[0], b, 0);
			break;
		case STATE_2PARAM_1:
			port->data[1] = b;
			port->state = STATE_2PARAM_2;
			break;
		case STATE_2PARAM_2:
			if (port->data[0] < 0xf0) {
				p0 |= port->data[0] >> 4;
				port->state = STATE_2PARAM_1;
			} else {
				p0 |= 0x03;
				port->state = STATE_UNKNOWN;
			}
			output_packet(urb, p0, port->data[0], port->data[1], b);
			break;
		case STATE_SYSEX_0:
			port->data[0] = b;
			port->state = STATE_SYSEX_1;
			break;
		case STATE_SYSEX_1:
			port->data[1] = b;
			port->state = STATE_SYSEX_2;
			break;
		case STATE_SYSEX_2:
			output_packet(urb, p0 | 0x04, port->data[0], port->data[1], b);
			port->state = STATE_SYSEX_0;
			break;
		}
	}
}

static void snd_usbmidi_standard_output(snd_usb_midi_out_endpoint_t* ep)
{
	struct urb* urb = ep->urb;
	int p;

	/* FIXME: lower-numbered ports can starve higher-numbered ports */
	for (p = 0; p < 0x10; ++p) {
		usbmidi_out_port_t* port = &ep->ports[p];
		if (!port->active)
			continue;
		while (urb->transfer_buffer_length + 3 < ep->max_transfer) {
			uint8_t b;
			if (snd_rawmidi_transmit(port->substream, &b, 1) != 1) {
				port->active = 0;
				break;
			}
			snd_usbmidi_transmit_byte(port, b, urb);
		}
	}
}

static struct usb_protocol_ops snd_usbmidi_standard_ops = {
	.input = snd_usbmidi_standard_input,
	.output = snd_usbmidi_standard_output,
	.output_packet = snd_usbmidi_output_standard_packet,
};

static struct usb_protocol_ops snd_usbmidi_midiman_ops = {
	.input = snd_usbmidi_midiman_input,
	.output = snd_usbmidi_standard_output, 
	.output_packet = snd_usbmidi_output_midiman_packet,
};

/*
 * Novation USB MIDI protocol: number of data bytes is in the first byte
 * (when receiving) (+1!) or in the second byte (when sending); data begins
 * at the third byte.
 */

static void snd_usbmidi_novation_input(snd_usb_midi_in_endpoint_t* ep,
				       uint8_t* buffer, int buffer_length)
{
	if (buffer_length < 2 || !buffer[0] || buffer_length < buffer[0] + 1)
		return;
	snd_usbmidi_input_data(ep, 0, &buffer[2], buffer[0] - 1);
}

static void snd_usbmidi_novation_output(snd_usb_midi_out_endpoint_t* ep)
{
	uint8_t* transfer_buffer;
	int count;

	if (!ep->ports[0].active)
		return;
	transfer_buffer = ep->urb->transfer_buffer;
	count = snd_rawmidi_transmit(ep->ports[0].substream,
				     &transfer_buffer[2],
				     ep->max_transfer - 2);
	if (count < 1) {
		ep->ports[0].active = 0;
		return;
	}
	transfer_buffer[0] = 0;
	transfer_buffer[1] = count;
	ep->urb->transfer_buffer_length = 2 + count;
}

static struct usb_protocol_ops snd_usbmidi_novation_ops = {
	.input = snd_usbmidi_novation_input,
	.output = snd_usbmidi_novation_output,
};

/*
 * "raw" protocol: used by the MOTU FastLane.
 */

static void snd_usbmidi_raw_input(snd_usb_midi_in_endpoint_t* ep,
				  uint8_t* buffer, int buffer_length)
{
	snd_usbmidi_input_data(ep, 0, buffer, buffer_length);
}

static void snd_usbmidi_raw_output(snd_usb_midi_out_endpoint_t* ep)
{
	int count;

	if (!ep->ports[0].active)
		return;
	count = snd_rawmidi_transmit(ep->ports[0].substream,
				     ep->urb->transfer_buffer,
				     ep->max_transfer);
	if (count < 1) {
		ep->ports[0].active = 0;
		return;
	}
	ep->urb->transfer_buffer_length = count;
}

static struct usb_protocol_ops snd_usbmidi_raw_ops = {
	.input = snd_usbmidi_raw_input,
	.output = snd_usbmidi_raw_output,
};

/*
 * Emagic USB MIDI protocol: raw MIDI with "F5 xx" port switching.
 */

static void snd_usbmidi_emagic_init_out(snd_usb_midi_out_endpoint_t* ep)
{
	static const u8 init_data[] = {
		/* initialization magic: "get version" */
		0xf0,
		0x00, 0x20, 0x31,	/* Emagic */
		0x64,			/* Unitor8 */
		0x0b,			/* version number request */
		0x00,			/* command version */
		0x00,			/* EEPROM, box 0 */
		0xf7
	};
	send_bulk_static_data(ep, init_data, sizeof(init_data));
	/* while we're at it, pour on more magic */
	send_bulk_static_data(ep, init_data, sizeof(init_data));
}

static void snd_usbmidi_emagic_finish_out(snd_usb_midi_out_endpoint_t* ep)
{
	static const u8 finish_data[] = {
		/* switch to patch mode with last preset */
		0xf0,
		0x00, 0x20, 0x31,	/* Emagic */
		0x64,			/* Unitor8 */
		0x10,			/* patch switch command */
		0x00,			/* command version */
		0x7f,			/* to all boxes */
		0x40,			/* last preset in EEPROM */
		0xf7
	};
	send_bulk_static_data(ep, finish_data, sizeof(finish_data));
}

static void snd_usbmidi_emagic_input(snd_usb_midi_in_endpoint_t* ep,
				     uint8_t* buffer, int buffer_length)
{
	int i;

	/* FF indicates end of valid data */
	for (i = 0; i < buffer_length; ++i)
		if (buffer[i] == 0xff) {
			buffer_length = i;
			break;
		}

	/* handle F5 at end of last buffer */
	if (ep->seen_f5)
		goto switch_port;

	while (buffer_length > 0) {
		/* determine size of data until next F5 */
		for (i = 0; i < buffer_length; ++i)
			if (buffer[i] == 0xf5)
				break;
		snd_usbmidi_input_data(ep, ep->current_port, buffer, i);
		buffer += i;
		buffer_length -= i;

		if (buffer_length <= 0)
			break;
		/* assert(buffer[0] == 0xf5); */
		ep->seen_f5 = 1;
		++buffer;
		--buffer_length;

	switch_port:
		if (buffer_length <= 0)
			break;
		if (buffer[0] < 0x80) {
			ep->current_port = (buffer[0] - 1) & 15;
			++buffer;
			--buffer_length;
		}
		ep->seen_f5 = 0;
	}
}

static void snd_usbmidi_emagic_output(snd_usb_midi_out_endpoint_t* ep)
{
	int port0 = ep->current_port;
	uint8_t* buf = ep->urb->transfer_buffer;
	int buf_free = ep->max_transfer;
	int length, i;

	for (i = 0; i < 0x10; ++i) {
		/* round-robin, starting at the last current port */
		int portnum = (port0 + i) & 15;
		usbmidi_out_port_t* port = &ep->ports[portnum];

		if (!port->active)
			continue;
		if (snd_rawmidi_transmit_peek(port->substream, buf, 1) != 1) {
			port->active = 0;
			continue;
		}

		if (portnum != ep->current_port) {
			if (buf_free < 2)
				break;
			ep->current_port = portnum;
			buf[0] = 0xf5;
			buf[1] = (portnum + 1) & 15;
			buf += 2;
			buf_free -= 2;
		}

		if (buf_free < 1)
			break;
		length = snd_rawmidi_transmit(port->substream, buf, buf_free);
		if (length > 0) {
			buf += length;
			buf_free -= length;
			if (buf_free < 1)
				break;
		}
	}
	if (buf_free < ep->max_transfer && buf_free > 0) {
		*buf = 0xff;
		--buf_free;
	}
	ep->urb->transfer_buffer_length = ep->max_transfer - buf_free;
}

static struct usb_protocol_ops snd_usbmidi_emagic_ops = {
	.input = snd_usbmidi_emagic_input,
	.output = snd_usbmidi_emagic_output,
	.init_out_endpoint = snd_usbmidi_emagic_init_out,
	.finish_out_endpoint = snd_usbmidi_emagic_finish_out,
};


static int snd_usbmidi_output_open(snd_rawmidi_substream_t* substream)
{
	snd_usb_midi_t* umidi = substream->rmidi->private_data;
	usbmidi_out_port_t* port = NULL;
	int i, j;

	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i)
		if (umidi->endpoints[i].out)
			for (j = 0; j < 0x10; ++j)
				if (umidi->endpoints[i].out->ports[j].substream == substream) {
					port = &umidi->endpoints[i].out->ports[j];
					break;
				}
	if (!port) {
		snd_BUG();
		return -ENXIO;
	}
	substream->runtime->private_data = port;
	port->state = STATE_UNKNOWN;
	return 0;
}

static int snd_usbmidi_output_close(snd_rawmidi_substream_t* substream)
{
	return 0;
}

static void snd_usbmidi_output_trigger(snd_rawmidi_substream_t* substream, int up)
{
	usbmidi_out_port_t* port = (usbmidi_out_port_t*)substream->runtime->private_data;

	port->active = up;
	if (up) {
		if (port->ep->umidi->chip->shutdown) {
			/* gobble up remaining bytes to prevent wait in
			 * snd_rawmidi_drain_output */
			while (!snd_rawmidi_transmit_empty(substream))
				snd_rawmidi_transmit_ack(substream, 1);
			return;
		}
		tasklet_hi_schedule(&port->ep->tasklet);
	}
}

static int snd_usbmidi_input_open(snd_rawmidi_substream_t* substream)
{
	return 0;
}

static int snd_usbmidi_input_close(snd_rawmidi_substream_t* substream)
{
	return 0;
}

static void snd_usbmidi_input_trigger(snd_rawmidi_substream_t* substream, int up)
{
	snd_usb_midi_t* umidi = substream->rmidi->private_data;

	if (up)
		set_bit(substream->number, &umidi->input_triggered);
	else
		clear_bit(substream->number, &umidi->input_triggered);
}

static snd_rawmidi_ops_t snd_usbmidi_output_ops = {
	.open = snd_usbmidi_output_open,
	.close = snd_usbmidi_output_close,
	.trigger = snd_usbmidi_output_trigger,
};

static snd_rawmidi_ops_t snd_usbmidi_input_ops = {
	.open = snd_usbmidi_input_open,
	.close = snd_usbmidi_input_close,
	.trigger = snd_usbmidi_input_trigger
};

/*
 * Frees an input endpoint.
 * May be called when ep hasn't been initialized completely.
 */
static void snd_usbmidi_in_endpoint_delete(snd_usb_midi_in_endpoint_t* ep)
{
	if (ep->urb) {
		usb_buffer_free(ep->umidi->chip->dev,
				ep->urb->transfer_buffer_length,
				ep->urb->transfer_buffer,
				ep->urb->transfer_dma);
		usb_free_urb(ep->urb);
	}
	kfree(ep);
}

/*
 * Creates an input endpoint.
 */
static int snd_usbmidi_in_endpoint_create(snd_usb_midi_t* umidi,
					  snd_usb_midi_endpoint_info_t* ep_info,
					  snd_usb_midi_endpoint_t* rep)
{
	snd_usb_midi_in_endpoint_t* ep;
	void* buffer;
	unsigned int pipe;
	int length;

	rep->in = NULL;
	ep = kzalloc(sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;
	ep->umidi = umidi;

	ep->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ep->urb) {
		snd_usbmidi_in_endpoint_delete(ep);
		return -ENOMEM;
	}
	if (ep_info->in_interval)
		pipe = usb_rcvintpipe(umidi->chip->dev, ep_info->in_ep);
	else
		pipe = usb_rcvbulkpipe(umidi->chip->dev, ep_info->in_ep);
	length = usb_maxpacket(umidi->chip->dev, pipe, 0);
	buffer = usb_buffer_alloc(umidi->chip->dev, length, GFP_KERNEL,
				  &ep->urb->transfer_dma);
	if (!buffer) {
		snd_usbmidi_in_endpoint_delete(ep);
		return -ENOMEM;
	}
	if (ep_info->in_interval)
		usb_fill_int_urb(ep->urb, umidi->chip->dev, pipe, buffer, length,
				 snd_usb_complete_callback(snd_usbmidi_in_urb_complete),
				 ep, ep_info->in_interval);
	else
		usb_fill_bulk_urb(ep->urb, umidi->chip->dev, pipe, buffer, length,
				  snd_usb_complete_callback(snd_usbmidi_in_urb_complete),
				  ep);
	ep->urb->transfer_flags = URB_NO_TRANSFER_DMA_MAP;

	rep->in = ep;
	return 0;
}

static unsigned int snd_usbmidi_count_bits(unsigned int x)
{
	unsigned int bits = 0;

	for (; x; x >>= 1)
		bits += x & 1;
	return bits;
}

/*
 * Frees an output endpoint.
 * May be called when ep hasn't been initialized completely.
 */
static void snd_usbmidi_out_endpoint_delete(snd_usb_midi_out_endpoint_t* ep)
{
	if (ep->urb) {
		usb_buffer_free(ep->umidi->chip->dev, ep->max_transfer,
				ep->urb->transfer_buffer,
				ep->urb->transfer_dma);
		usb_free_urb(ep->urb);
	}
	kfree(ep);
}

/*
 * Creates an output endpoint, and initializes output ports.
 */
static int snd_usbmidi_out_endpoint_create(snd_usb_midi_t* umidi,
					   snd_usb_midi_endpoint_info_t* ep_info,
			 		   snd_usb_midi_endpoint_t* rep)
{
	snd_usb_midi_out_endpoint_t* ep;
	int i;
	unsigned int pipe;
	void* buffer;

	rep->out = NULL;
	ep = kzalloc(sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;
	ep->umidi = umidi;

	ep->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ep->urb) {
		snd_usbmidi_out_endpoint_delete(ep);
		return -ENOMEM;
	}
	/* we never use interrupt output pipes */
	pipe = usb_sndbulkpipe(umidi->chip->dev, ep_info->out_ep);
	ep->max_transfer = usb_maxpacket(umidi->chip->dev, pipe, 1);
	buffer = usb_buffer_alloc(umidi->chip->dev, ep->max_transfer,
				  GFP_KERNEL, &ep->urb->transfer_dma);
	if (!buffer) {
		snd_usbmidi_out_endpoint_delete(ep);
		return -ENOMEM;
	}
	usb_fill_bulk_urb(ep->urb, umidi->chip->dev, pipe, buffer,
			  ep->max_transfer,
			  snd_usb_complete_callback(snd_usbmidi_out_urb_complete), ep);
	ep->urb->transfer_flags = URB_NO_TRANSFER_DMA_MAP;

	spin_lock_init(&ep->buffer_lock);
	tasklet_init(&ep->tasklet, snd_usbmidi_out_tasklet, (unsigned long)ep);

	for (i = 0; i < 0x10; ++i)
		if (ep_info->out_cables & (1 << i)) {
			ep->ports[i].ep = ep;
			ep->ports[i].cable = i << 4;
		}

	if (umidi->usb_protocol_ops->init_out_endpoint)
		umidi->usb_protocol_ops->init_out_endpoint(ep);

	rep->out = ep;
	return 0;
}

/*
 * Frees everything.
 */
static void snd_usbmidi_free(snd_usb_midi_t* umidi)
{
	int i;

	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i) {
		snd_usb_midi_endpoint_t* ep = &umidi->endpoints[i];
		if (ep->out)
			snd_usbmidi_out_endpoint_delete(ep->out);
		if (ep->in)
			snd_usbmidi_in_endpoint_delete(ep->in);
	}
	kfree(umidi);
}

/*
 * Unlinks all URBs (must be done before the usb_device is deleted).
 */
void snd_usbmidi_disconnect(struct list_head* p)
{
	snd_usb_midi_t* umidi;
	int i;

	umidi = list_entry(p, snd_usb_midi_t, list);
	del_timer_sync(&umidi->error_timer);
	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i) {
		snd_usb_midi_endpoint_t* ep = &umidi->endpoints[i];
		if (ep->out)
			tasklet_kill(&ep->out->tasklet);
		if (ep->out && ep->out->urb) {
			usb_kill_urb(ep->out->urb);
			if (umidi->usb_protocol_ops->finish_out_endpoint)
				umidi->usb_protocol_ops->finish_out_endpoint(ep->out);
		}
		if (ep->in && ep->in->urb)
			usb_kill_urb(ep->in->urb);
	}
}

static void snd_usbmidi_rawmidi_free(snd_rawmidi_t* rmidi)
{
	snd_usb_midi_t* umidi = rmidi->private_data;
	snd_usbmidi_free(umidi);
}

static snd_rawmidi_substream_t* snd_usbmidi_find_substream(snd_usb_midi_t* umidi,
							   int stream, int number)
{
	struct list_head* list;

	list_for_each(list, &umidi->rmidi->streams[stream].substreams) {
		snd_rawmidi_substream_t* substream = list_entry(list, snd_rawmidi_substream_t, list);
		if (substream->number == number)
			return substream;
	}
	return NULL;
}

/*
 * This list specifies names for ports that do not fit into the standard
 * "(product) MIDI (n)" schema because they aren't external MIDI ports,
 * such as internal control or synthesizer ports.
 */
static struct {
	u32 id;
	int port;
	const char *name_format;
} snd_usbmidi_port_names[] = {
	/* Roland UA-100 */
	{ USB_ID(0x0582, 0x0000), 2, "%s Control" },
	/* Roland SC-8850 */
	{ USB_ID(0x0582, 0x0003), 0, "%s Part A" },
	{ USB_ID(0x0582, 0x0003), 1, "%s Part B" },
	{ USB_ID(0x0582, 0x0003), 2, "%s Part C" },
	{ USB_ID(0x0582, 0x0003), 3, "%s Part D" },
	{ USB_ID(0x0582, 0x0003), 4, "%s MIDI 1" },
	{ USB_ID(0x0582, 0x0003), 5, "%s MIDI 2" },
	/* Roland U-8 */
	{ USB_ID(0x0582, 0x0004), 0, "%s MIDI" },
	{ USB_ID(0x0582, 0x0004), 1, "%s Control" },
	/* Roland SC-8820 */
	{ USB_ID(0x0582, 0x0007), 0, "%s Part A" },
	{ USB_ID(0x0582, 0x0007), 1, "%s Part B" },
	{ USB_ID(0x0582, 0x0007), 2, "%s MIDI" },
	/* Roland SK-500 */
	{ USB_ID(0x0582, 0x000b), 0, "%s Part A" },
	{ USB_ID(0x0582, 0x000b), 1, "%s Part B" },
	{ USB_ID(0x0582, 0x000b), 2, "%s MIDI" },
	/* Roland SC-D70 */
	{ USB_ID(0x0582, 0x000c), 0, "%s Part A" },
	{ USB_ID(0x0582, 0x000c), 1, "%s Part B" },
	{ USB_ID(0x0582, 0x000c), 2, "%s MIDI" },
	/* Edirol UM-880 */
	{ USB_ID(0x0582, 0x0014), 8, "%s Control" },
	/* Edirol SD-90 */
	{ USB_ID(0x0582, 0x0016), 0, "%s Part A" },
	{ USB_ID(0x0582, 0x0016), 1, "%s Part B" },
	{ USB_ID(0x0582, 0x0016), 2, "%s MIDI 1" },
	{ USB_ID(0x0582, 0x0016), 3, "%s MIDI 2" },
	/* Edirol UM-550 */
	{ USB_ID(0x0582, 0x0023), 5, "%s Control" },
	/* Edirol SD-20 */
	{ USB_ID(0x0582, 0x0027), 0, "%s Part A" },
	{ USB_ID(0x0582, 0x0027), 1, "%s Part B" },
	{ USB_ID(0x0582, 0x0027), 2, "%s MIDI" },
	/* Edirol SD-80 */
	{ USB_ID(0x0582, 0x0029), 0, "%s Part A" },
	{ USB_ID(0x0582, 0x0029), 1, "%s Part B" },
	{ USB_ID(0x0582, 0x0029), 2, "%s MIDI 1" },
	{ USB_ID(0x0582, 0x0029), 3, "%s MIDI 2" },
	/* Edirol UA-700 */
	{ USB_ID(0x0582, 0x002b), 0, "%s MIDI" },
	{ USB_ID(0x0582, 0x002b), 1, "%s Control" },
	/* Roland VariOS */
	{ USB_ID(0x0582, 0x002f), 0, "%s MIDI" },
	{ USB_ID(0x0582, 0x002f), 1, "%s External MIDI" },
	{ USB_ID(0x0582, 0x002f), 2, "%s Sync" },
	/* Edirol PCR */
	{ USB_ID(0x0582, 0x0033), 0, "%s MIDI" },
	{ USB_ID(0x0582, 0x0033), 1, "%s 1" },
	{ USB_ID(0x0582, 0x0033), 2, "%s 2" },
	/* BOSS GS-10 */
	{ USB_ID(0x0582, 0x003b), 0, "%s MIDI" },
	{ USB_ID(0x0582, 0x003b), 1, "%s Control" },
	/* Edirol UA-1000 */
	{ USB_ID(0x0582, 0x0044), 0, "%s MIDI" },
	{ USB_ID(0x0582, 0x0044), 1, "%s Control" },
	/* Edirol UR-80 */
	{ USB_ID(0x0582, 0x0048), 0, "%s MIDI" },
	{ USB_ID(0x0582, 0x0048), 1, "%s 1" },
	{ USB_ID(0x0582, 0x0048), 2, "%s 2" },
	/* Edirol PCR-A */
	{ USB_ID(0x0582, 0x004d), 0, "%s MIDI" },
	{ USB_ID(0x0582, 0x004d), 1, "%s 1" },
	{ USB_ID(0x0582, 0x004d), 2, "%s 2" },
	/* M-Audio MidiSport 8x8 */
	{ USB_ID(0x0763, 0x1031), 8, "%s Control" },
	{ USB_ID(0x0763, 0x1033), 8, "%s Control" },
	/* MOTU Fastlane */
	{ USB_ID(0x07fd, 0x0001), 0, "%s MIDI A" },
	{ USB_ID(0x07fd, 0x0001), 1, "%s MIDI B" },
	/* Emagic Unitor8/AMT8/MT4 */
	{ USB_ID(0x086a, 0x0001), 8, "%s Broadcast" },
	{ USB_ID(0x086a, 0x0002), 8, "%s Broadcast" },
	{ USB_ID(0x086a, 0x0003), 4, "%s Broadcast" },
};

static void snd_usbmidi_init_substream(snd_usb_midi_t* umidi,
				       int stream, int number,
				       snd_rawmidi_substream_t** rsubstream)
{
	int i;
	const char *name_format;

	snd_rawmidi_substream_t* substream = snd_usbmidi_find_substream(umidi, stream, number);
	if (!substream) {
		snd_printd(KERN_ERR "substream %d:%d not found\n", stream, number);
		return;
	}

	/* TODO: read port name from jack descriptor */
	name_format = "%s MIDI %d";
	for (i = 0; i < ARRAY_SIZE(snd_usbmidi_port_names); ++i) {
		if (snd_usbmidi_port_names[i].id == umidi->chip->usb_id &&
		    snd_usbmidi_port_names[i].port == number) {
			name_format = snd_usbmidi_port_names[i].name_format;
			break;
		}
	}
	snprintf(substream->name, sizeof(substream->name),
		 name_format, umidi->chip->card->shortname, number + 1);

	*rsubstream = substream;
}

/*
 * Creates the endpoints and their ports.
 */
static int snd_usbmidi_create_endpoints(snd_usb_midi_t* umidi,
					snd_usb_midi_endpoint_info_t* endpoints)
{
	int i, j, err;
	int out_ports = 0, in_ports = 0;

	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i) {
		if (endpoints[i].out_cables) {
			err = snd_usbmidi_out_endpoint_create(umidi, &endpoints[i],
							      &umidi->endpoints[i]);
			if (err < 0)
				return err;
		}
		if (endpoints[i].in_cables) {
			err = snd_usbmidi_in_endpoint_create(umidi, &endpoints[i],
							     &umidi->endpoints[i]);
			if (err < 0)
				return err;
		}

		for (j = 0; j < 0x10; ++j) {
			if (endpoints[i].out_cables & (1 << j)) {
				snd_usbmidi_init_substream(umidi, SNDRV_RAWMIDI_STREAM_OUTPUT, out_ports,
							   &umidi->endpoints[i].out->ports[j].substream);
				++out_ports;
			}
			if (endpoints[i].in_cables & (1 << j)) {
				snd_usbmidi_init_substream(umidi, SNDRV_RAWMIDI_STREAM_INPUT, in_ports,
							   &umidi->endpoints[i].in->ports[j].substream);
				++in_ports;
			}
		}
	}
	snd_printdd(KERN_INFO "created %d output and %d input ports\n",
		    out_ports, in_ports);
	return 0;
}

/*
 * Returns MIDIStreaming device capabilities.
 */
static int snd_usbmidi_get_ms_info(snd_usb_midi_t* umidi,
			   	   snd_usb_midi_endpoint_info_t* endpoints)
{
	struct usb_interface* intf;
	struct usb_host_interface *hostif;
	struct usb_interface_descriptor* intfd;
	struct usb_ms_header_descriptor* ms_header;
	struct usb_host_endpoint *hostep;
	struct usb_endpoint_descriptor* ep;
	struct usb_ms_endpoint_descriptor* ms_ep;
	int i, epidx;

	intf = umidi->iface;
	if (!intf)
		return -ENXIO;
	hostif = &intf->altsetting[0];
	intfd = get_iface_desc(hostif);
	ms_header = (struct usb_ms_header_descriptor*)hostif->extra;
	if (hostif->extralen >= 7 &&
	    ms_header->bLength >= 7 &&
	    ms_header->bDescriptorType == USB_DT_CS_INTERFACE &&
	    ms_header->bDescriptorSubtype == HEADER)
		snd_printdd(KERN_INFO "MIDIStreaming version %02x.%02x\n",
			    ms_header->bcdMSC[1], ms_header->bcdMSC[0]);
	else
		snd_printk(KERN_WARNING "MIDIStreaming interface descriptor not found\n");

	epidx = 0;
	for (i = 0; i < intfd->bNumEndpoints; ++i) {
		hostep = &hostif->endpoint[i];
		ep = get_ep_desc(hostep);
		if ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_BULK &&
		    (ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_INT)
			continue;
		ms_ep = (struct usb_ms_endpoint_descriptor*)hostep->extra;
		if (hostep->extralen < 4 ||
		    ms_ep->bLength < 4 ||
		    ms_ep->bDescriptorType != USB_DT_CS_ENDPOINT ||
		    ms_ep->bDescriptorSubtype != MS_GENERAL)
			continue;
		if ((ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT) {
			if (endpoints[epidx].out_ep) {
				if (++epidx >= MIDI_MAX_ENDPOINTS) {
					snd_printk(KERN_WARNING "too many endpoints\n");
					break;
				}
			}
			endpoints[epidx].out_ep = ep->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
			if ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT)
				endpoints[epidx].out_interval = ep->bInterval;
			endpoints[epidx].out_cables = (1 << ms_ep->bNumEmbMIDIJack) - 1;
			snd_printdd(KERN_INFO "EP %02X: %d jack(s)\n",
				    ep->bEndpointAddress, ms_ep->bNumEmbMIDIJack);
		} else {
			if (endpoints[epidx].in_ep) {
				if (++epidx >= MIDI_MAX_ENDPOINTS) {
					snd_printk(KERN_WARNING "too many endpoints\n");
					break;
				}
			}
			endpoints[epidx].in_ep = ep->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
			if ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT)
				endpoints[epidx].in_interval = ep->bInterval;
			endpoints[epidx].in_cables = (1 << ms_ep->bNumEmbMIDIJack) - 1;
			snd_printdd(KERN_INFO "EP %02X: %d jack(s)\n",
				    ep->bEndpointAddress, ms_ep->bNumEmbMIDIJack);
		}
	}
	return 0;
}

/*
 * On Roland devices, use the second alternate setting to be able to use
 * the interrupt input endpoint.
 */
static void snd_usbmidi_switch_roland_altsetting(snd_usb_midi_t* umidi)
{
	struct usb_interface* intf;
	struct usb_host_interface *hostif;
	struct usb_interface_descriptor* intfd;

	intf = umidi->iface;
	if (!intf || intf->num_altsetting != 2)
		return;

	hostif = &intf->altsetting[1];
	intfd = get_iface_desc(hostif);
	if (intfd->bNumEndpoints != 2 ||
	    (get_endpoint(hostif, 0)->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_BULK ||
	    (get_endpoint(hostif, 1)->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_INT)
		return;

	snd_printdd(KERN_INFO "switching to altsetting %d with int ep\n",
		    intfd->bAlternateSetting);
	usb_set_interface(umidi->chip->dev, intfd->bInterfaceNumber,
			  intfd->bAlternateSetting);
}

/*
 * Try to find any usable endpoints in the interface.
 */
static int snd_usbmidi_detect_endpoints(snd_usb_midi_t* umidi,
					snd_usb_midi_endpoint_info_t* endpoint,
					int max_endpoints)
{
	struct usb_interface* intf;
	struct usb_host_interface *hostif;
	struct usb_interface_descriptor* intfd;
	struct usb_endpoint_descriptor* epd;
	int i, out_eps = 0, in_eps = 0;

	if (USB_ID_VENDOR(umidi->chip->usb_id) == 0x0582)
		snd_usbmidi_switch_roland_altsetting(umidi);

	if (endpoint[0].out_ep || endpoint[0].in_ep)
		return 0;	

	intf = umidi->iface;
	if (!intf || intf->num_altsetting < 1)
		return -ENOENT;
	hostif = intf->cur_altsetting;
	intfd = get_iface_desc(hostif);

	for (i = 0; i < intfd->bNumEndpoints; ++i) {
		epd = get_endpoint(hostif, i);
		if ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_BULK &&
		    (epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_INT)
			continue;
		if (out_eps < max_endpoints &&
		    (epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT) {
			endpoint[out_eps].out_ep = epd->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
			if ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT)
				endpoint[out_eps].out_interval = epd->bInterval;
			++out_eps;
		}
		if (in_eps < max_endpoints &&
		    (epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) {
			endpoint[in_eps].in_ep = epd->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
			if ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT)
				endpoint[in_eps].in_interval = epd->bInterval;
			++in_eps;
		}
	}
	return (out_eps || in_eps) ? 0 : -ENOENT;
}

/*
 * Detects the endpoints for one-port-per-endpoint protocols.
 */
static int snd_usbmidi_detect_per_port_endpoints(snd_usb_midi_t* umidi,
						 snd_usb_midi_endpoint_info_t* endpoints)
{
	int err, i;
	
	err = snd_usbmidi_detect_endpoints(umidi, endpoints, MIDI_MAX_ENDPOINTS);
	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i) {
		if (endpoints[i].out_ep)
			endpoints[i].out_cables = 0x0001;
		if (endpoints[i].in_ep)
			endpoints[i].in_cables = 0x0001;
	}
	return err;
}

/*
 * Detects the endpoints and ports of Yamaha devices.
 */
static int snd_usbmidi_detect_yamaha(snd_usb_midi_t* umidi,
				     snd_usb_midi_endpoint_info_t* endpoint)
{
	struct usb_interface* intf;
	struct usb_host_interface *hostif;
	struct usb_interface_descriptor* intfd;
	uint8_t* cs_desc;

	intf = umidi->iface;
	if (!intf)
		return -ENOENT;
	hostif = intf->altsetting;
	intfd = get_iface_desc(hostif);
	if (intfd->bNumEndpoints < 1)
		return -ENOENT;

	/*
	 * For each port there is one MIDI_IN/OUT_JACK descriptor, not
	 * necessarily with any useful contents.  So simply count 'em.
	 */
	for (cs_desc = hostif->extra;
	     cs_desc < hostif->extra + hostif->extralen && cs_desc[0] >= 2;
	     cs_desc += cs_desc[0]) {
		if (cs_desc[1] == CS_AUDIO_INTERFACE) {
			if (cs_desc[2] == MIDI_IN_JACK)
				endpoint->in_cables = (endpoint->in_cables << 1) | 1;
			else if (cs_desc[2] == MIDI_OUT_JACK)
				endpoint->out_cables = (endpoint->out_cables << 1) | 1;
		}
	}
	if (!endpoint->in_cables && !endpoint->out_cables)
		return -ENOENT;

	return snd_usbmidi_detect_endpoints(umidi, endpoint, 1);
}

/*
 * Creates the endpoints and their ports for Midiman devices.
 */
static int snd_usbmidi_create_endpoints_midiman(snd_usb_midi_t* umidi,
						snd_usb_midi_endpoint_info_t* endpoint)
{
	snd_usb_midi_endpoint_info_t ep_info;
	struct usb_interface* intf;
	struct usb_host_interface *hostif;
	struct usb_interface_descriptor* intfd;
	struct usb_endpoint_descriptor* epd;
	int cable, err;

	intf = umidi->iface;
	if (!intf)
		return -ENOENT;
	hostif = intf->altsetting;
	intfd = get_iface_desc(hostif);
	/*
	 * The various MidiSport devices have more or less random endpoint
	 * numbers, so we have to identify the endpoints by their index in
	 * the descriptor array, like the driver for that other OS does.
	 *
	 * There is one interrupt input endpoint for all input ports, one
	 * bulk output endpoint for even-numbered ports, and one for odd-
	 * numbered ports.  Both bulk output endpoints have corresponding
	 * input bulk endpoints (at indices 1 and 3) which aren't used.
	 */
	if (intfd->bNumEndpoints < (endpoint->out_cables > 0x0001 ? 5 : 3)) {
		snd_printdd(KERN_ERR "not enough endpoints\n");
		return -ENOENT;
	}

	epd = get_endpoint(hostif, 0);
	if ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) != USB_DIR_IN ||
	    (epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_INT) {
		snd_printdd(KERN_ERR "endpoint[0] isn't interrupt\n");
		return -ENXIO;
	}
	epd = get_endpoint(hostif, 2);
	if ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) != USB_DIR_OUT ||
	    (epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_BULK) {
		snd_printdd(KERN_ERR "endpoint[2] isn't bulk output\n");
		return -ENXIO;
	}
	if (endpoint->out_cables > 0x0001) {
		epd = get_endpoint(hostif, 4);
		if ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) != USB_DIR_OUT ||
		    (epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_BULK) {
			snd_printdd(KERN_ERR "endpoint[4] isn't bulk output\n");
			return -ENXIO;
		}
	}

	ep_info.out_ep = get_endpoint(hostif, 2)->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	ep_info.out_cables = endpoint->out_cables & 0x5555;
	err = snd_usbmidi_out_endpoint_create(umidi, &ep_info, &umidi->endpoints[0]);
	if (err < 0)
		return err;

	ep_info.in_ep = get_endpoint(hostif, 0)->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	ep_info.in_interval = get_endpoint(hostif, 0)->bInterval;
	ep_info.in_cables = endpoint->in_cables;
	err = snd_usbmidi_in_endpoint_create(umidi, &ep_info, &umidi->endpoints[0]);
	if (err < 0)
		return err;

	if (endpoint->out_cables > 0x0001) {
		ep_info.out_ep = get_endpoint(hostif, 4)->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
		ep_info.out_cables = endpoint->out_cables & 0xaaaa;
		err = snd_usbmidi_out_endpoint_create(umidi, &ep_info, &umidi->endpoints[1]);
		if (err < 0)
			return err;
	}

	for (cable = 0; cable < 0x10; ++cable) {
		if (endpoint->out_cables & (1 << cable))
			snd_usbmidi_init_substream(umidi, SNDRV_RAWMIDI_STREAM_OUTPUT, cable,
						   &umidi->endpoints[cable & 1].out->ports[cable].substream);
		if (endpoint->in_cables & (1 << cable))
			snd_usbmidi_init_substream(umidi, SNDRV_RAWMIDI_STREAM_INPUT, cable,
						   &umidi->endpoints[0].in->ports[cable].substream);
	}
	return 0;
}

static int snd_usbmidi_create_rawmidi(snd_usb_midi_t* umidi,
				      int out_ports, int in_ports)
{
	snd_rawmidi_t* rmidi;
	int err;

	err = snd_rawmidi_new(umidi->chip->card, "USB MIDI",
			      umidi->chip->next_midi_device++,
			      out_ports, in_ports, &rmidi);
	if (err < 0)
		return err;
	strcpy(rmidi->name, umidi->chip->card->shortname);
	rmidi->info_flags = SNDRV_RAWMIDI_INFO_OUTPUT |
			    SNDRV_RAWMIDI_INFO_INPUT |
			    SNDRV_RAWMIDI_INFO_DUPLEX;
	rmidi->private_data = umidi;
	rmidi->private_free = snd_usbmidi_rawmidi_free;
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &snd_usbmidi_output_ops);
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_INPUT, &snd_usbmidi_input_ops);

	umidi->rmidi = rmidi;
	return 0;
}

/*
 * Temporarily stop input.
 */
void snd_usbmidi_input_stop(struct list_head* p)
{
	snd_usb_midi_t* umidi;
	int i;

	umidi = list_entry(p, snd_usb_midi_t, list);
	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i) {
		snd_usb_midi_endpoint_t* ep = &umidi->endpoints[i];
		if (ep->in)
			usb_kill_urb(ep->in->urb);
	}
}

static void snd_usbmidi_input_start_ep(snd_usb_midi_in_endpoint_t* ep)
{
	if (ep) {
		struct urb* urb = ep->urb;
		urb->dev = ep->umidi->chip->dev;
		snd_usbmidi_submit_urb(urb, GFP_KERNEL);
	}
}

/*
 * Resume input after a call to snd_usbmidi_input_stop().
 */
void snd_usbmidi_input_start(struct list_head* p)
{
	snd_usb_midi_t* umidi;
	int i;

	umidi = list_entry(p, snd_usb_midi_t, list);
	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i)
		snd_usbmidi_input_start_ep(umidi->endpoints[i].in);
}

/*
 * Creates and registers everything needed for a MIDI streaming interface.
 */
int snd_usb_create_midi_interface(snd_usb_audio_t* chip,
				  struct usb_interface* iface,
				  const snd_usb_audio_quirk_t* quirk)
{
	snd_usb_midi_t* umidi;
	snd_usb_midi_endpoint_info_t endpoints[MIDI_MAX_ENDPOINTS];
	int out_ports, in_ports;
	int i, err;

	umidi = kzalloc(sizeof(*umidi), GFP_KERNEL);
	if (!umidi)
		return -ENOMEM;
	umidi->chip = chip;
	umidi->iface = iface;
	umidi->quirk = quirk;
	umidi->usb_protocol_ops = &snd_usbmidi_standard_ops;
	init_timer(&umidi->error_timer);
	umidi->error_timer.function = snd_usbmidi_error_timer;
	umidi->error_timer.data = (unsigned long)umidi;

	/* detect the endpoint(s) to use */
	memset(endpoints, 0, sizeof(endpoints));
	if (!quirk) {
		err = snd_usbmidi_get_ms_info(umidi, endpoints);
	} else {
		switch (quirk->type) {
		case QUIRK_MIDI_FIXED_ENDPOINT:
			memcpy(&endpoints[0], quirk->data,
			       sizeof(snd_usb_midi_endpoint_info_t));
			err = snd_usbmidi_detect_endpoints(umidi, &endpoints[0], 1);
			break;
		case QUIRK_MIDI_YAMAHA:
			err = snd_usbmidi_detect_yamaha(umidi, &endpoints[0]);
			break;
		case QUIRK_MIDI_MIDIMAN:
			umidi->usb_protocol_ops = &snd_usbmidi_midiman_ops;
			memcpy(&endpoints[0], quirk->data,
			       sizeof(snd_usb_midi_endpoint_info_t));
			err = 0;
			break;
		case QUIRK_MIDI_NOVATION:
			umidi->usb_protocol_ops = &snd_usbmidi_novation_ops;
			err = snd_usbmidi_detect_per_port_endpoints(umidi, endpoints);
			break;
		case QUIRK_MIDI_RAW:
			umidi->usb_protocol_ops = &snd_usbmidi_raw_ops;
			err = snd_usbmidi_detect_per_port_endpoints(umidi, endpoints);
			break;
		case QUIRK_MIDI_EMAGIC:
			umidi->usb_protocol_ops = &snd_usbmidi_emagic_ops;
			memcpy(&endpoints[0], quirk->data,
			       sizeof(snd_usb_midi_endpoint_info_t));
			err = snd_usbmidi_detect_endpoints(umidi, &endpoints[0], 1);
			break;
		case QUIRK_MIDI_MIDITECH:
			err = snd_usbmidi_detect_per_port_endpoints(umidi, endpoints);
			break;
		default:
			snd_printd(KERN_ERR "invalid quirk type %d\n", quirk->type);
			err = -ENXIO;
			break;
		}
	}
	if (err < 0) {
		kfree(umidi);
		return err;
	}

	/* create rawmidi device */
	out_ports = 0;
	in_ports = 0;
	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i) {
		out_ports += snd_usbmidi_count_bits(endpoints[i].out_cables);
		in_ports += snd_usbmidi_count_bits(endpoints[i].in_cables);
	}
	err = snd_usbmidi_create_rawmidi(umidi, out_ports, in_ports);
	if (err < 0) {
		kfree(umidi);
		return err;
	}

	/* create endpoint/port structures */
	if (quirk && quirk->type == QUIRK_MIDI_MIDIMAN)
		err = snd_usbmidi_create_endpoints_midiman(umidi, &endpoints[0]);
	else
		err = snd_usbmidi_create_endpoints(umidi, endpoints);
	if (err < 0) {
		snd_usbmidi_free(umidi);
		return err;
	}

	list_add(&umidi->list, &umidi->chip->midi_list);

	for (i = 0; i < MIDI_MAX_ENDPOINTS; ++i)
		snd_usbmidi_input_start_ep(umidi->endpoints[i].in);
	return 0;
}

EXPORT_SYMBOL(snd_usb_create_midi_interface);
EXPORT_SYMBOL(snd_usbmidi_input_stop);
EXPORT_SYMBOL(snd_usbmidi_input_start);
EXPORT_SYMBOL(snd_usbmidi_disconnect);
