/*
 *	w1_int.c
 *
 * Copyright (c) 2004 Evgeniy Polyakov <johnpol@2ka.mipt.ru>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/delay.h>

#include "w1.h"
#include "w1_log.h"
#include "w1_netlink.h"

static u32 w1_ids = 1;

extern struct device_driver w1_master_driver;
extern struct bus_type w1_bus_type;
extern struct device w1_master_device;
extern int w1_max_slave_count;
extern int w1_max_slave_ttl;
extern struct list_head w1_masters;
extern spinlock_t w1_mlock;

extern int w1_process(void *);

static struct w1_master * w1_alloc_dev(u32 id, int slave_count, int slave_ttl,
				       struct device_driver *driver,
				       struct device *device)
{
	struct w1_master *dev;
	int err;

	/*
	 * We are in process context(kernel thread), so can sleep.
	 */
	dev = kmalloc(sizeof(struct w1_master) + sizeof(struct w1_bus_master), GFP_KERNEL);
	if (!dev) {
		printk(KERN_ERR
			"Failed to allocate %zd bytes for new w1 device.\n",
			sizeof(struct w1_master));
		return NULL;
	}

	memset(dev, 0, sizeof(struct w1_master) + sizeof(struct w1_bus_master));

	dev->bus_master = (struct w1_bus_master *)(dev + 1);

	dev->owner		= THIS_MODULE;
	dev->max_slave_count	= slave_count;
	dev->slave_count	= 0;
	dev->attempts		= 0;
	dev->kpid		= -1;
	dev->initialized	= 0;
	dev->id			= id;
	dev->slave_ttl		= slave_ttl;
        dev->search_count	= -1; /* continual scan */

	atomic_set(&dev->refcnt, 2);

	INIT_LIST_HEAD(&dev->slist);
	init_MUTEX(&dev->mutex);

	init_completion(&dev->dev_exited);

	memcpy(&dev->dev, device, sizeof(struct device));
	snprintf(dev->dev.bus_id, sizeof(dev->dev.bus_id),
		  "w1_bus_master%u", dev->id);
	snprintf(dev->name, sizeof(dev->name), "w1_bus_master%u", dev->id);

	dev->driver = driver;

	dev->groups = 1;
	dev->seq = 1;
	dev_init_netlink(dev);

	err = device_register(&dev->dev);
	if (err) {
		printk(KERN_ERR "Failed to register master device. err=%d\n", err);

		dev_fini_netlink(dev);

		memset(dev, 0, sizeof(struct w1_master));
		kfree(dev);
		dev = NULL;
	}

	return dev;
}

void w1_free_dev(struct w1_master *dev)
{
	device_unregister(&dev->dev);
}

int w1_add_master_device(struct w1_bus_master *master)
{
	struct w1_master *dev;
	int retval = 0;
	struct w1_netlink_msg msg;

        /* validate minimum functionality */
        if (!(master->touch_bit && master->reset_bus) &&
            !(master->write_bit && master->read_bit)) {
		printk(KERN_ERR "w1_add_master_device: invalid function set\n");
		return(-EINVAL);
        }

	dev = w1_alloc_dev(w1_ids++, w1_max_slave_count, w1_max_slave_ttl, &w1_master_driver, &w1_master_device);
	if (!dev)
		return -ENOMEM;

	dev->kpid = kernel_thread(&w1_process, dev, 0);
	if (dev->kpid < 0) {
		dev_err(&dev->dev,
			 "Failed to create new kernel thread. err=%d\n",
			 dev->kpid);
		retval = dev->kpid;
		goto err_out_free_dev;
	}

	retval =  w1_create_master_attributes(dev);
	if (retval)
		goto err_out_kill_thread;

	memcpy(dev->bus_master, master, sizeof(struct w1_bus_master));

	dev->initialized = 1;

	spin_lock(&w1_mlock);
	list_add(&dev->w1_master_entry, &w1_masters);
	spin_unlock(&w1_mlock);

	msg.id.mst.id = dev->id;
	msg.id.mst.pid = dev->kpid;
	msg.type = W1_MASTER_ADD;
	w1_netlink_send(dev, &msg);

	return 0;

err_out_kill_thread:
	set_bit(W1_MASTER_NEED_EXIT, &dev->flags);
	if (kill_proc(dev->kpid, SIGTERM, 1))
		dev_err(&dev->dev,
			 "Failed to send signal to w1 kernel thread %d.\n",
			 dev->kpid);
	wait_for_completion(&dev->dev_exited);

err_out_free_dev:
	w1_free_dev(dev);

	return retval;
}

void __w1_remove_master_device(struct w1_master *dev)
{
	int err;
	struct w1_netlink_msg msg;

	set_bit(W1_MASTER_NEED_EXIT, &dev->flags);
	err = kill_proc(dev->kpid, SIGTERM, 1);
	if (err)
		dev_err(&dev->dev,
			 "%s: Failed to send signal to w1 kernel thread %d.\n",
			 __func__, dev->kpid);

	while (atomic_read(&dev->refcnt)) {
		dev_dbg(&dev->dev, "Waiting for %s to become free: refcnt=%d.\n",
				dev->name, atomic_read(&dev->refcnt));

		if (msleep_interruptible(1000))
			flush_signals(current);
	}

	msg.id.mst.id = dev->id;
	msg.id.mst.pid = dev->kpid;
	msg.type = W1_MASTER_REMOVE;
	w1_netlink_send(dev, &msg);

	w1_free_dev(dev);
}

void w1_remove_master_device(struct w1_bus_master *bm)
{
	struct w1_master *dev = NULL;

	list_for_each_entry(dev, &w1_masters, w1_master_entry) {
		if (!dev->initialized)
			continue;

		if (dev->bus_master->data == bm->data)
			break;
	}

	if (!dev) {
		printk(KERN_ERR "Device doesn't exist.\n");
		return;
	}

	__w1_remove_master_device(dev);
}

EXPORT_SYMBOL(w1_add_master_device);
EXPORT_SYMBOL(w1_remove_master_device);

MODULE_ALIAS_NET_PF_PROTO(PF_NETLINK, NETLINK_W1);
