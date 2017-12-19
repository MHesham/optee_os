/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * All rights reserved.
 * Copyright (c) 2016, Wind River Systems.
 * All rights reserved.
 * Copyright (c) 2017, Microsoft.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define SCU_BASE			0x00A00000
#define PL310_BASE			0x00A02000
#define SRC_BASE			0x020D8000
#define SRC_SCR				0x000
#define SRC_GPR1			0x020
#define SRC_SCR_CPU_ENABLE_ALL		SHIFT_U32(0x7, 22)
#define SRC_SCR_CORE1_RST_OFFSET	14
#define SRC_SCR_CORE1_ENABLE_OFFSET	22
#define SRC_SCR_WARM_RESET_ENABLE       (1u << 0)
#define SRC_SCR_MASK_WDOG_RST           0x00000780
#define SRC_SCR_WDOG_NOTMASKED          (0xA << 7)

#define GIC_BASE			0x00A00000
#define GICC_OFFSET			0x100
#define GICD_OFFSET			0x1000
#define GIC_CPU_BASE			(GIC_BASE + GICC_OFFSET)
#define GIC_DIST_BASE			(GIC_BASE + GICD_OFFSET)
#define SNVS_BASE                       0x020CC000
#define WDOG_BASE                       0x020BC000

#define UART1_BASE			0x02020000
#define UART2_BASE			0x021E8000
#define UART4_BASE			0x021F0000

#if defined(CFG_MX6Q) || defined(CFG_MX6D)
#define UART3_BASE			0x021EC000
#define UART5_BASE			0x021F4000
#endif

/* Central Security Unit register values */
#define CSU_BASE			0x021C0000
#define CSU_CSL_START			0x0
#define CSU_CSL_END			0xA0
#define CSU_SETTING_LOCK		0x01000100

/*
 * Grant R+W access:
 * - Just to TZ Supervisor execution mode, and
 * - Just to a single device
 */
#define CSU_TZ_SUPERVISOR		0x22

/*
 * Grant R+W access:
 * - To all execution modes, and
 * - To a single device
 */
#define CSU_ALL_MODES			0xFF

/*
 * Grant R+W access:
 * - To all execution modes, and
 * - To both devices sharing a single CSU_CSL register
 */
#define CSU_ACCESS_ALL			((CSU_ALL_MODES << 16) | (CSU_ALL_MODES << 0))

#define DRAM0_BASE			0x10000000
#define CAAM_BASE			0x00100000

#if defined(CFG_MX6Q)

#define AIPS1_ARB_BASE_ADDR         0x02000000
#define AIPS2_ARB_BASE_ADDR         0x02100000

#define ATZ1_BASE_ADDR              AIPS1_ARB_BASE_ADDR
#define ATZ2_BASE_ADDR              AIPS2_ARB_BASE_ADDR

#define AIPS1_OFF_BASE_ADDR         (ATZ1_BASE_ADDR + 0x80000)
#define AIPS2_OFF_BASE_ADDR         (ATZ2_BASE_ADDR + 0x80000)

#define IP2APB_TZASC1_BASE_ADDR     (AIPS2_OFF_BASE_ADDR + 0x50000)

#define IMX_IOMUXC_BASE             0x020E0000

#define CCM_BASE_ADDR               (AIPS1_OFF_BASE_ADDR + 0x44000)

#define IMX_GPIO1_BASE_ADDR         (AIPS1_OFF_BASE_ADDR + 0x1C000)
#define IMX_GPIO_PORTS              7
#define IMX_GPIO_PORT_GRANULARITY   0x4000
#define IMX_GPIO_REGISTER_BITS      32

/* ECSPI */
#define MXC_ECSPI1_BASE_ADDR        (ATZ1_BASE_ADDR + 0x08000)
#define MXC_ECSPI_BUS_COUNT         5
#define MXC_ECSPI_BUS_GRANULARITY   0x4000

#define MXC_CSPICON_POL		4
#define MXC_CSPICON_PHA		0
#define MXC_CSPICON_SSPOL	12

#define MXC_CSPICTRL_EN		        (1 << 0)
#define MXC_CSPICTRL_MODE	        (1 << 1)
#define MXC_CSPICTRL_XCH	        (1 << 2)
#define MXC_CSPICTRL_MODE_MASK      (0xf << 4)
#define MXC_CSPICTRL_CHIPSELECT(x)	(((x) & 0x3) << 12)
#define MXC_CSPICTRL_BITCOUNT(x)	(((x) & 0xfff) << 20)
#define MXC_CSPICTRL_PREDIV(x)	    (((x) & 0xF) << 12)
#define MXC_CSPICTRL_POSTDIV(x)	    (((x) & 0xF) << 8)
#define MXC_CSPICTRL_SELCHAN(x)	    (((x) & 0x3) << 18)
#define MXC_CSPICTRL_MAXBITS	    0xfff
#define MXC_CSPICTRL_TC		        (1 << 7)
#define MXC_CSPICTRL_RXOVF	        (1 << 6)
#define MXC_CSPIPERIOD_32KHZ	    (1 << 15)
#define MXC_MAX_SPI_BYTES	        32

/* CCM */
#define IMX_CCM_CCR_BASE_ADDR       0x020C4000
#define IMX_CCM_CCGR1_BASE_ADDR     0x020C406C

#define IMX_CGR_CLK_ENABLED 0x3     /* Clock enabled except in STOP mode */
#define IMX_CCM_CCGR1_ECSPI2_CLK_SHIFT      2
#define IMX_CCM_CCGR1_ECSPI2_CLK_ENABLED    \
    (IMX_CGR_CLK_ENABLED << IMX_CCM_CCGR1_ECSPI2_CLK_SHIFT) /* ECSPI2 clock enabled */

/* SNVS */
#define SNVS_HPLR               0x00000000
#define SNVS_HPCOMR             0x00000004
#define SNVS_HPCR               0x00000008
#define SNVS_HPSICR             0x0000000C
#define SNVS_HPSVCR             0x00000010
#define SNVS_HPSR               0x00000014
#define SNVS_HPSVSR             0x00000018
#define SNVS_HPHACIVR           0x0000001C
#define SNVS_HPHACR             0x00000020
#define SNVS_HPRTCMR            0x00000024
#define SNVS_HPRTCLR            0x00000028
#define SNVS_HPTAMR             0x0000002C
#define SNVS_HPTALR             0x00000030
#define SNVS_LPLR               0x00000034
#define SNVS_LPCR               0x00000038
#define SNVS_LPMKCR             0x0000003C
#define SNVS_LPSVCR             0x00000040
#define SNVS_LPTGFCR            0x00000044
#define SNVS_LPTDCR             0x00000048
#define SNVS_LPSR               0x0000004C
#define SNVS_LPSRTCMR           0x00000050
#define SNVS_LPSRTCLR           0x00000054
#define SNVS_LPTAR              0x00000058
#define SNVS_LPSMCMR            0x0000005C
#define SNVS_LPSMCLR            0x00000060
#define SNVS_LPPGDR             0x00000064
#define SNVS_LPGPR              0x00000068
#define SNVS_LPZMK              0x0000006C
#define SNVS_HPVIDR1            0x00000BF8
#define SNVS_HPVIDR2            0x00000BFC

#define SNVS_LPPGDR_INIT        0x41736166

#define SNVS_LPCR_DP_EN         (1u << 5)
#define SNVS_LPCR_TOP           (1u << 6)

#define SNVS_LPSR_PGD           (1u << 3)

/* Watchdog */

#define WDOG1_WCR               0x00000000
#define WDOG1_WSR               0x00000002
#define WDOG1_WRSR              0x00000004
#define WDOG1_WICR              0x00000006
#define WDOG1_WMCR              0x00000008

#define WDOG_WCR_WDE            (1u << 2)
#define WDOG_WCR_WDT            (1u << 3)
#define WDOG_WCR_SRS            (1u << 4)
#define WDOG_WCR_WDA            (1u << 5)

#define WDOG_WSR_FEED1          0x5555
#define WDOG_WSR_FEED2          0xAAAA

#endif // #if defined(CFG_MX6Q)
