/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 * Copyright (c) 2017, Linaro Limited
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

#ifndef TLB_HELPERS_H
#define TLB_HELPERS_H

#include <arm.h>

#ifndef ASM
#include <types_ext.h>

void tlbi_all(void);
void tlbi_asid(unsigned long asid);
void tlbi_mva_allasid(unsigned long addr);
static inline void tlbi_mva_allasid_nosync(vaddr_t va)
{
#ifdef ARM64
	tlbi_vaae1is(va >> TLBI_MVA_SHIFT);
#else
	write_tlbimvaais(va);
#endif
}
#endif /*!ASM*/

#define DRAM0_BASE			0x10000000

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

#endif // #if defined(CFG_MX6Q)

#endif /* TLB_HELPERS_H */
