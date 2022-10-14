/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_IPC_CONFIG_H__
#define __SOF_AUDIO_IPC_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

/*
 * Generic IPC information for base components. Fields can be added here with NO impact on
 * IPC ABI version.
 */

/* generic host component */
struct ipc_config_host {
	uint32_t direction;		/**< SOF_IPC_STREAM_ */
	uint32_t no_irq;		/**< don't send periodic IRQ to host/DSP */
	uint32_t dmac_config;		/**< DMA engine specific */
	uint32_t dma_buffer_size;	/**< Requested DMA buffer size */
};

/* generic DAI component */
struct ipc_config_dai {
	uint32_t direction;		/**< SOF_IPC_STREAM_ */
	uint32_t dai_index;		/**< index of this type dai */
	uint32_t type;			/**< DAI type - SOF_DAI_ */
	uint32_t dma_buffer_size;	/**< Requested DMA buffer size */
	/* physical protocol and clocking */
	uint32_t sampling_frequency;	/**< DAI sampling frequency - required only with IPC4 */
	uint16_t format;		/**< SOF_DAI_FMT_ */
	uint16_t group_id;		/**< group ID, 0 means no group (ABI 3.17) */
	bool is_config_blob;		/**< DAI specific configuration is a blob */
};

/* generic volume component */
struct ipc_config_volume {
	uint32_t channels;
	uint32_t min_value;
	uint32_t max_value;
	uint32_t ramp;		/**< SOF_VOLUME_ */
	uint32_t initial_ramp;	/**< ramp space in ms */
};

/* generic SRC component */
struct ipc_config_src {
	/* either source or sink rate must be non zero */
	uint32_t source_rate;	/**< source rate or 0 for variable */
	uint32_t sink_rate;	/**< sink rate or 0 for variable */
	uint32_t rate_mask;	/**< SOF_RATE_ supported rates */
};

/* generic ASRC component */
struct ipc_config_asrc {
	/* either source or sink rate must be non zero */
	uint32_t source_rate;           /**< Define fixed source rate or */
					/**< use 0 to indicate need to get */
					/**< the rate from stream */
	uint32_t sink_rate;             /**< Define fixed sink rate or */
					/**< use 0 to indicate need to get */
					/**< the rate from stream */
	uint32_t asynchronous_mode;     /**< synchronous 0, asynchronous 1 */
					/**< When 1 the ASRC tracks and */
					/**< compensates for drift. */
	uint32_t operation_mode;        /**< push 0, pull 1, In push mode the */
					/**< ASRC consumes a defined number */
					/**< of frames at input, with varying */
					/**< number of frames at output. */
					/**< In pull mode the ASRC outputs */
					/**< a defined number of frames while */
					/**< number of input frames varies. */
};

/* generic tone generator component */
struct ipc_config_tone {
	int32_t sample_rate;
	int32_t frequency;
	int32_t amplitude;
	int32_t freq_mult;
	int32_t ampl_mult;
	int32_t length;
	int32_t period;
	int32_t repeats;
	int32_t ramp_step;
};

/* generic "effect", "codec" or proprietary processing component */
struct ipc_config_process {
	uint32_t size;	/**< size of bespoke data section in bytes */
	uint32_t type;	/**< sof_ipc_process_type */

	const unsigned char *data;
};

/* file IO ipc comp */
struct ipc_comp_file {
	uint32_t rate;
	uint32_t channels;
	char *fn;
	uint32_t mode;
	uint32_t frame_fmt;
	uint32_t direction;	/**< SOF_IPC_STREAM_ */
} __attribute__((packed, aligned(4)));

#endif
