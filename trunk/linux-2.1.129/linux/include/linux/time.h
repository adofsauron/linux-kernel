#ifndef _LINUX_TIME_H
#define _LINUX_TIME_H

#include <asm/param.h>
#include <linux/types.h>

#ifndef _STRUCT_TIMESPEC
#define _STRUCT_TIMESPEC
struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* nanoseconds */
};
#endif /* _STRUCT_TIMESPEC */

/*
 * change timeval to jiffies, trying to avoid the
 * most obvious overflows..
 */
static __inline__ unsigned long
timespec_to_jiffies(struct timespec *value)
{
	unsigned long sec = value->tv_sec;
	long nsec = value->tv_nsec;

	if (sec > ((long)(~0UL >> 1) / HZ))
		return ~0UL >> 1;
	nsec += 1000000000L / HZ - 1;
	nsec /= 1000000000L / HZ;
	return HZ * sec + nsec;
}

static __inline__ void
jiffies_to_timespec(unsigned long jiffies, struct timespec *value)
{
	value->tv_nsec = (jiffies % HZ) * (1000000000L / HZ);
	value->tv_sec = jiffies / HZ;
}
 
struct timeval {
	time_t		tv_sec;		/* seconds */
	suseconds_t	tv_usec;	/* microseconds */
};

struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};

#define NFDBITS			__NFDBITS

#ifdef __KERNEL__
extern void do_gettimeofday(struct timeval *tv);
extern void do_settimeofday(struct timeval *tv);
extern void get_fast_time(struct timeval *tv);
extern void (*do_get_fast_time)(struct timeval *);
#endif

#define FD_SETSIZE		__FD_SETSIZE
#define FD_SET(fd,fdsetp)	__FD_SET(fd,fdsetp)
#define FD_CLR(fd,fdsetp)	__FD_CLR(fd,fdsetp)
#define FD_ISSET(fd,fdsetp)	__FD_ISSET(fd,fdsetp)
#define FD_ZERO(fdsetp)		__FD_ZERO(fdsetp)

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2

struct  itimerspec {
        struct  timespec it_interval;    /* timer period */
        struct  timespec it_value;       /* timer expiration */
};

struct	itimerval {
	struct	timeval it_interval;	/* timer interval */
	struct	timeval it_value;	/* current value */
};

#endif
