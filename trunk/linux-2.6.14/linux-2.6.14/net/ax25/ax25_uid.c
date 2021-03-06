/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) Jonathan Naylor G4KLX (g4klx@g4klx.demon.co.uk)
 */
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/spinlock.h>
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/netfilter.h>
#include <linux/sysctl.h>
#include <net/ip.h>
#include <net/arp.h>

/*
 *	Callsign/UID mapper. This is in kernel space for security on multi-amateur machines.
 */

HLIST_HEAD(ax25_uid_list);
static DEFINE_RWLOCK(ax25_uid_lock);

int ax25_uid_policy = 0;

ax25_uid_assoc *ax25_findbyuid(uid_t uid)
{
	ax25_uid_assoc *ax25_uid, *res = NULL;
	struct hlist_node *node;

	read_lock(&ax25_uid_lock);
	ax25_uid_for_each(ax25_uid, node, &ax25_uid_list) {
		if (ax25_uid->uid == uid) {
			ax25_uid_hold(ax25_uid);
			res = ax25_uid;
			break;
		}
	}
	read_unlock(&ax25_uid_lock);

	return res;
}

int ax25_uid_ioctl(int cmd, struct sockaddr_ax25 *sax)
{
	ax25_uid_assoc *ax25_uid;
	struct hlist_node *node;
	ax25_uid_assoc *user;
	unsigned long res;

	switch (cmd) {
	case SIOCAX25GETUID:
		res = -ENOENT;
		read_lock(&ax25_uid_lock);
		ax25_uid_for_each(ax25_uid, node, &ax25_uid_list) {
			if (ax25cmp(&sax->sax25_call, &ax25_uid->call) == 0) {
				res = ax25_uid->uid;
				break;
			}
		}
		read_unlock(&ax25_uid_lock);

		return res;

	case SIOCAX25ADDUID:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		user = ax25_findbyuid(sax->sax25_uid);
		if (user) {
			ax25_uid_put(user);
			return -EEXIST;
		}
		if (sax->sax25_uid == 0)
			return -EINVAL;
		if ((ax25_uid = kmalloc(sizeof(*ax25_uid), GFP_KERNEL)) == NULL)
			return -ENOMEM;

		atomic_set(&ax25_uid->refcount, 1);
		ax25_uid->uid  = sax->sax25_uid;
		ax25_uid->call = sax->sax25_call;

		write_lock(&ax25_uid_lock);
		hlist_add_head(&ax25_uid->uid_node, &ax25_uid_list);
		write_unlock(&ax25_uid_lock);

		return 0;

	case SIOCAX25DELUID:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		ax25_uid = NULL;
		write_lock(&ax25_uid_lock);
		ax25_uid_for_each(ax25_uid, node, &ax25_uid_list) {
			if (ax25cmp(&sax->sax25_call, &ax25_uid->call) == 0)
				break;
		}
		if (ax25_uid == NULL) {
			write_unlock(&ax25_uid_lock);
			return -ENOENT;
		}
		hlist_del_init(&ax25_uid->uid_node);
		ax25_uid_put(ax25_uid);
		write_unlock(&ax25_uid_lock);

		return 0;

	default:
		return -EINVAL;
	}

	return -EINVAL;	/*NOTREACHED */
}

#ifdef CONFIG_PROC_FS

static void *ax25_uid_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct ax25_uid_assoc *pt;
	struct hlist_node *node;
	int i = 0;

	read_lock(&ax25_uid_lock);
	ax25_uid_for_each(pt, node, &ax25_uid_list) {
		if (i == *pos)
			return pt;
		++i;
	}
	return NULL;
}

static void *ax25_uid_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	++*pos;

	return hlist_entry(((ax25_uid_assoc *)v)->uid_node.next,
	                   ax25_uid_assoc, uid_node);
}

static void ax25_uid_seq_stop(struct seq_file *seq, void *v)
{
	read_unlock(&ax25_uid_lock);
}

static int ax25_uid_seq_show(struct seq_file *seq, void *v)
{
	char buf[11];

	if (v == SEQ_START_TOKEN)
		seq_printf(seq, "Policy: %d\n", ax25_uid_policy);
	else {
		struct ax25_uid_assoc *pt = v;

		seq_printf(seq, "%6d %s\n", pt->uid, ax2asc(buf, &pt->call));
	}
	return 0;
}

static struct seq_operations ax25_uid_seqops = {
	.start = ax25_uid_seq_start,
	.next = ax25_uid_seq_next,
	.stop = ax25_uid_seq_stop,
	.show = ax25_uid_seq_show,
};

static int ax25_uid_info_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ax25_uid_seqops);
}

struct file_operations ax25_uid_fops = {
	.owner = THIS_MODULE,
	.open = ax25_uid_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

#endif

/*
 *	Free all memory associated with UID/Callsign structures.
 */
void __exit ax25_uid_free(void)
{
	ax25_uid_assoc *ax25_uid;
	struct hlist_node *node;

	write_lock(&ax25_uid_lock);
	ax25_uid_for_each(ax25_uid, node, &ax25_uid_list) {
		hlist_del_init(&ax25_uid->uid_node);
		ax25_uid_put(ax25_uid);
	}
	write_unlock(&ax25_uid_lock);
}
