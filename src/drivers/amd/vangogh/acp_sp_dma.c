// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2023 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>

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
#include <platform/acp_sp_dma.h>

SOF_DEFINE_REG_UUID(acp_sp);
DECLARE_TR_CTX(acp_sp_vgh_tr, SOF_UUID(acp_sp_uuid), LOG_LEVEL_INFO);

/* Vangogh hw specific addr map */
#define ACP_DRAM_PHY_TRNS       0x0DEB0000

static uint64_t prev_tx_pos;
static uint64_t prev_rx_pos;
static uint32_t sp_buff_size;

int acp_dai_sp_dma_start(struct dma_chan_data *channel)
{
	acp_i2stdm_ier_t sp_ier;
	acp_i2stdm_iter_t sp_iter;
	acp_i2stdm_irer_t sp_irer;

	sp_iter = (acp_i2stdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_ITER));
	sp_irer = (acp_i2stdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_IRER));

	if (!sp_iter.bits.i2stdm_txen && !sp_irer.bits.i2stdm_rx_en)
		/* Request SMU to set aclk to 600 Mhz */
		acp_change_clock_notify(600000000);

	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
		channel->status = COMP_STATE_ACTIVE;
		prev_tx_pos = 0;
		sp_ier = (acp_i2stdm_ier_t)io_reg_read(PU_REGISTER_BASE + ACP_I2STDM_IER);
		sp_ier.bits.i2stdm_ien = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_IER), sp_ier.u32all);
		sp_iter.bits.i2stdm_txen = 1;
		sp_iter.bits.i2stdm_tx_protocol_mode = 0;
		sp_iter.bits.i2stdm_tx_data_path_mode = 1;
		sp_iter.bits.i2stdm_tx_samp_len = 2;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_ITER), sp_iter.u32all);
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		channel->status = COMP_STATE_ACTIVE;
		prev_rx_pos = 0;
		sp_ier = (acp_i2stdm_ier_t)io_reg_read(PU_REGISTER_BASE + ACP_I2STDM_IER);
		sp_ier.bits.i2stdm_ien = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_IER), sp_ier.u32all);
		sp_irer.bits.i2stdm_rx_en = 1;
		sp_irer.bits.i2stdm_rx_protocol_mode = 0;
		sp_irer.bits.i2stdm_rx_data_path_mode = 1;
		sp_irer.bits.i2stdm_rx_samplen = 2;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_IRER), sp_irer.u32all);
	} else {
		tr_err(&acp_sp_vgh_tr, "Start direction not defined %d", channel->direction);
		return -EINVAL;
	}

	return 0;
}

int acp_dai_sp_dma_stop(struct dma_chan_data *channel)
{
	acp_i2stdm_irer_t sp_irer;
	acp_i2stdm_iter_t sp_iter;
	acp_hstdm_iter_t hs_iter;
	acp_hstdm_irer_t hs_irer;

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
	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
		sp_iter = (acp_i2stdm_iter_t)io_reg_read(PU_REGISTER_BASE + ACP_I2STDM_ITER);
		sp_iter.bits.i2stdm_txen = 0;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_ITER), sp_iter.u32all);
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		sp_irer = (acp_i2stdm_irer_t)io_reg_read(PU_REGISTER_BASE + ACP_I2STDM_IRER);
		sp_irer.bits.i2stdm_rx_en = 0;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_IRER), sp_irer.u32all);
	} else {
		tr_err(&acp_sp_vgh_tr, "Stop direction not defined %d", channel->direction);
		return -EINVAL;
	}
	sp_iter = (acp_i2stdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_ITER));
	sp_irer = (acp_i2stdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_IRER));
	hs_iter = (acp_hstdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_HSTDM_ITER));
	hs_irer = (acp_hstdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_HSTDM_IRER));
	if (!sp_iter.bits.i2stdm_txen && !sp_irer.bits.i2stdm_rx_en && !hs_iter.bits.hstdm_txen &&
	    !hs_irer.bits.hstdm_rx_en) {
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_IER), SP_IER_DISABLE);
		/* Request SMU to scale down aclk to minimum clk */
		acp_change_clock_notify(0);
		io_reg_write((PU_REGISTER_BASE + ACP_CLKMUX_SEL), ACP_INTERNAL_CLK_SEL);
	}
	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
int acp_dai_sp_dma_set_config(struct dma_chan_data *channel,
			      struct dma_sg_config *config)
{
	uint32_t sp_buff_addr;
	uint32_t sp_fifo_addr;

	if (!config->cyclic) {
		tr_err(&acp_sp_vgh_tr, "cyclic configurations only supported!");
		return -EINVAL;
	}
	if (config->scatter) {
		tr_err(&acp_sp_vgh_tr, "scatter enabled, that is not supported for now!");
		return -EINVAL;
	}

	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_SCRATCH_REG_BASE + SCRATCH_REG_OFFSET);
	channel->is_scheduling_source = true;
	channel->direction = config->direction;
	sp_buff_size = config->elem_array.elems[0].size * config->elem_array.count;

	if (config->direction ==  DMA_DIR_MEM_TO_DEV) {
		/* SP Transmit FIFO Address and FIFO Size*/
		sp_fifo_addr = (uint32_t)(&pscratch_mem_cfg->acp_transmit_fifo_buffer);
		sp_fifo_addr = (sp_fifo_addr & ACP_DRAM_ADDRESS_MASK);
		sp_fifo_addr = (sp_fifo_addr - ACP_DRAM_PHY_TRNS);
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_TX_FIFOADDR), sp_fifo_addr);
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_TX_FIFOSIZE), (uint32_t)(SP_FIFO_SIZE));

		/* Transmit RINGBUFFER Address and size*/
		config->elem_array.elems[0].src =
			(config->elem_array.elems[0].src & ACP_DRAM_ADDRESS_MASK);
		sp_buff_addr = (config->elem_array.elems[0].src | ACP_DRAM_ADDR_TRNS);
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_TX_RINGBUFADDR), sp_buff_addr);
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_TX_RINGBUFSIZE), sp_buff_size);

		/* Transmit DMA transfer size in bytes */
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_TX_DMA_SIZE),
			     (uint32_t)(ACP_DMA_TRANS_SIZE_128));

		/* Watermark size for SP transmit FIFO - Half of SP buffer size */
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_TX_INTR_WATERMARK_SIZE),
			     (sp_buff_size >> 1));

	} else if (config->direction == DMA_DIR_DEV_TO_MEM) {
		/* SP Receive FIFO Address and FIFO Size*/
		sp_fifo_addr = (uint32_t)(&pscratch_mem_cfg->acp_receive_fifo_buffer);
		sp_fifo_addr = (sp_fifo_addr & ACP_DRAM_ADDRESS_MASK);
		sp_fifo_addr = (sp_fifo_addr - ACP_DRAM_PHY_TRNS);
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_RX_FIFOADDR), sp_fifo_addr);
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_RX_FIFOSIZE),
			     (uint32_t)(SP_FIFO_SIZE));

		/* Receive RINGBUFFER Address and size*/
		config->elem_array.elems[0].dest =
			(config->elem_array.elems[0].dest & ACP_DRAM_ADDRESS_MASK);
		sp_buff_addr = (config->elem_array.elems[0].dest | ACP_DRAM_ADDR_TRNS);
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_RX_RINGBUFADDR), sp_buff_addr);
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_RX_RINGBUFSIZE), sp_buff_size);

		/* Receive DMA transfer size in bytes */
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_RX_DMA_SIZE),
			     (uint32_t)(ACP_DMA_TRANS_SIZE_128));

		/* Watermark size for SP receive fifo - Half of SP buffer size*/
		io_reg_write((PU_REGISTER_BASE + ACP_I2S_RX_INTR_WATERMARK_SIZE),
			     (sp_buff_size >> 1));

	} else {
		tr_err(&acp_sp_vgh_tr, "DMA Config channel direction undefined %d",
		       channel->direction);
		return -EINVAL;
	}

	return 0;
}

int acp_dai_sp_dma_get_data_size(struct dma_chan_data *channel,
				 uint32_t *avail, uint32_t *free)
{
	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
#if CONFIG_DISABLE_DESCRIPTOR_SPLIT
		uint64_t tx_low, curr_tx_pos, tx_high;
		tx_low = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_I2S_TX_LINEARPOSITIONCNTR_LOW);
		tx_high = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_I2S_TX_LINEARPOSITIONCNTR_HIGH);
		curr_tx_pos = (uint64_t)((tx_high << 32) | tx_low);
		*free = (curr_tx_pos - prev_tx_pos) > sp_buff_size ?
			(curr_tx_pos - prev_tx_pos) % sp_buff_size :
			(curr_tx_pos - prev_tx_pos);
		*avail = sp_buff_size - *free;
		prev_tx_pos = curr_tx_pos;
#else
		//TODO  timer-based scheduling
		*free = (sp_buff_size >> 1);
		*avail = (sp_buff_size >> 1);
#endif
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
#if CONFIG_DISABLE_DESCRIPTOR_SPLIT
		uint64_t rx_low, curr_rx_pos, rx_high;
		rx_low = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_I2S_RX_LINEARPOSITIONCNTR_LOW);
		rx_high = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_I2S_RX_LINEARPOSITIONCNTR_HIGH);
		curr_rx_pos = (uint64_t)((rx_high << 32) | rx_low);
		*free = (curr_rx_pos - prev_rx_pos) > sp_buff_size ?
			(curr_rx_pos - prev_rx_pos) % sp_buff_size :
			(curr_rx_pos - prev_rx_pos);
		*avail = sp_buff_size - *free;
		prev_rx_pos = curr_rx_pos;
#else
		//TODO  timer-based scheduling
		*free = (sp_buff_size >> 1);
		*avail = (sp_buff_size >> 1);
#endif
	} else {
		tr_err(&acp_sp_vgh_tr, "Channel direction not defined %d", channel->direction);
		return -EINVAL;
	}
	return 0;
}

int acp_dai_sp_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
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
		return -ENOENT;
	}
	return 0;
}

int acp_dai_sp_dma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	uint32_t status;
	acp_dsp0_intr_stat_t acp_intr_stat;
	acp_dsp0_intr_cntl_t acp_intr_cntl;

	if (channel->status == COMP_STATE_INIT)
		return 0;
	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		acp_intr_stat =  (acp_dsp0_intr_stat_t)
			dma_reg_read(channel->dma, ACP_DSP0_INTR_STAT);
		status = acp_intr_stat.bits.audio_buffer_int_stat;
		return (status & (1 << channel->index));
	case DMA_IRQ_CLEAR:
		acp_intr_stat.u32all = 0;
		acp_intr_stat.bits.audio_buffer_int_stat =
					(1 << channel->index);
		status  = acp_intr_stat.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_STAT, status);
		return 0;
	case DMA_IRQ_MASK:
		acp_intr_cntl =  (acp_dsp0_intr_cntl_t)
				dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		acp_intr_cntl.bits.audio_buffer_int_mask &= (~(1 << channel->index));
		status  = acp_intr_cntl.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	case DMA_IRQ_UNMASK:
		acp_intr_cntl =  (acp_dsp0_intr_cntl_t)
				dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		acp_intr_cntl.bits.audio_buffer_int_mask  |=  (1 << channel->index);
		status = acp_intr_cntl.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	default:
		return -EINVAL;
	}
}
