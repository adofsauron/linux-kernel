/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#ifndef __XFS_UTILS_H__
#define __XFS_UTILS_H__

#define IRELE(ip)	VN_RELE(XFS_ITOV(ip))
#define IHOLD(ip)	VN_HOLD(XFS_ITOV(ip))
#define	ITRACE(ip)	vn_trace_ref(XFS_ITOV(ip), __FILE__, __LINE__, \
				(inst_t *)__return_address)

extern int xfs_rename (bhv_desc_t *, vname_t *, vnode_t *, vname_t *, cred_t *);
extern int xfs_get_dir_entry (vname_t *, xfs_inode_t **);
extern int xfs_dir_lookup_int (bhv_desc_t *, uint, vname_t *, xfs_ino_t *,
				xfs_inode_t **);
extern int xfs_truncate_file (xfs_mount_t *, xfs_inode_t *);
extern int xfs_dir_ialloc (xfs_trans_t **, xfs_inode_t *, mode_t, xfs_nlink_t,
				xfs_dev_t, cred_t *, prid_t, int,
				xfs_inode_t **, int *);
extern int xfs_droplink (xfs_trans_t *, xfs_inode_t *);
extern int xfs_bumplink (xfs_trans_t *, xfs_inode_t *);
extern void xfs_bump_ino_vers2 (xfs_trans_t *, xfs_inode_t *);

#endif	/* __XFS_UTILS_H__ */
