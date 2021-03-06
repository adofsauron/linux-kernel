/*
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	stevel@mvista.com or source@mvista.com
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Register offsets of the MIPS GT96100 Advanced Communication Controller.
 */
#ifndef _GT96100_H
#define _GT96100_H

/*
 * Galileo GT96100 internal register base.
 */
#define MIPS_GT96100_BASE (KSEG1ADDR(0x14000000))

#define GT96100_WRITE(ofs, data) \
    *(volatile u32 *)(MIPS_GT96100_BASE+ofs) = cpu_to_le32(data)
#define GT96100_READ(ofs) \
    le32_to_cpu(*(volatile u32 *)(MIPS_GT96100_BASE+ofs))

#define GT96100_ETH_IO_SIZE 0x4000

/************************************************************************
 *  Register offset addresses follow
 ************************************************************************/

/* CPU Interface Control Registers */
#define GT96100_CPU_INTERF_CONFIG 0x000000

/* Ethernet Ports */
#define GT96100_ETH_PHY_ADDR_REG             0x080800
#define GT96100_ETH_SMI_REG                  0x080810
/*
  These are offsets to port 0 registers. Add GT96100_ETH_IO_SIZE to
  get offsets to port 1 registers.
*/
#define GT96100_ETH_PORT_CONFIG          0x084800
#define GT96100_ETH_PORT_CONFIG_EXT      0x084808
#define GT96100_ETH_PORT_COMM            0x084810
#define GT96100_ETH_PORT_STATUS          0x084818
#define GT96100_ETH_SER_PARAM            0x084820
#define GT96100_ETH_HASH_TBL_PTR         0x084828
#define GT96100_ETH_FLOW_CNTRL_SRC_ADDR_L    0x084830
#define GT96100_ETH_FLOW_CNTRL_SRC_ADDR_H    0x084838
#define GT96100_ETH_SDMA_CONFIG          0x084840
#define GT96100_ETH_SDMA_COMM            0x084848
#define GT96100_ETH_INT_CAUSE            0x084850
#define GT96100_ETH_INT_MASK             0x084858
#define GT96100_ETH_1ST_RX_DESC_PTR0         0x084880
#define GT96100_ETH_1ST_RX_DESC_PTR1         0x084884
#define GT96100_ETH_1ST_RX_DESC_PTR2         0x084888
#define GT96100_ETH_1ST_RX_DESC_PTR3         0x08488C
#define GT96100_ETH_CURR_RX_DESC_PTR0        0x0848A0
#define GT96100_ETH_CURR_RX_DESC_PTR1        0x0848A4
#define GT96100_ETH_CURR_RX_DESC_PTR2        0x0848A8
#define GT96100_ETH_CURR_RX_DESC_PTR3        0x0848AC
#define GT96100_ETH_CURR_TX_DESC_PTR0        0x0848E0
#define GT96100_ETH_CURR_TX_DESC_PTR1        0x0848E4
#define GT96100_ETH_MIB_COUNT_BASE           0x085800

/* SDMAs */
#define GT96100_SDMA_GROUP_CONFIG           0x101AF0
/* SDMA Group 0 */
#define GT96100_SDMA_G0_CHAN0_CONFIG        0x000900
#define GT96100_SDMA_G0_CHAN0_COMM          0x000908
#define GT96100_SDMA_G0_CHAN0_RX_DESC_BASE      0x008900
#define GT96100_SDMA_G0_CHAN0_CURR_RX_DESC_PTR  0x008910
#define GT96100_SDMA_G0_CHAN0_TX_DESC_BASE      0x00C900
#define GT96100_SDMA_G0_CHAN0_CURR_TX_DESC_PTR  0x00C910
#define GT96100_SDMA_G0_CHAN0_1ST_TX_DESC_PTR   0x00C914
#define GT96100_SDMA_G0_CHAN1_CONFIG        0x010900
#define GT96100_SDMA_G0_CHAN1_COMM          0x010908
#define GT96100_SDMA_G0_CHAN1_RX_DESC_BASE      0x018900
#define GT96100_SDMA_G0_CHAN1_CURR_RX_DESC_PTR  0x018910
#define GT96100_SDMA_G0_CHAN1_TX_DESC_BASE      0x01C900
#define GT96100_SDMA_G0_CHAN1_CURR_TX_DESC_PTR  0x01C910
#define GT96100_SDMA_G0_CHAN1_1ST_TX_DESC_PTR   0x01C914
#define GT96100_SDMA_G0_CHAN2_CONFIG        0x020900
#define GT96100_SDMA_G0_CHAN2_COMM          0x020908
#define GT96100_SDMA_G0_CHAN2_RX_DESC_BASE      0x028900
#define GT96100_SDMA_G0_CHAN2_CURR_RX_DESC_PTR  0x028910
#define GT96100_SDMA_G0_CHAN2_TX_DESC_BASE      0x02C900
#define GT96100_SDMA_G0_CHAN2_CURR_TX_DESC_PTR  0x02C910
#define GT96100_SDMA_G0_CHAN2_1ST_TX_DESC_PTR   0x02C914
#define GT96100_SDMA_G0_CHAN3_CONFIG        0x030900
#define GT96100_SDMA_G0_CHAN3_COMM          0x030908
#define GT96100_SDMA_G0_CHAN3_RX_DESC_BASE      0x038900
#define GT96100_SDMA_G0_CHAN3_CURR_RX_DESC_PTR  0x038910
#define GT96100_SDMA_G0_CHAN3_TX_DESC_BASE      0x03C900
#define GT96100_SDMA_G0_CHAN3_CURR_TX_DESC_PTR  0x03C910
#define GT96100_SDMA_G0_CHAN3_1ST_TX_DESC_PTR   0x03C914
#define GT96100_SDMA_G0_CHAN4_CONFIG        0x040900
#define GT96100_SDMA_G0_CHAN4_COMM          0x040908
#define GT96100_SDMA_G0_CHAN4_RX_DESC_BASE      0x048900
#define GT96100_SDMA_G0_CHAN4_CURR_RX_DESC_PTR  0x048910
#define GT96100_SDMA_G0_CHAN4_TX_DESC_BASE      0x04C900
#define GT96100_SDMA_G0_CHAN4_CURR_TX_DESC_PTR  0x04C910
#define GT96100_SDMA_G0_CHAN4_1ST_TX_DESC_PTR   0x04C914
#define GT96100_SDMA_G0_CHAN5_CONFIG        0x050900
#define GT96100_SDMA_G0_CHAN5_COMM          0x050908
#define GT96100_SDMA_G0_CHAN5_RX_DESC_BASE      0x058900
#define GT96100_SDMA_G0_CHAN5_CURR_RX_DESC_PTR  0x058910
#define GT96100_SDMA_G0_CHAN5_TX_DESC_BASE      0x05C900
#define GT96100_SDMA_G0_CHAN5_CURR_TX_DESC_PTR  0x05C910
#define GT96100_SDMA_G0_CHAN5_1ST_TX_DESC_PTR   0x05C914
#define GT96100_SDMA_G0_CHAN6_CONFIG        0x060900
#define GT96100_SDMA_G0_CHAN6_COMM          0x060908
#define GT96100_SDMA_G0_CHAN6_RX_DESC_BASE      0x068900
#define GT96100_SDMA_G0_CHAN6_CURR_RX_DESC_PTR  0x068910
#define GT96100_SDMA_G0_CHAN6_TX_DESC_BASE      0x06C900
#define GT96100_SDMA_G0_CHAN6_CURR_TX_DESC_PTR  0x06C910
#define GT96100_SDMA_G0_CHAN6_1ST_TX_DESC_PTR   0x06C914
#define GT96100_SDMA_G0_CHAN7_CONFIG        0x070900
#define GT96100_SDMA_G0_CHAN7_COMM          0x070908
#define GT96100_SDMA_G0_CHAN7_RX_DESC_BASE      0x078900
#define GT96100_SDMA_G0_CHAN7_CURR_RX_DESC_PTR  0x078910
#define GT96100_SDMA_G0_CHAN7_TX_DESC_BASE      0x07C900
#define GT96100_SDMA_G0_CHAN7_CURR_TX_DESC_PTR  0x07C910
#define GT96100_SDMA_G0_CHAN7_1ST_TX_DESC_PTR   0x07C914
/* SDMA Group 1 */
#define GT96100_SDMA_G1_CHAN0_CONFIG        0x100900
#define GT96100_SDMA_G1_CHAN0_COMM          0x100908
#define GT96100_SDMA_G1_CHAN0_RX_DESC_BASE      0x108900
#define GT96100_SDMA_G1_CHAN0_CURR_RX_DESC_PTR  0x108910
#define GT96100_SDMA_G1_CHAN0_TX_DESC_BASE      0x10C900
#define GT96100_SDMA_G1_CHAN0_CURR_TX_DESC_PTR  0x10C910
#define GT96100_SDMA_G1_CHAN0_1ST_TX_DESC_PTR   0x10C914
#define GT96100_SDMA_G1_CHAN1_CONFIG        0x110900
#define GT96100_SDMA_G1_CHAN1_COMM          0x110908
#define GT96100_SDMA_G1_CHAN1_RX_DESC_BASE      0x118900
#define GT96100_SDMA_G1_CHAN1_CURR_RX_DESC_PTR  0x118910
#define GT96100_SDMA_G1_CHAN1_TX_DESC_BASE      0x11C900
#define GT96100_SDMA_G1_CHAN1_CURR_TX_DESC_PTR  0x11C910
#define GT96100_SDMA_G1_CHAN1_1ST_TX_DESC_PTR   0x11C914
#define GT96100_SDMA_G1_CHAN2_CONFIG        0x120900
#define GT96100_SDMA_G1_CHAN2_COMM          0x120908
#define GT96100_SDMA_G1_CHAN2_RX_DESC_BASE      0x128900
#define GT96100_SDMA_G1_CHAN2_CURR_RX_DESC_PTR  0x128910
#define GT96100_SDMA_G1_CHAN2_TX_DESC_BASE      0x12C900
#define GT96100_SDMA_G1_CHAN2_CURR_TX_DESC_PTR  0x12C910
#define GT96100_SDMA_G1_CHAN2_1ST_TX_DESC_PTR   0x12C914
#define GT96100_SDMA_G1_CHAN3_CONFIG        0x130900
#define GT96100_SDMA_G1_CHAN3_COMM          0x130908
#define GT96100_SDMA_G1_CHAN3_RX_DESC_BASE      0x138900
#define GT96100_SDMA_G1_CHAN3_CURR_RX_DESC_PTR  0x138910
#define GT96100_SDMA_G1_CHAN3_TX_DESC_BASE      0x13C900
#define GT96100_SDMA_G1_CHAN3_CURR_TX_DESC_PTR  0x13C910
#define GT96100_SDMA_G1_CHAN3_1ST_TX_DESC_PTR   0x13C914
#define GT96100_SDMA_G1_CHAN4_CONFIG        0x140900
#define GT96100_SDMA_G1_CHAN4_COMM          0x140908
#define GT96100_SDMA_G1_CHAN4_RX_DESC_BASE      0x148900
#define GT96100_SDMA_G1_CHAN4_CURR_RX_DESC_PTR  0x148910
#define GT96100_SDMA_G1_CHAN4_TX_DESC_BASE      0x14C900
#define GT96100_SDMA_G1_CHAN4_CURR_TX_DESC_PTR  0x14C910
#define GT96100_SDMA_G1_CHAN4_1ST_TX_DESC_PTR   0x14C914
#define GT96100_SDMA_G1_CHAN5_CONFIG        0x150900
#define GT96100_SDMA_G1_CHAN5_COMM          0x150908
#define GT96100_SDMA_G1_CHAN5_RX_DESC_BASE      0x158900
#define GT96100_SDMA_G1_CHAN5_CURR_RX_DESC_PTR  0x158910
#define GT96100_SDMA_G1_CHAN5_TX_DESC_BASE      0x15C900
#define GT96100_SDMA_G1_CHAN5_CURR_TX_DESC_PTR  0x15C910
#define GT96100_SDMA_G1_CHAN5_1ST_TX_DESC_PTR   0x15C914
#define GT96100_SDMA_G1_CHAN6_CONFIG        0x160900
#define GT96100_SDMA_G1_CHAN6_COMM          0x160908
#define GT96100_SDMA_G1_CHAN6_RX_DESC_BASE      0x168900
#define GT96100_SDMA_G1_CHAN6_CURR_RX_DESC_PTR  0x168910
#define GT96100_SDMA_G1_CHAN6_TX_DESC_BASE      0x16C900
#define GT96100_SDMA_G1_CHAN6_CURR_TX_DESC_PTR  0x16C910
#define GT96100_SDMA_G1_CHAN6_1ST_TX_DESC_PTR   0x16C914
#define GT96100_SDMA_G1_CHAN7_CONFIG        0x170900
#define GT96100_SDMA_G1_CHAN7_COMM          0x170908
#define GT96100_SDMA_G1_CHAN7_RX_DESC_BASE      0x178900
#define GT96100_SDMA_G1_CHAN7_CURR_RX_DESC_PTR  0x178910
#define GT96100_SDMA_G1_CHAN7_TX_DESC_BASE      0x17C900
#define GT96100_SDMA_G1_CHAN7_CURR_TX_DESC_PTR  0x17C910
#define GT96100_SDMA_G1_CHAN7_1ST_TX_DESC_PTR   0x17C914
/*  MPSCs  */
#define GT96100_MPSC0_MAIN_CONFIG_LOW   0x000A00
#define GT96100_MPSC0_MAIN_CONFIG_HIGH  0x000A04
#define GT96100_MPSC0_PROTOCOL_CONFIG   0x000A08
#define GT96100_MPSC_CHAN0_REG1         0x000A0C
#define GT96100_MPSC_CHAN0_REG2         0x000A10
#define GT96100_MPSC_CHAN0_REG3         0x000A14
#define GT96100_MPSC_CHAN0_REG4         0x000A18
#define GT96100_MPSC_CHAN0_REG5         0x000A1C
#define GT96100_MPSC_CHAN0_REG6         0x000A20
#define GT96100_MPSC_CHAN0_REG7         0x000A24
#define GT96100_MPSC_CHAN0_REG8         0x000A28
#define GT96100_MPSC_CHAN0_REG9         0x000A2C
#define GT96100_MPSC_CHAN0_REG10        0x000A30
#define GT96100_MPSC_CHAN0_REG11        0x000A34
#define GT96100_MPSC1_MAIN_CONFIG_LOW   0x008A00
#define GT96100_MPSC1_MAIN_CONFIG_HIGH  0x008A04
#define GT96100_MPSC1_PROTOCOL_CONFIG   0x008A08
#define GT96100_MPSC_CHAN1_REG1         0x008A0C
#define GT96100_MPSC_CHAN1_REG2         0x008A10
#define GT96100_MPSC_CHAN1_REG3         0x008A14
#define GT96100_MPSC_CHAN1_REG4         0x008A18
#define GT96100_MPSC_CHAN1_REG5         0x008A1C
#define GT96100_MPSC_CHAN1_REG6         0x008A20
#define GT96100_MPSC_CHAN1_REG7         0x008A24
#define GT96100_MPSC_CHAN1_REG8         0x008A28
#define GT96100_MPSC_CHAN1_REG9         0x008A2C
#define GT96100_MPSC_CHAN1_REG10        0x008A30
#define GT96100_MPSC_CHAN1_REG11        0x008A34
#define GT96100_MPSC2_MAIN_CONFIG_LOW   0x010A00
#define GT96100_MPSC2_MAIN_CONFIG_HIGH  0x010A04
#define GT96100_MPSC2_PROTOCOL_CONFIG   0x010A08
#define GT96100_MPSC_CHAN2_REG1         0x010A0C
#define GT96100_MPSC_CHAN2_REG2         0x010A10
#define GT96100_MPSC_CHAN2_REG3         0x010A14
#define GT96100_MPSC_CHAN2_REG4         0x010A18
#define GT96100_MPSC_CHAN2_REG5         0x010A1C
#define GT96100_MPSC_CHAN2_REG6         0x010A20
#define GT96100_MPSC_CHAN2_REG7         0x010A24
#define GT96100_MPSC_CHAN2_REG8         0x010A28
#define GT96100_MPSC_CHAN2_REG9         0x010A2C
#define GT96100_MPSC_CHAN2_REG10        0x010A30
#define GT96100_MPSC_CHAN2_REG11        0x010A34
#define GT96100_MPSC3_MAIN_CONFIG_LOW   0x018A00
#define GT96100_MPSC3_MAIN_CONFIG_HIGH  0x018A04
#define GT96100_MPSC3_PROTOCOL_CONFIG   0x018A08
#define GT96100_MPSC_CHAN3_REG1         0x018A0C
#define GT96100_MPSC_CHAN3_REG2         0x018A10
#define GT96100_MPSC_CHAN3_REG3         0x018A14
#define GT96100_MPSC_CHAN3_REG4         0x018A18
#define GT96100_MPSC_CHAN3_REG5         0x018A1C
#define GT96100_MPSC_CHAN3_REG6         0x018A20
#define GT96100_MPSC_CHAN3_REG7         0x018A24
#define GT96100_MPSC_CHAN3_REG8         0x018A28
#define GT96100_MPSC_CHAN3_REG9         0x018A2C
#define GT96100_MPSC_CHAN3_REG10        0x018A30
#define GT96100_MPSC_CHAN3_REG11        0x018A34
#define GT96100_MPSC4_MAIN_CONFIG_LOW   0x020A00
#define GT96100_MPSC4_MAIN_CONFIG_HIGH  0x020A04
#define GT96100_MPSC4_PROTOCOL_CONFIG   0x020A08
#define GT96100_MPSC_CHAN4_REG1         0x020A0C
#define GT96100_MPSC_CHAN4_REG2         0x020A10
#define GT96100_MPSC_CHAN4_REG3         0x020A14
#define GT96100_MPSC_CHAN4_REG4         0x020A18
#define GT96100_MPSC_CHAN4_REG5         0x020A1C
#define GT96100_MPSC_CHAN4_REG6         0x020A20
#define GT96100_MPSC_CHAN4_REG7         0x020A24
#define GT96100_MPSC_CHAN4_REG8         0x020A28
#define GT96100_MPSC_CHAN4_REG9         0x020A2C
#define GT96100_MPSC_CHAN4_REG10        0x020A30
#define GT96100_MPSC_CHAN4_REG11        0x020A34
#define GT96100_MPSC5_MAIN_CONFIG_LOW   0x028A00
#define GT96100_MPSC5_MAIN_CONFIG_HIGH  0x028A04
#define GT96100_MPSC5_PROTOCOL_CONFIG   0x028A08
#define GT96100_MPSC_CHAN5_REG1         0x028A0C
#define GT96100_MPSC_CHAN5_REG2         0x028A10
#define GT96100_MPSC_CHAN5_REG3         0x028A14
#define GT96100_MPSC_CHAN5_REG4         0x028A18
#define GT96100_MPSC_CHAN5_REG5         0x028A1C
#define GT96100_MPSC_CHAN5_REG6         0x028A20
#define GT96100_MPSC_CHAN5_REG7         0x028A24
#define GT96100_MPSC_CHAN5_REG8         0x028A28
#define GT96100_MPSC_CHAN5_REG9         0x028A2C
#define GT96100_MPSC_CHAN5_REG10        0x028A30
#define GT96100_MPSC_CHAN5_REG11        0x028A34
#define GT96100_MPSC6_MAIN_CONFIG_LOW   0x030A00
#define GT96100_MPSC6_MAIN_CONFIG_HIGH  0x030A04
#define GT96100_MPSC6_PROTOCOL_CONFIG   0x030A08
#define GT96100_MPSC_CHAN6_REG1         0x030A0C
#define GT96100_MPSC_CHAN6_REG2         0x030A10
#define GT96100_MPSC_CHAN6_REG3         0x030A14
#define GT96100_MPSC_CHAN6_REG4         0x030A18
#define GT96100_MPSC_CHAN6_REG5         0x030A1C
#define GT96100_MPSC_CHAN6_REG6         0x030A20
#define GT96100_MPSC_CHAN6_REG7         0x030A24
#define GT96100_MPSC_CHAN6_REG8         0x030A28
#define GT96100_MPSC_CHAN6_REG9         0x030A2C
#define GT96100_MPSC_CHAN6_REG10        0x030A30
#define GT96100_MPSC_CHAN6_REG11        0x030A34
#define GT96100_MPSC7_MAIN_CONFIG_LOW   0x038A00
#define GT96100_MPSC7_MAIN_CONFIG_HIGH  0x038A04
#define GT96100_MPSC7_PROTOCOL_CONFIG   0x038A08
#define GT96100_MPSC_CHAN7_REG1         0x038A0C
#define GT96100_MPSC_CHAN7_REG2         0x038A10
#define GT96100_MPSC_CHAN7_REG3         0x038A14
#define GT96100_MPSC_CHAN7_REG4         0x038A18
#define GT96100_MPSC_CHAN7_REG5         0x038A1C
#define GT96100_MPSC_CHAN7_REG6         0x038A20
#define GT96100_MPSC_CHAN7_REG7         0x038A24
#define GT96100_MPSC_CHAN7_REG8         0x038A28
#define GT96100_MPSC_CHAN7_REG9         0x038A2C
#define GT96100_MPSC_CHAN7_REG10        0x038A30
#define GT96100_MPSC_CHAN7_REG11        0x038A34
/*  FlexTDMs  */
/* TDPR0 - Transmit Dual Port RAM. block size 0xff */
#define GT96100_FXTDM0_TDPR0_BLK0_BASE  0x000B00
#define GT96100_FXTDM0_TDPR0_BLK1_BASE  0x001B00
#define GT96100_FXTDM0_TDPR0_BLK2_BASE  0x002B00
#define GT96100_FXTDM0_TDPR0_BLK3_BASE  0x003B00
/* RDPR0 - Receive Dual Port RAM. block size 0xff */
#define GT96100_FXTDM0_RDPR0_BLK0_BASE  0x004B00
#define GT96100_FXTDM0_RDPR0_BLK1_BASE  0x005B00
#define GT96100_FXTDM0_RDPR0_BLK2_BASE  0x006B00
#define GT96100_FXTDM0_RDPR0_BLK3_BASE  0x007B00
#define GT96100_FXTDM0_TX_READ_PTR      0x008B00
#define GT96100_FXTDM0_RX_READ_PTR      0x008B04
#define GT96100_FXTDM0_CONFIG       0x008B08
#define GT96100_FXTDM0_AUX_CHANA_TX 0x008B0C
#define GT96100_FXTDM0_AUX_CHANA_RX 0x008B10
#define GT96100_FXTDM0_AUX_CHANB_TX 0x008B14
#define GT96100_FXTDM0_AUX_CHANB_RX 0x008B18
#define GT96100_FXTDM1_TDPR1_BLK0_BASE  0x010B00
#define GT96100_FXTDM1_TDPR1_BLK1_BASE  0x011B00
#define GT96100_FXTDM1_TDPR1_BLK2_BASE  0x012B00
#define GT96100_FXTDM1_TDPR1_BLK3_BASE  0x013B00
#define GT96100_FXTDM1_RDPR1_BLK0_BASE  0x014B00
#define GT96100_FXTDM1_RDPR1_BLK1_BASE  0x015B00
#define GT96100_FXTDM1_RDPR1_BLK2_BASE  0x016B00
#define GT96100_FXTDM1_RDPR1_BLK3_BASE  0x017B00
#define GT96100_FXTDM1_TX_READ_PTR      0x018B00
#define GT96100_FXTDM1_RX_READ_PTR      0x018B04
#define GT96100_FXTDM1_CONFIG       0x018B08
#define GT96100_FXTDM1_AUX_CHANA_TX 0x018B0C
#define GT96100_FXTDM1_AUX_CHANA_RX 0x018B10
#define GT96100_FLTDM1_AUX_CHANB_TX 0x018B14
#define GT96100_FLTDM1_AUX_CHANB_RX 0x018B18
#define GT96100_FLTDM2_TDPR2_BLK0_BASE  0x020B00
#define GT96100_FLTDM2_TDPR2_BLK1_BASE  0x021B00
#define GT96100_FLTDM2_TDPR2_BLK2_BASE  0x022B00
#define GT96100_FLTDM2_TDPR2_BLK3_BASE  0x023B00
#define GT96100_FLTDM2_RDPR2_BLK0_BASE  0x024B00
#define GT96100_FLTDM2_RDPR2_BLK1_BASE  0x025B00
#define GT96100_FLTDM2_RDPR2_BLK2_BASE  0x026B00
#define GT96100_FLTDM2_RDPR2_BLK3_BASE  0x027B00
#define GT96100_FLTDM2_TX_READ_PTR      0x028B00
#define GT96100_FLTDM2_RX_READ_PTR      0x028B04
#define GT96100_FLTDM2_CONFIG       0x028B08
#define GT96100_FLTDM2_AUX_CHANA_TX 0x028B0C
#define GT96100_FLTDM2_AUX_CHANA_RX 0x028B10
#define GT96100_FLTDM2_AUX_CHANB_TX 0x028B14
#define GT96100_FLTDM2_AUX_CHANB_RX 0x028B18
#define GT96100_FLTDM3_TDPR3_BLK0_BASE  0x030B00
#define GT96100_FLTDM3_TDPR3_BLK1_BASE  0x031B00
#define GT96100_FLTDM3_TDPR3_BLK2_BASE  0x032B00
#define GT96100_FLTDM3_TDPR3_BLK3_BASE  0x033B00
#define GT96100_FXTDM3_RDPR3_BLK0_BASE  0x034B00
#define GT96100_FXTDM3_RDPR3_BLK1_BASE  0x035B00
#define GT96100_FXTDM3_RDPR3_BLK2_BASE  0x036B00
#define GT96100_FXTDM3_RDPR3_BLK3_BASE  0x037B00
#define GT96100_FXTDM3_TX_READ_PTR      0x038B00
#define GT96100_FXTDM3_RX_READ_PTR      0x038B04
#define GT96100_FXTDM3_CONFIG       0x038B08
#define GT96100_FXTDM3_AUX_CHANA_TX 0x038B0C
#define GT96100_FXTDM3_AUX_CHANA_RX 0x038B10
#define GT96100_FXTDM3_AUX_CHANB_TX 0x038B14
#define GT96100_FXTDM3_AUX_CHANB_RX 0x038B18
/*  Baud Rate Generators  */
#define GT96100_BRG0_CONFIG     0x102A00
#define GT96100_BRG0_BAUD_TUNE  0x102A04
#define GT96100_BRG1_CONFIG     0x102A08
#define GT96100_BRG1_BAUD_TUNE  0x102A0C
#define GT96100_BRG2_CONFIG     0x102A10
#define GT96100_BRG2_BAUD_TUNE  0x102A14
#define GT96100_BRG3_CONFIG     0x102A18
#define GT96100_BRG3_BAUD_TUNE  0x102A1C
#define GT96100_BRG4_CONFIG     0x102A20
#define GT96100_BRG4_BAUD_TUNE  0x102A24
#define GT96100_BRG5_CONFIG     0x102A28
#define GT96100_BRG5_BAUD_TUNE  0x102A2C
#define GT96100_BRG6_CONFIG     0x102A30
#define GT96100_BRG6_BAUD_TUNE  0x102A34
#define GT96100_BRG7_CONFIG     0x102A38
#define GT96100_BRG7_BAUD_TUNE  0x102A3C
/*  Routing Registers  */
#define GT96100_ROUTE_MAIN      0x101A00
#define GT96100_ROUTE_RX_CLOCK  0x101A10
#define GT96100_ROUTE_TX_CLOCK  0x101A20
/*  General Purpose Ports  */
#define GT96100_GPP_CONFIG0     0x100A00
#define GT96100_GPP_CONFIG1     0x100A04
#define GT96100_GPP_CONFIG2     0x100A08
#define GT96100_GPP_CONFIG3     0x100A0C
#define GT96100_GPP_IO0         0x100A20
#define GT96100_GPP_IO1         0x100A24
#define GT96100_GPP_IO2         0x100A28
#define GT96100_GPP_IO3         0x100A2C
#define GT96100_GPP_DATA0       0x100A40
#define GT96100_GPP_DATA1       0x100A44
#define GT96100_GPP_DATA2       0x100A48
#define GT96100_GPP_DATA3       0x100A4C
#define GT96100_GPP_LEVEL0      0x100A60
#define GT96100_GPP_LEVEL1      0x100A64
#define GT96100_GPP_LEVEL2      0x100A68
#define GT96100_GPP_LEVEL3      0x100A6C
/*  Watchdog  */
#define GT96100_WD_CONFIG   0x101A80
#define GT96100_WD_VALUE    0x101A84
/* Communication Unit Arbiter  */
#define GT96100_COMM_UNIT_ARBTR_CONFIG 0x101AC0
/*  PCI Arbiters  */
#define GT96100_PCI0_ARBTR_CONFIG 0x101AE0
#define GT96100_PCI1_ARBTR_CONFIG 0x101AE4
/* CIU Arbiter */
#define GT96100_CIU_ARBITER_CONFIG 0x101AC0
/* Interrupt Controller */
#define GT96100_MAIN_CAUSE     0x000C18
#define GT96100_INT0_MAIN_MASK 0x000C1C
#define GT96100_INT1_MAIN_MASK 0x000C24
#define GT96100_HIGH_CAUSE     0x000C98
#define GT96100_INT0_HIGH_MASK 0x000C9C
#define GT96100_INT1_HIGH_MASK 0x000CA4
#define GT96100_INT0_SELECT    0x000C70
#define GT96100_INT1_SELECT    0x000C74
#define GT96100_SERIAL_CAUSE   0x103A00
#define GT96100_SERINT0_MASK   0x103A80
#define GT96100_SERINT1_MASK   0x103A88

#endif /*  _GT96100_H */
