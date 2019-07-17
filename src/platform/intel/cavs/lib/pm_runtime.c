// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file platform/intel/cavs/pm_runtime.c
 * \brief Runtime power management implementation for Apollolake, Cannonlake
 *        and Icelake
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/lib/alloc.h>
#include <sof/lib/dai.h>
#include <sof/lib/io.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/shim.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <config.h>
#include <stdint.h>

#define trace_power(format, ...)	\
	trace_event(TRACE_CLASS_POWER, format, ##__VA_ARGS__)

#if CONFIG_APOLLOLAKE || CONFIG_CANNONLAKE
//TODO: add support or at least stub api for Icelake based on Cannonlake
#include <platform/power_down.h>
#endif

/** \brief Runtime power management data pointer. */
struct pm_runtime_data *_prd;

/**
 * \brief Forces Host DMAs to exit L1.
 */
static inline void cavs_pm_runtime_force_host_dma_l1_exit(void)
{
	uint32_t flags;

	spin_lock_irq(&_prd->lock, flags);

	if (!(shim_read(SHIM_SVCFG) & SHIM_SVCFG_FORCE_L1_EXIT)) {
		shim_write(SHIM_SVCFG,
			   shim_read(SHIM_SVCFG) | SHIM_SVCFG_FORCE_L1_EXIT);

		wait_delay(PLATFORM_FORCE_L1_EXIT_TIME);

		shim_write(SHIM_SVCFG,
			   shim_read(SHIM_SVCFG) & ~(SHIM_SVCFG_FORCE_L1_EXIT));
	}

	spin_unlock_irq(&_prd->lock, flags);
}

#if CONFIG_CAVS_SSP
static inline void cavs_pm_runtime_dis_ssp_clk_gating(uint32_t index)
{
#if CONFIG_APOLLOLAKE
	uint32_t shim_reg;

	shim_reg = shim_read(SHIM_CLKCTL) |
		(index < DAI_NUM_SSP_BASE ?
			SHIM_CLKCTL_I2SFDCGB(index) :
			SHIM_CLKCTL_I2SEFDCGB(index - DAI_NUM_SSP_BASE));

	shim_write(SHIM_CLKCTL, shim_reg);

	trace_power("dis-ssp-clk-gating index %d CLKCTL %08x", index, shim_reg);
#endif
}

static inline void cavs_pm_runtime_en_ssp_clk_gating(uint32_t index)
{
#if CONFIG_APOLLOLAKE
	uint32_t shim_reg;

	shim_reg = shim_read(SHIM_CLKCTL) &
		~(index < DAI_NUM_SSP_BASE ?
			SHIM_CLKCTL_I2SFDCGB(index) :
			SHIM_CLKCTL_I2SEFDCGB(index - DAI_NUM_SSP_BASE));

	shim_write(SHIM_CLKCTL, shim_reg);

	trace_power("en-ssp-clk-gating index %d CLKCTL %08x", index, shim_reg);
#endif
}
#endif

#if CONFIG_CAVS_DMIC
static inline void cavs_pm_runtime_dis_dmic_clk_gating(uint32_t index)
{
#if CONFIG_APOLLOLAKE || CONFIG_CANNONLAKE
	(void)index;
	uint32_t shim_reg;

	shim_reg = shim_read(SHIM_CLKCTL) | SHIM_CLKCTL_DMICFDCGB;

	shim_write(SHIM_CLKCTL, shim_reg);

	trace_power("dis-dmic-clk-gating index %d CLKCTL %08x", index,
		    shim_reg);
#endif
#if CONFIG_CANNONLAKE || CONFIG_ICELAKE || CONFIG_SUECREEK
	/* Disable DMIC clock gating */
	io_reg_write(DMICLCTL,
		    (io_reg_read(DMICLCTL) | DMIC_DCGD));
#endif
}

static inline void cavs_pm_runtime_en_dmic_clk_gating(uint32_t index)
{
#if CONFIG_APOLLOLAKE || CONFIG_CANNONLAKE
	(void)index;
	uint32_t shim_reg;

	shim_reg = shim_read(SHIM_CLKCTL) & ~SHIM_CLKCTL_DMICFDCGB;

	shim_write(SHIM_CLKCTL, shim_reg);

	trace_power("en-dmic-clk-gating index %d CLKCTL %08x", index, shim_reg);
#endif
#if CONFIG_CANNONLAKE || CONFIG_ICELAKE || CONFIG_SUECREEK
	/* Enable DMIC clock gating */
	io_reg_write(DMICLCTL,
		    (io_reg_read(DMICLCTL) & ~DMIC_DCGD));
#endif
}
static inline void cavs_pm_runtime_en_dmic_power(uint32_t index)
{
	(void) index;
#if CONFIG_CANNONLAKE || CONFIG_ICELAKE || CONFIG_SUECREEK
	/* Enable DMIC power */
	io_reg_write(DMICLCTL,
		    (io_reg_read(DMICLCTL) | DMICLCTL_SPA));
#endif
}
static inline void cavs_pm_runtime_dis_dmic_power(uint32_t index)
{
	(void) index;
#if CONFIG_CANNONLAKE || CONFIG_ICELAKE || CONFIG_SUECREEK
	/* Disable DMIC power */
	io_reg_write(DMICLCTL,
		    (io_reg_read(DMICLCTL) & (~DMICLCTL_SPA)));
#endif
}
#endif /* #if defined(CONFIG_CAVS_DMIC) */

static inline void cavs_pm_runtime_dis_dwdma_clk_gating(uint32_t index)
{
#if CONFIG_APOLLOLAKE
	uint32_t shim_reg;

	shim_reg = shim_read(SHIM_CLKCTL) | SHIM_CLKCTL_LPGPDMAFDCGB(index);

	shim_write(SHIM_CLKCTL, shim_reg);

	trace_power("dis-dwdma-clk-gating index %d CLKCTL %08x", index,
		    shim_reg);
#elif CONFIG_CANNONLAKE
	uint32_t shim_reg;

	shim_reg = shim_read(SHIM_GPDMA_CLKCTL(index)) |
		   SHIM_CLKCTL_LPGPDMAFDCGB;

	shim_write(SHIM_GPDMA_CLKCTL(index), shim_reg);

	trace_power("dis-dwdma-clk-gating index %d GPDMA_CLKCTL %08x", index,
		    shim_reg);
#endif
}

static inline void cavs_pm_runtime_en_dwdma_clk_gating(uint32_t index)
{
#if CONFIG_APOLLOLAKE
	uint32_t shim_reg;

	shim_reg = shim_read(SHIM_CLKCTL) & ~SHIM_CLKCTL_LPGPDMAFDCGB(index);

	shim_write(SHIM_CLKCTL, shim_reg);

	trace_power("en-dwdma-clk-gating index %d CLKCTL %08x", index,
		    shim_reg);
#elif CONFIG_CANNONLAKE
	uint32_t shim_reg;

	shim_reg = shim_read(SHIM_GPDMA_CLKCTL(index)) &
		   ~SHIM_CLKCTL_LPGPDMAFDCGB;

	shim_write(SHIM_GPDMA_CLKCTL(index), shim_reg);

	trace_power("en-dwdma-clk-gating index %d GPDMA_CLKCTL %08x", index,
		    shim_reg);
#endif
}

void platform_pm_runtime_init(struct pm_runtime_data *prd)
{
	struct platform_pm_runtime_data *pprd;

	_prd = prd;

	pprd = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*pprd));
	_prd->platform_data = pprd;
}

void platform_pm_runtime_get(enum pm_runtime_context context, uint32_t index,
			     uint32_t flags)
{
	/* Action based on context */
	switch (context) {
#if CONFIG_CAVS_SSP
	case SSP_CLK:
		cavs_pm_runtime_dis_ssp_clk_gating(index);
		break;
#endif
#if CONFIG_CAVS_DMIC
	case DMIC_CLK:
		cavs_pm_runtime_dis_dmic_clk_gating(index);
		break;
	case DMIC_POW:
		cavs_pm_runtime_en_dmic_power(index);
		break;
#endif
	case DW_DMAC_CLK:
		cavs_pm_runtime_dis_dwdma_clk_gating(index);
		break;
	default:
		break;
	}
}

void platform_pm_runtime_put(enum pm_runtime_context context, uint32_t index,
			     uint32_t flags)
{
	switch (context) {
	case PM_RUNTIME_HOST_DMA_L1:
		cavs_pm_runtime_force_host_dma_l1_exit();
		break;
#if CONFIG_CAVS_SSP
	case SSP_CLK:
		cavs_pm_runtime_en_ssp_clk_gating(index);
		break;
#endif
#if CONFIG_CAVS_DMIC
	case DMIC_CLK:
		cavs_pm_runtime_en_dmic_clk_gating(index);
		break;
	case DMIC_POW:
		cavs_pm_runtime_dis_dmic_power(index);
		break;
#endif
	case DW_DMAC_CLK:
		cavs_pm_runtime_en_dwdma_clk_gating(index);
		break;
	default:
		break;
	}
}

#if CONFIG_APOLLOLAKE || CONFIG_CANNONLAKE
void platform_pm_runtime_power_off(void)
{
	uint32_t hpsram_mask[PLATFORM_HPSRAM_SEGMENTS], i;
	//TODO: add LDO control for LP SRAM - set LDO BYPASS & LDO ON
	//TODO: hpsram_mask to be used in the future for run-time
	//power management of SRAM banks i.e use. HPSRAM_MASK() macro
	/* power down entire HPSRAM */
	for (i = 0; i < PLATFORM_HPSRAM_SEGMENTS; i++)
		hpsram_mask[i] = HPSRAM_MASK(i);

	power_down(true, hpsram_mask);
}
#endif
