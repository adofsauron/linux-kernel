/*
 * arch/ia64/kernel/cpufreq/acpi-cpufreq.c
 * This file provides the ACPI based P-state support. This
 * module works with generic cpufreq infrastructure. Most of
 * the code is based on i386 version
 * (arch/i386/kernel/cpu/cpufreq/acpi-cpufreq.c)
 *
 * Copyright (C) 2005 Intel Corp
 *      Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pal.h>

#include <linux/acpi.h>
#include <acpi/processor.h>

#define dprintk(msg...) cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER, "acpi-cpufreq", msg)

MODULE_AUTHOR("Venkatesh Pallipadi");
MODULE_DESCRIPTION("ACPI Processor P-States Driver");
MODULE_LICENSE("GPL");


struct cpufreq_acpi_io {
	struct acpi_processor_performance	acpi_data;
	struct cpufreq_frequency_table		*freq_table;
	unsigned int				resume;
};

static struct cpufreq_acpi_io	*acpi_io_data[NR_CPUS];

static struct cpufreq_driver acpi_cpufreq_driver;


static int
processor_set_pstate (
	u32	value)
{
	s64 retval;

	dprintk("processor_set_pstate\n");

	retval = ia64_pal_set_pstate((u64)value);

	if (retval) {
		dprintk("Failed to set freq to 0x%x, with error 0x%x\n",
		        value, retval);
		return -ENODEV;
	}
	return (int)retval;
}


static int
processor_get_pstate (
	u32	*value)
{
	u64	pstate_index = 0;
	s64 	retval;

	dprintk("processor_get_pstate\n");

	retval = ia64_pal_get_pstate(&pstate_index);
	*value = (u32) pstate_index;

	if (retval)
		dprintk("Failed to get current freq with "
		        "error 0x%x, idx 0x%x\n", retval, *value);

	return (int)retval;
}


/* To be used only after data->acpi_data is initialized */
static unsigned
extract_clock (
	struct cpufreq_acpi_io *data,
	unsigned value,
	unsigned int cpu)
{
	unsigned long i;

	dprintk("extract_clock\n");

	for (i = 0; i < data->acpi_data.state_count; i++) {
		if (value >= data->acpi_data.states[i].control)
			return data->acpi_data.states[i].core_frequency;
	}
	return data->acpi_data.states[i-1].core_frequency;
}


static unsigned int
processor_get_freq (
	struct cpufreq_acpi_io	*data,
	unsigned int		cpu)
{
	int			ret = 0;
	u32			value = 0;
	cpumask_t		saved_mask;
	unsigned long 		clock_freq;

	dprintk("processor_get_freq\n");

	saved_mask = current->cpus_allowed;
	set_cpus_allowed(current, cpumask_of_cpu(cpu));
	if (smp_processor_id() != cpu) {
		ret = -EAGAIN;
		goto migrate_end;
	}

	/*
	 * processor_get_pstate gets the average frequency since the
	 * last get. So, do two PAL_get_freq()...
	 */
	ret = processor_get_pstate(&value);
	ret = processor_get_pstate(&value);

	if (ret) {
		set_cpus_allowed(current, saved_mask);
		printk(KERN_WARNING "get performance failed with error %d\n",
		       ret);
		ret = -EAGAIN;
		goto migrate_end;
	}
	clock_freq = extract_clock(data, value, cpu);
	ret = (clock_freq*1000);

migrate_end:
	set_cpus_allowed(current, saved_mask);
	return ret;
}


static int
processor_set_freq (
	struct cpufreq_acpi_io	*data,
	unsigned int		cpu,
	int			state)
{
	int			ret = 0;
	u32			value = 0;
	struct cpufreq_freqs    cpufreq_freqs;
	cpumask_t		saved_mask;
	int			retval;

	dprintk("processor_set_freq\n");

	saved_mask = current->cpus_allowed;
	set_cpus_allowed(current, cpumask_of_cpu(cpu));
	if (smp_processor_id() != cpu) {
		retval = -EAGAIN;
		goto migrate_end;
	}

	if (state == data->acpi_data.state) {
		if (unlikely(data->resume)) {
			dprintk("Called after resume, resetting to P%d\n", state);
			data->resume = 0;
		} else {
			dprintk("Already at target state (P%d)\n", state);
			retval = 0;
			goto migrate_end;
		}
	}

	dprintk("Transitioning from P%d to P%d\n",
		data->acpi_data.state, state);

	/* cpufreq frequency struct */
	cpufreq_freqs.cpu = cpu;
	cpufreq_freqs.old = data->freq_table[data->acpi_data.state].frequency;
	cpufreq_freqs.new = data->freq_table[state].frequency;

	/* notify cpufreq */
	cpufreq_notify_transition(&cpufreq_freqs, CPUFREQ_PRECHANGE);

	/*
	 * First we write the target state's 'control' value to the
	 * control_register.
	 */

	value = (u32) data->acpi_data.states[state].control;

	dprintk("Transitioning to state: 0x%08x\n", value);

	ret = processor_set_pstate(value);
	if (ret) {
		unsigned int tmp = cpufreq_freqs.new;
		cpufreq_notify_transition(&cpufreq_freqs, CPUFREQ_POSTCHANGE);
		cpufreq_freqs.new = cpufreq_freqs.old;
		cpufreq_freqs.old = tmp;
		cpufreq_notify_transition(&cpufreq_freqs, CPUFREQ_PRECHANGE);
		cpufreq_notify_transition(&cpufreq_freqs, CPUFREQ_POSTCHANGE);
		printk(KERN_WARNING "Transition failed with error %d\n", ret);
		retval = -ENODEV;
		goto migrate_end;
	}

	cpufreq_notify_transition(&cpufreq_freqs, CPUFREQ_POSTCHANGE);

	data->acpi_data.state = state;

	retval = 0;

migrate_end:
	set_cpus_allowed(current, saved_mask);
	return (retval);
}


static unsigned int
acpi_cpufreq_get (
	unsigned int		cpu)
{
	struct cpufreq_acpi_io *data = acpi_io_data[cpu];

	dprintk("acpi_cpufreq_get\n");

	return processor_get_freq(data, cpu);
}


static int
acpi_cpufreq_target (
	struct cpufreq_policy   *policy,
	unsigned int target_freq,
	unsigned int relation)
{
	struct cpufreq_acpi_io *data = acpi_io_data[policy->cpu];
	unsigned int next_state = 0;
	unsigned int result = 0;

	dprintk("acpi_cpufreq_setpolicy\n");

	result = cpufreq_frequency_table_target(policy,
			data->freq_table, target_freq, relation, &next_state);
	if (result)
		return (result);

	result = processor_set_freq(data, policy->cpu, next_state);

	return (result);
}


static int
acpi_cpufreq_verify (
	struct cpufreq_policy   *policy)
{
	unsigned int result = 0;
	struct cpufreq_acpi_io *data = acpi_io_data[policy->cpu];

	dprintk("acpi_cpufreq_verify\n");

	result = cpufreq_frequency_table_verify(policy,
			data->freq_table);

	return (result);
}


/*
 * processor_init_pdc - let BIOS know about the SMP capabilities
 * of this driver
 * @perf: processor-specific acpi_io_data struct
 * @cpu: CPU being initialized
 *
 * To avoid issues with legacy OSes, some BIOSes require to be informed of
 * the SMP capabilities of OS P-state driver. Here we set the bits in _PDC
 * accordingly. Actual call to _PDC is done in driver/acpi/processor.c
 */
static void
processor_init_pdc (
		struct acpi_processor_performance *perf,
		unsigned int cpu,
		struct acpi_object_list *obj_list
		)
{
	union acpi_object *obj;
	u32 *buf;

	dprintk("processor_init_pdc\n");

	perf->pdc = NULL;
	/* Initialize pdc. It will be used later. */
	if (!obj_list)
		return;

	if (!(obj_list->count && obj_list->pointer))
		return;

	obj = obj_list->pointer;
	if ((obj->buffer.length == 12) && obj->buffer.pointer) {
		buf = (u32 *)obj->buffer.pointer;
       		buf[0] = ACPI_PDC_REVISION_ID;
       		buf[1] = 1;
       		buf[2] = ACPI_PDC_EST_CAPABILITY_SMP;
		perf->pdc = obj_list;
	}
	return;
}


static int
acpi_cpufreq_cpu_init (
	struct cpufreq_policy   *policy)
{
	unsigned int		i;
	unsigned int		cpu = policy->cpu;
	struct cpufreq_acpi_io	*data;
	unsigned int		result = 0;

	union acpi_object		arg0 = {ACPI_TYPE_BUFFER};
	u32				arg0_buf[3];
	struct acpi_object_list 	arg_list = {1, &arg0};

	dprintk("acpi_cpufreq_cpu_init\n");
	/* setup arg_list for _PDC settings */
        arg0.buffer.length = 12;
        arg0.buffer.pointer = (u8 *) arg0_buf;

	data = kmalloc(sizeof(struct cpufreq_acpi_io), GFP_KERNEL);
	if (!data)
		return (-ENOMEM);

	memset(data, 0, sizeof(struct cpufreq_acpi_io));

	acpi_io_data[cpu] = data;

	processor_init_pdc(&data->acpi_data, cpu, &arg_list);
	result = acpi_processor_register_performance(&data->acpi_data, cpu);
	data->acpi_data.pdc = NULL;

	if (result)
		goto err_free;

	/* capability check */
	if (data->acpi_data.state_count <= 1) {
		dprintk("No P-States\n");
		result = -ENODEV;
		goto err_unreg;
	}

	if ((data->acpi_data.control_register.space_id !=
					ACPI_ADR_SPACE_FIXED_HARDWARE) ||
	    (data->acpi_data.status_register.space_id !=
					ACPI_ADR_SPACE_FIXED_HARDWARE)) {
		dprintk("Unsupported address space [%d, %d]\n",
			(u32) (data->acpi_data.control_register.space_id),
			(u32) (data->acpi_data.status_register.space_id));
		result = -ENODEV;
		goto err_unreg;
	}

	/* alloc freq_table */
	data->freq_table = kmalloc(sizeof(struct cpufreq_frequency_table) *
	                           (data->acpi_data.state_count + 1),
	                           GFP_KERNEL);
	if (!data->freq_table) {
		result = -ENOMEM;
		goto err_unreg;
	}

	/* detect transition latency */
	policy->cpuinfo.transition_latency = 0;
	for (i=0; i<data->acpi_data.state_count; i++) {
		if ((data->acpi_data.states[i].transition_latency * 1000) >
		    policy->cpuinfo.transition_latency) {
			policy->cpuinfo.transition_latency =
			    data->acpi_data.states[i].transition_latency * 1000;
		}
	}
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	policy->cur = processor_get_freq(data, policy->cpu);

	/* table init */
	for (i = 0; i <= data->acpi_data.state_count; i++)
	{
		data->freq_table[i].index = i;
		if (i < data->acpi_data.state_count) {
			data->freq_table[i].frequency =
			      data->acpi_data.states[i].core_frequency * 1000;
		} else {
			data->freq_table[i].frequency = CPUFREQ_TABLE_END;
		}
	}

	result = cpufreq_frequency_table_cpuinfo(policy, data->freq_table);
	if (result) {
		goto err_freqfree;
	}

	/* notify BIOS that we exist */
	acpi_processor_notify_smm(THIS_MODULE);

	printk(KERN_INFO "acpi-cpufreq: CPU%u - ACPI performance management "
	       "activated.\n", cpu);

	for (i = 0; i < data->acpi_data.state_count; i++)
		dprintk("     %cP%d: %d MHz, %d mW, %d uS, %d uS, 0x%x 0x%x\n",
			(i == data->acpi_data.state?'*':' '), i,
			(u32) data->acpi_data.states[i].core_frequency,
			(u32) data->acpi_data.states[i].power,
			(u32) data->acpi_data.states[i].transition_latency,
			(u32) data->acpi_data.states[i].bus_master_latency,
			(u32) data->acpi_data.states[i].status,
			(u32) data->acpi_data.states[i].control);

	cpufreq_frequency_table_get_attr(data->freq_table, policy->cpu);

	/* the first call to ->target() should result in us actually
	 * writing something to the appropriate registers. */
	data->resume = 1;

	return (result);

 err_freqfree:
	kfree(data->freq_table);
 err_unreg:
	acpi_processor_unregister_performance(&data->acpi_data, cpu);
 err_free:
	kfree(data);
	acpi_io_data[cpu] = NULL;

	return (result);
}


static int
acpi_cpufreq_cpu_exit (
	struct cpufreq_policy   *policy)
{
	struct cpufreq_acpi_io *data = acpi_io_data[policy->cpu];

	dprintk("acpi_cpufreq_cpu_exit\n");

	if (data) {
		cpufreq_frequency_table_put_attr(policy->cpu);
		acpi_io_data[policy->cpu] = NULL;
		acpi_processor_unregister_performance(&data->acpi_data,
		                                      policy->cpu);
		kfree(data);
	}

	return (0);
}


static struct freq_attr* acpi_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};


static struct cpufreq_driver acpi_cpufreq_driver = {
	.verify 	= acpi_cpufreq_verify,
	.target 	= acpi_cpufreq_target,
	.get 		= acpi_cpufreq_get,
	.init		= acpi_cpufreq_cpu_init,
	.exit		= acpi_cpufreq_cpu_exit,
	.name		= "acpi-cpufreq",
	.owner		= THIS_MODULE,
	.attr           = acpi_cpufreq_attr,
};


static int __init
acpi_cpufreq_init (void)
{
	dprintk("acpi_cpufreq_init\n");

 	return cpufreq_register_driver(&acpi_cpufreq_driver);
}


static void __exit
acpi_cpufreq_exit (void)
{
	dprintk("acpi_cpufreq_exit\n");

	cpufreq_unregister_driver(&acpi_cpufreq_driver);
	return;
}


late_initcall(acpi_cpufreq_init);
module_exit(acpi_cpufreq_exit);

