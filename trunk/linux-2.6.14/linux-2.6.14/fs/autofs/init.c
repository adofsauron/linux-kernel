/* -*- linux-c -*- --------------------------------------------------------- *
 *
 * linux/fs/autofs/init.c
 *
 *  Copyright 1997-1998 Transmeta Corporation -- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * ------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/init.h>
#include "autofs_i.h"

static struct super_block *autofs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return get_sb_nodev(fs_type, flags, data, autofs_fill_super);
}

static struct file_system_type autofs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "autofs",
	.get_sb		= autofs_get_sb,
	.kill_sb	= kill_anon_super,
};

static int __init init_autofs_fs(void)
{
	return register_filesystem(&autofs_fs_type);
}

static void __exit exit_autofs_fs(void)
{
	unregister_filesystem(&autofs_fs_type);
}

module_init(init_autofs_fs);
module_exit(exit_autofs_fs);

#ifdef DEBUG
void autofs_say(const char *name, int len)
{
	printk("(%d: ", len);
	while ( len-- )
		printk("%c", *name++);
	printk(")\n");
}
#endif
MODULE_LICENSE("GPL");
