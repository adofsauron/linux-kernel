/*
 * The USB Monitor, inspired by Dave Harding's USBMon.
 *
 * This is the 's' or 'stat' reader which debugs usbmon itself.
 * Note that this code blows through locks, so make sure that
 * /dbg/usbmon/0s is well protected from non-root users.
 *
 */

#include <linux/kernel.h>
#include <linux/usb.h>
#include <asm/uaccess.h>

#include "usb_mon.h"

#define STAT_BUF_SIZE  80

struct snap {
	int slen;
	char str[STAT_BUF_SIZE];
};

static int mon_stat_open(struct inode *inode, struct file *file)
{
	struct mon_bus *mbus;
	struct snap *sp;

	if ((sp = kmalloc(sizeof(struct snap), GFP_KERNEL)) == NULL)
		return -ENOMEM;

	mbus = inode->u.generic_ip;

	sp->slen = snprintf(sp->str, STAT_BUF_SIZE,
	    "nreaders %d text_lost %u\n",
	    mbus->nreaders, mbus->cnt_text_lost);

	file->private_data = sp;
	return 0;
}

static ssize_t mon_stat_read(struct file *file, char __user *buf,
				size_t nbytes, loff_t *ppos)
{
	struct snap *sp = file->private_data;
	loff_t pos = *ppos;
	int cnt;

	if (pos < 0 || pos >= sp->slen)
		return 0;
	if (nbytes == 0)
		return 0;
	if ((cnt = sp->slen - pos) > nbytes)
		cnt = nbytes;
	if (copy_to_user(buf, sp->str + pos, cnt))
		return -EFAULT;
	*ppos = pos + cnt;
	return cnt;
}

static int mon_stat_release(struct inode *inode, struct file *file)
{
	return 0;
}

struct file_operations mon_fops_stat = {
	.owner =	THIS_MODULE,
	.open =		mon_stat_open,
	.llseek =	no_llseek,
	.read =		mon_stat_read,
	/* .write =	mon_stat_write, */
	/* .poll =		mon_stat_poll, */
	/* .ioctl =	mon_stat_ioctl, */
	.release =	mon_stat_release,
};
