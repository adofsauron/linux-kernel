/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2005 Silicon Graphics, Inc.  All Rights Reserved.
 */


/*
 * Cross Partition Communication (XPC) partition support.
 *
 *	This is the part of XPC that detects the presence/absence of
 *	other partitions. It provides a heartbeat and monitors the
 *	heartbeats of other partitions.
 *
 */


#include <linux/kernel.h>
#include <linux/sysctl.h>
#include <linux/cache.h>
#include <linux/mmzone.h>
#include <linux/nodemask.h>
#include <asm/uncached.h>
#include <asm/sn/bte.h>
#include <asm/sn/intr.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/addrs.h>
#include "xpc.h"


/* XPC is exiting flag */
int xpc_exiting;


/* SH_IPI_ACCESS shub register value on startup */
static u64 xpc_sh1_IPI_access;
static u64 xpc_sh2_IPI_access0;
static u64 xpc_sh2_IPI_access1;
static u64 xpc_sh2_IPI_access2;
static u64 xpc_sh2_IPI_access3;


/* original protection values for each node */
u64 xpc_prot_vec[MAX_COMPACT_NODES];


/* this partition's reserved page */
struct xpc_rsvd_page *xpc_rsvd_page;

/* this partition's XPC variables (within the reserved page) */
struct xpc_vars *xpc_vars;
struct xpc_vars_part *xpc_vars_part;


/*
 * For performance reasons, each entry of xpc_partitions[] is cacheline
 * aligned. And xpc_partitions[] is padded with an additional entry at the
 * end so that the last legitimate entry doesn't share its cacheline with
 * another variable.
 */
struct xpc_partition xpc_partitions[XP_MAX_PARTITIONS + 1];


/*
 * Generic buffer used to store a local copy of the remote partitions
 * reserved page or XPC variables.
 *
 * xpc_discovery runs only once and is a seperate thread that is
 * very likely going to be processing in parallel with receiving
 * interrupts.
 */
char ____cacheline_aligned
		xpc_remote_copy_buffer[XPC_RSVD_PAGE_ALIGNED_SIZE];


/* systune related variables */
int xpc_hb_interval = XPC_HB_DEFAULT_INTERVAL;
int xpc_hb_check_interval = XPC_HB_CHECK_DEFAULT_TIMEOUT;


/*
 * Given a nasid, get the physical address of the  partition's reserved page
 * for that nasid. This function returns 0 on any error.
 */
static u64
xpc_get_rsvd_page_pa(int nasid, u64 buf, u64 buf_size)
{
	bte_result_t bte_res;
	s64 status;
	u64 cookie = 0;
	u64 rp_pa = nasid;	/* seed with nasid */
	u64 len = 0;


	while (1) {

		status = sn_partition_reserved_page_pa(buf, &cookie, &rp_pa,
								&len);

		dev_dbg(xpc_part, "SAL returned with status=%li, cookie="
			"0x%016lx, address=0x%016lx, len=0x%016lx\n",
			status, cookie, rp_pa, len);

		if (status != SALRET_MORE_PASSES) {
			break;
		}

		if (len > buf_size) {
			dev_err(xpc_part, "len (=0x%016lx) > buf_size\n", len);
			status = SALRET_ERROR;
			break;
		}

		bte_res = xp_bte_copy(rp_pa, ia64_tpa(buf), buf_size,
					(BTE_NOTIFY | BTE_WACQUIRE), NULL);
		if (bte_res != BTE_SUCCESS) {
			dev_dbg(xpc_part, "xp_bte_copy failed %i\n", bte_res);
			status = SALRET_ERROR;
			break;
		}
	}

	if (status != SALRET_OK) {
		rp_pa = 0;
	}
	dev_dbg(xpc_part, "reserved page at phys address 0x%016lx\n", rp_pa);
	return rp_pa;
}


/*
 * Fill the partition reserved page with the information needed by
 * other partitions to discover we are alive and establish initial
 * communications.
 */
struct xpc_rsvd_page *
xpc_rsvd_page_init(void)
{
	struct xpc_rsvd_page *rp;
	AMO_t *amos_page;
	u64 rp_pa, next_cl, nasid_array = 0;
	int i, ret;


	/* get the local reserved page's address */

	rp_pa = xpc_get_rsvd_page_pa(cnodeid_to_nasid(0),
					(u64) xpc_remote_copy_buffer,
						XPC_RSVD_PAGE_ALIGNED_SIZE);
	if (rp_pa == 0) {
		dev_err(xpc_part, "SAL failed to locate the reserved page\n");
		return NULL;
	}
	rp = (struct xpc_rsvd_page *) __va(rp_pa);

	if (rp->partid != sn_partition_id) {
		dev_err(xpc_part, "the reserved page's partid of %d should be "
			"%d\n", rp->partid, sn_partition_id);
		return NULL;
	}

	rp->version = XPC_RP_VERSION;

	/*
	 * Place the XPC variables on the cache line following the
	 * reserved page structure.
	 */
	next_cl = (u64) rp + XPC_RSVD_PAGE_ALIGNED_SIZE;
	xpc_vars = (struct xpc_vars *) next_cl;

	/*
	 * Before clearing xpc_vars, see if a page of AMOs had been previously
	 * allocated. If not we'll need to allocate one and set permissions
	 * so that cross-partition AMOs are allowed.
	 *
	 * The allocated AMO page needs MCA reporting to remain disabled after
	 * XPC has unloaded.  To make this work, we keep a copy of the pointer
	 * to this page (i.e., amos_page) in the struct xpc_vars structure,
	 * which is pointed to by the reserved page, and re-use that saved copy
	 * on subsequent loads of XPC. This AMO page is never freed, and its
	 * memory protections are never restricted.
	 */
	if ((amos_page = xpc_vars->amos_page) == NULL) {
		amos_page = (AMO_t *) TO_AMO(uncached_alloc_page(0));
		if (amos_page == NULL) {
			dev_err(xpc_part, "can't allocate page of AMOs\n");
			return NULL;
		}

		/*
		 * Open up AMO-R/W to cpu.  This is done for Shub 1.1 systems
		 * when xpc_allow_IPI_ops() is called via xpc_hb_init().
		 */
		if (!enable_shub_wars_1_1()) {
			ret = sn_change_memprotect(ia64_tpa((u64) amos_page),
					PAGE_SIZE, SN_MEMPROT_ACCESS_CLASS_1,
					&nasid_array);
			if (ret != 0) {
				dev_err(xpc_part, "can't change memory "
					"protections\n");
				uncached_free_page(__IA64_UNCACHED_OFFSET |
						   TO_PHYS((u64) amos_page));
				return NULL;
			}
		}
	} else if (!IS_AMO_ADDRESS((u64) amos_page)) {
		/*
		 * EFI's XPBOOT can also set amos_page in the reserved page,
		 * but it happens to leave it as an uncached physical address
		 * and we need it to be an uncached virtual, so we'll have to
		 * convert it.
		 */
		if (!IS_AMO_PHYS_ADDRESS((u64) amos_page)) {
			dev_err(xpc_part, "previously used amos_page address "
				"is bad = 0x%p\n", (void *) amos_page);
			return NULL;
		}
		amos_page = (AMO_t *) TO_AMO((u64) amos_page);
	}

	memset(xpc_vars, 0, sizeof(struct xpc_vars));

	/*
	 * Place the XPC per partition specific variables on the cache line
	 * following the XPC variables structure.
	 */
	next_cl += XPC_VARS_ALIGNED_SIZE;
	memset((u64 *) next_cl, 0, sizeof(struct xpc_vars_part) *
							XP_MAX_PARTITIONS);
	xpc_vars_part = (struct xpc_vars_part *) next_cl;
	xpc_vars->vars_part_pa = __pa(next_cl);

	xpc_vars->version = XPC_V_VERSION;
	xpc_vars->act_nasid = cpuid_to_nasid(0);
	xpc_vars->act_phys_cpuid = cpu_physical_id(0);
	xpc_vars->amos_page = amos_page;  /* save for next load of XPC */


	/*
	 * Initialize the activation related AMO variables.
	 */
	xpc_vars->act_amos = xpc_IPI_init(XP_MAX_PARTITIONS);
	for (i = 1; i < XP_NASID_MASK_WORDS; i++) {
		xpc_IPI_init(i + XP_MAX_PARTITIONS);
	}
	/* export AMO page's physical address to other partitions */
	xpc_vars->amos_page_pa = ia64_tpa((u64) xpc_vars->amos_page);

	/*
	 * This signifies to the remote partition that our reserved
	 * page is initialized.
	 */
	rp->vars_pa = __pa(xpc_vars);

	return rp;
}


/*
 * Change protections to allow IPI operations (and AMO operations on
 * Shub 1.1 systems).
 */
void
xpc_allow_IPI_ops(void)
{
	int node;
	int nasid;


	// >>> Change SH_IPI_ACCESS code to use SAL call once it is available.

	if (is_shub2()) {
		xpc_sh2_IPI_access0 =
			(u64) HUB_L((u64 *) LOCAL_MMR_ADDR(SH2_IPI_ACCESS0));
		xpc_sh2_IPI_access1 =
			(u64) HUB_L((u64 *) LOCAL_MMR_ADDR(SH2_IPI_ACCESS1));
		xpc_sh2_IPI_access2 =
			(u64) HUB_L((u64 *) LOCAL_MMR_ADDR(SH2_IPI_ACCESS2));
		xpc_sh2_IPI_access3 =
			(u64) HUB_L((u64 *) LOCAL_MMR_ADDR(SH2_IPI_ACCESS3));

		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS0),
								-1UL);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS1),
								-1UL);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS2),
								-1UL);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS3),
								-1UL);
		}

	} else {
		xpc_sh1_IPI_access =
			(u64) HUB_L((u64 *) LOCAL_MMR_ADDR(SH1_IPI_ACCESS));

		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH1_IPI_ACCESS),
								-1UL);

			/*
			 * Since the BIST collides with memory operations on
			 * SHUB 1.1 sn_change_memprotect() cannot be used.
			 */
			if (enable_shub_wars_1_1()) {
				/* open up everything */
				xpc_prot_vec[node] = (u64) HUB_L((u64 *)
						GLOBAL_MMR_ADDR(nasid,
						SH1_MD_DQLP_MMR_DIR_PRIVEC0));
				HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid,
						SH1_MD_DQLP_MMR_DIR_PRIVEC0),
								-1UL);
				HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid,
						SH1_MD_DQRP_MMR_DIR_PRIVEC0),
								-1UL);
			}
		}
	}
}


/*
 * Restrict protections to disallow IPI operations (and AMO operations on
 * Shub 1.1 systems).
 */
void
xpc_restrict_IPI_ops(void)
{
	int node;
	int nasid;


	// >>> Change SH_IPI_ACCESS code to use SAL call once it is available.

	if (is_shub2()) {

		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS0),
							xpc_sh2_IPI_access0);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS1),
							xpc_sh2_IPI_access1);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS2),
							xpc_sh2_IPI_access2);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS3),
							xpc_sh2_IPI_access3);
		}

	} else {

		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid, SH1_IPI_ACCESS),
							xpc_sh1_IPI_access);

			if (enable_shub_wars_1_1()) {
				HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid,
						SH1_MD_DQLP_MMR_DIR_PRIVEC0),
							xpc_prot_vec[node]);
				HUB_S((u64 *) GLOBAL_MMR_ADDR(nasid,
						SH1_MD_DQRP_MMR_DIR_PRIVEC0),
							xpc_prot_vec[node]);
			}
		}
	}
}


/*
 * At periodic intervals, scan through all active partitions and ensure
 * their heartbeat is still active.  If not, the partition is deactivated.
 */
void
xpc_check_remote_hb(void)
{
	struct xpc_vars *remote_vars;
	struct xpc_partition *part;
	partid_t partid;
	bte_result_t bres;


	remote_vars = (struct xpc_vars *) xpc_remote_copy_buffer;

	for (partid = 1; partid < XP_MAX_PARTITIONS; partid++) {
		if (partid == sn_partition_id) {
			continue;
		}

		part = &xpc_partitions[partid];

		if (part->act_state == XPC_P_INACTIVE ||
				part->act_state == XPC_P_DEACTIVATING) {
			continue;
		}

		/* pull the remote_hb cache line */
		bres = xp_bte_copy(part->remote_vars_pa,
					ia64_tpa((u64) remote_vars),
					XPC_VARS_ALIGNED_SIZE,
					(BTE_NOTIFY | BTE_WACQUIRE), NULL);
		if (bres != BTE_SUCCESS) {
			XPC_DEACTIVATE_PARTITION(part,
						xpc_map_bte_errors(bres));
			continue;
		}

		dev_dbg(xpc_part, "partid = %d, heartbeat = %ld, last_heartbeat"
			" = %ld, kdb_status = %ld, HB_mask = 0x%lx\n", partid,
			remote_vars->heartbeat, part->last_heartbeat,
			remote_vars->kdb_status,
			remote_vars->heartbeating_to_mask);

		if (((remote_vars->heartbeat == part->last_heartbeat) &&
			(remote_vars->kdb_status == 0)) ||
			     !XPC_HB_ALLOWED(sn_partition_id, remote_vars)) {

			XPC_DEACTIVATE_PARTITION(part, xpcNoHeartbeat);
			continue;
		}

		part->last_heartbeat = remote_vars->heartbeat;
	}
}


/*
 * Get a copy of the remote partition's rsvd page.
 *
 * remote_rp points to a buffer that is cacheline aligned for BTE copies and
 * assumed to be of size XPC_RSVD_PAGE_ALIGNED_SIZE.
 */
static enum xpc_retval
xpc_get_remote_rp(int nasid, u64 *discovered_nasids,
		struct xpc_rsvd_page *remote_rp, u64 *remote_rsvd_page_pa)
{
	int bres, i;


	/* get the reserved page's physical address */

	*remote_rsvd_page_pa = xpc_get_rsvd_page_pa(nasid, (u64) remote_rp,
						XPC_RSVD_PAGE_ALIGNED_SIZE);
	if (*remote_rsvd_page_pa == 0) {
		return xpcNoRsvdPageAddr;
	}


	/* pull over the reserved page structure */

	bres = xp_bte_copy(*remote_rsvd_page_pa, ia64_tpa((u64) remote_rp),
				XPC_RSVD_PAGE_ALIGNED_SIZE,
				(BTE_NOTIFY | BTE_WACQUIRE), NULL);
	if (bres != BTE_SUCCESS) {
		return xpc_map_bte_errors(bres);
	}


	if (discovered_nasids != NULL) {
		for (i = 0; i < XP_NASID_MASK_WORDS; i++) {
			discovered_nasids[i] |= remote_rp->part_nasids[i];
		}
	}


	/* check that the partid is for another partition */

	if (remote_rp->partid < 1 ||
				remote_rp->partid > (XP_MAX_PARTITIONS - 1)) {
		return xpcInvalidPartid;
	}

	if (remote_rp->partid == sn_partition_id) {
		return xpcLocalPartid;
	}


	if (XPC_VERSION_MAJOR(remote_rp->version) !=
					XPC_VERSION_MAJOR(XPC_RP_VERSION)) {
		return xpcBadVersion;
	}

	return xpcSuccess;
}


/*
 * Get a copy of the remote partition's XPC variables.
 *
 * remote_vars points to a buffer that is cacheline aligned for BTE copies and
 * assumed to be of size XPC_VARS_ALIGNED_SIZE.
 */
static enum xpc_retval
xpc_get_remote_vars(u64 remote_vars_pa, struct xpc_vars *remote_vars)
{
	int bres;


	if (remote_vars_pa == 0) {
		return xpcVarsNotSet;
	}


	/* pull over the cross partition variables */

	bres = xp_bte_copy(remote_vars_pa, ia64_tpa((u64) remote_vars),
				XPC_VARS_ALIGNED_SIZE,
				(BTE_NOTIFY | BTE_WACQUIRE), NULL);
	if (bres != BTE_SUCCESS) {
		return xpc_map_bte_errors(bres);
	}

	if (XPC_VERSION_MAJOR(remote_vars->version) !=
					XPC_VERSION_MAJOR(XPC_V_VERSION)) {
		return xpcBadVersion;
	}

	return xpcSuccess;
}


/*
 * Prior code has determine the nasid which generated an IPI.  Inspect
 * that nasid to determine if its partition needs to be activated or
 * deactivated.
 *
 * A partition is consider "awaiting activation" if our partition
 * flags indicate it is not active and it has a heartbeat.  A
 * partition is considered "awaiting deactivation" if our partition
 * flags indicate it is active but it has no heartbeat or it is not
 * sending its heartbeat to us.
 *
 * To determine the heartbeat, the remote nasid must have a properly
 * initialized reserved page.
 */
static void
xpc_identify_act_IRQ_req(int nasid)
{
	struct xpc_rsvd_page *remote_rp;
	struct xpc_vars *remote_vars;
	u64 remote_rsvd_page_pa;
	u64 remote_vars_pa;
	partid_t partid;
	struct xpc_partition *part;
	enum xpc_retval ret;


	/* pull over the reserved page structure */

	remote_rp = (struct xpc_rsvd_page *) xpc_remote_copy_buffer;

	ret = xpc_get_remote_rp(nasid, NULL, remote_rp, &remote_rsvd_page_pa);
	if (ret != xpcSuccess) {
		dev_warn(xpc_part, "unable to get reserved page from nasid %d, "
			"which sent interrupt, reason=%d\n", nasid, ret);
		return;
	}

	remote_vars_pa = remote_rp->vars_pa;
	partid = remote_rp->partid;
	part = &xpc_partitions[partid];


	/* pull over the cross partition variables */

	remote_vars = (struct xpc_vars *) xpc_remote_copy_buffer;

	ret = xpc_get_remote_vars(remote_vars_pa, remote_vars);
	if (ret != xpcSuccess) {

		dev_warn(xpc_part, "unable to get XPC variables from nasid %d, "
			"which sent interrupt, reason=%d\n", nasid, ret);

		XPC_DEACTIVATE_PARTITION(part, ret);
		return;
	}


	part->act_IRQ_rcvd++;

	dev_dbg(xpc_part, "partid for nasid %d is %d; IRQs = %d; HB = "
		"%ld:0x%lx\n", (int) nasid, (int) partid, part->act_IRQ_rcvd,
		remote_vars->heartbeat, remote_vars->heartbeating_to_mask);


	if (part->act_state == XPC_P_INACTIVE) {

		part->remote_rp_pa = remote_rsvd_page_pa;
		dev_dbg(xpc_part, "  remote_rp_pa = 0x%016lx\n",
			part->remote_rp_pa);

		part->remote_vars_pa = remote_vars_pa;
		dev_dbg(xpc_part, "  remote_vars_pa = 0x%016lx\n",
			part->remote_vars_pa);

		part->last_heartbeat = remote_vars->heartbeat;
		dev_dbg(xpc_part, "  last_heartbeat = 0x%016lx\n",
			part->last_heartbeat);

		part->remote_vars_part_pa = remote_vars->vars_part_pa;
		dev_dbg(xpc_part, "  remote_vars_part_pa = 0x%016lx\n",
			part->remote_vars_part_pa);

		part->remote_act_nasid = remote_vars->act_nasid;
		dev_dbg(xpc_part, "  remote_act_nasid = 0x%x\n",
			part->remote_act_nasid);

		part->remote_act_phys_cpuid = remote_vars->act_phys_cpuid;
		dev_dbg(xpc_part, "  remote_act_phys_cpuid = 0x%x\n",
			part->remote_act_phys_cpuid);

		part->remote_amos_page_pa = remote_vars->amos_page_pa;
		dev_dbg(xpc_part, "  remote_amos_page_pa = 0x%lx\n",
			part->remote_amos_page_pa);

		xpc_activate_partition(part);

	} else if (part->remote_amos_page_pa != remote_vars->amos_page_pa ||
			!XPC_HB_ALLOWED(sn_partition_id, remote_vars)) {

		part->reactivate_nasid = nasid;
		XPC_DEACTIVATE_PARTITION(part, xpcReactivating);
	}
}


/*
 * Loop through the activation AMO variables and process any bits
 * which are set.  Each bit indicates a nasid sending a partition
 * activation or deactivation request.
 *
 * Return #of IRQs detected.
 */
int
xpc_identify_act_IRQ_sender(void)
{
	int word, bit;
	u64 nasid_mask;
	u64 nasid;			/* remote nasid */
	int n_IRQs_detected = 0;
	AMO_t *act_amos;
	struct xpc_rsvd_page *rp = (struct xpc_rsvd_page *) xpc_rsvd_page;


	act_amos = xpc_vars->act_amos;


	/* scan through act AMO variable looking for non-zero entries */
	for (word = 0; word < XP_NASID_MASK_WORDS; word++) {

		nasid_mask = xpc_IPI_receive(&act_amos[word]);
		if (nasid_mask == 0) {
			/* no IRQs from nasids in this variable */
			continue;
		}

		dev_dbg(xpc_part, "AMO[%d] gave back 0x%lx\n", word,
			nasid_mask);


		/*
		 * If this nasid has been added to the machine since
		 * our partition was reset, this will retain the
		 * remote nasid in our reserved pages machine mask.
		 * This is used in the event of module reload.
		 */
		rp->mach_nasids[word] |= nasid_mask;


		/* locate the nasid(s) which sent interrupts */

		for (bit = 0; bit < (8 * sizeof(u64)); bit++) {
			if (nasid_mask & (1UL << bit)) {
				n_IRQs_detected++;
				nasid = XPC_NASID_FROM_W_B(word, bit);
				dev_dbg(xpc_part, "interrupt from nasid %ld\n",
					nasid);
				xpc_identify_act_IRQ_req(nasid);
			}
		}
	}
	return n_IRQs_detected;
}


/*
 * Mark specified partition as active.
 */
enum xpc_retval
xpc_mark_partition_active(struct xpc_partition *part)
{
	unsigned long irq_flags;
	enum xpc_retval ret;


	dev_dbg(xpc_part, "setting partition %d to ACTIVE\n", XPC_PARTID(part));

	spin_lock_irqsave(&part->act_lock, irq_flags);
	if (part->act_state == XPC_P_ACTIVATING) {
		part->act_state = XPC_P_ACTIVE;
		ret = xpcSuccess;
	} else {
		DBUG_ON(part->reason == xpcSuccess);
		ret = part->reason;
	}
	spin_unlock_irqrestore(&part->act_lock, irq_flags);

	return ret;
}


/*
 * Notify XPC that the partition is down.
 */
void
xpc_deactivate_partition(const int line, struct xpc_partition *part,
				enum xpc_retval reason)
{
	unsigned long irq_flags;
	partid_t partid = XPC_PARTID(part);


	spin_lock_irqsave(&part->act_lock, irq_flags);

	if (part->act_state == XPC_P_INACTIVE) {
		XPC_SET_REASON(part, reason, line);
		spin_unlock_irqrestore(&part->act_lock, irq_flags);
		if (reason == xpcReactivating) {
			/* we interrupt ourselves to reactivate partition */
			xpc_IPI_send_reactivate(part);
		}
		return;
	}
	if (part->act_state == XPC_P_DEACTIVATING) {
		if ((part->reason == xpcUnloading && reason != xpcUnloading) ||
					reason == xpcReactivating) {
			XPC_SET_REASON(part, reason, line);
		}
		spin_unlock_irqrestore(&part->act_lock, irq_flags);
		return;
	}

	part->act_state = XPC_P_DEACTIVATING;
	XPC_SET_REASON(part, reason, line);

	spin_unlock_irqrestore(&part->act_lock, irq_flags);

	XPC_DISALLOW_HB(partid, xpc_vars);

	dev_dbg(xpc_part, "bringing partition %d down, reason = %d\n", partid,
		reason);

	xpc_partition_down(part, reason);
}


/*
 * Mark specified partition as active.
 */
void
xpc_mark_partition_inactive(struct xpc_partition *part)
{
	unsigned long irq_flags;


	dev_dbg(xpc_part, "setting partition %d to INACTIVE\n",
		XPC_PARTID(part));

	spin_lock_irqsave(&part->act_lock, irq_flags);
	part->act_state = XPC_P_INACTIVE;
	spin_unlock_irqrestore(&part->act_lock, irq_flags);
	part->remote_rp_pa = 0;
}


/*
 * SAL has provided a partition and machine mask.  The partition mask
 * contains a bit for each even nasid in our partition.  The machine
 * mask contains a bit for each even nasid in the entire machine.
 *
 * Using those two bit arrays, we can determine which nasids are
 * known in the machine.  Each should also have a reserved page
 * initialized if they are available for partitioning.
 */
void
xpc_discovery(void)
{
	void *remote_rp_base;
	struct xpc_rsvd_page *remote_rp;
	struct xpc_vars *remote_vars;
	u64 remote_rsvd_page_pa;
	u64 remote_vars_pa;
	int region;
	int max_regions;
	int nasid;
	struct xpc_rsvd_page *rp;
	partid_t partid;
	struct xpc_partition *part;
	u64 *discovered_nasids;
	enum xpc_retval ret;


	remote_rp = xpc_kmalloc_cacheline_aligned(XPC_RSVD_PAGE_ALIGNED_SIZE,
						GFP_KERNEL, &remote_rp_base);
	if (remote_rp == NULL) {
		return;
	}
	remote_vars = (struct xpc_vars *) remote_rp;


	discovered_nasids = kmalloc(sizeof(u64) * XP_NASID_MASK_WORDS,
							GFP_KERNEL);
	if (discovered_nasids == NULL) {
		kfree(remote_rp_base);
		return;
	}
	memset(discovered_nasids, 0, sizeof(u64) * XP_NASID_MASK_WORDS);

	rp = (struct xpc_rsvd_page *) xpc_rsvd_page;

	/*
	 * The term 'region' in this context refers to the minimum number of
	 * nodes that can comprise an access protection grouping. The access
	 * protection is in regards to memory, IOI and IPI.
	 */
//>>> move the next two #defines into either include/asm-ia64/sn/arch.h or
//>>> include/asm-ia64/sn/addrs.h
#define SH1_MAX_REGIONS		64
#define SH2_MAX_REGIONS		256
	max_regions = is_shub2() ? SH2_MAX_REGIONS : SH1_MAX_REGIONS;

	for (region = 0; region < max_regions; region++) {

		if ((volatile int) xpc_exiting) {
			break;
		}

		dev_dbg(xpc_part, "searching region %d\n", region);

		for (nasid = (region * sn_region_size * 2);
		     nasid < ((region + 1) * sn_region_size * 2);
		     nasid += 2) {

			if ((volatile int) xpc_exiting) {
				break;
			}

			dev_dbg(xpc_part, "checking nasid %d\n", nasid);


			if (XPC_NASID_IN_ARRAY(nasid, rp->part_nasids)) {
				dev_dbg(xpc_part, "PROM indicates Nasid %d is "
					"part of the local partition; skipping "
					"region\n", nasid);
				break;
			}

			if (!(XPC_NASID_IN_ARRAY(nasid, rp->mach_nasids))) {
				dev_dbg(xpc_part, "PROM indicates Nasid %d was "
					"not on Numa-Link network at reset\n",
					nasid);
				continue;
			}

			if (XPC_NASID_IN_ARRAY(nasid, discovered_nasids)) {
				dev_dbg(xpc_part, "Nasid %d is part of a "
					"partition which was previously "
					"discovered\n", nasid);
				continue;
			}


			/* pull over the reserved page structure */

			ret = xpc_get_remote_rp(nasid, discovered_nasids,
					      remote_rp, &remote_rsvd_page_pa);
			if (ret != xpcSuccess) {
				dev_dbg(xpc_part, "unable to get reserved page "
					"from nasid %d, reason=%d\n", nasid,
					ret);

				if (ret == xpcLocalPartid) {
					break;
				}
				continue;
			}

			remote_vars_pa = remote_rp->vars_pa;

			partid = remote_rp->partid;
			part = &xpc_partitions[partid];


			/* pull over the cross partition variables */

			ret = xpc_get_remote_vars(remote_vars_pa, remote_vars);
			if (ret != xpcSuccess) {
				dev_dbg(xpc_part, "unable to get XPC variables "
					"from nasid %d, reason=%d\n", nasid,
					ret);

				XPC_DEACTIVATE_PARTITION(part, ret);
				continue;
			}

			if (part->act_state != XPC_P_INACTIVE) {
				dev_dbg(xpc_part, "partition %d on nasid %d is "
					"already activating\n", partid, nasid);
				break;
			}

			/*
			 * Register the remote partition's AMOs with SAL so it
			 * can handle and cleanup errors within that address
			 * range should the remote partition go down. We don't
			 * unregister this range because it is difficult to
			 * tell when outstanding writes to the remote partition
			 * are finished and thus when it is thus safe to
			 * unregister. This should not result in wasted space
			 * in the SAL xp_addr_region table because we should
			 * get the same page for remote_act_amos_pa after
			 * module reloads and system reboots.
			 */
			if (sn_register_xp_addr_region(
					    remote_vars->amos_page_pa,
							PAGE_SIZE, 1) < 0) {
				dev_dbg(xpc_part, "partition %d failed to "
					"register xp_addr region 0x%016lx\n",
					partid, remote_vars->amos_page_pa);

				XPC_SET_REASON(part, xpcPhysAddrRegFailed,
						__LINE__);
				break;
			}

			/*
			 * The remote nasid is valid and available.
			 * Send an interrupt to that nasid to notify
			 * it that we are ready to begin activation.
			 */
			dev_dbg(xpc_part, "sending an interrupt to AMO 0x%lx, "
				"nasid %d, phys_cpuid 0x%x\n",
				remote_vars->amos_page_pa,
				remote_vars->act_nasid,
				remote_vars->act_phys_cpuid);

			xpc_IPI_send_activate(remote_vars);
		}
	}

	kfree(discovered_nasids);
	kfree(remote_rp_base);
}


/*
 * Given a partid, get the nasids owned by that partition from the
 * remote partition's reserved page.
 */
enum xpc_retval
xpc_initiate_partid_to_nasids(partid_t partid, void *nasid_mask)
{
	struct xpc_partition *part;
	u64 part_nasid_pa;
	int bte_res;


	part = &xpc_partitions[partid];
	if (part->remote_rp_pa == 0) {
		return xpcPartitionDown;
	}

	part_nasid_pa = part->remote_rp_pa +
		(u64) &((struct xpc_rsvd_page *) 0)->part_nasids;

	bte_res = xp_bte_copy(part_nasid_pa, ia64_tpa((u64) nasid_mask),
				L1_CACHE_ALIGN(XP_NASID_MASK_BYTES),
				(BTE_NOTIFY | BTE_WACQUIRE), NULL);

	return xpc_map_bte_errors(bte_res);
}

