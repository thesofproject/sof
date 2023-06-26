/* SPDX-License-Identifier: BSD-3-Clause
 *
 *  Copyright(c) 2023 AMD. All rights reserved.
 *
 *  Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *		SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
 */

#ifndef _COMMON_DMIC_DMA_HEADER
#define _COMMON_DMIC_DMA_HEADER

#include <stdint.h>
#include <platform/chip_registers.h>

/* 109c7aba-a7ba-43c3-b9-42-59-e2-0a-66-11-be */

#define DMIC_SETTLING_TIME_MS 360
#define DMIC_SMOOTH_TIME_MS 40

struct acp_dmic_silence {
	uint32_t bytes_per_sample;
	uint32_t num_chs;
	uint32_t samplerate_khz;
	uint32_t silence_cnt;
	uint32_t silence_incr;
	int coeff;
	int numfilterbuffers;
	char *dmic_rngbuff_addr1;
	int *dmic_rngbuff_iaddr;
};

int acp_dmic_dma_start(struct dma_chan_data *channel);
int acp_dmic_dma_stop(struct dma_chan_data *channel);
int acp_dmic_dma_set_config(struct dma_chan_data *channel,
			    struct dma_sg_config *config);
int acp_dmic_dma_get_attribute(struct dma *dma,
			       uint32_t type, uint32_t *value);
#endif
