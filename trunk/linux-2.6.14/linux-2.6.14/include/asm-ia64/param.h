#ifndef _ASM_IA64_PARAM_H
#define _ASM_IA64_PARAM_H

/*
 * Fundamental kernel parameters.
 *
 * Based on <asm-i386/param.h>.
 *
 * Modified 1998, 1999, 2002-2003
 *	David Mosberger-Tang <davidm@hpl.hp.com>, Hewlett-Packard Co
 */

#define EXEC_PAGESIZE	65536

#ifndef NOGROUP
# define NOGROUP	(-1)
#endif

#define MAXHOSTNAMELEN	64	/* max length of hostname */

#ifdef __KERNEL__
# include <linux/config.h>	/* mustn't include <linux/config.h> outside of #ifdef __KERNEL__ */
# ifdef CONFIG_IA64_HP_SIM
  /*
   * Yeah, simulating stuff is slow, so let us catch some breath between
   * timer interrupts...
   */
#  define HZ	  32
# else
#  define HZ	CONFIG_HZ
# endif
# define USER_HZ	HZ
# define CLOCKS_PER_SEC	HZ	/* frequency at which times() counts */
#else
   /*
    * Technically, this is wrong, but some old apps still refer to it.  The proper way to
    * get the HZ value is via sysconf(_SC_CLK_TCK).
    */
# define HZ 1024
#endif

#endif /* _ASM_IA64_PARAM_H */
