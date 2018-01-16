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
#include <drivers/imx_iomux.h>
#include <drivers/imx_uart.h>
#include <drivers/tzc380.h>
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

#define PADDR_INVALID	ULONG_MAX

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

static struct imx_uart_data console_data;

register_phys_mem(MEM_AREA_IO_NSEC, CONSOLE_UART_BASE, CORE_MMU_DEVICE_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, ANATOP_BASE, CORE_MMU_DEVICE_SIZE);
#ifdef CFG_WITH_PAGER
register_phys_mem(
	MEM_AREA_RAM_SEC, CFG_DDR_TEETZ_RESERVED_START, CFG_PAGEABLE_PART_SIZE);
#endif
#ifdef CFG_CYREP
register_phys_mem(MEM_AREA_IO_SEC, CAAM_BASE, CORE_MMU_DEVICE_SIZE);
#endif

const struct thread_handlers *generic_boot_get_handlers(void)
{
	return &handlers;
}

static void main_fiq(void)
{
	gic_it_handle(&gic_data);
}

void console_init(void)
{
	imx_uart_init(&console_data, CONSOLE_UART_BASE);
	register_serial_console(&console_data.chip);
}

void main_init_gic(void)
{
	vaddr_t gicc_base;
	vaddr_t gicd_base;

	gicc_base = core_mmu_get_va(GIC_BASE + GICC_OFFSET, MEM_AREA_IO_SEC);
	gicd_base = core_mmu_get_va(GIC_BASE + GICD_OFFSET, MEM_AREA_IO_SEC);

	if (!gicc_base || !gicd_base)
		panic();

	/* Initialize GIC */
	gic_init(&gic_data, gicc_base, gicd_base);
	itr_init(&gic_data.chip);
}

#if defined(CFG_MX6Q) || defined(CFG_MX6D) || defined(CFG_MX6DL) || \
	defined(CFG_MX7)
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

	DMSG("nsec_entry=0x%08lX, SPSR=0x%08X \n",nsec_entry, nsec_ctx->mon_spsr);
}

#ifdef CFG_TZC380
register_phys_mem(MEM_AREA_IO_SEC, IP2APB_TZASC1_BASE_ADDR, CORE_MMU_DEVICE_SIZE);

static void allow_unsecure_readwrite_entire_memory(void)
{
	// Region 0 always includes the entire physical memory. 
	tzc_configure_region(0, 0, TZC_ATTR_SP_ALL);
}

static TEE_Result init_tzc380(void)
{
	void *va;

	va = phys_to_virt(IP2APB_TZASC1_BASE_ADDR, MEM_AREA_IO_SEC);
	if (!va) {
		EMSG("TZASC1 not mapped");
		panic();
	}

	tzc_init((vaddr_t)va);
	tzc_set_action(TZC_ACTION_ERR);
	tzc_dump_state();

	// Start by allowing both TZ and the normal world to read and write, thus
	// simulating the behavior of systems where the TZASC_ENABLE fuse has not
	// been burnt. Restricting normal world's access to some of the memory
	// regions will be implemented later.
	allow_unsecure_readwrite_entire_memory();
	tzc_dump_state();

	return TEE_SUCCESS;
}

service_init(init_tzc380);
#endif // #ifdef CFG_TZC380

#ifdef CFG_IMX_IOMUX
static TEE_Result iomux_init(void)
{
    imx_iomux_init();
    return TEE_SUCCESS;
}

driver_init(iomux_init);
#endif // #ifdef CFG_IMX_IOMUX
