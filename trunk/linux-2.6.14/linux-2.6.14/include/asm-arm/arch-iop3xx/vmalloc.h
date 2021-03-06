/*
 * linux/include/asm-arm/arch-iop3xx/vmalloc.h
 */

/*
 * Just any arbitrary offset to the start of the vmalloc VM area: the
 * current 8MB value just means that there will be a 8MB "hole" after the
 * physical memory until the kernel virtual memory starts.  That means that
 * any out-of-bounds memory accesses will hopefully be caught.
 * The vmalloc() routines leaves a hole of 4kB between each vmalloced
 * area for the same reason. ;)
 */
//#define VMALLOC_END       (0xe8000000)
/* increase usable physical RAM to ~992M per RMK */
#define VMALLOC_END       (0xfe000000)

