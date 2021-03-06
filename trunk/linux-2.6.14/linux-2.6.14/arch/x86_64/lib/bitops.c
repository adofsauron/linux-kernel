#include <linux/bitops.h>

#undef find_first_zero_bit
#undef find_next_zero_bit
#undef find_first_bit
#undef find_next_bit

/**
 * find_first_zero_bit - find the first zero bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit-number of the first zero bit, not the number of the byte
 * containing a bit.
 */
inline long find_first_zero_bit(const unsigned long * addr, unsigned long size)
{
	long d0, d1, d2;
	long res;

	if (!size)
		return 0;
	asm volatile(
		"  repe; scasq\n"
		"  je 1f\n"
		"  xorq -8(%%rdi),%%rax\n"
		"  subq $8,%%rdi\n"
		"  bsfq %%rax,%%rdx\n"
		"1:  subq %[addr],%%rdi\n"
		"  shlq $3,%%rdi\n"
		"  addq %%rdi,%%rdx"
		:"=d" (res), "=&c" (d0), "=&D" (d1), "=&a" (d2)
		:"0" (0ULL), "1" ((size + 63) >> 6), "2" (addr), "3" (-1ULL),
		 [addr] "r" (addr) : "memory");
	return res;
}

/**
 * find_next_zero_bit - find the first zero bit in a memory region
 * @addr: The address to base the search on
 * @offset: The bitnumber to start searching at
 * @size: The maximum size to search
 */
long find_next_zero_bit (const unsigned long * addr, long size, long offset)
{
	unsigned long * p = ((unsigned long *) addr) + (offset >> 6);
	unsigned long set = 0;
	unsigned long res, bit = offset&63;

	if (bit) {
		/*
		 * Look for zero in first word
		 */
		asm("bsfq %1,%0\n\t"
		    "cmoveq %2,%0"
		    : "=r" (set)
		    : "r" (~(*p >> bit)), "r"(64L));
		if (set < (64 - bit))
			return set + offset;
		set = 64 - bit;
		p++;
	}
	/*
	 * No zero yet, search remaining full words for a zero
	 */
	res = find_first_zero_bit ((const unsigned long *)p,
				   size - 64 * (p - (unsigned long *) addr));
	return (offset + set + res);
}

static inline long
__find_first_bit(const unsigned long * addr, unsigned long size)
{
	long d0, d1;
	long res;

	asm volatile(
		"   repe; scasq\n"
		"   jz 1f\n"
		"   subq $8,%%rdi\n"
		"   bsfq (%%rdi),%%rax\n"
		"1: subq %[addr],%%rdi\n"
		"   shlq $3,%%rdi\n"
		"   addq %%rdi,%%rax"
		:"=a" (res), "=&c" (d0), "=&D" (d1)
		:"0" (0ULL),
		 "1" ((size + 63) >> 6), "2" (addr),
		 [addr] "r" (addr) : "memory");
	return res;
}

/**
 * find_first_bit - find the first set bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit-number of the first set bit, not the number of the byte
 * containing a bit.
 */
long find_first_bit(const unsigned long * addr, unsigned long size)
{
	return __find_first_bit(addr,size);
}

/**
 * find_next_bit - find the first set bit in a memory region
 * @addr: The address to base the search on
 * @offset: The bitnumber to start searching at
 * @size: The maximum size to search
 */
long find_next_bit(const unsigned long * addr, long size, long offset)
{
	const unsigned long * p = addr + (offset >> 6);
	unsigned long set = 0, bit = offset & 63, res;

	if (bit) {
		/*
		 * Look for nonzero in the first 64 bits:
		 */
		asm("bsfq %1,%0\n\t"
		    "cmoveq %2,%0\n\t"
		    : "=r" (set)
		    : "r" (*p >> bit), "r" (64L));
		if (set < (64 - bit))
			return set + offset;
		set = 64 - bit;
		p++;
	}
	/*
	 * No set bit yet, search remaining full words for a bit
	 */
	res = __find_first_bit (p, size - 64 * (p - addr));
	return (offset + set + res);
}

#include <linux/module.h>

EXPORT_SYMBOL(find_next_bit);
EXPORT_SYMBOL(find_first_bit);
EXPORT_SYMBOL(find_first_zero_bit);
EXPORT_SYMBOL(find_next_zero_bit);
