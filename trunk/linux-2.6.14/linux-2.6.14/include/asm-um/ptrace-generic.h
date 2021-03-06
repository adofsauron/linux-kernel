/* 
 * Copyright (C) 2000, 2001, 2002 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __UM_PTRACE_GENERIC_H
#define __UM_PTRACE_GENERIC_H

#ifndef __ASSEMBLY__

#include "linux/config.h"

#define pt_regs pt_regs_subarch
#define show_regs show_regs_subarch
#define send_sigtrap send_sigtrap_subarch

#include "asm/arch/ptrace.h"

#undef pt_regs
#undef show_regs
#undef send_sigtrap
#undef user_mode
#undef instruction_pointer

#include "sysdep/ptrace.h"

struct pt_regs {
	union uml_pt_regs regs;
};

#define EMPTY_REGS { regs : EMPTY_UML_PT_REGS }

#define PT_REGS_IP(r) UPT_IP(&(r)->regs)
#define PT_REGS_SP(r) UPT_SP(&(r)->regs)

#define PT_REG(r, reg) UPT_REG(&(r)->regs, reg)
#define PT_REGS_SET(r, reg, val) UPT_SET(&(r)->regs, reg, val)

#define PT_REGS_SET_SYSCALL_RETURN(r, res) \
	UPT_SET_SYSCALL_RETURN(&(r)->regs, res)
#define PT_REGS_RESTART_SYSCALL(r) UPT_RESTART_SYSCALL(&(r)->regs)

#define PT_REGS_SYSCALL_NR(r) UPT_SYSCALL_NR(&(r)->regs)

#define PT_REGS_SC(r) UPT_SC(&(r)->regs)

#define instruction_pointer(regs) PT_REGS_IP(regs)

struct task_struct;

extern unsigned long getreg(struct task_struct *child, int regno);
extern int putreg(struct task_struct *child, int regno, unsigned long value);
extern int get_fpregs(unsigned long buf, struct task_struct *child);
extern int set_fpregs(unsigned long buf, struct task_struct *child);
extern int get_fpxregs(unsigned long buf, struct task_struct *child);
extern int set_fpxregs(unsigned long buf, struct task_struct *tsk);

extern void show_regs(struct pt_regs *regs);

extern void send_sigtrap(struct task_struct *tsk, union uml_pt_regs *regs,
			 int error_code);

#endif

#endif

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
