/*
 *
 * BRIEF MODULE DESCRIPTION
 *	PB1000 board setup
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <linux/config.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>

int prom_argc;
char **prom_argv, **prom_envp;
extern void  __init prom_init_cmdline(void);
extern char *prom_getenv(char *envname);

const char *get_system_type(void)
{
#ifdef CONFIG_MIPS_BOSPORUS
	return "Alchemy Bosporus Gateway Reference";
#else
	return "Alchemy Db1x00";
#endif
}

int __init prom_init(int argc, char **argv, char **envp, int *prom_vec)
{
	unsigned char *memsize_str;
	unsigned long memsize;

	prom_argc = argc;
	prom_argv = argv;
	prom_envp = envp;

	mips_machgroup = MACH_GROUP_ALCHEMY;
	mips_machtype = MACH_DB1000;	/* set the platform # */
	prom_init_cmdline();

	memsize_str = prom_getenv("memsize");
	if (!memsize_str) {
		memsize = 0x04000000;
	} else {
		memsize = simple_strtol(memsize_str, NULL, 0);
	}
	add_memory_region(0, memsize, BOOT_MEM_RAM);
	return 0;
}
