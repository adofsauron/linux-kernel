/* 
 * Copyright (C) 2000, 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/kernel.h"
#include "asm/errno.h"
#include "linux/sched.h"
#include "linux/mm.h"
#include "linux/spinlock.h"
#include "linux/config.h"
#include "linux/init.h"
#include "linux/ptrace.h"
#include "asm/semaphore.h"
#include "asm/pgtable.h"
#include "asm/pgalloc.h"
#include "asm/tlbflush.h"
#include "asm/a.out.h"
#include "asm/current.h"
#include "asm/irq.h"
#include "sysdep/sigcontext.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"
#include "chan_kern.h"
#include "mconsole_kern.h"
#include "mem.h"
#include "mem_kern.h"
#ifdef CONFIG_MODE_SKAS
#include "skas.h"
#endif

/* Note this is constrained to return 0, -EFAULT, -EACCESS, -ENOMEM by segv(). */
int handle_page_fault(unsigned long address, unsigned long ip, 
		      int is_write, int is_user, int *code_out)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int err = -EFAULT;

	*code_out = SEGV_MAPERR;

	/* If the fault was during atomic operation, don't take the fault, just
	 * fail. */
	if (in_atomic())
		goto out_nosemaphore;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, address);
	if(!vma) 
		goto out;
	else if(vma->vm_start <= address) 
		goto good_area;
	else if(!(vma->vm_flags & VM_GROWSDOWN)) 
		goto out;
	else if(is_user && !ARCH_IS_STACKGROW(address))
		goto out;
	else if(expand_stack(vma, address)) 
		goto out;

good_area:
	*code_out = SEGV_ACCERR;
	if(is_write && !(vma->vm_flags & VM_WRITE)) 
		goto out;

	/* Don't require VM_READ|VM_EXEC for write faults! */
        if(!is_write && !(vma->vm_flags & (VM_READ | VM_EXEC)))
                goto out;

	do {
survive:
		switch (handle_mm_fault(mm, vma, address, is_write)){
		case VM_FAULT_MINOR:
			current->min_flt++;
			break;
		case VM_FAULT_MAJOR:
			current->maj_flt++;
			break;
		case VM_FAULT_SIGBUS:
			err = -EACCES;
			goto out;
		case VM_FAULT_OOM:
			err = -ENOMEM;
			goto out_of_memory;
		default:
			BUG();
		}
		pgd = pgd_offset(mm, address);
		pud = pud_offset(pgd, address);
		pmd = pmd_offset(pud, address);
		pte = pte_offset_kernel(pmd, address);
	} while(!pte_present(*pte));
	err = 0;
	WARN_ON(!pte_young(*pte) || (is_write && !pte_dirty(*pte)));
	flush_tlb_page(vma, address);
out:
	up_read(&mm->mmap_sem);
out_nosemaphore:
	return(err);

/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
 */
out_of_memory:
	if (current->pid == 1) {
		up_read(&mm->mmap_sem);
		yield();
		down_read(&mm->mmap_sem);
		goto survive;
	}
	goto out;
}

/*
 * We give a *copy* of the faultinfo in the regs to segv.
 * This must be done, since nesting SEGVs could overwrite
 * the info in the regs. A pointer to the info then would
 * give us bad data!
 */
unsigned long segv(struct faultinfo fi, unsigned long ip, int is_user, void *sc)
{
	struct siginfo si;
	void *catcher;
	int err;
        int is_write = FAULT_WRITE(fi);
        unsigned long address = FAULT_ADDRESS(fi);

        if(!is_user && (address >= start_vm) && (address < end_vm)){
                flush_tlb_kernel_vm();
                return(0);
        }
	else if(current->mm == NULL)
		panic("Segfault with no mm");

	if (SEGV_IS_FIXABLE(&fi) || SEGV_MAYBE_FIXABLE(&fi))
		err = handle_page_fault(address, ip, is_write, is_user, &si.si_code);
	else {
		err = -EFAULT;
		/* A thread accessed NULL, we get a fault, but CR2 is invalid.
		 * This code is used in __do_copy_from_user() of TT mode. */
		address = 0;
	}

	catcher = current->thread.fault_catcher;
	if(!err)
		return(0);
	else if(catcher != NULL){
		current->thread.fault_addr = (void *) address;
		do_longjmp(catcher, 1);
	} 
	else if(current->thread.fault_addr != NULL)
		panic("fault_addr set but no fault catcher");
        else if(!is_user && arch_fixup(ip, sc))
		return(0);

 	if(!is_user) 
		panic("Kernel mode fault at addr 0x%lx, ip 0x%lx", 
		      address, ip);

	if (err == -EACCES) {
		si.si_signo = SIGBUS;
		si.si_errno = 0;
		si.si_code = BUS_ADRERR;
		si.si_addr = (void *)address;
                current->thread.arch.faultinfo = fi;
		force_sig_info(SIGBUS, &si, current);
	} else if (err == -ENOMEM) {
		printk("VM: killing process %s\n", current->comm);
		do_exit(SIGKILL);
	} else {
		BUG_ON(err != -EFAULT);
		si.si_signo = SIGSEGV;
		si.si_addr = (void *) address;
                current->thread.arch.faultinfo = fi;
		force_sig_info(SIGSEGV, &si, current);
	}
	return(0);
}

void bad_segv(struct faultinfo fi, unsigned long ip)
{
	struct siginfo si;

	si.si_signo = SIGSEGV;
	si.si_code = SEGV_ACCERR;
        si.si_addr = (void *) FAULT_ADDRESS(fi);
        current->thread.arch.faultinfo = fi;
	force_sig_info(SIGSEGV, &si, current);
}

void relay_signal(int sig, union uml_pt_regs *regs)
{
	if(arch_handle_signal(sig, regs)) return;
	if(!UPT_IS_USER(regs))
		panic("Kernel mode signal %d", sig);
        current->thread.arch.faultinfo = *UPT_FAULTINFO(regs);
	force_sig(sig, current);
}

void bus_handler(int sig, union uml_pt_regs *regs)
{
	if(current->thread.fault_catcher != NULL)
		do_longjmp(current->thread.fault_catcher, 1);
	else relay_signal(sig, regs);
}

void winch(int sig, union uml_pt_regs *regs)
{
	do_IRQ(WINCH_IRQ, regs);
}

void trap_init(void)
{
}
