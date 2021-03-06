/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef __XFS_RTALLOC_H__
#define	__XFS_RTALLOC_H__

struct xfs_mount;
struct xfs_trans;

#define XFS_IS_REALTIME_INODE(ip) ((ip)->i_d.di_flags & XFS_DIFLAG_REALTIME)

/* Min and max rt extent sizes, specified in bytes */
#define	XFS_MAX_RTEXTSIZE	(1024 * 1024 * 1024)	/* 1GB */
#define	XFS_DFL_RTEXTSIZE	(64 * 1024)	        /* 64KB */
#define	XFS_MIN_RTEXTSIZE	(4 * 1024)		/* 4KB */

/*
 * Constants for bit manipulations.
 */
#define	XFS_NBBYLOG	3		/* log2(NBBY) */
#define	XFS_WORDLOG	2		/* log2(sizeof(xfs_rtword_t)) */
#define	XFS_NBWORDLOG	(XFS_NBBYLOG + XFS_WORDLOG)
#define	XFS_NBWORD	(1 << XFS_NBWORDLOG)
#define	XFS_WORDMASK	((1 << XFS_WORDLOG) - 1)

#define	XFS_BLOCKSIZE(mp)	((mp)->m_sb.sb_blocksize)
#define	XFS_BLOCKMASK(mp)	((mp)->m_blockmask)
#define	XFS_BLOCKWSIZE(mp)	((mp)->m_blockwsize)
#define	XFS_BLOCKWMASK(mp)	((mp)->m_blockwmask)

/*
 * Summary and bit manipulation macros.
 */
#define	XFS_SUMOFFS(mp,ls,bb)	((int)((ls) * (mp)->m_sb.sb_rbmblocks + (bb)))
#define	XFS_SUMOFFSTOBLOCK(mp,s)	\
	(((s) * (uint)sizeof(xfs_suminfo_t)) >> (mp)->m_sb.sb_blocklog)
#define	XFS_SUMPTR(mp,bp,so)	\
	((xfs_suminfo_t *)((char *)XFS_BUF_PTR(bp) + \
		(((so) * (uint)sizeof(xfs_suminfo_t)) & XFS_BLOCKMASK(mp))))

#define	XFS_BITTOBLOCK(mp,bi)	((bi) >> (mp)->m_blkbit_log)
#define	XFS_BLOCKTOBIT(mp,bb)	((bb) << (mp)->m_blkbit_log)
#define	XFS_BITTOWORD(mp,bi)	\
	((int)(((bi) >> XFS_NBWORDLOG) & XFS_BLOCKWMASK(mp)))

#define	XFS_RTMIN(a,b)	((a) < (b) ? (a) : (b))
#define	XFS_RTMAX(a,b)	((a) > (b) ? (a) : (b))

#define	XFS_RTLOBIT(w)	xfs_lowbit32(w)
#define	XFS_RTHIBIT(w)	xfs_highbit32(w)

#if XFS_BIG_BLKNOS
#define	XFS_RTBLOCKLOG(b)	xfs_highbit64(b)
#else
#define	XFS_RTBLOCKLOG(b)	xfs_highbit32(b)
#endif


#ifdef __KERNEL__

#ifdef CONFIG_XFS_RT
/*
 * Function prototypes for exported functions.
 */

/*
 * Allocate an extent in the realtime subvolume, with the usual allocation
 * parameters.  The length units are all in realtime extents, as is the
 * result block number.
 */
int					/* error */
xfs_rtallocate_extent(
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_rtblock_t		bno,	/* starting block number to allocate */
	xfs_extlen_t		minlen,	/* minimum length to allocate */
	xfs_extlen_t		maxlen,	/* maximum length to allocate */
	xfs_extlen_t		*len,	/* out: actual length allocated */
	xfs_alloctype_t		type,	/* allocation type XFS_ALLOCTYPE... */
	int			wasdel,	/* was a delayed allocation extent */
	xfs_extlen_t		prod,	/* extent product factor */
	xfs_rtblock_t		*rtblock); /* out: start block allocated */

/*
 * Free an extent in the realtime subvolume.  Length is expressed in
 * realtime extents, as is the block number.
 */
int					/* error */
xfs_rtfree_extent(
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_rtblock_t		bno,	/* starting block number to free */
	xfs_extlen_t		len);	/* length of extent freed */

/*
 * Initialize realtime fields in the mount structure.
 */
int					/* error */
xfs_rtmount_init(
	struct xfs_mount	*mp);	/* file system mount structure */

/*
 * Get the bitmap and summary inodes into the mount structure
 * at mount time.
 */
int					/* error */
xfs_rtmount_inodes(
	struct xfs_mount	*mp);	/* file system mount structure */

/*
 * Pick an extent for allocation at the start of a new realtime file.
 * Use the sequence number stored in the atime field of the bitmap inode.
 * Translate this to a fraction of the rtextents, and return the product
 * of rtextents and the fraction.
 * The fraction sequence is 0, 1/2, 1/4, 3/4, 1/8, ..., 7/8, 1/16, ...
 */
int					/* error */
xfs_rtpick_extent(
	struct xfs_mount	*mp,	/* file system mount point */
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_extlen_t		len,	/* allocation length (rtextents) */
	xfs_rtblock_t		*pick);	/* result rt extent */

/*
 * Debug code: print out the value of a range in the bitmap.
 */
void
xfs_rtprint_range(
	struct xfs_mount	*mp,	/* file system mount structure */
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_rtblock_t		start,	/* starting block to print */
	xfs_extlen_t		len);	/* length to print */

/*
 * Debug code: print the summary file.
 */
void
xfs_rtprint_summary(
	struct xfs_mount	*mp,	/* file system mount structure */
	struct xfs_trans	*tp);	/* transaction pointer */

/*
 * Grow the realtime area of the filesystem.
 */
int
xfs_growfs_rt(
	struct xfs_mount	*mp,	/* file system mount structure */
	xfs_growfs_rt_t		*in);	/* user supplied growfs struct */

#else
# define xfs_rtallocate_extent(t,b,min,max,l,a,f,p,rb)  (ENOSYS)
# define xfs_rtfree_extent(t,b,l)                       (ENOSYS)
# define xfs_rtpick_extent(m,t,l,rb)                    (ENOSYS)
# define xfs_growfs_rt(mp,in)                           (ENOSYS)
# define xfs_rtmount_init(m)    (((mp)->m_sb.sb_rblocks == 0)? 0 : (ENOSYS))
# define xfs_rtmount_inodes(m)  (((mp)->m_sb.sb_rblocks == 0)? 0 : (ENOSYS))
#endif	/* CONFIG_XFS_RT */

#endif	/* __KERNEL__ */

#endif	/* __XFS_RTALLOC_H__ */
