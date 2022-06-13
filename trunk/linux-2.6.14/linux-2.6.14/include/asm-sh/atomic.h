#ifndef __ASM_SH_ATOMIC_H
#define __ASM_SH_ATOMIC_H

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 *
 */

typedef struct { volatile int counter; } atomic_t;

#define ATOMIC_INIT(i)	( (atomic_t) { (i) } )

#define atomic_read(v)		((v)->counter)
#define atomic_set(v,i)		((v)->counter = (i))

#include <asm/system.h>

/*
 * To get proper branch prediction for the main line, we must branch
 * forward to code at the end of this object's .text section, then
 * branch back to restart the operation.
 */

static __inline__ void atomic_add(int i, atomic_t * v)
{
	unsigned long flags;

	local_irq_save(flags);
	*(long *)v += i;
	local_irq_restore(flags);
}

static __inline__ void atomic_sub(int i, atomic_t *v)
{
	unsigned long flags;

	local_irq_save(flags);
	*(long *)v -= i;
	local_irq_restore(flags);
}

static __inline__ int atomic_add_return(int i, atomic_t * v)
{
	unsigned long temp, flags;

	local_irq_save(flags);
	temp = *(long *)v;
	temp += i;
	*(long *)v = temp;
	local_irq_restore(flags);

	return temp;
}

#define atomic_add_negative(a, v)	(atomic_add_return((a), (v)) < 0)

static __inline__ int atomic_sub_return(int i, atomic_t * v)
{
	unsigned long temp, flags;

	local_irq_save(flags);
	temp = *(long *)v;
	temp -= i;
	*(long *)v = temp;
	local_irq_restore(flags);

	return temp;
}

#define atomic_dec_return(v) atomic_sub_return(1,(v))
#define atomic_inc_return(v) atomic_add_return(1,(v))

/*
 * atomic_inc_and_test - increment and test
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */
#define atomic_inc_and_test(v) (atomic_inc_return(v) == 0)

#define atomic_sub_and_test(i,v) (atomic_sub_return((i), (v)) == 0)
#define atomic_dec_and_test(v) (atomic_sub_return(1, (v)) == 0)

#define atomic_inc(v) atomic_add(1,(v))
#define atomic_dec(v) atomic_sub(1,(v))

static __inline__ void atomic_clear_mask(unsigned int mask, atomic_t *v)
{
	unsigned long flags;

	local_irq_save(flags);
	*(long *)v &= ~mask;
	local_irq_restore(flags);
}

static __inline__ void atomic_set_mask(unsigned int mask, atomic_t *v)
{
	unsigned long flags;

	local_irq_save(flags);
	*(long *)v |= mask;
	local_irq_restore(flags);
}

/* Atomic operations are already serializing on SH */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#endif /* __ASM_SH_ATOMIC_H */
