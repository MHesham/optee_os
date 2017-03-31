/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * All rights reserved.
 * Copyright (c) 2016, Wind River Systems.
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

#include <arm32.h>
#include <console.h>
#include <drivers/gic.h>
#include <drivers/imx_uart.h>
#include <io.h>
#include <kernel/generic_boot.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/pm_stubs.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <stdint.h>
#include <sm/optee_smc.h>
#include <sm/sm.h>
#include <tee/entry_fast.h>
#include <tee/entry_std.h>

#if defined(PLATFORM_FLAVOR_mx6qsabrelite) || \
	defined(PLATFORM_FLAVOR_mx6qsabresd) || \
	defined(PLATFORM_FLAVOR_mx6qhmbedge)
#include <kernel/tz_ssvce_pl310.h>
#endif

static void main_fiq(void);
static struct gic_data gic_data;

static const struct thread_handlers handlers = {
	.std_smc = tee_entry_std,
	.fast_smc = tee_entry_fast,
	.nintr = main_fiq,
	.cpu_on = pm_panic,
	.cpu_off = pm_panic,
	.cpu_suspend = pm_panic,
	.cpu_resume = pm_panic,
	.system_off = pm_panic,
	.system_reset = pm_panic,
};

register_phys_mem(MEM_AREA_IO_NSEC, CONSOLE_UART_BASE, CORE_MMU_DEVICE_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, GIC_BASE, CORE_MMU_DEVICE_SIZE);
register_phys_mem(MEM_AREA_RAM_SEC, OCRAM_BASE, 0x00100000);

#if defined(PLATFORM_FLAVOR_mx6qsabrelite) || \
	defined(PLATFORM_FLAVOR_mx6qsabresd) || \
	defined(PLATFORM_FLAVOR_mx6qhmbedge)
register_phys_mem(MEM_AREA_IO_SEC, PL310_BASE, CORE_MMU_DEVICE_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, SRC_BASE, CORE_MMU_DEVICE_SIZE);
#endif

const struct thread_handlers *generic_boot_get_handlers(void)
{
	return &handlers;
}

static void main_fiq(void)
{
	panic();
}

#if defined(PLATFORM_FLAVOR_mx6qsabrelite) || \
    defined(PLATFORM_FLAVOR_mx6qsabresd) || \
    defined(PLATFORM_FLAVOR_mx6qhmbedge)

static vaddr_t ccm_base(void)
{
	if (cpu_mmu_enabled()) {
	    return (vaddr_t)phys_to_virt(CCM_BASE, MEM_AREA_IO_SEC);
	}

	return CCM_BASE;
}

static vaddr_t iomuxc_base(void)
{
	if (cpu_mmu_enabled()) {
	    return (vaddr_t)phys_to_virt(IOMUXC_BASE, MEM_AREA_IO_SEC);
	}

	return IOMUXC_BASE;
}

static void setup_low_power_modes(void)
{
    vaddr_t ccm;
    vaddr_t iomuxc;
    uint32_t cgpr;
    uint32_t gpr1;
    uint32_t clpcr;

    ccm = ccm_base();
    iomuxc = iomuxc_base();

    DMSG(
        "Configuring CCM for low power modes. "
        "(ccm = 0x%08x, iomuxc = 0x%08x)\n",
        (uint32_t)ccm,
        (uint32_t)iomuxc);
    
    /* set required bits in CGPR */
    cgpr = read32(ccm + CCM_CGPR_OFFSET);
    cgpr |= CCM_CGPR_MUST_BE_ONE;
    cgpr |= CCM_CGPR_INT_MEM_CLK_LPM;
    write32(cgpr, ccm + CCM_CGPR_OFFSET);

    /* configure IOMUXC GINT to be always asserted */
    gpr1 = read32(iomuxc + IOMUXC_GPR1_OFFSET);
    gpr1 |= IOMUXC_GPR1_GINT;
    write32(gpr1, iomuxc + IOMUXC_GPR1_OFFSET);

    /* configure CLPCR for low power modes */
    clpcr = read32(ccm + CCM_CLPCR_OFFSET);
    clpcr &= ~CCM_CLPCR_LPM_MASK;
    clpcr |= CCM_CLPCR_ARM_CLK_DIS_ON_LPM;
    clpcr &= ~CCM_CLPCR_SBYOS;
    clpcr &= ~CCM_CLPCR_VSTBY;
    clpcr &= ~CCM_CLPCR_BYPASS_MMDC_CH0_LPM_HS;
    clpcr |= CCM_CLPCR_BYPASS_MMDC_CH1_LPM_HS;
    clpcr &= ~CCM_CLPCR_MASK_CORE0_WFI;
    clpcr &= ~CCM_CLPCR_MASK_CORE1_WFI;
    clpcr &= ~CCM_CLPCR_MASK_CORE2_WFI;
    clpcr &= ~CCM_CLPCR_MASK_CORE3_WFI;
    clpcr &= ~CCM_CLPCR_MASK_SCU_IDLE;
    clpcr &= ~CCM_CLPCR_MASK_L2CC_IDLE;
    write32(clpcr, ccm + CCM_CLPCR_OFFSET);
}

void plat_cpu_reset_late(void)
{
	uintptr_t addr;
    uint32_t scu_ctrl;

	if (!get_core_pos()) {
		/* primary core */
#if defined(CFG_BOOT_SYNC_CPU)
		/* set secondary entry address and release core */
		write32(CFG_TEE_LOAD_ADDR, SRC_BASE + SRC_GPR1 + 8);
		write32(CFG_TEE_LOAD_ADDR, SRC_BASE + SRC_GPR1 + 16);
		write32(CFG_TEE_LOAD_ADDR, SRC_BASE + SRC_GPR1 + 24);

		write32(SRC_SCR_CPU_ENABLE_ALL, SRC_BASE + SRC_SCR);
#endif

		/* SCU config */
		write32(SCU_INV_CTRL_INIT, SCU_BASE + SCU_INV_SEC);
		write32(SCU_SAC_CTRL_INIT, SCU_BASE + SCU_SAC);
		write32(SCU_NSAC_CTRL_INIT, SCU_BASE + SCU_NSAC);

		/* SCU enable */
        scu_ctrl = read32(SCU_BASE + SCU_CTRL);
        scu_ctrl |= SCU_CTRL_ENABLE;

#ifdef CFG_PSCI_ARM32
        scu_ctrl |= SCU_CTRL_STANDBY_ENABLE;
#endif /* CFG_PSCI_ARM32 */

		write32(scu_ctrl, SCU_BASE + SCU_CTRL);

		/* configure imx6 CSU */

		/* first grant all peripherals */
		for (addr = CSU_BASE + CSU_CSL_START;
			 addr != CSU_BASE + CSU_CSL_END;
			 addr += 4)
			write32(CSU_ACCESS_ALL, addr);

		/* lock the settings */
		for (addr = CSU_BASE + CSU_CSL_START;
			 addr != CSU_BASE + CSU_CSL_END;
			 addr += 4)
			write32(read32(addr) | CSU_SETTING_LOCK, addr);
	}
}
#endif

static vaddr_t console_base(void)
{
	static void *va;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(CONSOLE_UART_BASE,
					  MEM_AREA_IO_NSEC);
		return (vaddr_t)va;
	}
	return CONSOLE_UART_BASE;
}

void console_init(void)
{
	vaddr_t base = console_base();

	imx_uart_init(base);
}

void console_putc(int ch)
{
	vaddr_t base = console_base();

	/* If \n, also do \r */
	if (ch == '\n')
		imx_uart_putc('\r', base);
	imx_uart_putc(ch, base);
}

void console_flush(void)
{
	vaddr_t base = console_base();

	imx_uart_flush_tx_fifo(base);
}

void main_init_gic(void)
{
	vaddr_t gicc_base;
	vaddr_t gicd_base;

	gicc_base = (vaddr_t)phys_to_virt(GIC_BASE + GICC_OFFSET,
					  MEM_AREA_IO_SEC);
	gicd_base = (vaddr_t)phys_to_virt(GIC_BASE + GICD_OFFSET,
					  MEM_AREA_IO_SEC);

	if (!gicc_base || !gicd_base)
		panic();

	/* Initialize GIC */
	gic_init(&gic_data, gicc_base, gicd_base);
	itr_init(&gic_data.chip);

#ifdef CFG_PSCI_ARM32
    setup_low_power_modes();
#endif
}

#if defined(PLATFORM_FLAVOR_mx6qsabrelite) || \
    defined(PLATFORM_FLAVOR_mx6qsabresd) || \
    defined(PLATFORM_FLAVOR_mx6qhmbedge)
vaddr_t pl310_base(void)
{
	static void *va __early_bss;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(PL310_BASE, MEM_AREA_IO_SEC);
		return (vaddr_t)va;
	}
	return PL310_BASE;
}

void main_secondary_init_gic(void)
{
	gic_cpu_init(&gic_data);
}
#endif

void init_sec_mon(unsigned long nsec_entry)
{
    struct sm_nsec_ctx *nsec_ctx;

    assert(nsec_entry != PADDR_INVALID);

	/* Initialize secure monitor */
	nsec_ctx = sm_get_nsec_ctx();
	nsec_ctx->mon_lr = nsec_entry;
	nsec_ctx->mon_spsr = CPSR_MODE_SVC | CPSR_I;

	DMSG("nsec_entry=0x%08lX, SPSR=0x%08X \n", nsec_entry, nsec_ctx->mon_spsr);
}

