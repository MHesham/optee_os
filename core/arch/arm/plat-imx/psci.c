/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Peng Fan <peng.fan@nxp.com>
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
#include <console.h>
#include <drivers/imx_uart.h>
#include <io.h>
#include <imx.h>
#include <imx-regs.h>
#include <kernel/generic_boot.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/pm_stubs.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <stdint.h>
#include <sm/optee_smc.h>
#include <sm/psci.h>
#include <tee/entry_std.h>
#include <tee/entry_fast.h>
#include "imx_pl310.h"

#ifdef CFG_PL310
#define CORE_IDX_L2CACHE                   0x00100000
#endif

#ifdef CFG_BOOT_SECONDARY_REQUEST
int psci_features(uint32_t psci_fid)
{
    switch (psci_fid) {
    case PSCI_CPU_ON:
    case PSCI_SYSTEM_OFF:
    case PSCI_SYSTEM_RESET:
		return 0;

    default:
		return PSCI_RET_NOT_SUPPORTED;
    }
}

int psci_cpu_on(uint32_t core_idx, uint32_t entry,
		uint32_t context_id)
{
	uint32_t val;
	vaddr_t va;

#ifdef CFG_PL310
	if (core_idx == CORE_IDX_L2CACHE)
		return l2cache_op(entry);
#endif

	va = core_mmu_get_va(SRC_BASE, MEM_AREA_IO_SEC);
	if (!va)
		EMSG("No SRC mapping\n");

	if ((core_idx == 0) || (core_idx >= CFG_TEE_CORE_NB_CORE))
		return PSCI_RET_INVALID_PARAMETERS;

	/* set secondary cores' NS entry addresses */
	ns_entry_contexts[core_idx].entry_point = entry;
	ns_entry_contexts[core_idx].r0 = context_id;

	/* flush cache to PoU so other CPUs see the values */
	cache_op_inner(
		DCACHE_AREA_CLEAN,
		&ns_entry_contexts[core_idx],
		sizeof(struct ns_entry_context));

	if (soc_is_imx7ds()) {
		write32((uint32_t)CFG_TEE_LOAD_ADDR,
			va + SRC_GPR1_MX7 + core_idx * 8);

		imx_gpcv2_set_core1_pup_by_software();

		/* release secondary core */
		val = read32(va + SRC_A7RCR1);
		val |=  BIT32(SRC_A7RCR1_A7_CORE1_ENABLE_OFFSET +
			      (core_idx - 1));
		write32(val, va + SRC_A7RCR1);

		return PSCI_RET_SUCCESS;
	}

	/* boot secondary cores from OP-TEE load address */
	write32((uint32_t)CFG_TEE_LOAD_ADDR, va + SRC_GPR1 + core_idx * 8);

	/* release secondary core */
	val = read32(va + SRC_SCR);
	val |=  BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + (core_idx - 1));
	val |=  BIT32(SRC_SCR_CORE1_RST_OFFSET + (core_idx - 1));
	write32(val, va + SRC_SCR);

	return PSCI_RET_SUCCESS;
}

void __attribute__((noreturn)) psci_system_off(void)
{
	vaddr_t snvs = core_mmu_get_va(SNVS_BASE, MEM_AREA_IO_SEC);
	uint32_t val;

	/* Reset power glitch detector */
	write32(SNVS_LPPGDR_INIT, snvs + SNVS_LPPGDR);
	write32(SNVS_LPSR_PGD, snvs + SNVS_LPSR);

	/* Set dumb power manager mode to 1 and turn off power */
	val = read32(snvs + SNVS_LPCR);
	val |= SNVS_LPCR_DP_EN;
	val |= SNVS_LPCR_TOP;
	write32(val, snvs + SNVS_LPCR);

	/* Wait for the end */
	for (;;) wfi();
}

void __attribute__((noreturn)) psci_system_reset(void)
{
	vaddr_t src = core_mmu_get_va(SRC_BASE, MEM_AREA_IO_SEC);
	vaddr_t wdog = core_mmu_get_va(WDOG_BASE, MEM_AREA_IO_SEC);
	uint32_t val;

	if (soc_is_imx7ds()) {
	    /* Ensure watchdog is not masked */
	    val = read32(src + SRC_A7RCR0);
	    val &= ~SRC_SCR_MASK_WDOG_RST;
	    val |= SRC_SCR_WDOG_NOTMASKED;
	    write32(val, src + SRC_A7RCR0);
	}
	else
	{
	    /* Ensure watchdog is not masked */
	    val = read32(src + SRC_SCR);
	    val &= ~SRC_SCR_WARM_RESET_ENABLE;
	    val &= ~SRC_SCR_MASK_WDOG_RST;
	    val |= SRC_SCR_WDOG_NOTMASKED;
	    write32(val, src + SRC_SCR);
	}

        /* Enable watchdog timer */
        val = WDOG_WCR_WDE |
    	  WDOG_WCR_WDT |
    	  WDOG_WCR_SRS |
    	  WDOG_WCR_WDA;

        write16(val, wdog + WDOG1_WCR);

        /* Watchdog timer feed sequence */
        write16(WDOG_WSR_FEED1, wdog + WDOG1_WSR);
        write16(WDOG_WSR_FEED2, wdog + WDOG1_WSR);

	/* Wait for the end */
	for (;;) wfi();
}

int psci_cpu_off(void)
{
	uint32_t core_id;

	core_id = get_core_pos();

	DMSG("core_id: %" PRIu32, core_id);

	psci_armv7_cpu_off();

	imx_set_src_gpr(core_id, UINT32_MAX);

	thread_mask_exceptions(THREAD_EXCP_ALL);

	while (true)
		wfi();

	return PSCI_RET_INTERNAL_FAILURE;
}

int psci_affinity_info(uint32_t affinity,
		       uint32_t lowest_affnity_level __unused)
{
	vaddr_t va = core_mmu_get_va(SRC_BASE, MEM_AREA_IO_SEC);
	vaddr_t gpr5 = core_mmu_get_va(IOMUXC_BASE, MEM_AREA_IO_SEC) +
				       IOMUXC_GPR5_OFFSET;
	uint32_t cpu, val;
	bool wfi;

	cpu = affinity;

	if (soc_is_imx7d())
		wfi = true;
	else
		wfi = read32(gpr5) & ARM_WFI_STAT_MASK(cpu);

	if ((imx_get_src_gpr(cpu) == 0) || !wfi)
		return PSCI_AFFINITY_LEVEL_ON;

	DMSG("cpu: %" PRIu32 "GPR: %" PRIx32, cpu, imx_get_src_gpr(cpu));
	/*
	 * Wait secondary cpus ready to be killed
	 * TODO: Change to non dead loop
	 */
	if (soc_is_imx7d()) {
		while (read32(va + SRC_GPR1_MX7 + cpu * 8 + 4) != UINT_MAX)
			;

		val = read32(va + SRC_A7RCR1);
		val &=  ~BIT32(SRC_A7RCR1_A7_CORE1_ENABLE_OFFSET + (cpu - 1));
		write32(val, va + SRC_A7RCR1);
	} else {
		while (read32(va + SRC_GPR1 + cpu * 8 + 4) != UINT32_MAX)
			;

		/* Kill cpu */
		val = read32(va + SRC_SCR);
		val &= ~BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + cpu - 1);
		val |=  BIT32(SRC_SCR_CORE1_RST_OFFSET + cpu - 1);
		write32(val, va + SRC_SCR);
	}

	/* Clean arg */
	imx_set_src_gpr(cpu, 0);

	return PSCI_AFFINITY_LEVEL_OFF;
}
#endif

