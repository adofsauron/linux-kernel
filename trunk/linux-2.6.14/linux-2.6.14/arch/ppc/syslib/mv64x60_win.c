/*
 * arch/ppc/syslib/mv64x60_win.c
 *
 * Tables with info on how to manipulate the 32 & 64 bit windows on the
 * various types of Marvell bridge chips.
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * 2004 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/mv643xx.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <asm/delay.h>
#include <asm/mv64x60.h>


/*
 *****************************************************************************
 *
 *	Tables describing how to set up windows on each type of bridge
 *
 *****************************************************************************
 */
struct mv64x60_32bit_window
	gt64260_32bit_windows[MV64x60_32BIT_WIN_COUNT] __initdata = {
	/* CPU->MEM Windows */
	[MV64x60_CPU2MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_0_BASE,
		.size_reg		= MV64x60_CPU2MEM_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_1_BASE,
		.size_reg		= MV64x60_CPU2MEM_1_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_2_BASE,
		.size_reg		= MV64x60_CPU2MEM_2_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_3_BASE,
		.size_reg		= MV64x60_CPU2MEM_3_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->Device Windows */
	[MV64x60_CPU2DEV_0_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_0_BASE,
		.size_reg		= MV64x60_CPU2DEV_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2DEV_1_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_1_BASE,
		.size_reg		= MV64x60_CPU2DEV_1_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2DEV_2_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_2_BASE,
		.size_reg		= MV64x60_CPU2DEV_2_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2DEV_3_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_3_BASE,
		.size_reg		= MV64x60_CPU2DEV_3_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->Boot Window */
	[MV64x60_CPU2BOOT_WIN] = {
		.base_reg		= MV64x60_CPU2BOOT_0_BASE,
		.size_reg		= MV64x60_CPU2BOOT_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 0 Windows */
	[MV64x60_CPU2PCI0_IO_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_IO_BASE,
		.size_reg		= MV64x60_CPU2PCI0_IO_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_0_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_1_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_1_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_2_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_2_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_3_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_3_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 Windows */
	[MV64x60_CPU2PCI1_IO_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_IO_BASE,
		.size_reg		= MV64x60_CPU2PCI1_IO_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_0_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_1_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_1_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_2_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_2_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_3_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_3_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->SRAM Window (64260 has no integrated SRAM) */
	/* CPU->PCI 0 Remap I/O Window */
	[MV64x60_CPU2PCI0_IO_REMAP_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_IO_REMAP,
		.size_reg		= 0,
		.base_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 Remap I/O Window */
	[MV64x60_CPU2PCI1_IO_REMAP_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_IO_REMAP,
		.size_reg		= 0,
		.base_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU Memory Protection Windows */
	[MV64x60_CPU_PROT_0_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_0,
		.size_reg		= MV64x60_CPU_PROT_SIZE_0,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_PROT_1_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_1,
		.size_reg		= MV64x60_CPU_PROT_SIZE_1,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_PROT_2_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_2,
		.size_reg		= MV64x60_CPU_PROT_SIZE_2,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_PROT_3_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_3,
		.size_reg		= MV64x60_CPU_PROT_SIZE_3,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU Snoop Windows */
	[MV64x60_CPU_SNOOP_0_WIN] = {
		.base_reg		= GT64260_CPU_SNOOP_BASE_0,
		.size_reg		= GT64260_CPU_SNOOP_SIZE_0,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_SNOOP_1_WIN] = {
		.base_reg		= GT64260_CPU_SNOOP_BASE_1,
		.size_reg		= GT64260_CPU_SNOOP_SIZE_1,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_SNOOP_2_WIN] = {
		.base_reg		= GT64260_CPU_SNOOP_BASE_2,
		.size_reg		= GT64260_CPU_SNOOP_SIZE_2,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_SNOOP_3_WIN] = {
		.base_reg		= GT64260_CPU_SNOOP_BASE_3,
		.size_reg		= GT64260_CPU_SNOOP_SIZE_3,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 0->System Memory Remap Windows */
	[MV64x60_PCI02MEM_REMAP_0_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_0_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_1_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_2_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_3_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	/* PCI 1->System Memory Remap Windows */
	[MV64x60_PCI12MEM_REMAP_0_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_0_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_1_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_2_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_3_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	/* ENET->SRAM Window (64260 doesn't have separate windows) */
	/* MPSC->SRAM Window (64260 doesn't have separate windows) */
	/* IDMA->SRAM Window (64260 doesn't have separate windows) */
};

struct mv64x60_64bit_window
	gt64260_64bit_windows[MV64x60_64BIT_WIN_COUNT] __initdata = {
	/* CPU->PCI 0 MEM Remap Windows */
	[MV64x60_CPU2PCI0_MEM_0_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_0_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_0_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_1_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_1_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_1_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_2_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_2_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_2_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_3_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_3_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_3_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 MEM Remap Windows */
	[MV64x60_CPU2PCI1_MEM_0_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_0_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_0_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_1_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_1_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_1_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_2_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_2_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_2_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_3_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_3_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_3_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 0->MEM Access Control Windows */
	[MV64x60_PCI02MEM_ACC_CNTL_0_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_0_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_0_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_0_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_1_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_1_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_1_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_1_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_2_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_2_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_2_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_2_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_3_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_3_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_3_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_3_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 1->MEM Access Control Windows */
	[MV64x60_PCI12MEM_ACC_CNTL_0_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_0_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_0_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_0_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_1_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_1_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_1_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_1_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_2_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_2_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_2_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_2_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_3_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_3_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_3_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_3_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 0->MEM Snoop Windows */
	[MV64x60_PCI02MEM_SNOOP_0_WIN] = {
		.base_hi_reg		= GT64260_PCI0_SNOOP_0_BASE_HI,
		.base_lo_reg		= GT64260_PCI0_SNOOP_0_BASE_LO,
		.size_reg		= GT64260_PCI0_SNOOP_0_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_SNOOP_1_WIN] = {
		.base_hi_reg		= GT64260_PCI0_SNOOP_1_BASE_HI,
		.base_lo_reg		= GT64260_PCI0_SNOOP_1_BASE_LO,
		.size_reg		= GT64260_PCI0_SNOOP_1_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_SNOOP_2_WIN] = {
		.base_hi_reg		= GT64260_PCI0_SNOOP_2_BASE_HI,
		.base_lo_reg		= GT64260_PCI0_SNOOP_2_BASE_LO,
		.size_reg		= GT64260_PCI0_SNOOP_2_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_SNOOP_3_WIN] = {
		.base_hi_reg		= GT64260_PCI0_SNOOP_3_BASE_HI,
		.base_lo_reg		= GT64260_PCI0_SNOOP_3_BASE_LO,
		.size_reg		= GT64260_PCI0_SNOOP_3_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 1->MEM Snoop Windows */
	[MV64x60_PCI12MEM_SNOOP_0_WIN] = {
		.base_hi_reg		= GT64260_PCI1_SNOOP_0_BASE_HI,
		.base_lo_reg		= GT64260_PCI1_SNOOP_0_BASE_LO,
		.size_reg		= GT64260_PCI1_SNOOP_0_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_SNOOP_1_WIN] = {
		.base_hi_reg		= GT64260_PCI1_SNOOP_1_BASE_HI,
		.base_lo_reg		= GT64260_PCI1_SNOOP_1_BASE_LO,
		.size_reg		= GT64260_PCI1_SNOOP_1_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_SNOOP_2_WIN] = {
		.base_hi_reg		= GT64260_PCI1_SNOOP_2_BASE_HI,
		.base_lo_reg		= GT64260_PCI1_SNOOP_2_BASE_LO,
		.size_reg		= GT64260_PCI1_SNOOP_2_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_SNOOP_3_WIN] = {
		.base_hi_reg		= GT64260_PCI1_SNOOP_3_BASE_HI,
		.base_lo_reg		= GT64260_PCI1_SNOOP_3_BASE_LO,
		.size_reg		= GT64260_PCI1_SNOOP_3_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
};

struct mv64x60_32bit_window
	mv64360_32bit_windows[MV64x60_32BIT_WIN_COUNT] __initdata = {
	/* CPU->MEM Windows */
	[MV64x60_CPU2MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_0_BASE,
		.size_reg		= MV64x60_CPU2MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 0 },
	[MV64x60_CPU2MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_1_BASE,
		.size_reg		= MV64x60_CPU2MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 1 },
	[MV64x60_CPU2MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_2_BASE,
		.size_reg		= MV64x60_CPU2MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 2 },
	[MV64x60_CPU2MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_3_BASE,
		.size_reg		= MV64x60_CPU2MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 3 },
	/* CPU->Device Windows */
	[MV64x60_CPU2DEV_0_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_0_BASE,
		.size_reg		= MV64x60_CPU2DEV_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 4 },
	[MV64x60_CPU2DEV_1_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_1_BASE,
		.size_reg		= MV64x60_CPU2DEV_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 5 },
	[MV64x60_CPU2DEV_2_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_2_BASE,
		.size_reg		= MV64x60_CPU2DEV_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 6 },
	[MV64x60_CPU2DEV_3_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_3_BASE,
		.size_reg		= MV64x60_CPU2DEV_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 7 },
	/* CPU->Boot Window */
	[MV64x60_CPU2BOOT_WIN] = {
		.base_reg		= MV64x60_CPU2BOOT_0_BASE,
		.size_reg		= MV64x60_CPU2BOOT_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 8 },
	/* CPU->PCI 0 Windows */
	[MV64x60_CPU2PCI0_IO_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_IO_BASE,
		.size_reg		= MV64x60_CPU2PCI0_IO_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 9 },
	[MV64x60_CPU2PCI0_MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_0_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 10 },
	[MV64x60_CPU2PCI0_MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_1_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 11 },
	[MV64x60_CPU2PCI0_MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_2_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 12 },
	[MV64x60_CPU2PCI0_MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_3_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 13 },
	/* CPU->PCI 1 Windows */
	[MV64x60_CPU2PCI1_IO_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_IO_BASE,
		.size_reg		= MV64x60_CPU2PCI1_IO_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 14 },
	[MV64x60_CPU2PCI1_MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_0_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 15 },
	[MV64x60_CPU2PCI1_MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_1_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 16 },
	[MV64x60_CPU2PCI1_MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_2_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 17 },
	[MV64x60_CPU2PCI1_MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_3_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 18 },
	/* CPU->SRAM Window */
	[MV64x60_CPU2SRAM_WIN] = {
		.base_reg		= MV64360_CPU2SRAM_BASE,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUWIN_ENAB | 19 },
	/* CPU->PCI 0 Remap I/O Window */
	[MV64x60_CPU2PCI0_IO_REMAP_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_IO_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 Remap I/O Window */
	[MV64x60_CPU2PCI1_IO_REMAP_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_IO_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU Memory Protection Windows */
	[MV64x60_CPU_PROT_0_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_0,
		.size_reg		= MV64x60_CPU_PROT_SIZE_0,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUPROT_ENAB | 31 },
	[MV64x60_CPU_PROT_1_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_1,
		.size_reg		= MV64x60_CPU_PROT_SIZE_1,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUPROT_ENAB | 31 },
	[MV64x60_CPU_PROT_2_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_2,
		.size_reg		= MV64x60_CPU_PROT_SIZE_2,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUPROT_ENAB | 31 },
	[MV64x60_CPU_PROT_3_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_3,
		.size_reg		= MV64x60_CPU_PROT_SIZE_3,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= MV64x60_EXTRA_CPUPROT_ENAB | 31 },
	/* CPU Snoop Windows -- don't exist on 64360 */
	/* PCI 0->System Memory Remap Windows */
	[MV64x60_PCI02MEM_REMAP_0_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_0_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_1_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_2_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_3_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	/* PCI 1->System Memory Remap Windows */
	[MV64x60_PCI12MEM_REMAP_0_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_0_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_1_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_2_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_3_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	/* ENET->System Memory Windows */
	[MV64x60_ENET2MEM_0_WIN] = {
		.base_reg		= MV64360_ENET2MEM_0_BASE,
		.size_reg		= MV64360_ENET2MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_ENET_ENAB | 0 },
	[MV64x60_ENET2MEM_1_WIN] = {
		.base_reg		= MV64360_ENET2MEM_1_BASE,
		.size_reg		= MV64360_ENET2MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_ENET_ENAB | 1 },
	[MV64x60_ENET2MEM_2_WIN] = {
		.base_reg		= MV64360_ENET2MEM_2_BASE,
		.size_reg		= MV64360_ENET2MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_ENET_ENAB | 2 },
	[MV64x60_ENET2MEM_3_WIN] = {
		.base_reg		= MV64360_ENET2MEM_3_BASE,
		.size_reg		= MV64360_ENET2MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_ENET_ENAB | 3 },
	[MV64x60_ENET2MEM_4_WIN] = {
		.base_reg		= MV64360_ENET2MEM_4_BASE,
		.size_reg		= MV64360_ENET2MEM_4_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_ENET_ENAB | 4 },
	[MV64x60_ENET2MEM_5_WIN] = {
		.base_reg		= MV64360_ENET2MEM_5_BASE,
		.size_reg		= MV64360_ENET2MEM_5_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_ENET_ENAB | 5 },
	/* MPSC->System Memory Windows */
	[MV64x60_MPSC2MEM_0_WIN] = {
		.base_reg		= MV64360_MPSC2MEM_0_BASE,
		.size_reg		= MV64360_MPSC2MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_MPSC_ENAB | 0 },
	[MV64x60_MPSC2MEM_1_WIN] = {
		.base_reg		= MV64360_MPSC2MEM_1_BASE,
		.size_reg		= MV64360_MPSC2MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_MPSC_ENAB | 1 },
	[MV64x60_MPSC2MEM_2_WIN] = {
		.base_reg		= MV64360_MPSC2MEM_2_BASE,
		.size_reg		= MV64360_MPSC2MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_MPSC_ENAB | 2 },
	[MV64x60_MPSC2MEM_3_WIN] = {
		.base_reg		= MV64360_MPSC2MEM_3_BASE,
		.size_reg		= MV64360_MPSC2MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_MPSC_ENAB | 3 },
	/* IDMA->System Memory Windows */
	[MV64x60_IDMA2MEM_0_WIN] = {
		.base_reg		= MV64360_IDMA2MEM_0_BASE,
		.size_reg		= MV64360_IDMA2MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_IDMA_ENAB | 0 },
	[MV64x60_IDMA2MEM_1_WIN] = {
		.base_reg		= MV64360_IDMA2MEM_1_BASE,
		.size_reg		= MV64360_IDMA2MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_IDMA_ENAB | 1 },
	[MV64x60_IDMA2MEM_2_WIN] = {
		.base_reg		= MV64360_IDMA2MEM_2_BASE,
		.size_reg		= MV64360_IDMA2MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_IDMA_ENAB | 2 },
	[MV64x60_IDMA2MEM_3_WIN] = {
		.base_reg		= MV64360_IDMA2MEM_3_BASE,
		.size_reg		= MV64360_IDMA2MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_IDMA_ENAB | 3 },
	[MV64x60_IDMA2MEM_4_WIN] = {
		.base_reg		= MV64360_IDMA2MEM_4_BASE,
		.size_reg		= MV64360_IDMA2MEM_4_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_IDMA_ENAB | 4 },
	[MV64x60_IDMA2MEM_5_WIN] = {
		.base_reg		= MV64360_IDMA2MEM_5_BASE,
		.size_reg		= MV64360_IDMA2MEM_5_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_IDMA_ENAB | 5 },
	[MV64x60_IDMA2MEM_6_WIN] = {
		.base_reg		= MV64360_IDMA2MEM_6_BASE,
		.size_reg		= MV64360_IDMA2MEM_6_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_IDMA_ENAB | 6 },
	[MV64x60_IDMA2MEM_7_WIN] = {
		.base_reg		= MV64360_IDMA2MEM_7_BASE,
		.size_reg		= MV64360_IDMA2MEM_7_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_IDMA_ENAB | 7 },
};

struct mv64x60_64bit_window
	mv64360_64bit_windows[MV64x60_64BIT_WIN_COUNT] __initdata = {
	/* CPU->PCI 0 MEM Remap Windows */
	[MV64x60_CPU2PCI0_MEM_0_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_0_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_0_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_1_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_1_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_1_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_2_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_2_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_2_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_3_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_3_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_3_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 MEM Remap Windows */
	[MV64x60_CPU2PCI1_MEM_0_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_0_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_0_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_1_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_1_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_1_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_2_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_2_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_2_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_3_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_3_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_3_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 0->MEM Access Control Windows */
	[MV64x60_PCI02MEM_ACC_CNTL_0_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_0_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_0_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_0_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_PCIACC_ENAB | 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_1_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_1_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_1_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_1_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_PCIACC_ENAB | 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_2_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_2_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_2_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_2_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_PCIACC_ENAB | 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_3_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_3_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_3_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_3_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_PCIACC_ENAB | 0 },
	/* PCI 1->MEM Access Control Windows */
	[MV64x60_PCI12MEM_ACC_CNTL_0_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_0_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_0_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_0_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_PCIACC_ENAB | 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_1_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_1_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_1_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_1_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_PCIACC_ENAB | 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_2_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_2_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_2_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_2_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_PCIACC_ENAB | 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_3_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_3_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_3_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_3_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= MV64x60_EXTRA_PCIACC_ENAB | 0 },
	/* PCI 0->MEM Snoop Windows -- don't exist on 64360 */
	/* PCI 1->MEM Snoop Windows -- don't exist on 64360 */
};
