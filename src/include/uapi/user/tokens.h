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

/*
 * Topology IDs and tokens.
 *
 * ** MUST BE ALIGNED WITH TOPOLOGY CONFIGURATION TOKEN VALUES **
 */

#ifndef __INCLUDE_UAPI_USER_TOKENS_H__
#define __INCLUDE_UAPI_USER_TOKENS_H__

/*
 * Kcontrol IDs
 */
#define SOF_TPLG_KCTL_VOL_ID	256
#define SOF_TPLG_KCTL_ENUM_ID	257
#define SOF_TPLG_KCTL_BYTES_ID	258

/*
 * Tokens - must match values in topology configurations
 */

/* buffers */
#define SOF_TKN_BUF_SIZE			100
#define SOF_TKN_BUF_CAPS			101

/* DAI */
#define	SOF_TKN_DAI_DMAC_CONFIG			153
#define SOF_TKN_DAI_TYPE			154
#define SOF_TKN_DAI_INDEX			155
#define SOF_TKN_DAI_DIRECTION			156

/* scheduling */
#define SOF_TKN_SCHED_DEADLINE			200
#define SOF_TKN_SCHED_PRIORITY			201
#define SOF_TKN_SCHED_MIPS			202
#define SOF_TKN_SCHED_CORE			203
#define SOF_TKN_SCHED_FRAMES			204
#define SOF_TKN_SCHED_TIMER			205

/* volume */
#define SOF_TKN_VOLUME_RAMP_STEP_TYPE		250
#define SOF_TKN_VOLUME_RAMP_STEP_MS		251

/* SRC */
#define SOF_TKN_SRC_RATE_IN			300
#define SOF_TKN_SRC_RATE_OUT			301

/* PCM */
#define SOF_TKN_PCM_DMAC_CONFIG			353

/* Generic components */
#define SOF_TKN_COMP_PERIOD_SINK_COUNT		400
#define SOF_TKN_COMP_PERIOD_SOURCE_COUNT	401
#define SOF_TKN_COMP_FORMAT			402
#define SOF_TKN_COMP_PRELOAD_COUNT		403

/* SSP */
#define SOF_TKN_INTEL_SSP_CLKS_CONTROL		500
#define SOF_TKN_INTEL_SSP_MCLK_ID		501
#define SOF_TKN_INTEL_SSP_SAMPLE_BITS		502
#define SOF_TKN_INTEL_SSP_FRAME_PULSE_WIDTH	503
#define SOF_TKN_INTEL_SSP_QUIRKS		504
#define SOF_TKN_INTEL_SSP_TDM_PADDING_PER_SLOT	505

/* DMIC */
#define SOF_TKN_INTEL_DMIC_DRIVER_VERSION	600
#define SOF_TKN_INTEL_DMIC_CLK_MIN		601
#define SOF_TKN_INTEL_DMIC_CLK_MAX		602
#define SOF_TKN_INTEL_DMIC_DUTY_MIN		603
#define SOF_TKN_INTEL_DMIC_DUTY_MAX		604
#define SOF_TKN_INTEL_DMIC_NUM_PDM_ACTIVE	605
#define SOF_TKN_INTEL_DMIC_SAMPLE_RATE		608
#define SOF_TKN_INTEL_DMIC_FIFO_WORD_LENGTH	609

/* DMIC PDM */
#define SOF_TKN_INTEL_DMIC_PDM_CTRL_ID		700
#define SOF_TKN_INTEL_DMIC_PDM_MIC_A_Enable	701
#define SOF_TKN_INTEL_DMIC_PDM_MIC_B_Enable	702
#define SOF_TKN_INTEL_DMIC_PDM_POLARITY_A	703
#define SOF_TKN_INTEL_DMIC_PDM_POLARITY_B	704
#define SOF_TKN_INTEL_DMIC_PDM_CLK_EDGE		705
#define SOF_TKN_INTEL_DMIC_PDM_SKEW		706

#define SOF_TKN_EFFECT_TYPE                     900

#endif
