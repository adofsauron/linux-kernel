#ifdef __KERNEL__
#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H

#include <linux/config.h>
#include <linux/cache.h>
#include <linux/smp_lock.h>
#include <asm/irq.h>

/* The __last_jiffy_stamp field is needed to ensure that no decrementer
 * interrupt is lost on SMP machines. Since on most CPUs it is in the same
 * cache line as local_irq_count, it is cheap to access and is also used on UP
 * for uniformity.
 */
typedef struct {
	unsigned long __softirq_pending;	/* set_bit is used on this */
	unsigned int __last_jiffy_stamp;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */

#define last_jiffy_stamp(cpu) __IRQ_STAT((cpu), __last_jiffy_stamp)

static inline void ack_bad_irq(int irq)
{
	printk(KERN_CRIT "illegal vector %d received!\n", irq);
	BUG();
}

#endif /* __ASM_HARDIRQ_H */
#endif /* __KERNEL__ */
