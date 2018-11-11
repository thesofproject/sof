/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/uapi/ipc/dai.h
 * \brief IPC DAI definitions
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_IPC_DAI_H__
#define __INCLUDE_UAPI_IPC_DAI_H__

#include <uapi/ipc/header.h>
#include <uapi/ipc/dai-intel.h>

/*
 * DAI Configuration.
 *
 * Each different DAI type will have it's own structure and IPC cmd.
 */

#define SOF_DAI_FMT_I2S		1 /**< I2S mode */
#define SOF_DAI_FMT_RIGHT_J	2 /**< Right Justified mode */
#define SOF_DAI_FMT_LEFT_J	3 /**< Left Justified mode */
#define SOF_DAI_FMT_DSP_A	4 /**< L data MSB after FRM LRC */
#define SOF_DAI_FMT_DSP_B	5 /**< L data MSB during FRM LRC */
#define SOF_DAI_FMT_PDM		6 /**< Pulse density modulation */

#define SOF_DAI_FMT_CONT	(1 << 4) /**< continuous clock */
#define SOF_DAI_FMT_GATED	(0 << 4) /**< clock is gated */

#define SOF_DAI_FMT_NB_NF	(0 << 8) /**< normal bit clock + frame */
#define SOF_DAI_FMT_NB_IF	(2 << 8) /**< normal BCLK + inv FRM */
#define SOF_DAI_FMT_IB_NF	(3 << 8) /**< invert BCLK + nor FRM */
#define SOF_DAI_FMT_IB_IF	(4 << 8) /**< invert BCLK + FRM */

#define SOF_DAI_FMT_CBM_CFM	(0 << 12) /**< codec clk & FRM master */
#define SOF_DAI_FMT_CBS_CFM	(2 << 12) /**< codec clk slave & FRM master */
#define SOF_DAI_FMT_CBM_CFS	(3 << 12) /**< codec clk master & frame slave */
#define SOF_DAI_FMT_CBS_CFS	(4 << 12) /**< codec clk & FRM slave */

#define SOF_DAI_FMT_FORMAT_MASK		0x000f
#define SOF_DAI_FMT_CLOCK_MASK		0x00f0
#define SOF_DAI_FMT_INV_MASK		0x0f00
#define SOF_DAI_FMT_MASTER_MASK		0xf000

/** \brief Types of DAI */
enum sof_ipc_dai_type {
	SOF_DAI_INTEL_NONE = 0,		/**< None */
	SOF_DAI_INTEL_SSP,		/**< Intel SSP */
	SOF_DAI_INTEL_DMIC,		/**< Intel DMIC */
	SOF_DAI_INTEL_HDA,		/**< Intel HD/A */
};

/* general purpose DAI configuration */
struct sof_ipc_dai_config {
	struct sof_ipc_hdr hdr;
	uint32_t type;		/**< DAI type - enum sof_ipc_dai_type */
	uint32_t dai_index;	/**< index of this type dai */

	/* physical protocol and clocking */
	uint16_t format;	/**< SOF_DAI_FMT_ */
	uint16_t reserved16;	/**< alignment */

	/* reserved for future use */
	uint32_t reserved[8];

	/* HW specific data */
	union {
		struct sof_ipc_dai_ssp_params ssp;
		struct sof_ipc_dai_dmic_params dmic;
		struct sof_ipc_dai_hda_params hda;
	};
} __attribute__((packed));

#endif
