#ifndef __ASM_OFFSETS_H__
#define __ASM_OFFSETS_H__
/*
 * DO NOT MODIFY.
 *
 * This file was generated by arch/m68knommu/Makefile
 *
 */

#define TASK_STATE 0 /* offsetof(struct task_struct, state) */
#define TASK_FLAGS 12 /* offsetof(struct task_struct, flags) */
#define TASK_PTRACE 16 /* offsetof(struct task_struct, ptrace) */
#define TASK_BLOCKED 922 /* offsetof(struct task_struct, blocked) */
#define TASK_THREAD 772 /* offsetof(struct task_struct, thread) */
#define TASK_THREAD_INFO 4 /* offsetof(struct task_struct, thread_info) */
#define TASK_MM 92 /* offsetof(struct task_struct, mm) */
#define TASK_ACTIVE_MM 96 /* offsetof(struct task_struct, active_mm) */
#define CPUSTAT_SOFTIRQ_PENDING 0 /* offsetof(irq_cpustat_t, __softirq_pending) */
#define THREAD_KSP 0 /* offsetof(struct thread_struct, ksp) */
#define THREAD_USP 4 /* offsetof(struct thread_struct, usp) */
#define THREAD_SR 8 /* offsetof(struct thread_struct, sr) */
#define THREAD_FS 10 /* offsetof(struct thread_struct, fs) */
#define THREAD_CRP 12 /* offsetof(struct thread_struct, crp) */
#define THREAD_ESP0 20 /* offsetof(struct thread_struct, esp0) */
#define THREAD_FPREG 24 /* offsetof(struct thread_struct, fp) */
#define THREAD_FPCNTL 120 /* offsetof(struct thread_struct, fpcntl) */
#define THREAD_FPSTATE 132 /* offsetof(struct thread_struct, fpstate) */
#define PT_D0 32 /* offsetof(struct pt_regs, d0) */
#define PT_ORIG_D0 36 /* offsetof(struct pt_regs, orig_d0) */
#define PT_D1 0 /* offsetof(struct pt_regs, d1) */
#define PT_D2 4 /* offsetof(struct pt_regs, d2) */
#define PT_D3 8 /* offsetof(struct pt_regs, d3) */
#define PT_D4 12 /* offsetof(struct pt_regs, d4) */
#define PT_D5 16 /* offsetof(struct pt_regs, d5) */
#define PT_A0 20 /* offsetof(struct pt_regs, a0) */
#define PT_A1 24 /* offsetof(struct pt_regs, a1) */
#define PT_A2 28 /* offsetof(struct pt_regs, a2) */
#define PT_PC 48 /* offsetof(struct pt_regs, pc) */
#define PT_SR 46 /* offsetof(struct pt_regs, sr) */
#define PT_VECTOR 52 /* offsetof(struct pt_regs, pc) + 4 */
#define STAT_IRQ 5140 /* offsetof(struct kernel_stat, irqs) */
#define SIGSEGV 11 /* SIGSEGV */
#define SEGV_MAPERR 196609 /* SEGV_MAPERR */
#define SIGTRAP 5 /* SIGTRAP */
#define TRAP_TRACE 196610 /* TRAP_TRACE */
#define PT_PTRACED 1 /* PT_PTRACED */
#define PT_DTRACE 2 /* PT_DTRACE */

#endif
