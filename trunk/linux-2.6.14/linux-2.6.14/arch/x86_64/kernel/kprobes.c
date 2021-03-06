/*
 *  Kernel Probes (KProbes)
 *  arch/x86_64/kernel/kprobes.c
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) IBM Corporation, 2002, 2004
 *
 * 2002-Oct	Created by Vamsi Krishna S <vamsi_krishna@in.ibm.com> Kernel
 *		Probes initial implementation ( includes contributions from
 *		Rusty Russell).
 * 2004-July	Suparna Bhattacharya <suparna@in.ibm.com> added jumper probes
 *		interface to access function arguments.
 * 2004-Oct	Jim Keniston <kenistoj@us.ibm.com> and Prasanna S Panchamukhi
 *		<prasanna@in.ibm.com> adapted for x86_64
 * 2005-Mar	Roland McGrath <roland@redhat.com>
 *		Fixed to handle %rip-relative addressing mode correctly.
 * 2005-May     Rusty Lynch <rusty.lynch@intel.com>
 *              Added function return probes functionality
 */

#include <linux/config.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/preempt.h>

#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <asm/kdebug.h>

static DECLARE_MUTEX(kprobe_mutex);

static struct kprobe *current_kprobe;
static unsigned long kprobe_status, kprobe_old_rflags, kprobe_saved_rflags;
static struct kprobe *kprobe_prev;
static unsigned long kprobe_status_prev, kprobe_old_rflags_prev, kprobe_saved_rflags_prev;
static struct pt_regs jprobe_saved_regs;
static long *jprobe_saved_rsp;
void jprobe_return_end(void);

/* copy of the kernel stack at the probe fire time */
static kprobe_opcode_t jprobes_stack[MAX_STACK_SIZE];

/*
 * returns non-zero if opcode modifies the interrupt flag.
 */
static inline int is_IF_modifier(kprobe_opcode_t *insn)
{
	switch (*insn) {
	case 0xfa:		/* cli */
	case 0xfb:		/* sti */
	case 0xcf:		/* iret/iretd */
	case 0x9d:		/* popf/popfd */
		return 1;
	}

	if (*insn  >= 0x40 && *insn <= 0x4f && *++insn == 0xcf)
		return 1;
	return 0;
}

int __kprobes arch_prepare_kprobe(struct kprobe *p)
{
	/* insn: must be on special executable page on x86_64. */
	down(&kprobe_mutex);
	p->ainsn.insn = get_insn_slot();
	up(&kprobe_mutex);
	if (!p->ainsn.insn) {
		return -ENOMEM;
	}
	return 0;
}

/*
 * Determine if the instruction uses the %rip-relative addressing mode.
 * If it does, return the address of the 32-bit displacement word.
 * If not, return null.
 */
static inline s32 *is_riprel(u8 *insn)
{
#define W(row,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9,ba,bb,bc,bd,be,bf)		      \
	(((b0##UL << 0x0)|(b1##UL << 0x1)|(b2##UL << 0x2)|(b3##UL << 0x3) |   \
	  (b4##UL << 0x4)|(b5##UL << 0x5)|(b6##UL << 0x6)|(b7##UL << 0x7) |   \
	  (b8##UL << 0x8)|(b9##UL << 0x9)|(ba##UL << 0xa)|(bb##UL << 0xb) |   \
	  (bc##UL << 0xc)|(bd##UL << 0xd)|(be##UL << 0xe)|(bf##UL << 0xf))    \
	 << (row % 64))
	static const u64 onebyte_has_modrm[256 / 64] = {
		/*      0 1 2 3 4 5 6 7 8 9 a b c d e f         */
		/*      -------------------------------         */
		W(0x00, 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0)| /* 00 */
		W(0x10, 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0)| /* 10 */
		W(0x20, 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0)| /* 20 */
		W(0x30, 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0), /* 30 */
		W(0x40, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)| /* 40 */
		W(0x50, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)| /* 50 */
		W(0x60, 0,0,1,1,0,0,0,0,0,1,0,1,0,0,0,0)| /* 60 */
		W(0x70, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0), /* 70 */
		W(0x80, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)| /* 80 */
		W(0x90, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)| /* 90 */
		W(0xa0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)| /* a0 */
		W(0xb0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0), /* b0 */
		W(0xc0, 1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0)| /* c0 */
		W(0xd0, 1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1)| /* d0 */
		W(0xe0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)| /* e0 */
		W(0xf0, 0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1)  /* f0 */
		/*      -------------------------------         */
		/*      0 1 2 3 4 5 6 7 8 9 a b c d e f         */
	};
	static const u64 twobyte_has_modrm[256 / 64] = {
		/*      0 1 2 3 4 5 6 7 8 9 a b c d e f         */
		/*      -------------------------------         */
		W(0x00, 1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,1)| /* 0f */
		W(0x10, 1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0)| /* 1f */
		W(0x20, 1,1,1,1,1,0,1,0,1,1,1,1,1,1,1,1)| /* 2f */
		W(0x30, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0), /* 3f */
		W(0x40, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)| /* 4f */
		W(0x50, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)| /* 5f */
		W(0x60, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)| /* 6f */
		W(0x70, 1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1), /* 7f */
		W(0x80, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)| /* 8f */
		W(0x90, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)| /* 9f */
		W(0xa0, 0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1)| /* af */
		W(0xb0, 1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1), /* bf */
		W(0xc0, 1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0)| /* cf */
		W(0xd0, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)| /* df */
		W(0xe0, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)| /* ef */
		W(0xf0, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0)  /* ff */
		/*      -------------------------------         */
		/*      0 1 2 3 4 5 6 7 8 9 a b c d e f         */
	};
#undef	W
	int need_modrm;

	/* Skip legacy instruction prefixes.  */
	while (1) {
		switch (*insn) {
		case 0x66:
		case 0x67:
		case 0x2e:
		case 0x3e:
		case 0x26:
		case 0x64:
		case 0x65:
		case 0x36:
		case 0xf0:
		case 0xf3:
		case 0xf2:
			++insn;
			continue;
		}
		break;
	}

	/* Skip REX instruction prefix.  */
	if ((*insn & 0xf0) == 0x40)
		++insn;

	if (*insn == 0x0f) {	/* Two-byte opcode.  */
		++insn;
		need_modrm = test_bit(*insn, twobyte_has_modrm);
	} else {		/* One-byte opcode.  */
		need_modrm = test_bit(*insn, onebyte_has_modrm);
	}

	if (need_modrm) {
		u8 modrm = *++insn;
		if ((modrm & 0xc7) == 0x05) { /* %rip+disp32 addressing mode */
			/* Displacement follows ModRM byte.  */
			return (s32 *) ++insn;
		}
	}

	/* No %rip-relative addressing mode here.  */
	return NULL;
}

void __kprobes arch_copy_kprobe(struct kprobe *p)
{
	s32 *ripdisp;
	memcpy(p->ainsn.insn, p->addr, MAX_INSN_SIZE);
	ripdisp = is_riprel(p->ainsn.insn);
	if (ripdisp) {
		/*
		 * The copied instruction uses the %rip-relative
		 * addressing mode.  Adjust the displacement for the
		 * difference between the original location of this
		 * instruction and the location of the copy that will
		 * actually be run.  The tricky bit here is making sure
		 * that the sign extension happens correctly in this
		 * calculation, since we need a signed 32-bit result to
		 * be sign-extended to 64 bits when it's added to the
		 * %rip value and yield the same 64-bit result that the
		 * sign-extension of the original signed 32-bit
		 * displacement would have given.
		 */
		s64 disp = (u8 *) p->addr + *ripdisp - (u8 *) p->ainsn.insn;
		BUG_ON((s64) (s32) disp != disp); /* Sanity check.  */
		*ripdisp = disp;
	}
	p->opcode = *p->addr;
}

void __kprobes arch_arm_kprobe(struct kprobe *p)
{
	*p->addr = BREAKPOINT_INSTRUCTION;
	flush_icache_range((unsigned long) p->addr,
			   (unsigned long) p->addr + sizeof(kprobe_opcode_t));
}

void __kprobes arch_disarm_kprobe(struct kprobe *p)
{
	*p->addr = p->opcode;
	flush_icache_range((unsigned long) p->addr,
			   (unsigned long) p->addr + sizeof(kprobe_opcode_t));
}

void __kprobes arch_remove_kprobe(struct kprobe *p)
{
	down(&kprobe_mutex);
	free_insn_slot(p->ainsn.insn);
	up(&kprobe_mutex);
}

static inline void save_previous_kprobe(void)
{
	kprobe_prev = current_kprobe;
	kprobe_status_prev = kprobe_status;
	kprobe_old_rflags_prev = kprobe_old_rflags;
	kprobe_saved_rflags_prev = kprobe_saved_rflags;
}

static inline void restore_previous_kprobe(void)
{
	current_kprobe = kprobe_prev;
	kprobe_status = kprobe_status_prev;
	kprobe_old_rflags = kprobe_old_rflags_prev;
	kprobe_saved_rflags = kprobe_saved_rflags_prev;
}

static inline void set_current_kprobe(struct kprobe *p, struct pt_regs *regs)
{
	current_kprobe = p;
	kprobe_saved_rflags = kprobe_old_rflags
		= (regs->eflags & (TF_MASK | IF_MASK));
	if (is_IF_modifier(p->ainsn.insn))
		kprobe_saved_rflags &= ~IF_MASK;
}

static void __kprobes prepare_singlestep(struct kprobe *p, struct pt_regs *regs)
{
	regs->eflags |= TF_MASK;
	regs->eflags &= ~IF_MASK;
	/*single step inline if the instruction is an int3*/
	if (p->opcode == BREAKPOINT_INSTRUCTION)
		regs->rip = (unsigned long)p->addr;
	else
		regs->rip = (unsigned long)p->ainsn.insn;
}

void __kprobes arch_prepare_kretprobe(struct kretprobe *rp,
				      struct pt_regs *regs)
{
	unsigned long *sara = (unsigned long *)regs->rsp;
        struct kretprobe_instance *ri;

        if ((ri = get_free_rp_inst(rp)) != NULL) {
                ri->rp = rp;
                ri->task = current;
		ri->ret_addr = (kprobe_opcode_t *) *sara;

		/* Replace the return addr with trampoline addr */
		*sara = (unsigned long) &kretprobe_trampoline;

                add_rp_inst(ri);
        } else {
                rp->nmissed++;
        }
}

/*
 * Interrupts are disabled on entry as trap3 is an interrupt gate and they
 * remain disabled thorough out this function.
 */
int __kprobes kprobe_handler(struct pt_regs *regs)
{
	struct kprobe *p;
	int ret = 0;
	kprobe_opcode_t *addr = (kprobe_opcode_t *)(regs->rip - sizeof(kprobe_opcode_t));

	/* We're in an interrupt, but this is clear and BUG()-safe. */
	preempt_disable();

	/* Check we're not actually recursing */
	if (kprobe_running()) {
		/* We *are* holding lock here, so this is safe.
		   Disarm the probe we just hit, and ignore it. */
		p = get_kprobe(addr);
		if (p) {
			if (kprobe_status == KPROBE_HIT_SS &&
				*p->ainsn.insn == BREAKPOINT_INSTRUCTION) {
				regs->eflags &= ~TF_MASK;
				regs->eflags |= kprobe_saved_rflags;
				unlock_kprobes();
				goto no_kprobe;
			} else if (kprobe_status == KPROBE_HIT_SSDONE) {
				/* TODO: Provide re-entrancy from
				 * post_kprobes_handler() and avoid exception
				 * stack corruption while single-stepping on
				 * the instruction of the new probe.
				 */
				arch_disarm_kprobe(p);
				regs->rip = (unsigned long)p->addr;
				ret = 1;
			} else {
				/* We have reentered the kprobe_handler(), since
				 * another probe was hit while within the
				 * handler. We here save the original kprobe
				 * variables and just single step on instruction
				 * of the new probe without calling any user
				 * handlers.
				 */
				save_previous_kprobe();
				set_current_kprobe(p, regs);
				p->nmissed++;
				prepare_singlestep(p, regs);
				kprobe_status = KPROBE_REENTER;
				return 1;
			}
		} else {
			p = current_kprobe;
			if (p->break_handler && p->break_handler(p, regs)) {
				goto ss_probe;
			}
		}
		/* If it's not ours, can't be delete race, (we hold lock). */
		goto no_kprobe;
	}

	lock_kprobes();
	p = get_kprobe(addr);
	if (!p) {
		unlock_kprobes();
		if (*addr != BREAKPOINT_INSTRUCTION) {
			/*
			 * The breakpoint instruction was removed right
			 * after we hit it.  Another cpu has removed
			 * either a probepoint or a debugger breakpoint
			 * at this address.  In either case, no further
			 * handling of this interrupt is appropriate.
			 * Back up over the (now missing) int3 and run
			 * the original instruction.
			 */
			regs->rip = (unsigned long)addr;
			ret = 1;
		}
		/* Not one of ours: let kernel handle it */
		goto no_kprobe;
	}

	kprobe_status = KPROBE_HIT_ACTIVE;
	set_current_kprobe(p, regs);

	if (p->pre_handler && p->pre_handler(p, regs))
		/* handler has already set things up, so skip ss setup */
		return 1;

ss_probe:
	prepare_singlestep(p, regs);
	kprobe_status = KPROBE_HIT_SS;
	return 1;

no_kprobe:
	preempt_enable_no_resched();
	return ret;
}

/*
 * For function-return probes, init_kprobes() establishes a probepoint
 * here. When a retprobed function returns, this probe is hit and
 * trampoline_probe_handler() runs, calling the kretprobe's handler.
 */
 void kretprobe_trampoline_holder(void)
 {
 	asm volatile (  ".global kretprobe_trampoline\n"
 			"kretprobe_trampoline: \n"
 			"nop\n");
 }

/*
 * Called when we hit the probe point at kretprobe_trampoline
 */
int __kprobes trampoline_probe_handler(struct kprobe *p, struct pt_regs *regs)
{
        struct kretprobe_instance *ri = NULL;
        struct hlist_head *head;
        struct hlist_node *node, *tmp;
	unsigned long orig_ret_address = 0;
	unsigned long trampoline_address =(unsigned long)&kretprobe_trampoline;

        head = kretprobe_inst_table_head(current);

	/*
	 * It is possible to have multiple instances associated with a given
	 * task either because an multiple functions in the call path
	 * have a return probe installed on them, and/or more then one return
	 * return probe was registered for a target function.
	 *
	 * We can handle this because:
	 *     - instances are always inserted at the head of the list
	 *     - when multiple return probes are registered for the same
         *       function, the first instance's ret_addr will point to the
	 *       real return address, and all the rest will point to
	 *       kretprobe_trampoline
	 */
	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
                if (ri->task != current)
			/* another task is sharing our hash bucket */
                        continue;

		if (ri->rp && ri->rp->handler)
			ri->rp->handler(ri, regs);

		orig_ret_address = (unsigned long)ri->ret_addr;
		recycle_rp_inst(ri);

		if (orig_ret_address != trampoline_address)
			/*
			 * This is the real return address. Any other
			 * instances associated with this task are for
			 * other calls deeper on the call stack
			 */
			break;
	}

	BUG_ON(!orig_ret_address || (orig_ret_address == trampoline_address));
	regs->rip = orig_ret_address;

	unlock_kprobes();
	preempt_enable_no_resched();

        /*
         * By returning a non-zero value, we are telling
         * kprobe_handler() that we have handled unlocking
         * and re-enabling preemption.
         */
        return 1;
}

/*
 * Called after single-stepping.  p->addr is the address of the
 * instruction whose first byte has been replaced by the "int 3"
 * instruction.  To avoid the SMP problems that can occur when we
 * temporarily put back the original opcode to single-step, we
 * single-stepped a copy of the instruction.  The address of this
 * copy is p->ainsn.insn.
 *
 * This function prepares to return from the post-single-step
 * interrupt.  We have to fix up the stack as follows:
 *
 * 0) Except in the case of absolute or indirect jump or call instructions,
 * the new rip is relative to the copied instruction.  We need to make
 * it relative to the original instruction.
 *
 * 1) If the single-stepped instruction was pushfl, then the TF and IF
 * flags are set in the just-pushed eflags, and may need to be cleared.
 *
 * 2) If the single-stepped instruction was a call, the return address
 * that is atop the stack is the address following the copied instruction.
 * We need to make it the address following the original instruction.
 */
static void __kprobes resume_execution(struct kprobe *p, struct pt_regs *regs)
{
	unsigned long *tos = (unsigned long *)regs->rsp;
	unsigned long next_rip = 0;
	unsigned long copy_rip = (unsigned long)p->ainsn.insn;
	unsigned long orig_rip = (unsigned long)p->addr;
	kprobe_opcode_t *insn = p->ainsn.insn;

	/*skip the REX prefix*/
	if (*insn >= 0x40 && *insn <= 0x4f)
		insn++;

	switch (*insn) {
	case 0x9c:		/* pushfl */
		*tos &= ~(TF_MASK | IF_MASK);
		*tos |= kprobe_old_rflags;
		break;
	case 0xc3:		/* ret/lret */
	case 0xcb:
	case 0xc2:
	case 0xca:
		regs->eflags &= ~TF_MASK;
		/* rip is already adjusted, no more changes required*/
		return;
	case 0xe8:		/* call relative - Fix return addr */
		*tos = orig_rip + (*tos - copy_rip);
		break;
	case 0xff:
		if ((*insn & 0x30) == 0x10) {
			/* call absolute, indirect */
			/* Fix return addr; rip is correct. */
			next_rip = regs->rip;
			*tos = orig_rip + (*tos - copy_rip);
		} else if (((*insn & 0x31) == 0x20) ||	/* jmp near, absolute indirect */
			   ((*insn & 0x31) == 0x21)) {	/* jmp far, absolute indirect */
			/* rip is correct. */
			next_rip = regs->rip;
		}
		break;
	case 0xea:		/* jmp absolute -- rip is correct */
		next_rip = regs->rip;
		break;
	default:
		break;
	}

	regs->eflags &= ~TF_MASK;
	if (next_rip) {
		regs->rip = next_rip;
	} else {
		regs->rip = orig_rip + (regs->rip - copy_rip);
	}
}

/*
 * Interrupts are disabled on entry as trap1 is an interrupt gate and they
 * remain disabled thoroughout this function.  And we hold kprobe lock.
 */
int __kprobes post_kprobe_handler(struct pt_regs *regs)
{
	if (!kprobe_running())
		return 0;

	if ((kprobe_status != KPROBE_REENTER) && current_kprobe->post_handler) {
		kprobe_status = KPROBE_HIT_SSDONE;
		current_kprobe->post_handler(current_kprobe, regs, 0);
	}

	resume_execution(current_kprobe, regs);
	regs->eflags |= kprobe_saved_rflags;

	/* Restore the original saved kprobes variables and continue. */
	if (kprobe_status == KPROBE_REENTER) {
		restore_previous_kprobe();
		goto out;
	} else {
		unlock_kprobes();
	}
out:
	preempt_enable_no_resched();

	/*
	 * if somebody else is singlestepping across a probe point, eflags
	 * will have TF set, in which case, continue the remaining processing
	 * of do_debug, as if this is not a probe hit.
	 */
	if (regs->eflags & TF_MASK)
		return 0;

	return 1;
}

/* Interrupts disabled, kprobe_lock held. */
int __kprobes kprobe_fault_handler(struct pt_regs *regs, int trapnr)
{
	if (current_kprobe->fault_handler
	    && current_kprobe->fault_handler(current_kprobe, regs, trapnr))
		return 1;

	if (kprobe_status & KPROBE_HIT_SS) {
		resume_execution(current_kprobe, regs);
		regs->eflags |= kprobe_old_rflags;

		unlock_kprobes();
		preempt_enable_no_resched();
	}
	return 0;
}

/*
 * Wrapper routine for handling exceptions.
 */
int __kprobes kprobe_exceptions_notify(struct notifier_block *self,
				       unsigned long val, void *data)
{
	struct die_args *args = (struct die_args *)data;
	switch (val) {
	case DIE_INT3:
		if (kprobe_handler(args->regs))
			return NOTIFY_STOP;
		break;
	case DIE_DEBUG:
		if (post_kprobe_handler(args->regs))
			return NOTIFY_STOP;
		break;
	case DIE_GPF:
		if (kprobe_running() &&
		    kprobe_fault_handler(args->regs, args->trapnr))
			return NOTIFY_STOP;
		break;
	case DIE_PAGE_FAULT:
		if (kprobe_running() &&
		    kprobe_fault_handler(args->regs, args->trapnr))
			return NOTIFY_STOP;
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

int __kprobes setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct jprobe *jp = container_of(p, struct jprobe, kp);
	unsigned long addr;

	jprobe_saved_regs = *regs;
	jprobe_saved_rsp = (long *) regs->rsp;
	addr = (unsigned long)jprobe_saved_rsp;
	/*
	 * As Linus pointed out, gcc assumes that the callee
	 * owns the argument space and could overwrite it, e.g.
	 * tailcall optimization. So, to be absolutely safe
	 * we also save and restore enough stack bytes to cover
	 * the argument area.
	 */
	memcpy(jprobes_stack, (kprobe_opcode_t *) addr, MIN_STACK_SIZE(addr));
	regs->eflags &= ~IF_MASK;
	regs->rip = (unsigned long)(jp->entry);
	return 1;
}

void __kprobes jprobe_return(void)
{
	preempt_enable_no_resched();
	asm volatile ("       xchg   %%rbx,%%rsp     \n"
		      "       int3			\n"
		      "       .globl jprobe_return_end	\n"
		      "       jprobe_return_end:	\n"
		      "       nop			\n"::"b"
		      (jprobe_saved_rsp):"memory");
}

int __kprobes longjmp_break_handler(struct kprobe *p, struct pt_regs *regs)
{
	u8 *addr = (u8 *) (regs->rip - 1);
	unsigned long stack_addr = (unsigned long)jprobe_saved_rsp;
	struct jprobe *jp = container_of(p, struct jprobe, kp);

	if ((addr > (u8 *) jprobe_return) && (addr < (u8 *) jprobe_return_end)) {
		if ((long *)regs->rsp != jprobe_saved_rsp) {
			struct pt_regs *saved_regs =
			    container_of(jprobe_saved_rsp, struct pt_regs, rsp);
			printk("current rsp %p does not match saved rsp %p\n",
			       (long *)regs->rsp, jprobe_saved_rsp);
			printk("Saved registers for jprobe %p\n", jp);
			show_registers(saved_regs);
			printk("Current registers\n");
			show_registers(regs);
			BUG();
		}
		*regs = jprobe_saved_regs;
		memcpy((kprobe_opcode_t *) stack_addr, jprobes_stack,
		       MIN_STACK_SIZE(stack_addr));
		return 1;
	}
	return 0;
}

static struct kprobe trampoline_p = {
	.addr = (kprobe_opcode_t *) &kretprobe_trampoline,
	.pre_handler = trampoline_probe_handler
};

int __init arch_init_kprobes(void)
{
	return register_kprobe(&trampoline_p);
}
