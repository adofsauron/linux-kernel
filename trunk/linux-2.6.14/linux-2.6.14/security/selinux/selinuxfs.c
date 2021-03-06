/* Updated: Karl MacMillan <kmacmillan@tresys.com>
 *
 * 	Added conditional policy language extensions
 *
 * Copyright (C) 2003 - 2004 Tresys Technology, LLC
 * Copyright (C) 2004 Red Hat, Inc., James Morris <jmorris@redhat.com>
 *	This program is free software; you can redistribute it and/or modify
 *  	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/security.h>
#include <linux/major.h>
#include <linux/seq_file.h>
#include <linux/percpu.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

/* selinuxfs pseudo filesystem for exporting the security policy API.
   Based on the proc code and the fs/nfsd/nfsctl.c code. */

#include "flask.h"
#include "avc.h"
#include "avc_ss.h"
#include "security.h"
#include "objsec.h"
#include "conditional.h"

unsigned int selinux_checkreqprot = CONFIG_SECURITY_SELINUX_CHECKREQPROT_VALUE;

static int __init checkreqprot_setup(char *str)
{
	selinux_checkreqprot = simple_strtoul(str,NULL,0) ? 1 : 0;
	return 1;
}
__setup("checkreqprot=", checkreqprot_setup);


static DECLARE_MUTEX(sel_sem);

/* global data for booleans */
static struct dentry *bool_dir = NULL;
static int bool_num = 0;
static int *bool_pending_values = NULL;

extern void selnl_notify_setenforce(int val);

/* Check whether a task is allowed to use a security operation. */
static int task_has_security(struct task_struct *tsk,
			     u32 perms)
{
	struct task_security_struct *tsec;

	tsec = tsk->security;
	if (!tsec)
		return -EACCES;

	return avc_has_perm(tsec->sid, SECINITSID_SECURITY,
			    SECCLASS_SECURITY, perms, NULL);
}

enum sel_inos {
	SEL_ROOT_INO = 2,
	SEL_LOAD,	/* load policy */
	SEL_ENFORCE,	/* get or set enforcing status */
	SEL_CONTEXT,	/* validate context */
	SEL_ACCESS,	/* compute access decision */
	SEL_CREATE,	/* compute create labeling decision */
	SEL_RELABEL,	/* compute relabeling decision */
	SEL_USER,	/* compute reachable user contexts */
	SEL_POLICYVERS,	/* return policy version for this kernel */
	SEL_COMMIT_BOOLS, /* commit new boolean values */
	SEL_MLS,	/* return if MLS policy is enabled */
	SEL_DISABLE,	/* disable SELinux until next reboot */
	SEL_AVC,	/* AVC management directory */
	SEL_MEMBER,	/* compute polyinstantiation membership decision */
	SEL_CHECKREQPROT, /* check requested protection, not kernel-applied one */
};

#define TMPBUFLEN	12
static ssize_t sel_read_enforce(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%d", selinux_enforcing);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
static ssize_t sel_write_enforce(struct file * file, const char __user * buf,
				 size_t count, loff_t *ppos)

{
	char *page;
	ssize_t length;
	int new_value;

	if (count < 0 || count >= PAGE_SIZE)
		return -ENOMEM;
	if (*ppos != 0) {
		/* No partial writes. */
		return -EINVAL;
	}
	page = (char*)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;
	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value != selinux_enforcing) {
		length = task_has_security(current, SECURITY__SETENFORCE);
		if (length)
			goto out;
		selinux_enforcing = new_value;
		if (selinux_enforcing)
			avc_ss_reset(0);
		selnl_notify_setenforce(selinux_enforcing);
	}
	length = count;
out:
	free_page((unsigned long) page);
	return length;
}
#else
#define sel_write_enforce NULL
#endif

static struct file_operations sel_enforce_ops = {
	.read		= sel_read_enforce,
	.write		= sel_write_enforce,
};

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
static ssize_t sel_write_disable(struct file * file, const char __user * buf,
				 size_t count, loff_t *ppos)

{
	char *page;
	ssize_t length;
	int new_value;
	extern int selinux_disable(void);

	if (count < 0 || count >= PAGE_SIZE)
		return -ENOMEM;
	if (*ppos != 0) {
		/* No partial writes. */
		return -EINVAL;
	}
	page = (char*)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;
	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value) {
		length = selinux_disable();
		if (length < 0)
			goto out;
	}

	length = count;
out:
	free_page((unsigned long) page);
	return length;
}
#else
#define sel_write_disable NULL
#endif

static struct file_operations sel_disable_ops = {
	.write		= sel_write_disable,
};

static ssize_t sel_read_policyvers(struct file *filp, char __user *buf,
                                   size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%u", POLICYDB_VERSION_MAX);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static struct file_operations sel_policyvers_ops = {
	.read		= sel_read_policyvers,
};

/* declaration for sel_write_load */
static int sel_make_bools(void);

static ssize_t sel_read_mls(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%d", selinux_mls_enabled);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static struct file_operations sel_mls_ops = {
	.read		= sel_read_mls,
};

static ssize_t sel_write_load(struct file * file, const char __user * buf,
			      size_t count, loff_t *ppos)

{
	int ret;
	ssize_t length;
	void *data = NULL;

	down(&sel_sem);

	length = task_has_security(current, SECURITY__LOAD_POLICY);
	if (length)
		goto out;

	if (*ppos != 0) {
		/* No partial writes. */
		length = -EINVAL;
		goto out;
	}

	if ((count < 0) || (count > 64 * 1024 * 1024)
	    || (data = vmalloc(count)) == NULL) {
		length = -ENOMEM;
		goto out;
	}

	length = -EFAULT;
	if (copy_from_user(data, buf, count) != 0)
		goto out;

	length = security_load_policy(data, count);
	if (length)
		goto out;

	ret = sel_make_bools();
	if (ret)
		length = ret;
	else
		length = count;
out:
	up(&sel_sem);
	vfree(data);
	return length;
}

static struct file_operations sel_load_ops = {
	.write		= sel_write_load,
};


static ssize_t sel_write_context(struct file * file, const char __user * buf,
				 size_t count, loff_t *ppos)

{
	char *page;
	u32 sid;
	ssize_t length;

	length = task_has_security(current, SECURITY__CHECK_CONTEXT);
	if (length)
		return length;

	if (count < 0 || count >= PAGE_SIZE)
		return -ENOMEM;
	if (*ppos != 0) {
		/* No partial writes. */
		return -EINVAL;
	}
	page = (char*)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;
	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = security_context_to_sid(page, count, &sid);
	if (length < 0)
		goto out;

	length = count;
out:
	free_page((unsigned long) page);
	return length;
}

static struct file_operations sel_context_ops = {
	.write		= sel_write_context,
};

static ssize_t sel_read_checkreqprot(struct file *filp, char __user *buf,
				     size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%u", selinux_checkreqprot);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static ssize_t sel_write_checkreqprot(struct file * file, const char __user * buf,
				      size_t count, loff_t *ppos)
{
	char *page;
	ssize_t length;
	unsigned int new_value;

	length = task_has_security(current, SECURITY__SETCHECKREQPROT);
	if (length)
		return length;

	if (count < 0 || count >= PAGE_SIZE)
		return -ENOMEM;
	if (*ppos != 0) {
		/* No partial writes. */
		return -EINVAL;
	}
	page = (char*)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;
	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%u", &new_value) != 1)
		goto out;

	selinux_checkreqprot = new_value ? 1 : 0;
	length = count;
out:
	free_page((unsigned long) page);
	return length;
}
static struct file_operations sel_checkreqprot_ops = {
	.read		= sel_read_checkreqprot,
	.write		= sel_write_checkreqprot,
};

/*
 * Remaining nodes use transaction based IO methods like nfsd/nfsctl.c
 */
static ssize_t sel_write_access(struct file * file, char *buf, size_t size);
static ssize_t sel_write_create(struct file * file, char *buf, size_t size);
static ssize_t sel_write_relabel(struct file * file, char *buf, size_t size);
static ssize_t sel_write_user(struct file * file, char *buf, size_t size);
static ssize_t sel_write_member(struct file * file, char *buf, size_t size);

static ssize_t (*write_op[])(struct file *, char *, size_t) = {
	[SEL_ACCESS] = sel_write_access,
	[SEL_CREATE] = sel_write_create,
	[SEL_RELABEL] = sel_write_relabel,
	[SEL_USER] = sel_write_user,
	[SEL_MEMBER] = sel_write_member,
};

static ssize_t selinux_transaction_write(struct file *file, const char __user *buf, size_t size, loff_t *pos)
{
	ino_t ino =  file->f_dentry->d_inode->i_ino;
	char *data;
	ssize_t rv;

	if (ino >= sizeof(write_op)/sizeof(write_op[0]) || !write_op[ino])
		return -EINVAL;

	data = simple_transaction_get(file, buf, size);
	if (IS_ERR(data))
		return PTR_ERR(data);

	rv =  write_op[ino](file, data, size);
	if (rv>0) {
		simple_transaction_set(file, rv);
		rv = size;
	}
	return rv;
}

static struct file_operations transaction_ops = {
	.write		= selinux_transaction_write,
	.read		= simple_transaction_read,
	.release	= simple_transaction_release,
};

/*
 * payload - write methods
 * If the method has a response, the response should be put in buf,
 * and the length returned.  Otherwise return 0 or and -error.
 */

static ssize_t sel_write_access(struct file * file, char *buf, size_t size)
{
	char *scon, *tcon;
	u32 ssid, tsid;
	u16 tclass;
	u32 req;
	struct av_decision avd;
	ssize_t length;

	length = task_has_security(current, SECURITY__COMPUTE_AV);
	if (length)
		return length;

	length = -ENOMEM;
	scon = kmalloc(size+1, GFP_KERNEL);
	if (!scon)
		return length;
	memset(scon, 0, size+1);

	tcon = kmalloc(size+1, GFP_KERNEL);
	if (!tcon)
		goto out;
	memset(tcon, 0, size+1);

	length = -EINVAL;
	if (sscanf(buf, "%s %s %hu %x", scon, tcon, &tclass, &req) != 4)
		goto out2;

	length = security_context_to_sid(scon, strlen(scon)+1, &ssid);
	if (length < 0)
		goto out2;
	length = security_context_to_sid(tcon, strlen(tcon)+1, &tsid);
	if (length < 0)
		goto out2;

	length = security_compute_av(ssid, tsid, tclass, req, &avd);
	if (length < 0)
		goto out2;

	length = scnprintf(buf, SIMPLE_TRANSACTION_LIMIT,
			  "%x %x %x %x %u",
			  avd.allowed, avd.decided,
			  avd.auditallow, avd.auditdeny,
			  avd.seqno);
out2:
	kfree(tcon);
out:
	kfree(scon);
	return length;
}

static ssize_t sel_write_create(struct file * file, char *buf, size_t size)
{
	char *scon, *tcon;
	u32 ssid, tsid, newsid;
	u16 tclass;
	ssize_t length;
	char *newcon;
	u32 len;

	length = task_has_security(current, SECURITY__COMPUTE_CREATE);
	if (length)
		return length;

	length = -ENOMEM;
	scon = kmalloc(size+1, GFP_KERNEL);
	if (!scon)
		return length;
	memset(scon, 0, size+1);

	tcon = kmalloc(size+1, GFP_KERNEL);
	if (!tcon)
		goto out;
	memset(tcon, 0, size+1);

	length = -EINVAL;
	if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
		goto out2;

	length = security_context_to_sid(scon, strlen(scon)+1, &ssid);
	if (length < 0)
		goto out2;
	length = security_context_to_sid(tcon, strlen(tcon)+1, &tsid);
	if (length < 0)
		goto out2;

	length = security_transition_sid(ssid, tsid, tclass, &newsid);
	if (length < 0)
		goto out2;

	length = security_sid_to_context(newsid, &newcon, &len);
	if (length < 0)
		goto out2;

	if (len > SIMPLE_TRANSACTION_LIMIT) {
		printk(KERN_ERR "%s:  context size (%u) exceeds payload "
		       "max\n", __FUNCTION__, len);
		length = -ERANGE;
		goto out3;
	}

	memcpy(buf, newcon, len);
	length = len;
out3:
	kfree(newcon);
out2:
	kfree(tcon);
out:
	kfree(scon);
	return length;
}

static ssize_t sel_write_relabel(struct file * file, char *buf, size_t size)
{
	char *scon, *tcon;
	u32 ssid, tsid, newsid;
	u16 tclass;
	ssize_t length;
	char *newcon;
	u32 len;

	length = task_has_security(current, SECURITY__COMPUTE_RELABEL);
	if (length)
		return length;

	length = -ENOMEM;
	scon = kmalloc(size+1, GFP_KERNEL);
	if (!scon)
		return length;
	memset(scon, 0, size+1);

	tcon = kmalloc(size+1, GFP_KERNEL);
	if (!tcon)
		goto out;
	memset(tcon, 0, size+1);

	length = -EINVAL;
	if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
		goto out2;

	length = security_context_to_sid(scon, strlen(scon)+1, &ssid);
	if (length < 0)
		goto out2;
	length = security_context_to_sid(tcon, strlen(tcon)+1, &tsid);
	if (length < 0)
		goto out2;

	length = security_change_sid(ssid, tsid, tclass, &newsid);
	if (length < 0)
		goto out2;

	length = security_sid_to_context(newsid, &newcon, &len);
	if (length < 0)
		goto out2;

	if (len > SIMPLE_TRANSACTION_LIMIT) {
		length = -ERANGE;
		goto out3;
	}

	memcpy(buf, newcon, len);
	length = len;
out3:
	kfree(newcon);
out2:
	kfree(tcon);
out:
	kfree(scon);
	return length;
}

static ssize_t sel_write_user(struct file * file, char *buf, size_t size)
{
	char *con, *user, *ptr;
	u32 sid, *sids;
	ssize_t length;
	char *newcon;
	int i, rc;
	u32 len, nsids;

	length = task_has_security(current, SECURITY__COMPUTE_USER);
	if (length)
		return length;

	length = -ENOMEM;
	con = kmalloc(size+1, GFP_KERNEL);
	if (!con)
		return length;
	memset(con, 0, size+1);

	user = kmalloc(size+1, GFP_KERNEL);
	if (!user)
		goto out;
	memset(user, 0, size+1);

	length = -EINVAL;
	if (sscanf(buf, "%s %s", con, user) != 2)
		goto out2;

	length = security_context_to_sid(con, strlen(con)+1, &sid);
	if (length < 0)
		goto out2;

	length = security_get_user_sids(sid, user, &sids, &nsids);
	if (length < 0)
		goto out2;

	length = sprintf(buf, "%u", nsids) + 1;
	ptr = buf + length;
	for (i = 0; i < nsids; i++) {
		rc = security_sid_to_context(sids[i], &newcon, &len);
		if (rc) {
			length = rc;
			goto out3;
		}
		if ((length + len) >= SIMPLE_TRANSACTION_LIMIT) {
			kfree(newcon);
			length = -ERANGE;
			goto out3;
		}
		memcpy(ptr, newcon, len);
		kfree(newcon);
		ptr += len;
		length += len;
	}
out3:
	kfree(sids);
out2:
	kfree(user);
out:
	kfree(con);
	return length;
}

static ssize_t sel_write_member(struct file * file, char *buf, size_t size)
{
	char *scon, *tcon;
	u32 ssid, tsid, newsid;
	u16 tclass;
	ssize_t length;
	char *newcon;
	u32 len;

	length = task_has_security(current, SECURITY__COMPUTE_MEMBER);
	if (length)
		return length;

	length = -ENOMEM;
	scon = kmalloc(size+1, GFP_KERNEL);
	if (!scon)
		return length;
	memset(scon, 0, size+1);

	tcon = kmalloc(size+1, GFP_KERNEL);
	if (!tcon)
		goto out;
	memset(tcon, 0, size+1);

	length = -EINVAL;
	if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
		goto out2;

	length = security_context_to_sid(scon, strlen(scon)+1, &ssid);
	if (length < 0)
		goto out2;
	length = security_context_to_sid(tcon, strlen(tcon)+1, &tsid);
	if (length < 0)
		goto out2;

	length = security_member_sid(ssid, tsid, tclass, &newsid);
	if (length < 0)
		goto out2;

	length = security_sid_to_context(newsid, &newcon, &len);
	if (length < 0)
		goto out2;

	if (len > SIMPLE_TRANSACTION_LIMIT) {
		printk(KERN_ERR "%s:  context size (%u) exceeds payload "
		       "max\n", __FUNCTION__, len);
		length = -ERANGE;
		goto out3;
	}

	memcpy(buf, newcon, len);
	length = len;
out3:
	kfree(newcon);
out2:
	kfree(tcon);
out:
	kfree(scon);
	return length;
}

static struct inode *sel_make_inode(struct super_block *sb, int mode)
{
	struct inode *ret = new_inode(sb);

	if (ret) {
		ret->i_mode = mode;
		ret->i_uid = ret->i_gid = 0;
		ret->i_blksize = PAGE_CACHE_SIZE;
		ret->i_blocks = 0;
		ret->i_atime = ret->i_mtime = ret->i_ctime = CURRENT_TIME;
	}
	return ret;
}

#define BOOL_INO_OFFSET 30

static ssize_t sel_read_bool(struct file *filep, char __user *buf,
			     size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t length;
	ssize_t end;
	ssize_t ret;
	int cur_enforcing;
	struct inode *inode;

	down(&sel_sem);

	ret = -EFAULT;

	/* check to see if this file has been deleted */
	if (!filep->f_op)
		goto out;

	if (count < 0 || count > PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}
	if (!(page = (char*)get_zeroed_page(GFP_KERNEL))) {
		ret = -ENOMEM;
		goto out;
	}

	inode = filep->f_dentry->d_inode;
	cur_enforcing = security_get_bool_value(inode->i_ino - BOOL_INO_OFFSET);
	if (cur_enforcing < 0) {
		ret = cur_enforcing;
		goto out;
	}

	length = scnprintf(page, PAGE_SIZE, "%d %d", cur_enforcing,
			  bool_pending_values[inode->i_ino - BOOL_INO_OFFSET]);
	if (length < 0) {
		ret = length;
		goto out;
	}

	if (*ppos >= length) {
		ret = 0;
		goto out;
	}
	if (count + *ppos > length)
		count = length - *ppos;
	end = count + *ppos;
	if (copy_to_user(buf, (char *) page + *ppos, count)) {
		ret = -EFAULT;
		goto out;
	}
	*ppos = end;
	ret = count;
out:
	up(&sel_sem);
	if (page)
		free_page((unsigned long)page);
	return ret;
}

static ssize_t sel_write_bool(struct file *filep, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t length = -EFAULT;
	int new_value;
	struct inode *inode;

	down(&sel_sem);

	length = task_has_security(current, SECURITY__SETBOOL);
	if (length)
		goto out;

	/* check to see if this file has been deleted */
	if (!filep->f_op)
		goto out;

	if (count < 0 || count >= PAGE_SIZE) {
		length = -ENOMEM;
		goto out;
	}
	if (*ppos != 0) {
		/* No partial writes. */
		goto out;
	}
	page = (char*)get_zeroed_page(GFP_KERNEL);
	if (!page) {
		length = -ENOMEM;
		goto out;
	}

	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value)
		new_value = 1;

	inode = filep->f_dentry->d_inode;
	bool_pending_values[inode->i_ino - BOOL_INO_OFFSET] = new_value;
	length = count;

out:
	up(&sel_sem);
	if (page)
		free_page((unsigned long) page);
	return length;
}

static struct file_operations sel_bool_ops = {
	.read           = sel_read_bool,
	.write          = sel_write_bool,
};

static ssize_t sel_commit_bools_write(struct file *filep,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t length = -EFAULT;
	int new_value;

	down(&sel_sem);

	length = task_has_security(current, SECURITY__SETBOOL);
	if (length)
		goto out;

	/* check to see if this file has been deleted */
	if (!filep->f_op)
		goto out;

	if (count < 0 || count >= PAGE_SIZE) {
		length = -ENOMEM;
		goto out;
	}
	if (*ppos != 0) {
		/* No partial writes. */
		goto out;
	}
	page = (char*)get_zeroed_page(GFP_KERNEL);
	if (!page) {
		length = -ENOMEM;
		goto out;
	}

	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value && bool_pending_values) {
		security_set_bools(bool_num, bool_pending_values);
	}

	length = count;

out:
	up(&sel_sem);
	if (page)
		free_page((unsigned long) page);
	return length;
}

static struct file_operations sel_commit_bools_ops = {
	.write          = sel_commit_bools_write,
};

/* delete booleans - partial revoke() from
 * fs/proc/generic.c proc_kill_inodes */
static void sel_remove_bools(struct dentry *de)
{
	struct list_head *p, *node;
	struct super_block *sb = de->d_sb;

	spin_lock(&dcache_lock);
	node = de->d_subdirs.next;
	while (node != &de->d_subdirs) {
		struct dentry *d = list_entry(node, struct dentry, d_child);
		list_del_init(node);

		if (d->d_inode) {
			d = dget_locked(d);
			spin_unlock(&dcache_lock);
			d_delete(d);
			simple_unlink(de->d_inode, d);
			dput(d);
			spin_lock(&dcache_lock);
		}
		node = de->d_subdirs.next;
	}

	spin_unlock(&dcache_lock);

	file_list_lock();
	list_for_each(p, &sb->s_files) {
		struct file * filp = list_entry(p, struct file, f_list);
		struct dentry * dentry = filp->f_dentry;

		if (dentry->d_parent != de) {
			continue;
		}
		filp->f_op = NULL;
	}
	file_list_unlock();
}

#define BOOL_DIR_NAME "booleans"

static int sel_make_bools(void)
{
	int i, ret = 0;
	ssize_t len;
	struct dentry *dentry = NULL;
	struct dentry *dir = bool_dir;
	struct inode *inode = NULL;
	struct inode_security_struct *isec;
	char **names = NULL, *page;
	int num;
	int *values = NULL;
	u32 sid;

	/* remove any existing files */
	kfree(bool_pending_values);
	bool_pending_values = NULL;

	sel_remove_bools(dir);

	if (!(page = (char*)get_zeroed_page(GFP_KERNEL)))
		return -ENOMEM;

	ret = security_get_bools(&num, &names, &values);
	if (ret != 0)
		goto out;

	for (i = 0; i < num; i++) {
		dentry = d_alloc_name(dir, names[i]);
		if (!dentry) {
			ret = -ENOMEM;
			goto err;
		}
		inode = sel_make_inode(dir->d_sb, S_IFREG | S_IRUGO | S_IWUSR);
		if (!inode) {
			ret = -ENOMEM;
			goto err;
		}

		len = snprintf(page, PAGE_SIZE, "/%s/%s", BOOL_DIR_NAME, names[i]);
		if (len < 0) {
			ret = -EINVAL;
			goto err;
		} else if (len >= PAGE_SIZE) {
			ret = -ENAMETOOLONG;
			goto err;
		}
		isec = (struct inode_security_struct*)inode->i_security;
		if ((ret = security_genfs_sid("selinuxfs", page, SECCLASS_FILE, &sid)))
			goto err;
		isec->sid = sid;
		isec->initialized = 1;
		inode->i_fop = &sel_bool_ops;
		inode->i_ino = i + BOOL_INO_OFFSET;
		d_add(dentry, inode);
	}
	bool_num = num;
	bool_pending_values = values;
out:
	free_page((unsigned long)page);
	if (names) {
		for (i = 0; i < num; i++)
			kfree(names[i]);
		kfree(names);
	}
	return ret;
err:
	kfree(values);
	d_genocide(dir);
	ret = -ENOMEM;
	goto out;
}

#define NULL_FILE_NAME "null"

struct dentry *selinux_null = NULL;

static ssize_t sel_read_avc_cache_threshold(struct file *filp, char __user *buf,
					    size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%u", avc_cache_threshold);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static ssize_t sel_write_avc_cache_threshold(struct file * file,
					     const char __user * buf,
					     size_t count, loff_t *ppos)

{
	char *page;
	ssize_t ret;
	int new_value;

	if (count < 0 || count >= PAGE_SIZE) {
		ret = -ENOMEM;
		goto out;
	}

	if (*ppos != 0) {
		/* No partial writes. */
		ret = -EINVAL;
		goto out;
	}

	page = (char*)get_zeroed_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(page, buf, count)) {
		ret = -EFAULT;
		goto out_free;
	}

	if (sscanf(page, "%u", &new_value) != 1) {
		ret = -EINVAL;
		goto out;
	}

	if (new_value != avc_cache_threshold) {
		ret = task_has_security(current, SECURITY__SETSECPARAM);
		if (ret)
			goto out_free;
		avc_cache_threshold = new_value;
	}
	ret = count;
out_free:
	free_page((unsigned long)page);
out:
	return ret;
}

static ssize_t sel_read_avc_hash_stats(struct file *filp, char __user *buf,
				       size_t count, loff_t *ppos)
{
	char *page;
	ssize_t ret = 0;

	page = (char *)__get_free_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto out;
	}
	ret = avc_get_hash_stats(page);
	if (ret >= 0)
		ret = simple_read_from_buffer(buf, count, ppos, page, ret);
	free_page((unsigned long)page);
out:
	return ret;
}

static struct file_operations sel_avc_cache_threshold_ops = {
	.read		= sel_read_avc_cache_threshold,
	.write		= sel_write_avc_cache_threshold,
};

static struct file_operations sel_avc_hash_stats_ops = {
	.read		= sel_read_avc_hash_stats,
};

#ifdef CONFIG_SECURITY_SELINUX_AVC_STATS
static struct avc_cache_stats *sel_avc_get_stat_idx(loff_t *idx)
{
	int cpu;

	for (cpu = *idx; cpu < NR_CPUS; ++cpu) {
		if (!cpu_possible(cpu))
			continue;
		*idx = cpu + 1;
		return &per_cpu(avc_cache_stats, cpu);
	}
	return NULL;
}

static void *sel_avc_stats_seq_start(struct seq_file *seq, loff_t *pos)
{
	loff_t n = *pos - 1;

	if (*pos == 0)
		return SEQ_START_TOKEN;

	return sel_avc_get_stat_idx(&n);
}

static void *sel_avc_stats_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return sel_avc_get_stat_idx(pos);
}

static int sel_avc_stats_seq_show(struct seq_file *seq, void *v)
{
	struct avc_cache_stats *st = v;

	if (v == SEQ_START_TOKEN)
		seq_printf(seq, "lookups hits misses allocations reclaims "
			   "frees\n");
	else
		seq_printf(seq, "%u %u %u %u %u %u\n", st->lookups,
			   st->hits, st->misses, st->allocations,
			   st->reclaims, st->frees);
	return 0;
}

static void sel_avc_stats_seq_stop(struct seq_file *seq, void *v)
{ }

static struct seq_operations sel_avc_cache_stats_seq_ops = {
	.start		= sel_avc_stats_seq_start,
	.next		= sel_avc_stats_seq_next,
	.show		= sel_avc_stats_seq_show,
	.stop		= sel_avc_stats_seq_stop,
};

static int sel_open_avc_cache_stats(struct inode *inode, struct file *file)
{
	return seq_open(file, &sel_avc_cache_stats_seq_ops);
}

static struct file_operations sel_avc_cache_stats_ops = {
	.open		= sel_open_avc_cache_stats,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
#endif

static int sel_make_avc_files(struct dentry *dir)
{
	int i, ret = 0;
	static struct tree_descr files[] = {
		{ "cache_threshold",
		  &sel_avc_cache_threshold_ops, S_IRUGO|S_IWUSR },
		{ "hash_stats", &sel_avc_hash_stats_ops, S_IRUGO },
#ifdef CONFIG_SECURITY_SELINUX_AVC_STATS
		{ "cache_stats", &sel_avc_cache_stats_ops, S_IRUGO },
#endif
	};

	for (i = 0; i < sizeof (files) / sizeof (files[0]); i++) {
		struct inode *inode;
		struct dentry *dentry;

		dentry = d_alloc_name(dir, files[i].name);
		if (!dentry) {
			ret = -ENOMEM;
			goto err;
		}

		inode = sel_make_inode(dir->d_sb, S_IFREG|files[i].mode);
		if (!inode) {
			ret = -ENOMEM;
			goto err;
		}
		inode->i_fop = files[i].ops;
		d_add(dentry, inode);
	}
out:
	return ret;
err:
	d_genocide(dir);
	goto out;
}

static int sel_make_dir(struct super_block *sb, struct dentry *dentry)
{
	int ret = 0;
	struct inode *inode;

	inode = sel_make_inode(sb, S_IFDIR | S_IRUGO | S_IXUGO);
	if (!inode) {
		ret = -ENOMEM;
		goto out;
	}
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	d_add(dentry, inode);
out:
	return ret;
}

static int sel_fill_super(struct super_block * sb, void * data, int silent)
{
	int ret;
	struct dentry *dentry;
	struct inode *inode;
	struct inode_security_struct *isec;

	static struct tree_descr selinux_files[] = {
		[SEL_LOAD] = {"load", &sel_load_ops, S_IRUSR|S_IWUSR},
		[SEL_ENFORCE] = {"enforce", &sel_enforce_ops, S_IRUGO|S_IWUSR},
		[SEL_CONTEXT] = {"context", &sel_context_ops, S_IRUGO|S_IWUGO},
		[SEL_ACCESS] = {"access", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_CREATE] = {"create", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_RELABEL] = {"relabel", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_USER] = {"user", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_POLICYVERS] = {"policyvers", &sel_policyvers_ops, S_IRUGO},
		[SEL_COMMIT_BOOLS] = {"commit_pending_bools", &sel_commit_bools_ops, S_IWUSR},
		[SEL_MLS] = {"mls", &sel_mls_ops, S_IRUGO},
		[SEL_DISABLE] = {"disable", &sel_disable_ops, S_IWUSR},
		[SEL_MEMBER] = {"member", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_CHECKREQPROT] = {"checkreqprot", &sel_checkreqprot_ops, S_IRUGO|S_IWUSR},
		/* last one */ {""}
	};
	ret = simple_fill_super(sb, SELINUX_MAGIC, selinux_files);
	if (ret)
		return ret;

	dentry = d_alloc_name(sb->s_root, BOOL_DIR_NAME);
	if (!dentry)
		return -ENOMEM;

	inode = sel_make_inode(sb, S_IFDIR | S_IRUGO | S_IXUGO);
	if (!inode)
		goto out;
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	d_add(dentry, inode);
	bool_dir = dentry;
	ret = sel_make_bools();
	if (ret)
		goto out;

	dentry = d_alloc_name(sb->s_root, NULL_FILE_NAME);
	if (!dentry)
		return -ENOMEM;

	inode = sel_make_inode(sb, S_IFCHR | S_IRUGO | S_IWUGO);
	if (!inode)
		goto out;
	isec = (struct inode_security_struct*)inode->i_security;
	isec->sid = SECINITSID_DEVNULL;
	isec->sclass = SECCLASS_CHR_FILE;
	isec->initialized = 1;

	init_special_inode(inode, S_IFCHR | S_IRUGO | S_IWUGO, MKDEV(MEM_MAJOR, 3));
	d_add(dentry, inode);
	selinux_null = dentry;

	dentry = d_alloc_name(sb->s_root, "avc");
	if (!dentry)
		return -ENOMEM;

	ret = sel_make_dir(sb, dentry);
	if (ret)
		goto out;

	ret = sel_make_avc_files(dentry);
	if (ret)
		goto out;

	return 0;
out:
	dput(dentry);
	printk(KERN_ERR "%s:  failed while creating inodes\n", __FUNCTION__);
	return -ENOMEM;
}

static struct super_block *sel_get_sb(struct file_system_type *fs_type,
				      int flags, const char *dev_name, void *data)
{
	return get_sb_single(fs_type, flags, data, sel_fill_super);
}

static struct file_system_type sel_fs_type = {
	.name		= "selinuxfs",
	.get_sb		= sel_get_sb,
	.kill_sb	= kill_litter_super,
};

struct vfsmount *selinuxfs_mount;

static int __init init_sel_fs(void)
{
	int err;

	if (!selinux_enabled)
		return 0;
	err = register_filesystem(&sel_fs_type);
	if (!err) {
		selinuxfs_mount = kern_mount(&sel_fs_type);
		if (IS_ERR(selinuxfs_mount)) {
			printk(KERN_ERR "selinuxfs:  could not mount!\n");
			err = PTR_ERR(selinuxfs_mount);
			selinuxfs_mount = NULL;
		}
	}
	return err;
}

__initcall(init_sel_fs);

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
void exit_sel_fs(void)
{
	unregister_filesystem(&sel_fs_type);
}
#endif
