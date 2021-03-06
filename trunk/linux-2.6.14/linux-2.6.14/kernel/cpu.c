/* CPU control.
 * (C) 2001, 2002, 2003, 2004 Rusty Russell
 *
 * This code is licenced under the GPL.
 */
#include <linux/proc_fs.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/stop_machine.h>
#include <asm/semaphore.h>

/* This protects CPUs going up and down... */
DECLARE_MUTEX(cpucontrol);

static struct notifier_block *cpu_chain;

/* Need to know about CPUs going up/down? */
int register_cpu_notifier(struct notifier_block *nb)
{
	int ret;

	if ((ret = down_interruptible(&cpucontrol)) != 0)
		return ret;
	ret = notifier_chain_register(&cpu_chain, nb);
	up(&cpucontrol);
	return ret;
}
EXPORT_SYMBOL(register_cpu_notifier);

void unregister_cpu_notifier(struct notifier_block *nb)
{
	down(&cpucontrol);
	notifier_chain_unregister(&cpu_chain, nb);
	up(&cpucontrol);
}
EXPORT_SYMBOL(unregister_cpu_notifier);

#ifdef CONFIG_HOTPLUG_CPU
static inline void check_for_tasks(int cpu)
{
	struct task_struct *p;

	write_lock_irq(&tasklist_lock);
	for_each_process(p) {
		if (task_cpu(p) == cpu &&
		    (!cputime_eq(p->utime, cputime_zero) ||
		     !cputime_eq(p->stime, cputime_zero)))
			printk(KERN_WARNING "Task %s (pid = %d) is on cpu %d\
				(state = %ld, flags = %lx) \n",
				 p->comm, p->pid, cpu, p->state, p->flags);
	}
	write_unlock_irq(&tasklist_lock);
}

/* Take this CPU down. */
static int take_cpu_down(void *unused)
{
	int err;

	/* Ensure this CPU doesn't handle any more interrupts. */
	err = __cpu_disable();
	if (err < 0)
		return err;

	/* Force idle task to run as soon as we yield: it should
	   immediately notice cpu is offline and die quickly. */
	sched_idle_next();
	return 0;
}

int cpu_down(unsigned int cpu)
{
	int err;
	struct task_struct *p;
	cpumask_t old_allowed, tmp;

	if ((err = lock_cpu_hotplug_interruptible()) != 0)
		return err;

	if (num_online_cpus() == 1) {
		err = -EBUSY;
		goto out;
	}

	if (!cpu_online(cpu)) {
		err = -EINVAL;
		goto out;
	}

	err = notifier_call_chain(&cpu_chain, CPU_DOWN_PREPARE,
						(void *)(long)cpu);
	if (err == NOTIFY_BAD) {
		printk("%s: attempt to take down CPU %u failed\n",
				__FUNCTION__, cpu);
		err = -EINVAL;
		goto out;
	}

	/* Ensure that we are not runnable on dying cpu */
	old_allowed = current->cpus_allowed;
	tmp = CPU_MASK_ALL;
	cpu_clear(cpu, tmp);
	set_cpus_allowed(current, tmp);

	p = __stop_machine_run(take_cpu_down, NULL, cpu);
	if (IS_ERR(p)) {
		/* CPU didn't die: tell everyone.  Can't complain. */
		if (notifier_call_chain(&cpu_chain, CPU_DOWN_FAILED,
				(void *)(long)cpu) == NOTIFY_BAD)
			BUG();

		err = PTR_ERR(p);
		goto out_allowed;
	}

	if (cpu_online(cpu))
		goto out_thread;

	/* Wait for it to sleep (leaving idle task). */
	while (!idle_cpu(cpu))
		yield();

	/* This actually kills the CPU. */
	__cpu_die(cpu);

	/* Move it here so it can run. */
	kthread_bind(p, get_cpu());
	put_cpu();

	/* CPU is completely dead: tell everyone.  Too late to complain. */
	if (notifier_call_chain(&cpu_chain, CPU_DEAD, (void *)(long)cpu)
	    == NOTIFY_BAD)
		BUG();

	check_for_tasks(cpu);

out_thread:
	err = kthread_stop(p);
out_allowed:
	set_cpus_allowed(current, old_allowed);
out:
	unlock_cpu_hotplug();
	return err;
}
#endif /*CONFIG_HOTPLUG_CPU*/

int __devinit cpu_up(unsigned int cpu)
{
	int ret;
	void *hcpu = (void *)(long)cpu;

	if ((ret = down_interruptible(&cpucontrol)) != 0)
		return ret;

	if (cpu_online(cpu) || !cpu_present(cpu)) {
		ret = -EINVAL;
		goto out;
	}
	ret = notifier_call_chain(&cpu_chain, CPU_UP_PREPARE, hcpu);
	if (ret == NOTIFY_BAD) {
		printk("%s: attempt to bring up CPU %u failed\n",
				__FUNCTION__, cpu);
		ret = -EINVAL;
		goto out_notify;
	}

	/* Arch-specific enabling code. */
	ret = __cpu_up(cpu);
	if (ret != 0)
		goto out_notify;
	if (!cpu_online(cpu))
		BUG();

	/* Now call notifier in preparation. */
	notifier_call_chain(&cpu_chain, CPU_ONLINE, hcpu);

out_notify:
	if (ret != 0)
		notifier_call_chain(&cpu_chain, CPU_UP_CANCELED, hcpu);
out:
	up(&cpucontrol);
	return ret;
}
