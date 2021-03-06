/*
 * include/asm-xtensa/unistd.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_UNISTD_H
#define _XTENSA_UNISTD_H

#include <linux/linkage.h>

#define __NR_spill		  0
#define __NR_exit		  1
#define __NR_read		  3
#define __NR_write		  4
#define __NR_open		  5
#define __NR_close		  6
#define __NR_creat		  8
#define __NR_link		  9
#define __NR_unlink		 10
#define __NR_execve		 11
#define __NR_chdir		 12
#define __NR_mknod		 14
#define __NR_chmod		 15
#define __NR_lchown		 16
#define __NR_break		 17
#define __NR_lseek		 19
#define __NR_getpid		 20
#define __NR_mount		 21
#define __NR_setuid		 23
#define __NR_getuid		 24
#define __NR_ptrace		 26
#define __NR_utime		 30
#define __NR_stty		 31
#define __NR_gtty		 32
#define __NR_access		 33
#define __NR_ftime		 35
#define __NR_sync		 36
#define __NR_kill		 37
#define __NR_rename		 38
#define __NR_mkdir		 39
#define __NR_rmdir		 40
#define __NR_dup		 41
#define __NR_pipe		 42
#define __NR_times		 43
#define __NR_prof		 44
#define __NR_brk		 45
#define __NR_setgid		 46
#define __NR_getgid		 47
#define __NR_signal		 48
#define __NR_geteuid		 49
#define __NR_getegid		 50
#define __NR_acct		 51
#define __NR_lock		 53
#define __NR_ioctl		 54
#define __NR_fcntl		 55
#define __NR_setpgid		 57
#define __NR_ulimit		 58
#define __NR_umask		 60
#define __NR_chroot		 61
#define __NR_ustat		 62
#define __NR_dup2		 63
#define __NR_getppid		 64
#define __NR_setsid		 66
#define __NR_sigaction		 67
#define __NR_setreuid		 70
#define __NR_setregid		 71
#define __NR_sigsuspend		 72
#define __NR_sigpending		 73
#define __NR_sethostname	 74
#define __NR_setrlimit		 75
#define __NR_getrlimit	 	 76	/* Back compatible 2Gig limited rlimit */
#define __NR_getrusage		 77
#define __NR_gettimeofday	 78
#define __NR_settimeofday	 79
#define __NR_getgroups		 80
#define __NR_setgroups		 81
#define __NR_select		 82
#define __NR_symlink		 83
#define __NR_readlink		 85
#define __NR_uselib		 86
#define __NR_swapon		 87
#define __NR_reboot		 88
#define __NR_munmap		 91
#define __NR_truncate		 92
#define __NR_ftruncate		 93
#define __NR_fchmod		 94
#define __NR_fchown		 95
#define __NR_getpriority	 96
#define __NR_setpriority	 97
#define __NR_profil		 98
#define __NR_statfs		 99
#define __NR_fstatfs		100
#define __NR_ioperm		101
#define __NR_syslog		103
#define __NR_setitimer		104
#define __NR_getitimer		105
#define __NR_stat		106
#define __NR_lstat		107
#define __NR_fstat		108
#define __NR_iopl		110
#define __NR_vhangup		111
#define __NR_idle		112
#define __NR_wait4		114
#define __NR_swapoff		115
#define __NR_sysinfo		116
#define __NR_fsync		118
#define __NR_sigreturn		119
#define __NR_clone		120
#define __NR_setdomainname	121
#define __NR_uname		122
#define __NR_modify_ldt		123
#define __NR_adjtimex		124
#define __NR_mprotect		125
#define __NR_create_module	127
#define __NR_init_module	128
#define __NR_delete_module	129
#define __NR_quotactl		131
#define __NR_getpgid		132
#define __NR_fchdir		133
#define __NR_bdflush		134
#define __NR_sysfs		135
#define __NR_personality	136
#define __NR_setfsuid		138
#define __NR_setfsgid		139
#define __NR__llseek		140
#define __NR_getdents		141
#define __NR__newselect		142
#define __NR_flock		143
#define __NR_msync		144
#define __NR_readv		145
#define __NR_writev		146
#define __NR_cacheflush         147
#define __NR_cachectl           148
#define __NR_sysxtensa          149
#define __NR_sysdummy           150
#define __NR_getsid		151
#define __NR_fdatasync		152
#define __NR__sysctl		153
#define __NR_mlock		154
#define __NR_munlock		155
#define __NR_mlockall		156
#define __NR_munlockall		157
#define __NR_sched_setparam		158
#define __NR_sched_getparam		159
#define __NR_sched_setscheduler		160
#define __NR_sched_getscheduler		161
#define __NR_sched_yield		162
#define __NR_sched_get_priority_max	163
#define __NR_sched_get_priority_min	164
#define __NR_sched_rr_get_interval	165
#define __NR_nanosleep		166
#define __NR_mremap		167
#define __NR_accept             168
#define __NR_bind               169
#define __NR_connect            170
#define __NR_getpeername        171
#define __NR_getsockname        172
#define __NR_getsockopt         173
#define __NR_listen             174
#define __NR_recv               175
#define __NR_recvfrom           176
#define __NR_recvmsg            177
#define __NR_send               178
#define __NR_sendmsg            179
#define __NR_sendto             180
#define __NR_setsockopt         181
#define __NR_shutdown           182
#define __NR_socket             183
#define __NR_socketpair         184
#define __NR_setresuid		185
#define __NR_getresuid		186
#define __NR_query_module	187
#define __NR_poll		188
#define __NR_nfsservctl		189
#define __NR_setresgid		190
#define __NR_getresgid		191
#define __NR_prctl              192
#define __NR_rt_sigreturn	193
#define __NR_rt_sigaction	194
#define __NR_rt_sigprocmask	195
#define __NR_rt_sigpending	196
#define __NR_rt_sigtimedwait	197
#define __NR_rt_sigqueueinfo	198
#define __NR_rt_sigsuspend	199
#define __NR_pread		200
#define __NR_pwrite		201
#define __NR_chown		202
#define __NR_getcwd		203
#define __NR_capget		204
#define __NR_capset		205
#define __NR_sigaltstack	206
#define __NR_sendfile		207
#define __NR_mmap2		210
#define __NR_truncate64		211
#define __NR_ftruncate64	212
#define __NR_stat64		213
#define __NR_lstat64		214
#define __NR_fstat64		215
#define __NR_pivot_root		216
#define __NR_mincore		217
#define __NR_madvise		218
#define __NR_getdents64		219

/* Keep this last; should always equal the last valid call number. */
#define __NR_Linux_syscalls     220

/* user-visible error numbers are in the range -1 - -125: see
 * <asm-xtensa/errno.h> */

#define SYSXTENSA_RESERVED	   0	/* don't use this */
#define SYSXTENSA_ATOMIC_SET	   1	/* set variable */
#define SYSXTENSA_ATOMIC_EXG_ADD   2	/* exchange memory and add */
#define SYSXTENSA_ATOMIC_ADD	   3	/* add to memory */
#define SYSXTENSA_ATOMIC_CMP_SWP   4	/* compare and swap */

#define SYSXTENSA_COUNT		   5	/* count of syscall0 functions*/

#ifdef __KERNEL__
#define __syscall_return(type, res) return ((type)(res))
#else
#define __syscall_return(type, res) \
do { \
	if ((unsigned long)(res) >= (unsigned long)(-125)) { \
	/* Avoid using "res" which is declared to be in register r2; \
	 * errno might expand to a function call and clobber it.  */ \
		int __err = -(res); \
		errno = __err; \
		res = -1; \
	} \
	return (type) (res); \
} while (0)
#endif


/* Tensilica's xt-xcc compiler is much more agressive at code
 * optimization than gcc.  Multiple __asm__ statements are
 * insufficient for xt-xcc because subsequent optimization passes
 * (beyond the front-end that knows of __asm__ statements and other
 * such GNU Extensions to C) can modify the register selection for
 * containment of C variables.
 *
 * xt-xcc cannot modify the contents of a single __asm__ statement, so
 * we create single-asm versions of the syscall macros that are
 * suitable and optimal for both xt-xcc and gcc.
 *
 * Linux takes system-call arguments in registers.  The following
 * design is optimized for user-land apps (e.g., glibc) which
 * typically have a function wrapper around the "syscall" assembly
 * instruction.  It satisfies the Xtensa ABI while minizing argument
 * shifting.
 *
 * The Xtensa ABI and software conventions require the system-call
 * number in a2.  If an argument exists in a2, we move it to the next
 * available register.  Note that for improved efficiency, we do NOT
 * shift all parameters down one register to maintain the original
 * order.
 *
 * At best case (zero arguments), we just write the syscall number to
 * a2.  At worst case (1 to 6 arguments), we move the argument in a2
 * to the next available register, then write the syscall number to
 * a2.
 *
 * For clarity, the following truth table enumerates all possibilities.
 *
 * arguments	syscall number	arg0, arg1, arg2, arg3, arg4, arg5
 * ---------	--------------	----------------------------------
 *	0	      a2
 *	1	      a2	a3
 *	2	      a2	a4,   a3
 *	3	      a2	a5,   a3,   a4
 *	4	      a2	a6,   a3,   a4,   a5
 *	5	      a2	a7,   a3,   a4,   a5,   a6
 *	6	      a2	a8,   a3,   a4,   a5,   a6,   a7
 */

#define _syscall0(type,name) \
type name(void) \
{ \
long __res; \
__asm__ __volatile__ ( \
	"  movi  a2, %1 \n" \
	"  syscall      \n" \
	"  mov   %0, a2 \n" \
	: "=a" (__res) \
	: "i" (__NR_##name) \
	: "a2" \
	); \
__syscall_return(type,__res); \
}

#define _syscall1(type,name,type0,arg0) \
type name(type0 arg0) \
{ \
long __res; \
__asm__ __volatile__ ( \
	"  mov   a3, %2 \n" \
	"  movi  a2, %1 \n" \
	"  syscall      \n" \
	"  mov   %0, a2 \n" \
	: "=a" (__res) \
	: "i" (__NR_##name), "a" (arg0) \
	: "a2", "a3" \
	); \
__syscall_return(type,__res); \
}

#define _syscall2(type,name,type0,arg0,type1,arg1) \
type name(type0 arg0,type1 arg1) \
{ \
long __res; \
__asm__ __volatile__ ( \
	"  mov   a4, %2 \n" \
	"  mov   a3, %3 \n" \
	"  movi  a2, %1 \n" \
	"  syscall      \n" \
	"  mov   %0, a2 \n" \
	: "=a" (__res) \
	: "i" (__NR_##name), "a" (arg0), "a" (arg1) \
	: "a2", "a3", "a4" \
	); \
__syscall_return(type,__res); \
}

#define _syscall3(type,name,type0,arg0,type1,arg1,type2,arg2) \
type name(type0 arg0,type1 arg1,type2 arg2) \
{ \
long __res; \
__asm__ __volatile__ ( \
	"  mov   a5, %2 \n" \
	"  mov   a4, %4 \n" \
	"  mov   a3, %3 \n" \
	"  movi  a2, %1 \n" \
	"  syscall      \n" \
	"  mov   %0, a2 \n" \
	: "=a" (__res) \
	: "i" (__NR_##name), "a" (arg0), "a" (arg1), "a" (arg2) \
	: "a2", "a3", "a4", "a5" \
	); \
__syscall_return(type,__res); \
}

#define _syscall4(type,name,type0,arg0,type1,arg1,type2,arg2,type3,arg3) \
type name(type0 arg0,type1 arg1,type2 arg2,type3 arg3) \
{ \
long __res; \
__asm__ __volatile__ ( \
	"  mov   a6, %2 \n" \
	"  mov   a5, %5 \n" \
	"  mov   a4, %4 \n" \
	"  mov   a3, %3 \n" \
	"  movi  a2, %1 \n" \
	"  syscall      \n" \
	"  mov   %0, a2 \n" \
	: "=a" (__res) \
	: "i" (__NR_##name), "a" (arg0), "a" (arg1), "a" (arg2), "a" (arg3) \
	: "a2", "a3", "a4", "a5", "a6" \
	); \
__syscall_return(type,__res); \
}

/* Note that we save and restore the a7 frame pointer.
 * Including a7 in the clobber list doesn't do what you'd expect.
 */
#define _syscall5(type,name,type0,arg0,type1,arg1,type2,arg2,type3,arg3,type4,arg4) \
type name(type0 arg0,type1 arg1,type2 arg2,type3 arg3,type4 arg4) \
{ \
long __res; \
__asm__ __volatile__ ( \
	"  mov   a9, a7 \n" \
	"  mov   a7, %2 \n" \
	"  mov   a6, %6 \n" \
	"  mov   a5, %5 \n" \
	"  mov   a4, %4 \n" \
	"  mov   a3, %3 \n" \
	"  movi  a2, %1 \n" \
	"  syscall      \n" \
	"  mov   a7, a9 \n" \
	"  mov   %0, a2 \n" \
	: "=a" (__res) \
	: "i" (__NR_##name), "a" (arg0), "a" (arg1), "a" (arg2), \
                             "a" (arg3), "a" (arg4) \
	: "a2", "a3", "a4", "a5", "a6", "a9" \
	); \
__syscall_return(type,__res); \
}

/* Note that we save and restore the a7 frame pointer.
 * Including a7 in the clobber list doesn't do what you'd expect.
 */
#define _syscall6(type,name,type0,arg0,type1,arg1,type2,arg2,type3,arg3,type4,arg4,type5,arg5) \
type name(type0 arg0,type1 arg1,type2 arg2,type3 arg3,type4 arg4,type5 arg5) \
{ \
long __res; \
__asm__ __volatile__ ( \
	"  mov   a9, a7 \n" \
	"  mov   a8, %2 \n" \
	"  mov   a7, %7 \n" \
	"  mov   a6, %6 \n" \
	"  mov   a5, %5 \n" \
	"  mov   a4, %4 \n" \
	"  mov   a3, %3 \n" \
	"  movi  a2, %1 \n" \
	"  syscall      \n" \
	"  mov   a7, a9 \n" \
	"  mov   %0, a2 \n" \
	: "=a" (__res) \
	: "i" (__NR_##name), "a" (arg0), "a" (arg1), "a" (arg2), \
                             "a" (arg3), "a" (arg4), "a" (arg5)  \
	: "a2", "a3", "a4", "a5", "a6", "a8", "a9" \
	); \
__syscall_return(type,__res); \
}


#ifdef __KERNEL_SYSCALLS__
static __inline__ _syscall3(int,execve,const char*,file,char**,argv,char**,envp)
#endif

/*
 * "Conditional" syscalls
 *
 * What we want is __attribute__((weak,alias("sys_ni_syscall"))),
 * but it doesn't work on all toolchains, so we just do it by hand
 */
#define cond_syscall(x) asm(".weak\t" #x "\n\t.set\t" #x ",sys_ni_syscall");

#ifdef __KERNEL__
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_SYS_UTIME
#define __ARCH_WANT_SYS_LLSEEK
#define __ARCH_WANT_SYS_RT_SIGACTION
#endif

#endif	/* _XTENSA_UNISTD_H */
