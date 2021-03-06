/*
 * Copyright 2003 PathScale, Inc.
 *
 * Licensed under the GPL
 */

#ifndef __UM_PROCESSOR_X86_64_H
#define __UM_PROCESSOR_X86_64_H

/* include faultinfo structure */
#include "sysdep/faultinfo.h"

struct arch_thread {
        unsigned long debugregs[8];
        int debugregs_seq;
        struct faultinfo faultinfo;
};

/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
extern inline void rep_nop(void)
{
	__asm__ __volatile__("rep;nop": : :"memory");
}

#define cpu_relax()   rep_nop()

#define INIT_ARCH_THREAD { .debugregs  		= { [ 0 ... 7 ] = 0 }, \
                           .debugregs_seq	= 0, \
                           .faultinfo		= { 0, 0, 0 } }

#include "asm/arch/user.h"

#define current_text_addr() \
	({ void *pc; __asm__("movq $1f,%0\n1:":"=g" (pc)); pc; })

#define ARCH_IS_STACKGROW(address) \
        (address + 128 >= UPT_SP(&current->thread.regs.regs))

#define KSTK_EIP(tsk) KSTK_REG(tsk, RIP)
#define KSTK_ESP(tsk) KSTK_REG(tsk, RSP)

#include "asm/processor-generic.h"

#endif
