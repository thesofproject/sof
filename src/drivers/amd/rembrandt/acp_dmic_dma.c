// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2022 AMD. All rights reserved.
//
//Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		Bala Kishore <balakishore.pati@amd.com>

#include <rtos/atomic.h>
#include <sof/audio/component.h>
#include <rtos/bit.h>
#include <sof/drivers/acp_dai_dma.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <rtos/spinlock.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/fw_scratch_mem.h>
#include <platform/chip_registers.h>
#include <platform/acp_dmic_dma.h>

/* 109c7aba-a7ba-43c3-b9-42-59-e2-0a-66-11-be */
SOF_DEFINE_UUID("acp_dmic_dma", acp_dmic_dma_uuid, 0x109c7aba, 0xa7ba, 0x43c3,
		0xb9, 0x42, 0x59, 0xe2, 0x0a, 0x66, 0x11, 0xbe);
DECLARE_TR_CTX(acp_dmic_dma_rmb_tr, SOF_UUID(acp_dmic_dma_uuid), LOG_LEVEL_INFO);

uint32_t dmic_rngbuff_size;
extern struct acp_dmic_silence acp_initsilence;

int acp_dmic_dma_start(struct dma_chan_data *channel)
{
	acp_wov_pdm_decimation_factor_t deci_fctr;
	acp_wov_misc_ctrl_t wov_misc_ctrl;
	acp_wov_pdm_dma_enable_t  pdm_dma_enable;
	acp_hstdm_iter_t	hs_iter;
	acp_hstdm_irer_t	hs_irer;
	uint32_t		acp_pdm_en;
	struct timer *timer = timer_get();
	uint64_t deadline = platform_timer_get(timer) +
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) * 500 / 1000;

	hs_iter = (acp_hstdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_HSTDM_ITER));
	hs_irer = (acp_hstdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_HSTDM_IRER));
	acp_pdm_en = (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE);
	acp_initsilence.silence_incr = 0;
	acp_initsilence.coeff = 0;

	if (!hs_iter.bits.hstdm_txen && !hs_irer.bits.hstdm_rx_en && !acp_pdm_en) {
		io_reg_write((PU_REGISTER_BASE + ACP_CLKMUX_SEL), ACP_ACLK_CLK_SEL);
		/* Request SMU to set aclk to 600 Mhz */
		acp_change_clock_notify(600000000);
	}
	channel->status = COMP_STATE_ACTIVE;
	if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		/* Decimation Factor */
		deci_fctr.u32all = 2;
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_DECIMATION_FACTOR,
							deci_fctr.u32all);
		/* PDM Control */
		wov_misc_ctrl = (acp_wov_misc_ctrl_t)
			io_reg_read(PU_REGISTER_BASE + ACP_WOV_MISC_CTRL);
		wov_misc_ctrl.u32all |= 0x10;
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_MISC_CTRL,
						wov_misc_ctrl.u32all);
		/* PDM Enable */
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE, 1);
		/* PDM DMA Enable */
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE, 1);
		/* Check the PDM DMA Enable status bit */
		pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
			io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
		while (!(uint32_t)pdm_dma_enable.bits.pdm_dma_en_status) {
			if (deadline < platform_timer_get(timer)) {
				/* safe check in case we've got preempted after read */
				if ((uint32_t)pdm_dma_enable.bits.pdm_dma_en_status)
					return 0;
				tr_err(&acp_dmic_dma_rmb_tr, "timed out for dma start");
				return -ETIME;
			}
			pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
					io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
		}
	}
	return 0;
}

int acp_dmic_dma_stop(struct dma_chan_data *channel)
{
	acp_wov_pdm_dma_enable_t  pdm_dma_enable;
	acp_hstdm_iter_t	hs_iter;
	acp_hstdm_irer_t	hs_irer;
	uint32_t acp_pdm_en;
	struct timer *timer = timer_get();
	uint64_t deadline = platform_timer_get(timer) +
			clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) * 500 / 1000;
	switch (channel->status) {
	case COMP_STATE_READY:
	case COMP_STATE_PREPARE:
		return 0;
	case COMP_STATE_PAUSED:
	case COMP_STATE_ACTIVE:
		break;
	default:
		return -EINVAL;
	}
	channel->status = COMP_STATE_READY;
	/* Disable PDM DMA */
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE, 0);
	/* Disable PDM */
	pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
		io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
	/* Check PDM DMA Status */
	while ((uint32_t)pdm_dma_enable.bits.pdm_dma_en_status) {
		if (deadline < platform_timer_get(timer)) {
			/* safe check in case we've got preempted after read */
			if ((uint32_t)pdm_dma_enable.bits.pdm_dma_en_status)
				return 0;
			tr_err(&acp_dmic_dma_rmb_tr, "timed out for dma stop");
			return -ETIME;
		}
		pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
			io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
	}
	/* Disable  PDM */
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE, 0);
	/* Clear PDM FIFO */
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_FIFO_FLUSH, 1);
	hs_iter = (acp_hstdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_HSTDM_ITER));
	hs_irer = (acp_hstdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_HSTDM_IRER));
	acp_pdm_en = (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE);

	if (!hs_iter.bits.hstdm_txen && !hs_irer.bits.hstdm_rx_en && !acp_pdm_en) {
		/* Request SMU to set aclk to minimum aclk */
		acp_change_clock_notify(0);
		io_reg_write((PU_REGISTER_BASE + ACP_CLKMUX_SEL), ACP_INTERNAL_CLK_SEL);
	}
	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
int acp_dmic_dma_set_config(struct dma_chan_data *channel,
			    struct dma_sg_config *config)
{
	uint32_t ring_buff_addr;
	uint32_t timeperiod_ms;
	acp_wov_rx_ringbufsize_t dmic_ringbuff_size;
	acp_wov_rx_intr_watermark_size_t watermark;

	channel->is_scheduling_source = true;
	channel->direction = config->direction;
	switch (config->direction) {
	case DMA_DIR_DEV_TO_MEM:
	case DMA_DIR_MEM_TO_DEV:
	acp_initsilence.dmic_rngbuff_addr1 = (char *)config->elem_array.elems[0].dest;
		config->elem_array.elems[0].dest =
			(config->elem_array.elems[0].dest & ACP_DRAM_ADDRESS_MASK);
		ring_buff_addr = (config->elem_array.elems[0].dest | ACP_SRAM);
		/* Load Ring buffer address */
		io_reg_write(PU_REGISTER_BASE +
			ACP_WOV_RX_RINGBUFADDR, ring_buff_addr);
		/* Load Ring buffer Size */
		dmic_rngbuff_size = (config->elem_array.elems[0].size *
				config->elem_array.count);
		dmic_ringbuff_size.bits.rx_ringbufsize = dmic_rngbuff_size;
		io_reg_write(PU_REGISTER_BASE +
			ACP_WOV_RX_RINGBUFSIZE, dmic_ringbuff_size.u32all);
		/* Write the ring buffer size to register */
		watermark.bits.rx_intr_watermark_size = (dmic_rngbuff_size >> 1);
		io_reg_write(PU_REGISTER_BASE +
			ACP_WOV_RX_INTR_WATERMARK_SIZE, watermark.u32all);

		timeperiod_ms = dmic_rngbuff_size / (acp_initsilence.num_chs *
			acp_initsilence.samplerate_khz * acp_initsilence.bytes_per_sample *
			config->elem_array.count);
		acp_initsilence.silence_cnt = DMIC_SETTLING_TIME_MS / timeperiod_ms;
		acp_initsilence.numfilterbuffers = DMIC_SMOOTH_TIME_MS / timeperiod_ms;

		break;
	default:
		tr_err(&acp_dmic_dma_rmb_tr, "unsupported config direction");
		return -EINVAL;
	}
	if (!config->cyclic) {
		tr_err(&acp_dmic_dma_rmb_tr, "cyclic configurations only supported!");
		return -EINVAL;
	}
	if (config->scatter) {
		tr_err(&acp_dmic_dma_rmb_tr, "scatter enabled, not supported for now!");
		return -EINVAL;
	}
	return 0;
}

int acp_dmic_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = ACP_DMA_BUFFER_ALIGN_128;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = PLATFORM_DCACHE_ALIGN;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = ACP_DAI_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		return -ENOENT; /* Attribute not found */
	}
	return 0;
}
