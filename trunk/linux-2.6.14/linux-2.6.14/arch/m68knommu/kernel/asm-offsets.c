/*
 * This program is used to generate definitions needed by
 * assembly language modules.
 *
 * We use the technique used in the OSF Mach kernel code:
 * generate asm statements containing #defines,
 * compile this file to assembler, and then extract the
 * #defines from the assembly-language output.
 */

#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/ptrace.h>
#include <linux/hardirq.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/thread_info.h>

#define DEFINE(sym, val) \
        asm volatile("\n->" #sym " %0 " #val : : "i" (val))

#define BLANK() asm volatile("\n->" : : )

int main(void)
{
	/* offsets into the task struct */
	DEFINE(TASK_STATE, offsetof(struct task_struct, state));
	DEFINE(TASK_FLAGS, offsetof(struct task_struct, flags));
	DEFINE(TASK_PTRACE, offsetof(struct task_struct, ptrace));
	DEFINE(TASK_BLOCKED, offsetof(struct task_struct, blocked));
	DEFINE(TASK_THREAD, offsetof(struct task_struct, thread));
	DEFINE(TASK_THREAD_INFO, offsetof(struct task_struct, thread_info));
	DEFINE(TASK_MM, offsetof(struct task_struct, mm));
	DEFINE(TASK_ACTIVE_MM, offsetof(struct task_struct, active_mm));

	/* offsets into the kernel_stat struct */
	DEFINE(STAT_IRQ, offsetof(struct kernel_stat, irqs));

	/* offsets into the irq_cpustat_t struct */
	DEFINE(CPUSTAT_SOFTIRQ_PENDING, offsetof(irq_cpustat_t, __softirq_pending));

	/* offsets into the thread struct */
	DEFINE(THREAD_KSP, offsetof(struct thread_struct, ksp));
	DEFINE(THREAD_USP, offsetof(struct thread_struct, usp));
	DEFINE(THREAD_SR, offsetof(struct thread_struct, sr));
	DEFINE(THREAD_FS, offsetof(struct thread_struct, fs));
	DEFINE(THREAD_CRP, offsetof(struct thread_struct, crp));
	DEFINE(THREAD_ESP0, offsetof(struct thread_struct, esp0));
	DEFINE(THREAD_FPREG, offsetof(struct thread_struct, fp));
	DEFINE(THREAD_FPCNTL, offsetof(struct thread_struct, fpcntl));
	DEFINE(THREAD_FPSTATE, offsetof(struct thread_struct, fpstate));

	/* offsets into the pt_regs */
	DEFINE(PT_D0, offsetof(struct pt_regs, d0));
	DEFINE(PT_ORIG_D0, offsetof(struct pt_regs, orig_d0));
	DEFINE(PT_D1, offsetof(struct pt_regs, d1));
	DEFINE(PT_D2, offsetof(struct pt_regs, d2));
	DEFINE(PT_D3, offsetof(struct pt_regs, d3));
	DEFINE(PT_D4, offsetof(struct pt_regs, d4));
	DEFINE(PT_D5, offsetof(struct pt_regs, d5));
	DEFINE(PT_A0, offsetof(struct pt_regs, a0));
	DEFINE(PT_A1, offsetof(struct pt_regs, a1));
	DEFINE(PT_A2, offsetof(struct pt_regs, a2));
	DEFINE(PT_PC, offsetof(struct pt_regs, pc));
	DEFINE(PT_SR, offsetof(struct pt_regs, sr));

#ifdef CONFIG_COLDFIRE
	/* bitfields are a bit difficult */
	DEFINE(PT_FORMATVEC, offsetof(struct pt_regs, sr) - 2);
#else
	/* bitfields are a bit difficult */
	DEFINE(PT_VECTOR, offsetof(struct pt_regs, pc) + 4);
	/* offsets into the irq_handler struct */
	DEFINE(IRQ_HANDLER, offsetof(struct irq_node, handler));
	DEFINE(IRQ_DEVID, offsetof(struct irq_node, dev_id));
	DEFINE(IRQ_NEXT, offsetof(struct irq_node, next));
#endif

	/* offsets into the kernel_stat struct */
	DEFINE(STAT_IRQ, offsetof(struct kernel_stat, irqs));

	/* signal defines */
	DEFINE(SIGSEGV, SIGSEGV);
	DEFINE(SEGV_MAPERR, SEGV_MAPERR);
	DEFINE(SIGTRAP, SIGTRAP);
	DEFINE(TRAP_TRACE, TRAP_TRACE);

	DEFINE(PT_PTRACED, PT_PTRACED);
	DEFINE(PT_DTRACE, PT_DTRACE);

	DEFINE(THREAD_SIZE, THREAD_SIZE);

	/* Offsets in thread_info structure */
	DEFINE(TI_TASK, offsetof(struct thread_info, task));
	DEFINE(TI_EXECDOMAIN, offsetof(struct thread_info, exec_domain));
	DEFINE(TI_FLAGS, offsetof(struct thread_info, flags));
	DEFINE(TI_CPU, offsetof(struct thread_info, cpu));

	return 0;
}
