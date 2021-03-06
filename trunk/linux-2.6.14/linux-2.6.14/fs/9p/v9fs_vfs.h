/*
 * V9FS VFS extensions.
 *
 *  Copyright (C) 2004 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

/* plan9 semantics are that created files are implicitly opened.
 * But linux semantics are that you call create, then open.
 * the plan9 approach is superior as it provides an atomic
 * open.
 * we track the create fid here. When the file is opened, if fidopen is
 * non-zero, we use the fid and can skip some steps.
 * there may be a better way to do this, but I don't know it.
 * one BAD way is to clunk the fid on create, then open it again:
 * you lose the atomicity of file open
 */

/* special case:
 * unlink calls remove, which is an implicit clunk. So we have to track
 * that kind of thing so that we don't try to clunk a dead fid.
 */

extern struct file_system_type v9fs_fs_type;
extern struct file_operations v9fs_file_operations;
extern struct file_operations v9fs_dir_operations;
extern struct dentry_operations v9fs_dentry_operations;

struct inode *v9fs_get_inode(struct super_block *sb, int mode);
ino_t v9fs_qid2ino(struct v9fs_qid *qid);
void v9fs_mistat2inode(struct v9fs_stat *, struct inode *,
		       struct super_block *);
int v9fs_dir_release(struct inode *inode, struct file *filp);
int v9fs_file_open(struct inode *inode, struct file *file);
void v9fs_inode2mistat(struct inode *inode, struct v9fs_stat *mistat);
void v9fs_dentry_release(struct dentry *);
