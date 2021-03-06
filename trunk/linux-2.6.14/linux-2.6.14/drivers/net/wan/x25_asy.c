/*
 *	Things to sort out:
 *
 *	o	tbusy handling
 *	o	allow users to set the parameters
 *	o	sync/async switching ?
 *
 *	Note: This does _not_ implement CCITT X.25 asynchronous framing
 *	recommendations. Its primarily for testing purposes. If you wanted
 *	to do CCITT then in theory all you need is to nick the HDLC async
 *	checksum routines from ppp.c
 *      Changes:
 *
 *	2000-10-29	Henner Eisen	lapb_data_indication() return status.
 */

#include <linux/module.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/x25.h>
#include <linux/lapb.h>
#include <linux/init.h>
#include "x25_asy.h"

#include <net/x25device.h>

static struct net_device **x25_asy_devs;
static int x25_asy_maxdev = SL_NRUNIT;

module_param(x25_asy_maxdev, int, 0);
MODULE_LICENSE("GPL");

static int x25_asy_esc(unsigned char *p, unsigned char *d, int len);
static void x25_asy_unesc(struct x25_asy *sl, unsigned char c);
static void x25_asy_setup(struct net_device *dev);

/* Find a free X.25 channel, and link in this `tty' line. */
static struct x25_asy *x25_asy_alloc(void)
{
	struct net_device *dev = NULL;
	struct x25_asy *sl;
	int i;

	if (x25_asy_devs == NULL)
		return NULL;	/* Master array missing ! */

	for (i = 0; i < x25_asy_maxdev; i++) {
		dev = x25_asy_devs[i];

		/* Not allocated ? */
		if (dev == NULL)
			break;

		sl = dev->priv;
		/* Not in use ? */
		if (!test_and_set_bit(SLF_INUSE, &sl->flags))
			return sl;
	}


	/* Sorry, too many, all slots in use */
	if (i >= x25_asy_maxdev)
		return NULL;

	/* If no channels are available, allocate one */
	if (!dev) {
		char name[IFNAMSIZ];
		sprintf(name, "x25asy%d", i);

		dev = alloc_netdev(sizeof(struct x25_asy),
				   name, x25_asy_setup);
		if (!dev)
			return NULL;

		/* Initialize channel control data */
		sl = dev->priv;
		dev->base_addr    = i;

		/* register device so that it can be ifconfig'ed       */
		if (register_netdev(dev) == 0) {
			/* (Re-)Set the INUSE bit.   Very Important! */
			set_bit(SLF_INUSE, &sl->flags);
			x25_asy_devs[i] = dev;
			return sl;
		} else {
			printk("x25_asy_alloc() - register_netdev() failure.\n");
			free_netdev(dev);
		}
	}
	return NULL;
}


/* Free an X.25 channel. */
static void x25_asy_free(struct x25_asy *sl)
{
	/* Free all X.25 frame buffers. */
	kfree(sl->rbuff);
	sl->rbuff = NULL;
	kfree(sl->xbuff);
	sl->xbuff = NULL;

	if (!test_and_clear_bit(SLF_INUSE, &sl->flags)) {
		printk("%s: x25_asy_free for already free unit.\n", sl->dev->name);
	}
}

static int x25_asy_change_mtu(struct net_device *dev, int newmtu)
{
	struct x25_asy *sl = dev->priv;
	unsigned char *xbuff, *rbuff;
	int len = 2* newmtu;

	xbuff = (unsigned char *) kmalloc (len + 4, GFP_ATOMIC);
	rbuff = (unsigned char *) kmalloc (len + 4, GFP_ATOMIC);

	if (xbuff == NULL || rbuff == NULL)  
	{
		printk("%s: unable to grow X.25 buffers, MTU change cancelled.\n",
		       dev->name);
		kfree(xbuff);
		kfree(rbuff);
		return -ENOMEM;
	}

	spin_lock_bh(&sl->lock);
	xbuff    = xchg(&sl->xbuff, xbuff);
	if (sl->xleft)  {
		if (sl->xleft <= len)  {
			memcpy(sl->xbuff, sl->xhead, sl->xleft);
		} else  {
			sl->xleft = 0;
			sl->stats.tx_dropped++;
		}
	}
	sl->xhead = sl->xbuff;

	rbuff	 = xchg(&sl->rbuff, rbuff);
	if (sl->rcount)  {
		if (sl->rcount <= len) {
			memcpy(sl->rbuff, rbuff, sl->rcount);
		} else  {
			sl->rcount = 0;
			sl->stats.rx_over_errors++;
			set_bit(SLF_ERROR, &sl->flags);
		}
	}

	dev->mtu    = newmtu;
	sl->buffsize = len;

	spin_unlock_bh(&sl->lock);

	kfree(xbuff);
	kfree(rbuff);
	return 0;
}


/* Set the "sending" flag.  This must be atomic, hence the ASM. */

static inline void x25_asy_lock(struct x25_asy *sl)
{
	netif_stop_queue(sl->dev);
}


/* Clear the "sending" flag.  This must be atomic, hence the ASM. */

static inline void x25_asy_unlock(struct x25_asy *sl)
{
	netif_wake_queue(sl->dev);
}

/* Send one completely decapsulated IP datagram to the IP layer. */

static void x25_asy_bump(struct x25_asy *sl)
{
	struct sk_buff *skb;
	int count;
	int err;

	count = sl->rcount;
	sl->stats.rx_bytes+=count;
	
	skb = dev_alloc_skb(count+1);
	if (skb == NULL)  
	{
		printk("%s: memory squeeze, dropping packet.\n", sl->dev->name);
		sl->stats.rx_dropped++;
		return;
	}
	skb_push(skb,1);	/* LAPB internal control */
	memcpy(skb_put(skb,count), sl->rbuff, count);
	skb->protocol = x25_type_trans(skb, sl->dev);
	if((err=lapb_data_received(skb->dev, skb))!=LAPB_OK)
	{
		kfree_skb(skb);
		printk(KERN_DEBUG "x25_asy: data received err - %d\n",err);
	}
	else
	{
		netif_rx(skb);
		sl->dev->last_rx = jiffies;
		sl->stats.rx_packets++;
	}
}

/* Encapsulate one IP datagram and stuff into a TTY queue. */
static void x25_asy_encaps(struct x25_asy *sl, unsigned char *icp, int len)
{
	unsigned char *p;
	int actual, count, mtu = sl->dev->mtu;

	if (len > mtu) 
	{		/* Sigh, shouldn't occur BUT ... */
		len = mtu;
		printk ("%s: truncating oversized transmit packet!\n", sl->dev->name);
		sl->stats.tx_dropped++;
		x25_asy_unlock(sl);
		return;
	}

	p = icp;
	count = x25_asy_esc(p, (unsigned char *) sl->xbuff, len);

	/* Order of next two lines is *very* important.
	 * When we are sending a little amount of data,
	 * the transfer may be completed inside driver.write()
	 * routine, because it's running with interrupts enabled.
	 * In this case we *never* got WRITE_WAKEUP event,
	 * if we did not request it before write operation.
	 *       14 Oct 1994  Dmitry Gorodchanin.
	 */
	sl->tty->flags |= (1 << TTY_DO_WRITE_WAKEUP);
	actual = sl->tty->driver->write(sl->tty, sl->xbuff, count);
	sl->xleft = count - actual;
	sl->xhead = sl->xbuff + actual;
	/* VSV */
	clear_bit(SLF_OUTWAIT, &sl->flags);	/* reset outfill flag */
}

/*
 * Called by the driver when there's room for more data.  If we have
 * more packets to send, we send them here.
 */
static void x25_asy_write_wakeup(struct tty_struct *tty)
{
	int actual;
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != X25_ASY_MAGIC || !netif_running(sl->dev))
		return;

	if (sl->xleft <= 0)  
	{
		/* Now serial buffer is almost free & we can start
		 * transmission of another packet */
		sl->stats.tx_packets++;
		tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
		x25_asy_unlock(sl);
		return;
	}

	actual = tty->driver->write(tty, sl->xhead, sl->xleft);
	sl->xleft -= actual;
	sl->xhead += actual;
}

static void x25_asy_timeout(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);

	spin_lock(&sl->lock);
	if (netif_queue_stopped(dev)) {
		/* May be we must check transmitter timeout here ?
		 *      14 Oct 1994 Dmitry Gorodchanin.
		 */
		printk(KERN_WARNING "%s: transmit timed out, %s?\n", dev->name,
		       (sl->tty->driver->chars_in_buffer(sl->tty) || sl->xleft) ?
		       "bad line quality" : "driver error");
		sl->xleft = 0;
		sl->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
		x25_asy_unlock(sl);
	}
	spin_unlock(&sl->lock);
}

/* Encapsulate an IP datagram and kick it into a TTY queue. */

static int x25_asy_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	int err;

	if (!netif_running(sl->dev)) {
		printk("%s: xmit call when iface is down\n", dev->name);
		kfree_skb(skb);
		return 0;
	}
	
	switch(skb->data[0])
	{
		case 0x00:break;
		case 0x01: /* Connection request .. do nothing */
			if((err=lapb_connect_request(dev))!=LAPB_OK)
				printk(KERN_ERR "x25_asy: lapb_connect_request error - %d\n", err);
			kfree_skb(skb);
			return 0;
		case 0x02: /* Disconnect request .. do nothing - hang up ?? */
			if((err=lapb_disconnect_request(dev))!=LAPB_OK)
				printk(KERN_ERR "x25_asy: lapb_disconnect_request error - %d\n", err);
		default:
			kfree_skb(skb);
			return  0;
	}
	skb_pull(skb,1);	/* Remove control byte */
	/*
	 * If we are busy already- too bad.  We ought to be able
	 * to queue things at this point, to allow for a little
	 * frame buffer.  Oh well...
	 * -----------------------------------------------------
	 * I hate queues in X.25 driver. May be it's efficient,
	 * but for me latency is more important. ;)
	 * So, no queues !
	 *        14 Oct 1994  Dmitry Gorodchanin.
	 */
	
	if((err=lapb_data_request(dev,skb))!=LAPB_OK)
	{
		printk(KERN_ERR "lapbeth: lapb_data_request error - %d\n", err);
		kfree_skb(skb);
		return 0;
	}
	return 0;
}


/*
 *	LAPB interface boilerplate
 */

/*
 *	Called when I frame data arrives. We did the work above - throw it
 *	at the net layer.
 */
  
static int x25_asy_data_indication(struct net_device *dev, struct sk_buff *skb)
{
	skb->dev->last_rx = jiffies;
	return netif_rx(skb);
}

/*
 *	Data has emerged from the LAPB protocol machine. We don't handle
 *	busy cases too well. Its tricky to see how to do this nicely -
 *	perhaps lapb should allow us to bounce this ?
 */
 
static void x25_asy_data_transmit(struct net_device *dev, struct sk_buff *skb)
{
	struct x25_asy *sl=dev->priv;
	
	spin_lock(&sl->lock);
	if (netif_queue_stopped(sl->dev) || sl->tty == NULL)
	{
		spin_unlock(&sl->lock);
		printk(KERN_ERR "x25_asy: tbusy drop\n");
		kfree_skb(skb);
		return;
	}
	/* We were not busy, so we are now... :-) */
	if (skb != NULL) 
	{
		x25_asy_lock(sl);
		sl->stats.tx_bytes+=skb->len;
		x25_asy_encaps(sl, skb->data, skb->len);
		dev_kfree_skb(skb);
	}
	spin_unlock(&sl->lock);
}

/*
 *	LAPB connection establish/down information.
 */
 
static void x25_asy_connected(struct net_device *dev, int reason)
{
	struct x25_asy *sl = dev->priv;
	struct sk_buff *skb;
	unsigned char *ptr;

	if ((skb = dev_alloc_skb(1)) == NULL) {
		printk(KERN_ERR "lapbeth: out of memory\n");
		return;
	}

	ptr  = skb_put(skb, 1);
	*ptr = 0x01;

	skb->protocol = x25_type_trans(skb, sl->dev);
	netif_rx(skb);
	sl->dev->last_rx = jiffies;
}

static void x25_asy_disconnected(struct net_device *dev, int reason)
{
	struct x25_asy *sl = dev->priv;
	struct sk_buff *skb;
	unsigned char *ptr;

	if ((skb = dev_alloc_skb(1)) == NULL) {
		printk(KERN_ERR "x25_asy: out of memory\n");
		return;
	}

	ptr  = skb_put(skb, 1);
	*ptr = 0x02;

	skb->protocol = x25_type_trans(skb, sl->dev);
	netif_rx(skb);
	sl->dev->last_rx = jiffies;
}

static struct lapb_register_struct x25_asy_callbacks = {
	.connect_confirmation = x25_asy_connected,
	.connect_indication = x25_asy_connected,
	.disconnect_confirmation = x25_asy_disconnected,
	.disconnect_indication = x25_asy_disconnected,
	.data_indication = x25_asy_data_indication,
	.data_transmit = x25_asy_data_transmit,

};


/* Open the low-level part of the X.25 channel. Easy! */
static int x25_asy_open(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	unsigned long len;
	int err;

	if (sl->tty == NULL)
		return -ENODEV;

	/*
	 * Allocate the X.25 frame buffers:
	 *
	 * rbuff	Receive buffer.
	 * xbuff	Transmit buffer.
	 */

	len = dev->mtu * 2;

	sl->rbuff = (unsigned char *) kmalloc(len + 4, GFP_KERNEL);
	if (sl->rbuff == NULL)   {
		goto norbuff;
	}
	sl->xbuff = (unsigned char *) kmalloc(len + 4, GFP_KERNEL);
	if (sl->xbuff == NULL)   {
		goto noxbuff;
	}

	sl->buffsize = len;
	sl->rcount   = 0;
	sl->xleft    = 0;
	sl->flags   &= (1 << SLF_INUSE);      /* Clear ESCAPE & ERROR flags */

	netif_start_queue(dev);
			
	/*
	 *	Now attach LAPB
	 */
	if((err=lapb_register(dev, &x25_asy_callbacks))==LAPB_OK)
		return 0;

	/* Cleanup */
	kfree(sl->xbuff);
noxbuff:
	kfree(sl->rbuff);
norbuff:
	return -ENOMEM;
}


/* Close the low-level part of the X.25 channel. Easy! */
static int x25_asy_close(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	int err;

	spin_lock(&sl->lock);
	if (sl->tty) 
		sl->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);

	netif_stop_queue(dev);
	sl->rcount = 0;
	sl->xleft  = 0;
	if((err=lapb_unregister(dev))!=LAPB_OK)
		printk(KERN_ERR "x25_asy_close: lapb_unregister error -%d\n",err);
	spin_unlock(&sl->lock);
	return 0;
}

static int x25_asy_receive_room(struct tty_struct *tty)
{
	return 65536;  /* We can handle an infinite amount of data. :-) */
}

/*
 * Handle the 'receiver data ready' interrupt.
 * This function is called by the 'tty_io' module in the kernel when
 * a block of X.25 data has been received, which can now be decapsulated
 * and sent on to some IP layer for further processing.
 */
 
static void x25_asy_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;

	if (!sl || sl->magic != X25_ASY_MAGIC || !netif_running(sl->dev))
		return;


	/* Read the characters out of the buffer */
	while (count--) {
		if (fp && *fp++) {
			if (!test_and_set_bit(SLF_ERROR, &sl->flags))  {
				sl->stats.rx_errors++;
			}
			cp++;
			continue;
		}
		x25_asy_unesc(sl, *cp++);
	}
}

/*
 * Open the high-level part of the X.25 channel.
 * This function is called by the TTY module when the
 * X.25 line discipline is called for.  Because we are
 * sure the tty line exists, we only have to link it to
 * a free X.25 channel...
 */

static int x25_asy_open_tty(struct tty_struct *tty)
{
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;
	int err;

	/* First make sure we're not already connected. */
	if (sl && sl->magic == X25_ASY_MAGIC) {
		return -EEXIST;
	}

	/* OK.  Find a free X.25 channel to use. */
	if ((sl = x25_asy_alloc()) == NULL) {
		return -ENFILE;
	}

	sl->tty = tty;
	tty->disc_data = sl;
	if (tty->driver->flush_buffer)  {
		tty->driver->flush_buffer(tty);
	}
	if (tty->ldisc.flush_buffer)  {
		tty->ldisc.flush_buffer(tty);
	}

	/* Restore default settings */
	sl->dev->type = ARPHRD_X25;
	
	/* Perform the low-level X.25 async init */
	if ((err = x25_asy_open(sl->dev)))
		return err;

	/* Done.  We have linked the TTY line to a channel. */
	return sl->dev->base_addr;
}


/*
 * Close down an X.25 channel.
 * This means flushing out any pending queues, and then restoring the
 * TTY line discipline to what it was before it got hooked to X.25
 * (which usually is TTY again).
 */
static void x25_asy_close_tty(struct tty_struct *tty)
{
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != X25_ASY_MAGIC)
		return;

	if (sl->dev->flags & IFF_UP)
	{
		(void) dev_close(sl->dev);
	}

	tty->disc_data = NULL;
	sl->tty = NULL;
	x25_asy_free(sl);
}


static struct net_device_stats *x25_asy_get_stats(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);

	return &sl->stats;
}


 /************************************************************************
  *			STANDARD X.25 ENCAPSULATION		  	 *
  ************************************************************************/

int x25_asy_esc(unsigned char *s, unsigned char *d, int len)
{
	unsigned char *ptr = d;
	unsigned char c;

	/*
	 * Send an initial END character to flush out any
	 * data that may have accumulated in the receiver
	 * due to line noise.
	 */

	*ptr++ = X25_END;	/* Send 10111110 bit seq */

	/*
	 * For each byte in the packet, send the appropriate
	 * character sequence, according to the X.25 protocol.
	 */

	while (len-- > 0) 
	{
		switch(c = *s++) 
		{
			case X25_END:
				*ptr++ = X25_ESC;
				*ptr++ = X25_ESCAPE(X25_END);
				break;
			case X25_ESC:
				*ptr++ = X25_ESC;
				*ptr++ = X25_ESCAPE(X25_ESC);
				break;
			 default:
				*ptr++ = c;
				break;
		}
	}
	*ptr++ = X25_END;
	return (ptr - d);
}

static void x25_asy_unesc(struct x25_asy *sl, unsigned char s)
{

	switch(s) 
	{
		case X25_END:
			if (!test_and_clear_bit(SLF_ERROR, &sl->flags) && (sl->rcount > 2))  
			{
				x25_asy_bump(sl);
			}
			clear_bit(SLF_ESCAPE, &sl->flags);
			sl->rcount = 0;
			return;

		case X25_ESC:
			set_bit(SLF_ESCAPE, &sl->flags);
			return;
			
		case X25_ESCAPE(X25_ESC):
		case X25_ESCAPE(X25_END):
			if (test_and_clear_bit(SLF_ESCAPE, &sl->flags))
				s = X25_UNESCAPE(s);
			break;
	}
	if (!test_bit(SLF_ERROR, &sl->flags))  
	{
		if (sl->rcount < sl->buffsize)  
		{
			sl->rbuff[sl->rcount++] = s;
			return;
		}
		sl->stats.rx_over_errors++;
		set_bit(SLF_ERROR, &sl->flags);
	}
}


/* Perform I/O control on an active X.25 channel. */
static int x25_asy_ioctl(struct tty_struct *tty, struct file *file,
			 unsigned int cmd,  unsigned long arg)
{
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != X25_ASY_MAGIC)
		return -EINVAL;

	switch(cmd) {
	case SIOCGIFNAME:
		if (copy_to_user((void __user *)arg, sl->dev->name,
					strlen(sl->dev->name) + 1))
			return -EFAULT;
		return 0;
	case SIOCSIFHWADDR:
		return -EINVAL;
	/* Allow stty to read, but not set, the serial port */
	case TCGETS:
	case TCGETA:
		return n_tty_ioctl(tty, file, cmd, arg);
	default:
		return -ENOIOCTLCMD;
	}
}

static int x25_asy_open_dev(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	if(sl->tty==NULL)
		return -ENODEV;
	return 0;
}

/* Initialise the X.25 driver.  Called by the device init code */
static void x25_asy_setup(struct net_device *dev)
{
	struct x25_asy *sl = dev->priv;

	sl->magic  = X25_ASY_MAGIC;
	sl->dev	   = dev;
	spin_lock_init(&sl->lock);
	set_bit(SLF_INUSE, &sl->flags);

	/*
	 *	Finish setting up the DEVICE info. 
	 */
	 
	dev->mtu		= SL_MTU;
	dev->hard_start_xmit	= x25_asy_xmit;
	dev->tx_timeout		= x25_asy_timeout;
	dev->watchdog_timeo	= HZ*20;
	dev->open		= x25_asy_open_dev;
	dev->stop		= x25_asy_close;
	dev->get_stats	        = x25_asy_get_stats;
	dev->change_mtu		= x25_asy_change_mtu;
	dev->hard_header_len	= 0;
	dev->addr_len		= 0;
	dev->type		= ARPHRD_X25;
	dev->tx_queue_len	= 10;

	/* New-style flags. */
	dev->flags		= IFF_NOARP;
}

static struct tty_ldisc x25_ldisc = {
	.owner		= THIS_MODULE,
	.magic		= TTY_LDISC_MAGIC,
	.name		= "X.25",
	.open		= x25_asy_open_tty,
	.close		= x25_asy_close_tty,
	.ioctl		= x25_asy_ioctl,
	.receive_buf	= x25_asy_receive_buf,
	.receive_room	= x25_asy_receive_room,
	.write_wakeup	= x25_asy_write_wakeup,
};

static int __init init_x25_asy(void)
{
	if (x25_asy_maxdev < 4)
		x25_asy_maxdev = 4; /* Sanity */

	printk(KERN_INFO "X.25 async: version 0.00 ALPHA "
			"(dynamic channels, max=%d).\n", x25_asy_maxdev );

	x25_asy_devs = kmalloc(sizeof(struct net_device *)*x25_asy_maxdev, 
			       GFP_KERNEL);
	if (!x25_asy_devs) {
		printk(KERN_WARNING "X25 async: Can't allocate x25_asy_ctrls[] "
				"array! Uaargh! (-> No X.25 available)\n");
		return -ENOMEM;
	}
	memset(x25_asy_devs, 0, sizeof(struct net_device *)*x25_asy_maxdev); 

	return tty_register_ldisc(N_X25, &x25_ldisc);
}


static void __exit exit_x25_asy(void)
{
	struct net_device *dev;
	int i;

	for (i = 0; i < x25_asy_maxdev; i++) {
		dev = x25_asy_devs[i];
		if (dev) {
			struct x25_asy *sl = dev->priv;

			spin_lock_bh(&sl->lock);
			if (sl->tty) 
				tty_hangup(sl->tty);

			spin_unlock_bh(&sl->lock);
			/*
			 * VSV = if dev->start==0, then device
			 * unregistered while close proc.
			 */
			unregister_netdev(dev);
			free_netdev(dev);
		}
	}

	kfree(x25_asy_devs);
	tty_unregister_ldisc(N_X25);
}

module_init(init_x25_asy);
module_exit(exit_x25_asy);
