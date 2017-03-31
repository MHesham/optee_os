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
#include <arm32.h>
#include <console.h>
#include <drivers/imx_uart.h>
#include <io.h>
#include <kernel/generic_boot.h>
#include <kernel/panic.h>
#include <kernel/pm_stubs.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <mm/tee_pager.h>
#include <platform_config.h>
#include <stdint.h>
#include <string.h>
#include <sm/optee_smc.h>
#include <sm/psci.h>
#include <tee/entry_std.h>
#include <tee/entry_fast.h>

#define POWER_BTN_GPIO          93
#define LED_GPIO                2

extern void imx_resume (uint32_t r0);
extern uint8_t imx_resume_start;
extern uint8_t imx_resume_end;

static vaddr_t src_base(void)
{
	static void *va __data; /* in case it's used before .bss is cleared */

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(SRC_BASE, MEM_AREA_IO_SEC);
		return (vaddr_t)va;
	}
	return SRC_BASE;
}

/*
 * phys_base - physical base address of peripheral
 * mem_area - MEM_AREA_IO_SEC or MEM_AREA_IO_NSEC
 */
static vaddr_t periph_base(uint32_t phys_base, uint32_t mem_area)
{

	if (cpu_mmu_enabled()) {
	    return (vaddr_t)phys_to_virt(phys_base, mem_area);
	}
	return phys_base;
}

int psci_cpu_on(uint32_t core_idx, uint32_t entry,
		uint32_t context_id __attribute__((unused)))
{
	uint32_t val;
	vaddr_t va = src_base();

	if ((core_idx == 0) || (core_idx >= CFG_TEE_CORE_NB_CORE))
		return PSCI_RET_INVALID_PARAMETERS;

	/* set secondary cores' NS entry addresses */
	ns_entry_addrs[core_idx] = entry;

	/* boot secondary cores from OP-TEE load address */
	write32((uint32_t)CFG_TEE_LOAD_ADDR, va + SRC_GPR1 + core_idx * 8);

	/* release secondary core */
	val = read32(va + SRC_SCR);
	val |=  BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + (core_idx - 1));
	val |=  BIT32(SRC_SCR_CORE1_RST_OFFSET + (core_idx - 1));
	write32(val, va + SRC_SCR);

	return PSCI_RET_SUCCESS;
}


static void enable_power_btn_int(void)
{
    uint32_t value;
    vaddr_t bank_base;

    bank_base = periph_base(GPIO_BASE, MEM_AREA_IO_NSEC) + 
            GPIO_BANK_SIZE * BANK_FROM_PIN(POWER_BTN_GPIO);

    // ensure power button pin is an input
    value = read32(bank_base + GPIO_DIR_OFFSET);
    value &= ~BIT_FROM_PIN(POWER_BTN_GPIO);
    write32(value, bank_base + GPIO_DIR_OFFSET);

    // configure interrupt for falling edge sensitivity
    value = read32(bank_base + GPIO_EDGE_SEL_OFFSET);
    value &= ~BIT_FROM_PIN(POWER_BTN_GPIO);
    write32(value, bank_base + GPIO_EDGE_SEL_OFFSET);
    
    value = read32(bank_base + GPIO_ICR_OFFSET(POWER_BTN_GPIO));
    value |= GPIO_ICR_FALLING_EDGE << GPIO_ICR_SHIFT(POWER_BTN_GPIO);
    write32(value, bank_base + GPIO_ICR_OFFSET(POWER_BTN_GPIO));

    // enable interrupt
    value = read32(bank_base + GPIO_IMR_OFFSET);
    value |= BIT_FROM_PIN(POWER_BTN_GPIO);
    write32(BIT_FROM_PIN(POWER_BTN_GPIO), bank_base + GPIO_ISR_OFFSET);
    write32(value, bank_base + GPIO_IMR_OFFSET);
}

static void init_led(void)
{
    vaddr_t bank_base;
    uint32_t value;

    DMSG("Setting LED pin to output in ON state\n");

    bank_base = periph_base(GPIO_BASE, MEM_AREA_IO_NSEC) + 
        GPIO_BANK_SIZE * BANK_FROM_PIN(LED_GPIO);

    value = read32(bank_base + GPIO_DIR_OFFSET);
    value |= BIT_FROM_PIN(LED_GPIO);
    write32(value, bank_base + GPIO_DIR_OFFSET);

    value = read32(bank_base + GPIO_DR_OFFSET);
    value |= BIT_FROM_PIN(LED_GPIO);
    write32(value, bank_base + GPIO_DR_OFFSET);

    DMSG("Successfully configured LED\n");
}

int psci_cpu_suspend (
    uint32_t power_state,
    uintptr_t entry __unused,
    uint32_t context_id __unused
    )
{
    vaddr_t gpio_base;
    vaddr_t ocram_base;
    vaddr_t ccm_base;
    vaddr_t anatop_base;
    vaddr_t gpc_base;
    vaddr_t src;
    uint32_t resume_fn_len;

    //uint32_t dr;

    DMSG("Hello from psci_cpu_suspend (power_state = %d)\n", power_state);  

    DMSG(
        "Length of imx_resume = %d\n",
        &imx_resume_end - &imx_resume_start);

    gpio_base = periph_base(GPIO_BASE, MEM_AREA_IO_NSEC);
    ocram_base = periph_base(OCRAM_BASE, MEM_AREA_RAM_SEC);
    ccm_base = periph_base(CCM_BASE, MEM_AREA_IO_SEC);
    anatop_base = periph_base(ANATOP_BASE, MEM_AREA_IO_SEC);
    gpc_base = periph_base(GPC_BASE, MEM_AREA_IO_SEC);
    src = src_base();

    init_led();

    DMSG(
        "gpio_base = 0x%08x, ocram_base = 0x%08x, anatop_base = 0x%08x, "
        "src = 0x%08x, imx_resume = 0x%x, "
        "&imx_resume_start = 0x%x",
        (uint32_t)gpio_base,
        (uint32_t)ocram_base,
        (uint32_t)anatop_base,
        (uint32_t)src,
        (uint32_t)imx_resume,
        (uint32_t)&imx_resume_start);
    
    DMSG("Configuring LPM=STOP\n");
    // Configure LPM for STOP mode
    {
        uint32_t clpcr;

        clpcr = read32(ccm_base + CCM_CLPCR_OFFSET);
        clpcr &= ~CCM_CLPCR_LPM_MASK;
        clpcr |= (2 << CCM_CLPCR_LPM_SHIFT);    // STOP
        clpcr |= CCM_CLPCR_ARM_CLK_DIS_ON_LPM;
        clpcr |= CCM_CLPCR_SBYOS;               // turn off 24Mhz oscillator
        // clpcr |= CCM_CLPCR_VSTBY;              // request standby voltage
        // clpcr |= (0x3 << CCM_CLPCR_STBY_COUNT_SHIFT);
        // clpcr |= CCM_CLPCR_WB_PER_AT_LPM;      // enable well biasing
        
        write32(clpcr, ccm_base + CCM_CLPCR_OFFSET); 
    }

    DMSG("Configuring light sleep mode\n");
    // Configure PMU_MISC0 for light sleep mode
    write32(
        ANATOP_MISC0_STOP_MODE_CONFIG,
        anatop_base + ANATOP_MISC0_SET_OFFSET);

    DMSG("Clearing INT_MEM_CLK_LPM\n");
    // Clear INT_MEM_CLK_LPM
    // (Control for the Deep Sleep signal to the ARM Platform memories)
    {
        uint32_t cgpr;
        cgpr = read32(ccm_base + CCM_CGPR_OFFSET);
        cgpr &= ~CCM_CGPR_INT_MEM_CLK_LPM;
        write32(cgpr, ccm_base + CCM_CGPR_OFFSET);
    }

    DMSG("Writing PGC_CPU to configure CPU power gating\n");
    // Configure PGC to power down CPU on next stop mode request
    write32(GPC_PGC_CTRL_PCR, gpc_base + GPC_PGC_CPU_CTRL_OFFSET);

    DMSG("Configuring GPC interrupt controller for wakeup\n");
    // Enable GPIO 93 as the only wakeup source (GPIO3) IRQ 103
    write32(0xffffffff, gpc_base + GPC_IMR1_OFFSET);
    write32(0xffffffff, gpc_base + GPC_IMR2_OFFSET);
    write32(~BIT32(7), gpc_base + GPC_IMR3_OFFSET);
    write32(0xffffffff, gpc_base + GPC_IMR4_OFFSET);

    enable_power_btn_int();

    // Program resume address into SRC
    DMSG("Configuring resume address\n");
    write32(OCRAM_BASE, src + SRC_GPR1);
    write32(GPIO_BASE, src + SRC_GPR2);

    // copy imx_resume to ocram
    DMSG("Copying resume stub to ocram\n");
    resume_fn_len = &imx_resume_end - &imx_resume_start;
    memcpy(
        (void*)ocram_base,
        &imx_resume_start,
        resume_fn_len);
  
    cache_maintenance_l1(DCACHE_AREA_CLEAN, (void*)ocram_base, resume_fn_len);
    cache_maintenance_l2(
        DCACHE_AREA_CLEAN,
        virt_to_phys((void*)ocram_base),
        resume_fn_len);

    cache_maintenance_l1(ICACHE_AREA_INVALIDATE, (void*)ocram_base, resume_fn_len);

    DMSG("Successfully copied memory to OCRAM and flushed cache, executing wfi\n");

    wfi();

    //((void (*)(uint32_t r0))ocram_base)((uint32_t)gpio_base);

    DMSG("WFI should not have returned!\n");
    
    // need to copy resume code into OCRAM
    // First prove that we can run LED blink code from OCRAM
    // Then program the resume address into GPR1 and execute WFI

    return PSCI_RET_SUCCESS;
}

