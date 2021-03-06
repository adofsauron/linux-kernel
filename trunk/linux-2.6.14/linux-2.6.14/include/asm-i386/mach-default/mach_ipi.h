#ifndef __ASM_MACH_IPI_H
#define __ASM_MACH_IPI_H

void send_IPI_mask_bitmask(cpumask_t mask, int vector);
void __send_IPI_shortcut(unsigned int shortcut, int vector);

extern int no_broadcast;

static inline void send_IPI_mask(cpumask_t mask, int vector)
{
	send_IPI_mask_bitmask(mask, vector);
}

static inline void __local_send_IPI_allbutself(int vector)
{
	if (no_broadcast) {
		cpumask_t mask = cpu_online_map;
		int this_cpu = get_cpu();

		cpu_clear(this_cpu, mask);
		send_IPI_mask(mask, vector);
		put_cpu();
	} else
		__send_IPI_shortcut(APIC_DEST_ALLBUT, vector);
}

static inline void __local_send_IPI_all(int vector)
{
	if (no_broadcast)
		send_IPI_mask(cpu_online_map, vector);
	else
		__send_IPI_shortcut(APIC_DEST_ALLINC, vector);
}

static inline void send_IPI_allbutself(int vector)
{
	/*
	 * if there are no other CPUs in the system then we get an APIC send 
	 * error if we try to broadcast, thus avoid sending IPIs in this case.
	 */
	if (!(num_online_cpus() > 1))
		return;

	__local_send_IPI_allbutself(vector);
	return;
}

static inline void send_IPI_all(int vector)
{
	__local_send_IPI_all(vector);
}

#endif /* __ASM_MACH_IPI_H */
