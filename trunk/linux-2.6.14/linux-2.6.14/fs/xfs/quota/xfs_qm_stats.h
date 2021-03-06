/*
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * or the like.	 Any license provided herein, whether implied or
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
#ifndef __XFS_QM_STATS_H__
#define __XFS_QM_STATS_H__


#if defined(CONFIG_PROC_FS) && !defined(XFS_STATS_OFF)

/*
 * XQM global statistics
 */
struct xqmstats {
	__uint32_t		xs_qm_dqreclaims;
	__uint32_t		xs_qm_dqreclaim_misses;
	__uint32_t		xs_qm_dquot_dups;
	__uint32_t		xs_qm_dqcachemisses;
	__uint32_t		xs_qm_dqcachehits;
	__uint32_t		xs_qm_dqwants;
	__uint32_t		xs_qm_dqshake_reclaims;
	__uint32_t		xs_qm_dqinact_reclaims;
};

extern struct xqmstats xqmstats;

# define XQM_STATS_INC(count)	( (count)++ )

extern void xfs_qm_init_procfs(void);
extern void xfs_qm_cleanup_procfs(void);

#else

# define XQM_STATS_INC(count)	do { } while (0)

static __inline void xfs_qm_init_procfs(void) { };
static __inline void xfs_qm_cleanup_procfs(void) { };

#endif

#endif	/* __XFS_QM_STATS_H__ */
