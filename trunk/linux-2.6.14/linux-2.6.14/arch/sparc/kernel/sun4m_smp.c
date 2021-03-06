/* sun4m_smp.c: Sparc SUN4M SMP support.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#include <asm/head.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/threads.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/profile.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>

#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/oplib.h>
#include <asm/cpudata.h>

#define IRQ_RESCHEDULE		13
#define IRQ_STOP_CPU		14
#define IRQ_CROSS_CALL		15

extern ctxd_t *srmmu_ctx_table_phys;

extern void calibrate_delay(void);

extern volatile int smp_processors_ready;
extern int smp_num_cpus;
extern volatile unsigned long cpu_callin_map[NR_CPUS];
extern unsigned char boot_cpu_id;
extern int smp_activated;
extern volatile int __cpu_number_map[NR_CPUS];
extern volatile int __cpu_logical_map[NR_CPUS];
extern volatile unsigned long ipi_count;
extern volatile int smp_process_available;
extern volatile int smp_commenced;
extern int __smp4m_processor_id(void);

/*#define SMP_DEBUG*/

#ifdef SMP_DEBUG
#define SMP_PRINTK(x)	printk x
#else
#define SMP_PRINTK(x)
#endif

static inline unsigned long swap(volatile unsigned long *ptr, unsigned long val)
{
	__asm__ __volatile__("swap [%1], %0\n\t" :
			     "=&r" (val), "=&r" (ptr) :
			     "0" (val), "1" (ptr));
	return val;
}

static void smp_setup_percpu_timer(void);
extern void cpu_probe(void);

void __init smp4m_callin(void)
{
	int cpuid = hard_smp_processor_id();

	local_flush_cache_all();
	local_flush_tlb_all();

	set_irq_udt(boot_cpu_id);

	/* Get our local ticker going. */
	smp_setup_percpu_timer();

	calibrate_delay();
	smp_store_cpu_info(cpuid);

	local_flush_cache_all();
	local_flush_tlb_all();

	/*
	 * Unblock the master CPU _only_ when the scheduler state
	 * of all secondary CPUs will be up-to-date, so after
	 * the SMP initialization the master will be just allowed
	 * to call the scheduler code.
	 */
	/* Allow master to continue. */
	swap((unsigned long *)&cpu_callin_map[cpuid], 1);

	local_flush_cache_all();
	local_flush_tlb_all();
	
	cpu_probe();

	/* Fix idle thread fields. */
	__asm__ __volatile__("ld [%0], %%g6\n\t"
			     : : "r" (&current_set[cpuid])
			     : "memory" /* paranoid */);

	/* Attach to the address space of init_task. */
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;

	while(!smp_commenced)
		barrier();

	local_flush_cache_all();
	local_flush_tlb_all();

	local_irq_enable();
}

extern void init_IRQ(void);
extern void cpu_panic(void);

/*
 *	Cycle through the processors asking the PROM to start each one.
 */
 
extern struct linux_prom_registers smp_penguin_ctable;
extern unsigned long trapbase_cpu1[];
extern unsigned long trapbase_cpu2[];
extern unsigned long trapbase_cpu3[];

void __init smp4m_boot_cpus(void)
{
	int cpucount = 0;
	int i, mid;

	printk("Entering SMP Mode...\n");

	local_irq_enable();
	cpus_clear(cpu_present_map);

	for (i = 0; !cpu_find_by_instance(i, NULL, &mid); i++)
		cpu_set(mid, cpu_present_map);

	for(i=0; i < NR_CPUS; i++) {
		__cpu_number_map[i] = -1;
		__cpu_logical_map[i] = -1;
	}

	__cpu_number_map[boot_cpu_id] = 0;
	__cpu_logical_map[0] = boot_cpu_id;
	current_thread_info()->cpu = boot_cpu_id;

	smp_store_cpu_info(boot_cpu_id);
	set_irq_udt(boot_cpu_id);
	smp_setup_percpu_timer();
	local_flush_cache_all();
	if(cpu_find_by_instance(1, NULL, NULL))
		return;  /* Not an MP box. */
	for(i = 0; i < NR_CPUS; i++) {
		if(i == boot_cpu_id)
			continue;

		if (cpu_isset(i, cpu_present_map)) {
			extern unsigned long sun4m_cpu_startup;
			unsigned long *entry = &sun4m_cpu_startup;
			struct task_struct *p;
			int timeout;

			/* Cook up an idler for this guy. */
			p = fork_idle(i);
			cpucount++;
			current_set[i] = p->thread_info;
			/* See trampoline.S for details... */
			entry += ((i-1) * 3);

			/*
			 * Initialize the contexts table
			 * Since the call to prom_startcpu() trashes the structure,
			 * we need to re-initialize it for each cpu
			 */
			smp_penguin_ctable.which_io = 0;
			smp_penguin_ctable.phys_addr = (unsigned int) srmmu_ctx_table_phys;
			smp_penguin_ctable.reg_size = 0;

			/* whirrr, whirrr, whirrrrrrrrr... */
			printk("Starting CPU %d at %p\n", i, entry);
			local_flush_cache_all();
			prom_startcpu(cpu_data(i).prom_node,
				      &smp_penguin_ctable, 0, (char *)entry);

			/* wheee... it's going... */
			for(timeout = 0; timeout < 10000; timeout++) {
				if(cpu_callin_map[i])
					break;
				udelay(200);
			}
			if(cpu_callin_map[i]) {
				/* Another "Red Snapper". */
				__cpu_number_map[i] = i;
				__cpu_logical_map[i] = i;
			} else {
				cpucount--;
				printk("Processor %d is stuck.\n", i);
			}
		}
		if(!(cpu_callin_map[i])) {
			cpu_clear(i, cpu_present_map);
			__cpu_number_map[i] = -1;
		}
	}
	local_flush_cache_all();
	if(cpucount == 0) {
		printk("Error: only one Processor found.\n");
		cpu_present_map = cpumask_of_cpu(smp_processor_id());
	} else {
		unsigned long bogosum = 0;
		for(i = 0; i < NR_CPUS; i++) {
			if (cpu_isset(i, cpu_present_map))
				bogosum += cpu_data(i).udelay_val;
		}
		printk("Total of %d Processors activated (%lu.%02lu BogoMIPS).\n",
		       cpucount + 1,
		       bogosum/(500000/HZ),
		       (bogosum/(5000/HZ))%100);
		smp_activated = 1;
		smp_num_cpus = cpucount + 1;
	}

	/* Free unneeded trap tables */
	if (!cpu_isset(i, cpu_present_map)) {
		ClearPageReserved(virt_to_page(trapbase_cpu1));
		set_page_count(virt_to_page(trapbase_cpu1), 1);
		free_page((unsigned long)trapbase_cpu1);
		totalram_pages++;
		num_physpages++;
	}
	if (!cpu_isset(2, cpu_present_map)) {
		ClearPageReserved(virt_to_page(trapbase_cpu2));
		set_page_count(virt_to_page(trapbase_cpu2), 1);
		free_page((unsigned long)trapbase_cpu2);
		totalram_pages++;
		num_physpages++;
	}
	if (!cpu_isset(3, cpu_present_map)) {
		ClearPageReserved(virt_to_page(trapbase_cpu3));
		set_page_count(virt_to_page(trapbase_cpu3), 1);
		free_page((unsigned long)trapbase_cpu3);
		totalram_pages++;
		num_physpages++;
	}

	/* Ok, they are spinning and ready to go. */
	smp_processors_ready = 1;
}

/* At each hardware IRQ, we get this called to forward IRQ reception
 * to the next processor.  The caller must disable the IRQ level being
 * serviced globally so that there are no double interrupts received.
 *
 * XXX See sparc64 irq.c.
 */
void smp4m_irq_rotate(int cpu)
{
}

/* Cross calls, in order to work efficiently and atomically do all
 * the message passing work themselves, only stopcpu and reschedule
 * messages come through here.
 */
void smp4m_message_pass(int target, int msg, unsigned long data, int wait)
{
	static unsigned long smp_cpu_in_msg[NR_CPUS];
	cpumask_t mask;
	int me = smp_processor_id();
	int irq, i;

	if(msg == MSG_RESCHEDULE) {
		irq = IRQ_RESCHEDULE;

		if(smp_cpu_in_msg[me])
			return;
	} else if(msg == MSG_STOP_CPU) {
		irq = IRQ_STOP_CPU;
	} else {
		goto barf;
	}

	smp_cpu_in_msg[me]++;
	if(target == MSG_ALL_BUT_SELF || target == MSG_ALL) {
		mask = cpu_present_map;
		if(target == MSG_ALL_BUT_SELF)
			cpu_clear(me, mask);
		for(i = 0; i < 4; i++) {
			if (cpu_isset(i, mask))
				set_cpu_int(i, irq);
		}
	} else {
		set_cpu_int(target, irq);
	}
	smp_cpu_in_msg[me]--;

	return;
barf:
	printk("Yeeee, trying to send SMP msg(%d) on cpu %d\n", msg, me);
	panic("Bogon SMP message pass.");
}

static struct smp_funcall {
	smpfunc_t func;
	unsigned long arg1;
	unsigned long arg2;
	unsigned long arg3;
	unsigned long arg4;
	unsigned long arg5;
	unsigned long processors_in[NR_CPUS];  /* Set when ipi entered. */
	unsigned long processors_out[NR_CPUS]; /* Set when ipi exited. */
} ccall_info;

static DEFINE_SPINLOCK(cross_call_lock);

/* Cross calls must be serialized, at least currently. */
void smp4m_cross_call(smpfunc_t func, unsigned long arg1, unsigned long arg2,
		    unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
	if(smp_processors_ready) {
		register int ncpus = smp_num_cpus;
		unsigned long flags;

		spin_lock_irqsave(&cross_call_lock, flags);

		/* Init function glue. */
		ccall_info.func = func;
		ccall_info.arg1 = arg1;
		ccall_info.arg2 = arg2;
		ccall_info.arg3 = arg3;
		ccall_info.arg4 = arg4;
		ccall_info.arg5 = arg5;

		/* Init receive/complete mapping, plus fire the IPI's off. */
		{
			cpumask_t mask = cpu_present_map;
			register int i;

			cpu_clear(smp_processor_id(), mask);
			for(i = 0; i < ncpus; i++) {
				if (cpu_isset(i, mask)) {
					ccall_info.processors_in[i] = 0;
					ccall_info.processors_out[i] = 0;
					set_cpu_int(i, IRQ_CROSS_CALL);
				} else {
					ccall_info.processors_in[i] = 1;
					ccall_info.processors_out[i] = 1;
				}
			}
		}

		{
			register int i;

			i = 0;
			do {
				while(!ccall_info.processors_in[i])
					barrier();
			} while(++i < ncpus);

			i = 0;
			do {
				while(!ccall_info.processors_out[i])
					barrier();
			} while(++i < ncpus);
		}

		spin_unlock_irqrestore(&cross_call_lock, flags);
	}
}

/* Running cross calls. */
void smp4m_cross_call_irq(void)
{
	int i = smp_processor_id();

	ccall_info.processors_in[i] = 1;
	ccall_info.func(ccall_info.arg1, ccall_info.arg2, ccall_info.arg3,
			ccall_info.arg4, ccall_info.arg5);
	ccall_info.processors_out[i] = 1;
}

void smp4m_percpu_timer_interrupt(struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	clear_profile_irq(cpu);

	profile_tick(CPU_PROFILING, regs);

	if(!--prof_counter(cpu)) {
		int user = user_mode(regs);

		irq_enter();
		update_process_times(user);
		irq_exit();

		prof_counter(cpu) = prof_multiplier(cpu);
	}
}

extern unsigned int lvl14_resolution;

static void __init smp_setup_percpu_timer(void)
{
	int cpu = smp_processor_id();

	prof_counter(cpu) = prof_multiplier(cpu) = 1;
	load_profile_irq(cpu, lvl14_resolution);

	if(cpu == boot_cpu_id)
		enable_pil_irq(14);
}

void __init smp4m_blackbox_id(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	int rs1 = rd >> 11;
	
	addr[0] = 0x81580000 | rd;		/* rd %tbr, reg */
	addr[1] = 0x8130200c | rd | rs1;    	/* srl reg, 0xc, reg */
	addr[2] = 0x80082003 | rd | rs1;	/* and reg, 3, reg */
}

void __init smp4m_blackbox_current(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	int rs1 = rd >> 11;
	
	addr[0] = 0x81580000 | rd;		/* rd %tbr, reg */
	addr[2] = 0x8130200a | rd | rs1;    	/* srl reg, 0xa, reg */
	addr[4] = 0x8008200c | rd | rs1;	/* and reg, 3, reg */
}

void __init sun4m_init_smp(void)
{
	BTFIXUPSET_BLACKBOX(hard_smp_processor_id, smp4m_blackbox_id);
	BTFIXUPSET_BLACKBOX(load_current, smp4m_blackbox_current);
	BTFIXUPSET_CALL(smp_cross_call, smp4m_cross_call, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(smp_message_pass, smp4m_message_pass, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(__hard_smp_processor_id, __smp4m_processor_id, BTFIXUPCALL_NORM);
}
