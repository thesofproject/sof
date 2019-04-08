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
 * \file include/uapi/ipc/topology.h
 * \brief IPC definitions
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_IPC_TOPOLOGY_H__
#define __INCLUDE_UAPI_IPC_TOPOLOGY_H__

#include <uapi/ipc/header.h>

/*
 * Component
 */

/* types of component */
enum sof_comp_type {
	SOF_COMP_NONE = 0,
	SOF_COMP_HOST,
	SOF_COMP_DAI,
	SOF_COMP_SG_HOST,	/**< scatter gather variant */
	SOF_COMP_SG_DAI,	/**< scatter gather variant */
	SOF_COMP_VOLUME,
	SOF_COMP_MIXER,
	SOF_COMP_MUX,
	SOF_COMP_SRC,
	SOF_COMP_SPLITTER,
	SOF_COMP_TONE,
	SOF_COMP_SWITCH,
	SOF_COMP_BUFFER,
	SOF_COMP_EQ_IIR,
	SOF_COMP_EQ_FIR,
	SOF_COMP_KEYWORD_DETECT,
	SOF_COMP_KPB,			/* A key phrase buffer component */
	SOF_COMP_SELECTOR,		/**< channel selector component */
	/* keep FILEREAD/FILEWRITE as the last ones */
	SOF_COMP_FILEREAD = 10000,	/**< host test based file IO */
	SOF_COMP_FILEWRITE = 10001,	/**< host test based file IO */
};

/* XRUN action for component */
#define SOF_XRUN_STOP		1	/**< stop stream */
#define SOF_XRUN_UNDER_ZERO	2	/**< send 0s to sink */
#define SOF_XRUN_OVER_NULL	4	/**< send data to NULL */

/* create new generic component - SOF_IPC_TPLG_COMP_NEW */
struct sof_ipc_comp {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t id;
	enum sof_comp_type type;
	uint32_t pipeline_id;

	/* reserved for future use */
	uint32_t reserved[2];
} __attribute__((packed));

/*
 * Component Buffers
 */

/*
 * SOF memory capabilities, add new ones at the end
 */
#define SOF_MEM_CAPS_RAM			(1 << 0)
#define SOF_MEM_CAPS_ROM			(1 << 1)
#define SOF_MEM_CAPS_EXT			(1 << 2) /**< external */
#define SOF_MEM_CAPS_LP			(1 << 3) /**< low power */
#define SOF_MEM_CAPS_HP			(1 << 4) /**< high performance */
#define SOF_MEM_CAPS_DMA			(1 << 5) /**< DMA'able */
#define SOF_MEM_CAPS_CACHE			(1 << 6) /**< cacheable */
#define SOF_MEM_CAPS_EXEC			(1 << 7) /**< executable */

/* create new component buffer - SOF_IPC_TPLG_BUFFER_NEW */
struct sof_ipc_buffer {
	struct sof_ipc_comp comp;
	uint32_t size;		/**< buffer size in bytes */
	uint32_t caps;		/**< SOF_MEM_CAPS_ */
} __attribute__((packed));

/* generic component config data - must always be after struct sof_ipc_comp */
struct sof_ipc_comp_config {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t periods_sink;	/**< 0 means variable */
	uint32_t periods_source;/**< 0 means variable */
	uint32_t reserved1;	/**< reserved */
	uint32_t frame_fmt;	/**< SOF_IPC_FRAME_ */
	uint32_t xrun_action;

	/* reserved for future use */
	uint32_t reserved[2];
} __attribute__((packed));

/* generic host component */
struct sof_ipc_comp_host {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	uint32_t direction;	/**< SOF_IPC_STREAM_ */
	uint32_t no_irq;	/**< don't send periodic IRQ to host/DSP */
	uint32_t dmac_config; /**< DMA engine specific */
} __attribute__((packed));

/* generic DAI component */
struct sof_ipc_comp_dai {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	uint32_t direction;	/**< SOF_IPC_STREAM_ */
	uint32_t dai_index;	/**< index of this type dai */
	uint32_t type;		/**< DAI type - SOF_DAI_ */
	uint32_t reserved;	/**< reserved */
} __attribute__((packed));

/* generic mixer component */
struct sof_ipc_comp_mixer {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
} __attribute__((packed));

/* volume ramping types */
enum sof_volume_ramp {
	SOF_VOLUME_LINEAR	= 0,
	SOF_VOLUME_LOG,
	SOF_VOLUME_LINEAR_ZC,
	SOF_VOLUME_LOG_ZC,
};

/* generic volume component */
struct sof_ipc_comp_volume {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	uint32_t channels;
	uint32_t min_value;
	uint32_t max_value;
	uint32_t ramp;		/**< SOF_VOLUME_ */
	uint32_t initial_ramp;	/**< ramp space in ms */
} __attribute__((packed));

/* generic selector component */
struct sof_ipc_comp_selector {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;

	/* selector supports 1 input and 1 output */
	uint32_t input_channels_count;	/**< accepted values 2 or 4 */
	uint32_t output_channels_count;	/**< accepted values 1 or 2 or 4 */

	/* note: if 2 or 4 output channels selected, the component works
	 * in a bypass mode
	 */
	uint32_t selected_channel;	/**< 0..3 */
} __attribute__((packed));

/* generic SRC component */
struct sof_ipc_comp_src {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	/* either source or sink rate must be non zero */
	uint32_t source_rate;	/**< source rate or 0 for variable */
	uint32_t sink_rate;	/**< sink rate or 0 for variable */
	uint32_t rate_mask;	/**< SOF_RATE_ supported rates */
} __attribute__((packed));

/* generic MUX component */
struct sof_ipc_comp_mux {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
} __attribute__((packed));

/* generic tone generator component */
struct sof_ipc_comp_tone {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	int32_t sample_rate;
	int32_t frequency;
	int32_t amplitude;
	int32_t freq_mult;
	int32_t ampl_mult;
	int32_t length;
	int32_t period;
	int32_t repeats;
	int32_t ramp_step;
} __attribute__((packed));

/** \brief Types of processing components */
enum sof_ipc_process_type {
	SOF_PROCESS_NONE = 0,		/**< None */
	SOF_PROCESS_EQFIR,		/**< Intel FIR */
	SOF_PROCESS_EQIIR,		/**< Intel IIR */
	SOF_PROCESS_KEYWORD_DETECT,     /**< Keyword Detection */
	SOF_PROCESS_KPB,		/**< KeyPhrase Buffer Manager */
	SOF_PROCESS_CHAN_SELECTOR,	/**< Channel Selector */
};

/* generic "effect", "codec" or proprietary processing component */
struct sof_ipc_comp_process {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	uint32_t size;	/**< size of bespoke data section in bytes */
	uint32_t type;	/**< sof_ipc_process_type */

	/* reserved for future use */
	uint32_t reserved[7];

	unsigned char data[0];
} __attribute__((packed));

/* frees components, buffers and pipelines
 * SOF_IPC_TPLG_COMP_FREE, SOF_IPC_TPLG_PIPE_FREE, SOF_IPC_TPLG_BUFFER_FREE
 */
struct sof_ipc_free {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t id;
} __attribute__((packed));

struct sof_ipc_comp_reply {
	struct sof_ipc_reply rhdr;
	uint32_t id;
	uint32_t offset;
} __attribute__((packed));

/*
 * Pipeline
 */

/** \brief Types of pipeline scheduling time domains */
enum sof_ipc_pipe_sched_time_domain {
	SOF_TIME_DOMAIN_DMA = 0,	/**< DMA interrupt */
	SOF_TIME_DOMAIN_TIMER,		/**< Timer interrupt */
};

/* new pipeline - SOF_IPC_TPLG_PIPE_NEW */
struct sof_ipc_pipe_new {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t comp_id;	/**< component id for pipeline */
	uint32_t pipeline_id;	/**< pipeline id */
	uint32_t sched_id;	/**< Scheduling component id */
	uint32_t core;		/**< core we run on */
	uint32_t period;	/**< execution period in us*/
	uint32_t priority;	/**< priority level 0 (low) to 10 (max) */
	uint32_t period_mips;	/**< worst case instruction count per period */
	uint32_t frames_per_sched;/**< output frames of pipeline, 0 is variable */
	uint32_t xrun_limit_usecs; /**< report xruns greater than limit */
	uint32_t time_domain;	/**< scheduling time domain */
} __attribute__((packed));

/* pipeline construction complete - SOF_IPC_TPLG_PIPE_COMPLETE */
struct sof_ipc_pipe_ready {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t comp_id;
} __attribute__((packed));

struct sof_ipc_pipe_free {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t comp_id;
} __attribute__((packed));

/* connect two components in pipeline - SOF_IPC_TPLG_COMP_CONNECT */
struct sof_ipc_pipe_comp_connect {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t source_id;
	uint32_t sink_id;
} __attribute__((packed));

/* create new component kpb - SOF_IPC_TPLG_KPB_NEW */
struct sof_ipc_comp_kpb {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	uint32_t size; /**< kpb size in bytes */
	uint32_t caps; /**< SOF_MEM_CAPS_ */
	uint8_t no_channels; /**< no of channels */
	uint32_t history_depth; /**< time of buffering in milliseconds */
	uint32_t sampling_freq; /**< frequency in hertz */
	uint8_t sampling_width; /**< number of bits */
} __attribute__((packed));

#endif
