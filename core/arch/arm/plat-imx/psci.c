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
#include <sm/sm.h>
#include <sm/optee_smc.h>
#include <sm/psci.h>
#include <tee/entry_std.h>
#include <tee/entry_fast.h>
#include "imx_suspend.h"

#define POWER_BTN_GPIO          93
#define LED_GPIO                2


static void do_suspend(void);
/*static*/ void setup_low_power_modes(void);
/*static*/ void enable_power_btn_int(void);
static void init_led(void);
static void set_led(bool on);
/*static*/ void init_suspend_resume_stub(void);
static void write_clpcr (uint32_t clpcr);

struct armv7_processor_state* resume_state;
vaddr_t suspend_resume_stub;

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

void imx_psci_init(void)
{
    setup_low_power_modes();

    init_suspend_resume_stub();

    init_led();
}

void setup_low_power_modes(void)
{
    vaddr_t ccm;
    vaddr_t iomuxc;
    uint32_t cgpr;
    uint32_t gpr1;
    uint32_t clpcr;

    ccm = periph_base(CCM_BASE, MEM_AREA_IO_SEC); 
    iomuxc = periph_base(IOMUXC_BASE, MEM_AREA_IO_NSEC); 

    DMSG(
        "Configuring CCM for low power modes. "
        "(ccm = 0x%08x, iomuxc = 0x%08x)\n",
        (uint32_t)ccm,
        (uint32_t)iomuxc);

    /* set required bits in CGPR */
    cgpr = read32(ccm + CCM_CGPR_OFFSET);
    cgpr |= CCM_CGPR_MUST_BE_ONE;
    cgpr &= ~CCM_CGPR_INT_MEM_CLK_LPM;
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
    write_clpcr(clpcr);
}

int psci_features(uint32_t psci_fid)
{
    DMSG(
        "psci_features was called. (psci_fid = 0x%x, PSCI_CPU_SUSPEND = 0x%x)",
        psci_fid,
        PSCI_CPU_SUSPEND);

    switch (psci_fid) {
    case PSCI_CPU_SUSPEND:
        return (1 << 1) | // Use extended StateID format
               (1 << 0);  // OS-initiated mode supported

    default:
        return PSCI_RET_NOT_SUPPORTED;
    }
}

#if defined(CFG_BOOT_SECONDARY_REQUEST)

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

#endif /* CFG_BOOT_SECONDARY_REQUEST */

void enable_power_btn_int(void)
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

void init_led(void)
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

void set_led(bool on)
{
    vaddr_t bank_base;
    uint32_t value;

    //DMSG("Setting LED state: %d\n", on);

    bank_base = periph_base(GPIO_BASE, MEM_AREA_IO_NSEC) + 
        GPIO_BANK_SIZE * BANK_FROM_PIN(LED_GPIO);

    value = read32(bank_base + GPIO_DR_OFFSET);
    if (on) {
        value |= BIT_FROM_PIN(LED_GPIO);
    } else {
        value &= ~BIT_FROM_PIN(LED_GPIO);
    }

    write32(value, bank_base + GPIO_DR_OFFSET);
}


void init_suspend_resume_stub(void)
{
    vaddr_t ocram_base;
    uint32_t resume_fn_len;

    ocram_base = periph_base(OCRAM_BASE, MEM_AREA_RAM_SEC);
    
    resume_state = (struct armv7_processor_state*)ocram_base;
    suspend_resume_stub = 
        (((vaddr_t)resume_state + sizeof(*resume_state) + 0x7) & ~0x7);

    // copy imx_resume to ocram
    DMSG("Copying resume stub to ocram\n");
    resume_fn_len = &imx_resume_end - &imx_resume_start;
    memcpy(
        (void*)suspend_resume_stub,
        &imx_resume_start,
        resume_fn_len);
}

uint32_t *stack_cookie_ptr;
uint32_t stack_cookie_value;
void check_stack_cookie(uint32_t lineno);
#define CHECK_STACK_COOKIE() check_stack_cookie(__LINE__);

void check_stack_cookie(uint32_t lineno)
{
    if (stack_cookie_ptr != NULL) {
        if (*stack_cookie_ptr != stack_cookie_value) {
            DMSG("Stack cookie check failed at line %d\n", lineno);
            panic("Stack cookie is corrupt!\n");
        }
    }
}

uint32_t hash_section(void* start);
uint32_t hash_section(void* start)
{
    uint32_t sum = 0;
    uint32_t* cur = (uint32_t*)start;
    uint32_t* end = cur + 1024 * 1024 / sizeof(uint32_t);
    while (cur != end) {
        sum += *cur;
        ++cur;
    }

    return sum;
}

void init_hashes(void);
void init_hashes(void)
{
    vaddr_t s;
    vaddr_t e;
    uint32_t i;
    uint32_t num_sections;
                    
    /* get virtual addr/size of NSec shared mem allcated from teecore */
    core_mmu_get_mem_by_type(MEM_AREA_NSEC_SHM, &s, &e);

    num_sections = (e - s) / (1024 * 1024);

    DMSG(
        "Computing memory hashes. (s = 0x%x, e = 0x%x, num_sections = %d)",
        (uint32_t)s,
        (uint32_t)e,
        num_sections);

    if (ARRAY_SIZE(resume_state->hashes) < num_sections)
        panic("Not enough storage for hashes");

    // compute hashes of each 1MB chunk of nonsecure memory 
    for (i = 0; i < num_sections; ++i) {
        //DMSG(
        //    "Computing hash of section %d at 0x%lx\n",
        //    i,
        //    s + (1024 * 1024 * i));

        resume_state->hashes[i] = hash_section((void*)(s + (1024 * 1024 * i)));
    }
}

void check_hashes(void);
void check_hashes(void)
{
    vaddr_t s;
    vaddr_t e;
    uint32_t i;
    uint32_t num_sections;
                    
    DMSG("Checking hashes ... ");

    /* get virtual addr/size of NSec shared mem allcated from teecore */
    core_mmu_get_mem_by_type(MEM_AREA_NSEC_SHM, &s, &e);

    num_sections = (e - s) / (1024 * 1024);

    // compute hashes of each 1MB chunk of nonsecure memory 
    for (i = 0; i < num_sections; ++i) {
        uint32_t actual = hash_section((void*)(s + (1024 * 1024 * i)));
        if (actual != resume_state->hashes[i]) {
            DMSG(
                "Hash of section 0x%lx does not match. "
                "(expected = 0x%x, actual= 0x%x)\n",
                s + (1024 * 1024 * i),
                resume_state->hashes[i],
                actual);

            panic("Memory hash check failure!");
        }
    }

    DMSG("ok");
}

void set_timer_interrupt(uint32_t ms);
void set_timer_interrupt(uint32_t ms)
{
    vaddr_t gpc_base;
    vaddr_t epit_base;
    uint32_t value;

    DMSG("Programming wakeup interrupt in %d ms\n", ms);

    gpc_base = periph_base(GPC_BASE, MEM_AREA_IO_SEC);
    epit_base = periph_base(EPIT1_BASE, MEM_AREA_IO_SEC);

    write32(0xffffffff, gpc_base + GPC_IMR1_OFFSET);
    write32(0xffffffff, gpc_base + GPC_IMR2_OFFSET);
    write32(0xffffffff, gpc_base + GPC_IMR3_OFFSET);
    write32(0xffffffff, gpc_base + GPC_IMR4_OFFSET);

    // disable EPIT and software reset
    value =
        EPIT_CR_ENMOD |
        EPIT_CR_RLD |
        EPIT_CR_SWR |          // software reset
        EPIT_CR_WAITEN |
        EPIT_CR_STOPEN |
        EPIT_CR_CLKSRC(3);     // low freq ref clock

    write32(value, epit_base + EPIT_CR_OFFSET);

    // clear interrupt
    write32(EPIT_SR_OCIF, epit_base + EPIT_SR_OFFSET);

    // ensure compare register is 0
    write32(0,  epit_base + EPIT_CMPR_OFFSET);

    // write counter value to load register
    write32(EPIT_FREQ * ms / 1000, epit_base + EPIT_LR_OFFSET);

    value =
        EPIT_CR_ENMOD |        // counter starts from reload value
        EPIT_CR_RLD |          // reload from lr
        EPIT_CR_PRESCALER(0) | // divide by 1
        EPIT_CR_WAITEN |
        EPIT_CR_STOPEN |
        EPIT_CR_CLKSRC(3);

    write32(value, epit_base + EPIT_CR_OFFSET);
    write32(value | EPIT_CR_EN, epit_base + EPIT_CR_OFFSET);

    // unmask interrupt in GPC
    write32(~0x01000000, gpc_base + GPC_IMR2_OFFSET);
}

void do_suspend (void)
{
    vaddr_t gpio_base __unused;
    vaddr_t ocram_base __unused;
    vaddr_t ccm_base;
    vaddr_t anatop_base;
    vaddr_t gpc_base;
    vaddr_t src;
    vaddr_t imx_resume_addr;

    //uint32_t dr;

    //DMSG("Suspending CPU\n");  

    imx_resume_addr = (vaddr_t)imx_resume; //suspend_resume_stub;

    //DMSG(
    //    "Are we identity mapped? imx_resume_addr = 0x%x, "
    //    "virt_to_phys(imx_resume_addr) = 0x%x\n",
    //    (unsigned)imx_resume_addr,
    //    (unsigned)virt_to_phys((void*)imx_resume_addr));


    //DMSG(
    //    "Length of imx_resume = %d\n",
    //    &imx_resume_end - &imx_resume_start);

    gpio_base = periph_base(GPIO_BASE, MEM_AREA_IO_NSEC);
    ocram_base = periph_base(OCRAM_BASE, MEM_AREA_RAM_SEC);
    ccm_base = periph_base(CCM_BASE, MEM_AREA_IO_SEC);
    anatop_base = periph_base(ANATOP_BASE, MEM_AREA_IO_SEC);
    gpc_base = periph_base(GPC_BASE, MEM_AREA_IO_SEC);
    src = src_base();

    //DMSG(
    //    "gpio_base = 0x%08x, ocram_base = 0x%08x, anatop_base = 0x%08x, "
    //    "src = 0x%08x, imx_resume = 0x%x, "
    //    "&imx_resume_start = 0x%x",
    //    (uint32_t)gpio_base,
    //    (uint32_t)ocram_base,
    //    (uint32_t)anatop_base,
    //    (uint32_t)src,
    //    (uint32_t)imx_resume,
    //    (uint32_t)&imx_resume_start);
    
    //DMSG("Configuring LPM=STOP\n");
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
       
        write_clpcr(clpcr); 
    }

    //DMSG("Configuring light sleep mode\n");
    // Configure PMU_MISC0 for light sleep mode
    write32(
        ANATOP_MISC0_STOP_MODE_CONFIG,
        anatop_base + ANATOP_MISC0_SET_OFFSET);

    //DMSG("Clearing INT_MEM_CLK_LPM\n");
    // Clear INT_MEM_CLK_LPM
    // (Control for the Deep Sleep signal to the ARM Platform memories)
    {
        uint32_t cgpr;
        cgpr = read32(ccm_base + CCM_CGPR_OFFSET);
        cgpr |= (1 << 1);
        cgpr &= ~CCM_CGPR_INT_MEM_CLK_LPM;
        write32(cgpr, ccm_base + CCM_CGPR_OFFSET);
    }

    //DMSG("Writing PGC_CPU to configure CPU power gating\n");
    // Configure PGC to power down CPU on next stop mode request
    write32(GPC_PGC_CTRL_PCR, gpc_base + GPC_PGC_CPU_CTRL_OFFSET);

    //DMSG("Configuring GPC interrupt controller for wakeup\n");
    // Enable GPIO 93 as the only wakeup source (GPIO3) IRQ 103
    // XXX
    /*
    enable_power_btn_int();
    {
        uint32_t val;

        val = read32(gpc_base + GPC_IMR2_OFFSET);
        val |= 0x01000000;  // mask EPIT
        write32(val, gpc_base + GPC_IMR2_OFFSET);
        
        val = read32(gpc_base + GPC_IMR3_OFFSET);
        val &= ~BIT32(7);   // unmask GPIO
        write32(val, gpc_base + GPC_IMR3_OFFSET);

        //write32(0xffffffff, gpc_base + GPC_IMR1_OFFSET);
        //write32(0xffffffff, gpc_base + GPC_IMR2_OFFSET);
        //write32(~BIT32(7), gpc_base + GPC_IMR3_OFFSET);
        //write32(0xffffffff, gpc_base + GPC_IMR4_OFFSET);
    }
    */

    // set a timer to wake us up 1 second from now
    //set_timer_interrupt(1000);

    //set_led(false);

    // Program resume address into SRC
    // XXX use virt_to_phys and suspend_resume_stub. This should really be
    // programmed from the assembly side
    //DMSG(
    //    "Configuring resume address. 0x%x\n",
    //    (uint32_t)virt_to_phys((void*)imx_resume_addr));

    //write32(virt_to_phys((void*)imx_resume_addr), src + SRC_GPR1);
    write32(virt_to_phys((void*)imx_resume_addr), src + SRC_GPR1);
    write32(virt_to_phys(resume_state), src + SRC_GPR2);

    //DMSG("Executing WFI\n");

    // Flush cache
    cache_maintenance_l1(DCACHE_CLEAN, NULL, 0);

#if defined(CFG_PL310)
    cache_maintenance_l2(L2CACHE_CLEAN, 0, 0);
#endif

    // Invalidate icache before jumping to code in OCRAM
    //cache_maintenance_l1(ICACHE_INVALIDATE, NULL, 0);

    //DMSG("Successfully flushed cache, executing wfi\n");

    // XXX here's where we would jump to the suspend stub
    // to put DDR in self-refresh and execute WFI

    // XXX: precondition to resume: ROM_CLK_ENABLE must be ungated
    // XXX: add assertion that OCRAM_CLK_ENABLE is ungated


    wfi();

    /*
    DMSG(
        "WFI should not have returned! 0x%08x 0x%08x 0x%08x 0x%08x\n",
        ~read32(gpc_base + GPC_IMR1_OFFSET) &
             read32(gpc_base + GPC_ISR1_OFFSET),
        ~read32(gpc_base + GPC_IMR2_OFFSET) &
             read32(gpc_base + GPC_ISR2_OFFSET),
        ~read32(gpc_base + GPC_IMR3_OFFSET) &
             read32(gpc_base + GPC_ISR3_OFFSET),
        ~read32(gpc_base + GPC_IMR4_OFFSET) &
             read32(gpc_base + GPC_ISR4_OFFSET));
   */

    // disarm CPU power gating 
    write32(0, gpc_base + GPC_PGC_CPU_CTRL_OFFSET);

    // Configure LPM for RUN mode
    // XXX this should be done regardless of whether we entered WFI or not
    // XXX need to follow errata sequence for updating clpcr
    {
        uint32_t clpcr;

        clpcr = read32(ccm_base + CCM_CLPCR_OFFSET);
        clpcr &= ~CCM_CLPCR_LPM_MASK;
        clpcr &= ~CCM_CLPCR_SBYOS;
        write_clpcr(clpcr);
    }
}

void freeze_epit(bool freeze);
void freeze_epit(bool freeze)
{
    vaddr_t epit_base;
    uint32_t val;

    epit_base = periph_base(EPIT1_BASE, MEM_AREA_IO_SEC);

    val = read32(epit_base + EPIT_CR_OFFSET);
    if (freeze) {
        val &= ~EPIT_CR_EN;
    } else {
        val |= EPIT_CR_EN;
    }
    write32(val, epit_base + EPIT_CR_OFFSET);
}

int psci_cpu_suspend (
    uint32_t power_state __unused,
    uintptr_t entry,
    uint32_t context_id,
    struct sm_ctx *ctx
    )
{
    bool waking;

    //freeze_epit(true);

    //DMSG(
    //    "Hello from psci_cpu_suspend (power_state = %d, "
    //    "entry = 0x%08x, context_id - 0x%x)\n",
    //    power_state,
    //    (unsigned)entry,
    //    context_id);  

    resume_state->gpio_virt_base = periph_base(GPIO_BASE, MEM_AREA_IO_NSEC);
    resume_state->resume_state_virt_base = (uint32_t)resume_state;

    //init_hashes();  // XXX

    // XXX

    waking = save_state_for_suspend(resume_state);
    if (!waking) {
        // Normally this should not return
        do_suspend();

        //DMSG("Failed to suspend, most likely due to pending interrupt\n");
        //freeze_epit(false);
        return PSCI_RET_SUCCESS;
    }

    set_led(true);
    
    //DMSG("Successfully woke from suspend\n");
    
    //check_hashes(); // XXX
    
    // Disarm CPU power gating
    {
        vaddr_t gpc_base;
        gpc_base = periph_base(GPC_BASE, MEM_AREA_IO_SEC); 
        write32(0, gpc_base + GPC_PGC_CPU_CTRL_OFFSET);
    }

    // disarm STOP mode
    // XXX follow errata sequence for updating LPM
    {
        uint32_t clpcr;
        uint32_t cgpr;
        vaddr_t ccm_base = periph_base(CCM_BASE, MEM_AREA_IO_SEC);

        clpcr = read32(ccm_base + CCM_CLPCR_OFFSET);
        clpcr &= ~CCM_CLPCR_LPM_MASK;
        clpcr &= ~CCM_CLPCR_SBYOS;
        write_clpcr(clpcr);

        cgpr = read32(ccm_base + CCM_CGPR_OFFSET);
        cgpr |= CCM_CGPR_INT_MEM_CLK_LPM;
        write32(cgpr, ccm_base + CCM_CGPR_OFFSET);
    }

    // clear out resume address and parameter
    {
        vaddr_t src;
        src = src_base();

        write32(0, src + SRC_GPR1);
        write32(0, src + SRC_GPR2);
    }
    
    //freeze_epit(false);     // XXX
    //blink_led(resume_state->gpio_virt_base);

    ctx->nsec.mon_lr = entry;

    /* Return to SVC mode in ARM state with IRQ and FIQ disabled */
    ctx->nsec.mon_spsr = CPSR_MODE_SVC | CPSR_F | CPSR_I | CPSR_A;
    
    return context_id;
}

void armv7_save_arch_state (struct armv7_arch_state* state)
{
    state->cp15_sctlr = read_sctlr();
    state->cp15_actlr = read_actlr();
    state->cp15_cpacr = read_cpacr();
    state->cp15_ttbcr = read_ttbcr();
    state->cp15_ttbr0 = read_ttbr0();
    state->cp15_ttbr1 = read_ttbr1();
    state->cp15_dacr = read_dacr();
    state->cp15_dfsr = read_dfsr();
    state->cp15_ifsr = read_ifsr();
    state->cp15_dfar = read_dfar();
    state->cp15_ifar = read_ifar();
    state->cp15_prrr = read_prrr();
    state->cp15_nmrr = read_nmrr();
    state->cp15_vbar = read_vbar();
    state->cp15_contextidr = read_contextidr();
}

#define IOMUXC_IRQ_NUM      32

void write_clpcr(uint32_t clpcr)
{
    vaddr_t ccm_base;
    vaddr_t gpc_base;
    uint32_t imr;
    uint32_t imr_offset;

    ccm_base = periph_base(CCM_BASE, MEM_AREA_IO_SEC);
    gpc_base = periph_base(GPC_BASE, MEM_AREA_IO_SEC);
    
    imr_offset = GPC_IMR1_OFFSET + 4 * (IOMUXC_IRQ_NUM - 32) / 32;

    // Unmask IOMUXC interrupt in GPC
    imr = read32(gpc_base + imr_offset);
    imr &= ~(1 << (IOMUXC_IRQ_NUM % 32));
    write32(imr, gpc_base + imr_offset);

    write32(clpcr, ccm_base + CCM_CLPCR_OFFSET);

    // Mask IOMUXC interrupt again
    imr |= 1 << (IOMUXC_IRQ_NUM % 32);
    write32(imr, gpc_base + imr_offset);
}

