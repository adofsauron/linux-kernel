/*
 * Driver for the Cirrus PD6729 PCI-PCMCIA bridge.
 *
 * Based on the i82092.c driver.
 *
 * This software may be used and distributed according to the terms of
 * the GNU General Public License, incorporated herein by reference.
 */

#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/device.h>

#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>

#include <asm/system.h>
#include <asm/io.h>

#include "pd6729.h"
#include "i82365.h"
#include "cirrus.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for the Cirrus PD6729 PCI-PCMCIA bridge");
MODULE_AUTHOR("Jun Komuro <komurojun-mbn@nifty.com>");

#define MAX_SOCKETS 2

/*
 * simple helper functions
 * External clock time, in nanoseconds.  120 ns = 8.33 MHz
 */
#define to_cycles(ns)	((ns)/120)

#ifndef NO_IRQ
#define NO_IRQ	((unsigned int)(0))
#endif

/*
 * PARAMETERS
 *  irq_mode=n
 *     Specifies the interrupt delivery mode.  The default (1) is to use PCI
 *     interrupts; a value of 0 selects ISA interrupts. This must be set for
 *     correct operation of PCI card readers.
 *
 *  irq_list=i,j,...
 *     This list limits the set of interrupts that can be used by PCMCIA
 *     cards.
 *     The default list is 3,4,5,7,9,10,11.
 *     (irq_list parameter is not used, if irq_mode = 1)
 */

static int irq_mode = 1; /* 0 = ISA interrupt, 1 = PCI interrupt */
static int irq_list[16];
static int irq_list_count = 0;

module_param(irq_mode, int, 0444);
module_param_array(irq_list, int, &irq_list_count, 0444);
MODULE_PARM_DESC(irq_mode,
		"interrupt delivery mode. 0 = ISA, 1 = PCI. default is 1");
MODULE_PARM_DESC(irq_list, "interrupts that can be used by PCMCIA cards");

static DEFINE_SPINLOCK(port_lock);

/* basic value read/write functions */

static unsigned char indirect_read(struct pd6729_socket *socket,
				   unsigned short reg)
{
	unsigned long port;
	unsigned char val;
	unsigned long flags;

	spin_lock_irqsave(&port_lock, flags);
	reg += socket->number * 0x40;
	port = socket->io_base;
	outb(reg, port);
	val = inb(port + 1);
	spin_unlock_irqrestore(&port_lock, flags);

	return val;
}

static unsigned short indirect_read16(struct pd6729_socket *socket,
				      unsigned short reg)
{
	unsigned long port;
	unsigned short tmp;
	unsigned long flags;

	spin_lock_irqsave(&port_lock, flags);
	reg  = reg + socket->number * 0x40;
	port = socket->io_base;
	outb(reg, port);
	tmp = inb(port + 1);
	reg++;
	outb(reg, port);
	tmp = tmp | (inb(port + 1) << 8);
	spin_unlock_irqrestore(&port_lock, flags);

	return tmp;
}

static void indirect_write(struct pd6729_socket *socket, unsigned short reg,
			   unsigned char value)
{
	unsigned long port;
	unsigned long flags;

	spin_lock_irqsave(&port_lock, flags);
	reg = reg + socket->number * 0x40;
	port = socket->io_base;
	outb(reg, port);
	outb(value, port + 1);
	spin_unlock_irqrestore(&port_lock, flags);
}

static void indirect_setbit(struct pd6729_socket *socket, unsigned short reg,
			    unsigned char mask)
{
	unsigned long port;
	unsigned char val;
	unsigned long flags;

	spin_lock_irqsave(&port_lock, flags);
	reg = reg + socket->number * 0x40;
	port = socket->io_base;
	outb(reg, port);
	val = inb(port + 1);
	val |= mask;
	outb(reg, port);
	outb(val, port + 1);
	spin_unlock_irqrestore(&port_lock, flags);
}

static void indirect_resetbit(struct pd6729_socket *socket, unsigned short reg,
			      unsigned char mask)
{
	unsigned long port;
	unsigned char val;
	unsigned long flags;

	spin_lock_irqsave(&port_lock, flags);
	reg = reg + socket->number * 0x40;
	port = socket->io_base;
	outb(reg, port);
	val = inb(port + 1);
	val &= ~mask;
	outb(reg, port);
	outb(val, port + 1);
	spin_unlock_irqrestore(&port_lock, flags);
}

static void indirect_write16(struct pd6729_socket *socket, unsigned short reg,
			     unsigned short value)
{
	unsigned long port;
	unsigned char val;
	unsigned long flags;

	spin_lock_irqsave(&port_lock, flags);
	reg = reg + socket->number * 0x40;
	port = socket->io_base;

	outb(reg, port);
	val = value & 255;
	outb(val, port + 1);

	reg++;

	outb(reg, port);
	val = value >> 8;
	outb(val, port + 1);
	spin_unlock_irqrestore(&port_lock, flags);
}

/* Interrupt handler functionality */

static irqreturn_t pd6729_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	struct pd6729_socket *socket = (struct pd6729_socket *)dev;
	int i;
	int loopcount = 0;
	int handled = 0;
	unsigned int events, active = 0;

	while (1) {
		loopcount++;
		if (loopcount > 20) {
			printk(KERN_ERR "pd6729: infinite eventloop "
			       "in interrupt\n");
			break;
		}

		active = 0;

		for (i = 0; i < MAX_SOCKETS; i++) {
			unsigned int csc;

			/* card status change register */
			csc = indirect_read(&socket[i], I365_CSC);
			if (csc == 0)  /* no events on this socket */
				continue;

			handled = 1;
			events = 0;

			if (csc & I365_CSC_DETECT) {
				events |= SS_DETECT;
				dprintk("Card detected in socket %i!\n", i);
			}

			if (indirect_read(&socket[i], I365_INTCTL)
						& I365_PC_IOCARD) {
				/* For IO/CARDS, bit 0 means "read the card" */
				events |= (csc & I365_CSC_STSCHG)
						? SS_STSCHG : 0;
			} else {
				/* Check for battery/ready events */
				events |= (csc & I365_CSC_BVD1)
						? SS_BATDEAD : 0;
				events |= (csc & I365_CSC_BVD2)
						? SS_BATWARN : 0;
				events |= (csc & I365_CSC_READY)
						? SS_READY : 0;
			}

			if (events) {
				pcmcia_parse_events(&socket[i].socket, events);
			}
			active |= events;
		}

		if (active == 0) /* no more events to handle */
			break;
	}
	return IRQ_RETVAL(handled);
}

/* socket functions */

static void pd6729_interrupt_wrapper(unsigned long data)
{
	struct pd6729_socket *socket = (struct pd6729_socket *) data;

	pd6729_interrupt(0, (void *)socket, NULL);
	mod_timer(&socket->poll_timer, jiffies + HZ);
}

static int pd6729_get_status(struct pcmcia_socket *sock, u_int *value)
{
	struct pd6729_socket *socket
			= container_of(sock, struct pd6729_socket, socket);
	unsigned int status;
	unsigned int data;
	struct pd6729_socket *t;

	/* Interface Status Register */
	status = indirect_read(socket, I365_STATUS);
	*value = 0;

	if ((status & I365_CS_DETECT) == I365_CS_DETECT) {
		*value |= SS_DETECT;
	}

	/*
	 * IO cards have a different meaning of bits 0,1
	 * Also notice the inverse-logic on the bits
	 */
	if (indirect_read(socket, I365_INTCTL) & I365_PC_IOCARD) {
		/* IO card */
		if (!(status & I365_CS_STSCHG))
			*value |= SS_STSCHG;
	} else {
		/* non I/O card */
		if (!(status & I365_CS_BVD1))
			*value |= SS_BATDEAD;
		if (!(status & I365_CS_BVD2))
			*value |= SS_BATWARN;
	}

	if (status & I365_CS_WRPROT)
		*value |= SS_WRPROT;	/* card is write protected */

	if (status & I365_CS_READY)
		*value |= SS_READY;	/* card is not busy */

	if (status & I365_CS_POWERON)
		*value |= SS_POWERON;	/* power is applied to the card */

	t = (socket->number) ? socket : socket + 1;
	indirect_write(t, PD67_EXT_INDEX, PD67_EXTERN_DATA);
	data = indirect_read16(t, PD67_EXT_DATA);
	*value |= (data & PD67_EXD_VS1(socket->number)) ? 0 : SS_3VCARD;

	return 0;
}


static int pd6729_get_socket(struct pcmcia_socket *sock, socket_state_t *state)
{
	struct pd6729_socket *socket
			= container_of(sock, struct pd6729_socket, socket);
	unsigned char reg, vcc, vpp;

	state->flags    = 0;
	state->Vcc      = 0;
	state->Vpp      = 0;
	state->io_irq   = 0;
	state->csc_mask = 0;

	/* First the power status of the socket */
	reg = indirect_read(socket, I365_POWER);

	if (reg & I365_PWR_AUTO)
		state->flags |= SS_PWR_AUTO;  /* Automatic Power Switch */

	if (reg & I365_PWR_OUT)
		state->flags |= SS_OUTPUT_ENA; /* Output signals are enabled */

	vcc = reg & I365_VCC_MASK;    vpp = reg & I365_VPP1_MASK;

	if (reg & I365_VCC_5V) {
		state->Vcc = (indirect_read(socket, PD67_MISC_CTL_1) &
			PD67_MC1_VCC_3V) ? 33 : 50;

		if (vpp == I365_VPP1_5V) {
			if (state->Vcc == 50)
				state->Vpp = 50;
			else
				state->Vpp = 33;
		}
		if (vpp == I365_VPP1_12V)
			state->Vpp = 120;
	}

	/* Now the IO card, RESET flags and IO interrupt */
	reg = indirect_read(socket, I365_INTCTL);

	if ((reg & I365_PC_RESET) == 0)
		state->flags |= SS_RESET;
	if (reg & I365_PC_IOCARD)
		state->flags |= SS_IOCARD; /* This is an IO card */

	/* Set the IRQ number */
	state->io_irq = socket->card_irq;

	/* Card status change */
	reg = indirect_read(socket, I365_CSCINT);

	if (reg & I365_CSC_DETECT)
		state->csc_mask |= SS_DETECT; /* Card detect is enabled */

	if (state->flags & SS_IOCARD) {/* IO Cards behave different */
		if (reg & I365_CSC_STSCHG)
			state->csc_mask |= SS_STSCHG;
	} else {
		if (reg & I365_CSC_BVD1)
			state->csc_mask |= SS_BATDEAD;
		if (reg & I365_CSC_BVD2)
			state->csc_mask |= SS_BATWARN;
		if (reg & I365_CSC_READY)
			state->csc_mask |= SS_READY;
	}

	return 0;
}

static int pd6729_set_socket(struct pcmcia_socket *sock, socket_state_t *state)
{
	struct pd6729_socket *socket
			= container_of(sock, struct pd6729_socket, socket);
	unsigned char reg, data;

	/* First, set the global controller options */
	indirect_write(socket, I365_GBLCTL, 0x00);
	indirect_write(socket, I365_GENCTL, 0x00);

	/* Values for the IGENC register */
	socket->card_irq = state->io_irq;

	reg = 0;
 	/* The reset bit has "inverse" logic */
	if (!(state->flags & SS_RESET))
		reg |= I365_PC_RESET;
	if (state->flags & SS_IOCARD)
		reg |= I365_PC_IOCARD;

	/* IGENC, Interrupt and General Control Register */
	indirect_write(socket, I365_INTCTL, reg);

	/* Power registers */

	reg = I365_PWR_NORESET; /* default: disable resetdrv on resume */

	if (state->flags & SS_PWR_AUTO) {
		dprintk("Auto power\n");
		reg |= I365_PWR_AUTO;	/* automatic power mngmnt */
	}
	if (state->flags & SS_OUTPUT_ENA) {
		dprintk("Power Enabled\n");
		reg |= I365_PWR_OUT;	/* enable power */
	}

	switch (state->Vcc) {
	case 0:
		break;
	case 33:
		dprintk("setting voltage to Vcc to 3.3V on socket %i\n",
			socket->number);
		reg |= I365_VCC_5V;
		indirect_setbit(socket, PD67_MISC_CTL_1, PD67_MC1_VCC_3V);
		break;
	case 50:
		dprintk("setting voltage to Vcc to 5V on socket %i\n",
			socket->number);
		reg |= I365_VCC_5V;
		indirect_resetbit(socket, PD67_MISC_CTL_1, PD67_MC1_VCC_3V);
		break;
	default:
		dprintk("pd6729: pd6729_set_socket called with "
				"invalid VCC power value: %i\n",
			state->Vcc);
		return -EINVAL;
	}

	switch (state->Vpp) {
	case 0:
		dprintk("not setting Vpp on socket %i\n", socket->number);
		break;
	case 33:
	case 50:
		dprintk("setting Vpp to Vcc for socket %i\n", socket->number);
		reg |= I365_VPP1_5V;
		break;
	case 120:
		dprintk("setting Vpp to 12.0\n");
		reg |= I365_VPP1_12V;
		break;
	default:
		dprintk("pd6729: pd6729_set_socket called with invalid VPP power value: %i\n",
			state->Vpp);
		return -EINVAL;
	}

	/* only write if changed */
	if (reg != indirect_read(socket, I365_POWER))
		indirect_write(socket, I365_POWER, reg);

	if (irq_mode == 1) {
		 /* all interrupts are to be done as PCI interrupts */
		data = PD67_EC1_INV_MGMT_IRQ | PD67_EC1_INV_CARD_IRQ;
	} else
		data = 0;

	indirect_write(socket, PD67_EXT_INDEX, PD67_EXT_CTL_1);
	indirect_write(socket, PD67_EXT_DATA, data);

	/* Enable specific interrupt events */

	reg = 0x00;
	if (state->csc_mask & SS_DETECT) {
		reg |= I365_CSC_DETECT;
	}
	if (state->flags & SS_IOCARD) {
		if (state->csc_mask & SS_STSCHG)
			reg |= I365_CSC_STSCHG;
	} else {
		if (state->csc_mask & SS_BATDEAD)
			reg |= I365_CSC_BVD1;
		if (state->csc_mask & SS_BATWARN)
			reg |= I365_CSC_BVD2;
		if (state->csc_mask & SS_READY)
			reg |= I365_CSC_READY;
	}
	if (irq_mode == 1)
		reg |= 0x30;	/* management IRQ: PCI INTA# = "irq 3" */
	indirect_write(socket, I365_CSCINT, reg);

	reg = indirect_read(socket, I365_INTCTL);
	if (irq_mode == 1)
		reg |= 0x03;	/* card IRQ: PCI INTA# = "irq 3" */
	else
		reg |= socket->card_irq;
	indirect_write(socket, I365_INTCTL, reg);

	/* now clear the (probably bogus) pending stuff by doing a dummy read */
	(void)indirect_read(socket, I365_CSC);

	return 0;
}

static int pd6729_set_io_map(struct pcmcia_socket *sock,
			     struct pccard_io_map *io)
{
	struct pd6729_socket *socket
			= container_of(sock, struct pd6729_socket, socket);
	unsigned char map, ioctl;

	map = io->map;

	/* Check error conditions */
	if (map > 1) {
		dprintk("pd6729_set_io_map with invalid map");
		return -EINVAL;
	}

	/* Turn off the window before changing anything */
	if (indirect_read(socket, I365_ADDRWIN) & I365_ENA_IO(map))
		indirect_resetbit(socket, I365_ADDRWIN, I365_ENA_IO(map));

	/* dprintk("set_io_map: Setting range to %x - %x\n",
	   io->start, io->stop);*/

	/* write the new values */
	indirect_write16(socket, I365_IO(map)+I365_W_START, io->start);
	indirect_write16(socket, I365_IO(map)+I365_W_STOP, io->stop);

	ioctl = indirect_read(socket, I365_IOCTL) & ~I365_IOCTL_MASK(map);

	if (io->flags & MAP_0WS) ioctl |= I365_IOCTL_0WS(map);
	if (io->flags & MAP_16BIT) ioctl |= I365_IOCTL_16BIT(map);
	if (io->flags & MAP_AUTOSZ) ioctl |= I365_IOCTL_IOCS16(map);

	indirect_write(socket, I365_IOCTL, ioctl);

	/* Turn the window back on if needed */
	if (io->flags & MAP_ACTIVE)
		indirect_setbit(socket, I365_ADDRWIN, I365_ENA_IO(map));

	return 0;
}

static int pd6729_set_mem_map(struct pcmcia_socket *sock,
			      struct pccard_mem_map *mem)
{
	struct pd6729_socket *socket
			 = container_of(sock, struct pd6729_socket, socket);
	unsigned short base, i;
	unsigned char map;

	map = mem->map;
	if (map > 4) {
		printk("pd6729_set_mem_map: invalid map");
		return -EINVAL;
	}

	if ((mem->res->start > mem->res->end) || (mem->speed > 1000)) {
		printk("pd6729_set_mem_map: invalid address / speed");
		return -EINVAL;
	}

	/* Turn off the window before changing anything */
	if (indirect_read(socket, I365_ADDRWIN) & I365_ENA_MEM(map))
		indirect_resetbit(socket, I365_ADDRWIN, I365_ENA_MEM(map));

	/* write the start address */
	base = I365_MEM(map);
	i = (mem->res->start >> 12) & 0x0fff;
	if (mem->flags & MAP_16BIT)
		i |= I365_MEM_16BIT;
	if (mem->flags & MAP_0WS)
		i |= I365_MEM_0WS;
	indirect_write16(socket, base + I365_W_START, i);

	/* write the stop address */

	i= (mem->res->end >> 12) & 0x0fff;
	switch (to_cycles(mem->speed)) {
	case 0:
		break;
	case 1:
		i |= I365_MEM_WS0;
		break;
	case 2:
		i |= I365_MEM_WS1;
		break;
	default:
		i |= I365_MEM_WS1 | I365_MEM_WS0;
		break;
	}

	indirect_write16(socket, base + I365_W_STOP, i);

	/* Take care of high byte */
	indirect_write(socket, PD67_EXT_INDEX, PD67_MEM_PAGE(map));
	indirect_write(socket, PD67_EXT_DATA, mem->res->start >> 24);

	/* card start */

	i = ((mem->card_start - mem->res->start) >> 12) & 0x3fff;
	if (mem->flags & MAP_WRPROT)
		i |= I365_MEM_WRPROT;
	if (mem->flags & MAP_ATTRIB) {
		/* dprintk("requesting attribute memory for socket %i\n",
			socket->number);*/
		i |= I365_MEM_REG;
	} else {
		/* dprintk("requesting normal memory for socket %i\n",
			socket->number);*/
	}
	indirect_write16(socket, base + I365_W_OFF, i);

	/* Enable the window if necessary */
	if (mem->flags & MAP_ACTIVE)
		indirect_setbit(socket, I365_ADDRWIN, I365_ENA_MEM(map));

	return 0;
}

static int pd6729_init(struct pcmcia_socket *sock)
{
	int i;
	struct resource res = { .end = 0x0fff };
	pccard_io_map io = { 0, 0, 0, 0, 1 };
	pccard_mem_map mem = { .res = &res, };

	pd6729_set_socket(sock, &dead_socket);
	for (i = 0; i < 2; i++) {
		io.map = i;
		pd6729_set_io_map(sock, &io);
	}
	for (i = 0; i < 5; i++) {
		mem.map = i;
		pd6729_set_mem_map(sock, &mem);
	}

	return 0;
}


/* the pccard structure and its functions */
static struct pccard_operations pd6729_operations = {
	.init 			= pd6729_init,
	.get_status		= pd6729_get_status,
	.get_socket		= pd6729_get_socket,
	.set_socket		= pd6729_set_socket,
	.set_io_map		= pd6729_set_io_map,
	.set_mem_map		= pd6729_set_mem_map,
};

static irqreturn_t pd6729_test(int irq, void *dev, struct pt_regs *regs)
{
	dprintk("-> hit on irq %d\n", irq);
	return IRQ_HANDLED;
}

static int pd6729_check_irq(int irq, int flags)
{
	if (request_irq(irq, pd6729_test, flags, "x", pd6729_test) != 0)
		return -1;
	free_irq(irq, pd6729_test);
	return 0;
}

static u_int __init pd6729_isa_scan(void)
{
	u_int mask0, mask = 0;
	int i;

	if (irq_mode == 1) {
		printk(KERN_INFO "pd6729: PCI card interrupts, "
						"PCI status changes\n");
		return 0;
	}

	if (irq_list_count == 0)
		mask0 = 0xffff;
	else
		for (i = mask0 = 0; i < irq_list_count; i++)
			mask0 |= (1<<irq_list[i]);

	mask0 &= PD67_MASK;

	/* just find interrupts that aren't in use */
	for (i = 0; i < 16; i++)
		if ((mask0 & (1 << i)) && (pd6729_check_irq(i, 0) == 0))
			mask |= (1 << i);

	printk(KERN_INFO "pd6729: ISA irqs = ");
	for (i = 0; i < 16; i++)
		if (mask & (1<<i))
			printk("%s%d", ((mask & ((1<<i)-1)) ? "," : ""), i);

	if (mask == 0) printk("none!");

	printk("  polling status changes.\n");

	return mask;
}

static int __devinit pd6729_pci_probe(struct pci_dev *dev,
				      const struct pci_device_id *id)
{
	int i, j, ret;
	u_int mask;
	char configbyte;
	struct pd6729_socket *socket;

	socket = kmalloc(sizeof(struct pd6729_socket) * MAX_SOCKETS,
			 GFP_KERNEL);
	if (!socket)
		return -ENOMEM;

	memset(socket, 0, sizeof(struct pd6729_socket) * MAX_SOCKETS);

	if ((ret = pci_enable_device(dev)))
		goto err_out_free_mem;

	printk(KERN_INFO "pd6729: Cirrus PD6729 PCI to PCMCIA Bridge "
		"at 0x%lx on irq %d\n",	pci_resource_start(dev, 0), dev->irq);
 	/*
	 * Since we have no memory BARs some firmware may not
	 * have had PCI_COMMAND_MEMORY enabled, yet the device needs it.
	 */
	pci_read_config_byte(dev, PCI_COMMAND, &configbyte);
	if (!(configbyte & PCI_COMMAND_MEMORY)) {
		printk(KERN_DEBUG "pd6729: Enabling PCI_COMMAND_MEMORY.\n");
		configbyte |= PCI_COMMAND_MEMORY;
		pci_write_config_byte(dev, PCI_COMMAND, configbyte);
	}

	ret = pci_request_regions(dev, "pd6729");
	if (ret) {
		printk(KERN_INFO "pd6729: pci request region failed.\n");
		goto err_out_disable;
	}

	if (dev->irq == NO_IRQ)
		irq_mode = 0;	/* fall back to ISA interrupt mode */

	mask = pd6729_isa_scan();
	if (irq_mode == 0 && mask == 0) {
		printk(KERN_INFO "pd6729: no ISA interrupt is available.\n");
		goto err_out_free_res;
	}

	for (i = 0; i < MAX_SOCKETS; i++) {
		socket[i].io_base = pci_resource_start(dev, 0);
		socket[i].socket.features |= SS_CAP_PAGE_REGS | SS_CAP_PCCARD;
		socket[i].socket.map_size = 0x1000;
		socket[i].socket.irq_mask = mask;
		socket[i].socket.pci_irq  = dev->irq;
		socket[i].socket.owner = THIS_MODULE;

		socket[i].number = i;

		socket[i].socket.ops = &pd6729_operations;
		socket[i].socket.resource_ops = &pccard_nonstatic_ops;
		socket[i].socket.dev.dev = &dev->dev;
		socket[i].socket.driver_data = &socket[i];
	}

	pci_set_drvdata(dev, socket);
	if (irq_mode == 1) {
		/* Register the interrupt handler */
		if ((ret = request_irq(dev->irq, pd6729_interrupt, SA_SHIRQ,
							"pd6729", socket))) {
			printk(KERN_ERR "pd6729: Failed to register irq %d, "
							"aborting\n", dev->irq);
			goto err_out_free_res;
		}
	} else {
		/* poll Card status change */
		init_timer(&socket->poll_timer);
		socket->poll_timer.function = pd6729_interrupt_wrapper;
		socket->poll_timer.data = (unsigned long)socket;
		socket->poll_timer.expires = jiffies + HZ;
		add_timer(&socket->poll_timer);
	}

	for (i = 0; i < MAX_SOCKETS; i++) {
		ret = pcmcia_register_socket(&socket[i].socket);
		if (ret) {
			printk(KERN_INFO "pd6729: pcmcia_register_socket "
					       "failed.\n");
			for (j = 0; j < i ; j++)
				pcmcia_unregister_socket(&socket[j].socket);
			goto err_out_free_res2;
		}
	}

	return 0;

 err_out_free_res2:
	if (irq_mode == 1)
		free_irq(dev->irq, socket);
	else
		del_timer_sync(&socket->poll_timer);
 err_out_free_res:
	pci_release_regions(dev);
 err_out_disable:
	pci_disable_device(dev);

 err_out_free_mem:
	kfree(socket);
	return ret;
}

static void __devexit pd6729_pci_remove(struct pci_dev *dev)
{
	int i;
	struct pd6729_socket *socket = pci_get_drvdata(dev);

	for (i = 0; i < MAX_SOCKETS; i++) {
		/* Turn off all interrupt sources */
		indirect_write(&socket[i], I365_CSCINT, 0);
		indirect_write(&socket[i], I365_INTCTL, 0);

		pcmcia_unregister_socket(&socket[i].socket);
	}

	if (irq_mode == 1)
		free_irq(dev->irq, socket);
	else
		del_timer_sync(&socket->poll_timer);
	pci_release_regions(dev);
	pci_disable_device(dev);

	kfree(socket);
}

static int pd6729_socket_suspend(struct pci_dev *dev, pm_message_t state)
{
	return pcmcia_socket_dev_suspend(&dev->dev, state);
}

static int pd6729_socket_resume(struct pci_dev *dev)
{
	return pcmcia_socket_dev_resume(&dev->dev);
}

static struct pci_device_id pd6729_pci_ids[] = {
	{
		.vendor		= PCI_VENDOR_ID_CIRRUS,
		.device		= PCI_DEVICE_ID_CIRRUS_6729,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
	},
	{ }
};
MODULE_DEVICE_TABLE(pci, pd6729_pci_ids);

static struct pci_driver pd6729_pci_drv = {
	.name		= "pd6729",
	.id_table	= pd6729_pci_ids,
	.probe		= pd6729_pci_probe,
	.remove		= __devexit_p(pd6729_pci_remove),
	.suspend	= pd6729_socket_suspend,
	.resume		= pd6729_socket_resume,
};

static int pd6729_module_init(void)
{
	return pci_register_driver(&pd6729_pci_drv);
}

static void pd6729_module_exit(void)
{
	pci_unregister_driver(&pd6729_pci_drv);
}

module_init(pd6729_module_init);
module_exit(pd6729_module_exit);
