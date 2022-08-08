/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench_api.h"
#include "bench_utils.h"

#define MIN_DELAY  1000ULL

/* neorv32-machine-timer */
#if DT_HAS_COMPAT_STATUS_OKAY(andestech_machine_timer)
#define DT_DRV_COMPAT andestech_machine_timer

#define MTIME_REG	DT_INST_REG_ADDR(0)
#define MTIMECMP_REG	(DT_INST_REG_ADDR(0) + 8)
#define TIMER_IRQN	DT_INST_IRQN(0)
/* neorv32-machine-timer */
#elif DT_HAS_COMPAT_STATUS_OKAY(neorv32_machine_timer)
#define DT_DRV_COMPAT neorv32_machine_timer

#define MTIME_REG	DT_INST_REG_ADDR(0)
#define MTIMECMP_REG	(DT_INST_REG_ADDR(0) + 8)
#define TIMER_IRQN	DT_INST_IRQN(0)
/* nuclei,systimer */
#elif DT_HAS_COMPAT_STATUS_OKAY(nuclei_systimer)
#define DT_DRV_COMPAT nuclei_systimer

#define MTIME_REG	DT_INST_REG_ADDR(0)
#define MTIMECMP_REG	(DT_INST_REG_ADDR(0) + 8)
#define TIMER_IRQN	DT_INST_IRQN(0)
/* sifive,clint0 */
#elif DT_HAS_COMPAT_STATUS_OKAY(sifive_clint0)
#define DT_DRV_COMPAT sifive_clint0

#define MTIME_REG	(DT_INST_REG_ADDR(0) + 0xbff8U)
#define MTIMECMP_REG	(DT_INST_REG_ADDR(0) + 0x4000U)
#define TIMER_IRQN	DT_INST_IRQ_BY_IDX(0, 1, irq)
/* telink,machine-timer */
#elif DT_HAS_COMPAT_STATUS_OKAY(telink_machine_timer)
#define DT_DRV_COMPAT telink_machine_timer

#define MTIME_REG	DT_INST_REG_ADDR(0)
#define MTIMECMP_REG	(DT_INST_REG_ADDR(0) + 8)
#define TIMER_IRQN	DT_INST_IRQN(0)
#endif

uint32_t riscv_machine_timer_irq = TIMER_IRQN;

/**
 * @brief Read the current cycle count from the MTIME register
 */
static uint64_t mtime(void)
{
#ifdef CONFIG_64BIT
        return *(volatile uint64_t *)MTIME_REG;
#else
        volatile uint32_t *r = (uint32_t *)MTIME_REG;
        uint32_t  lo;
        uint32_t  hi;

        do {
                hi = r[1];
                lo = r[0];
        } while (r[1] != hi);

        return (((uint64_t)hi) << 32) | lo;
#endif
}

static uint64_t get_hart_mtimecmp(void)
{
	return MTIMECMP_REG + (_current_cpu->id * 8);
}

/**
 * @brief Set the MTIME comparison register
 */
static void set_mtimecmp(uint64_t time)
{
#ifdef CONFIG_64BIT
        *(volatile uint64_t *)get_hart_mtimecmp() = time;
#else
        volatile uint32_t *r = (uint32_t *)(uint32_t)get_hart_mtimecmp();

        r[1] = 0xffffffff;
        r[0] = (uint32_t)time;
        r[1] = (uint32_t)(time >> 32);
#endif
}

/**
 * @brief Get the number of elapsed cycles between two raw cycle counts.
 *
 * This routine is expected only to be used with the interrupt latency test,
 * and is expected to be called shortly after the timer interrupt has fired.
 */
bench_time_t bench_timer_cycles_diff(bench_time_t trigger_point,
				     bench_time_t sample_point)
{
	return (sample_point - trigger_point);
}

/**
 * @brief Get the timer's current cycles value
 *
 * This gets the raw cycle count. The HPET timer is upward counting.
 */
bench_time_t bench_timer_cycles_get(void)
{
	return (bench_time_t)mtime();
}

/**
 * @brief Sets the timer ISR to trigger in @a usec microseconds
 *
 * This routine assumes that @a usec leads to a valid cycles calculation.
 */
bench_time_t bench_timer_isr_expiry_set(uint32_t usec)
{
	uint64_t cyc_per_sec;
	uint64_t cyc_per_tick;
	uint64_t next;

	cyc_per_sec = (uint64_t) bench_timer_cycles_per_second();
	cyc_per_sec >>= CONFIG_RISCV_MACHINE_TIMER_SYSTEM_CLOCK_DIVIDER;
	cyc_per_tick = (cyc_per_sec * usec) / 1000000ULL;

	if (cyc_per_tick < MIN_DELAY) {
		cyc_per_tick += MIN_DELAY;
	}

	next = mtime() + cyc_per_tick;

	set_mtimecmp(next);

	return (bench_time_t) next;
}

/**
 * @brief Restore both the old timer ISR handler and its tick rate
 */
void bench_timer_isr_restore(bench_isr_handler_t handler)
{
	bench_timer_isr_set(handler);
}
