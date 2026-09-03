/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@intel.com>
 */

#ifndef __WAVE_H__
#define __WAVE_H__

#define HEADER_RIFF 0x46464952	/**< ASCII "RIFF" */
#define HEADER_WAVE 0x45564157	/**< ASCII "WAVE" */
#define HEADER_FMT  0x20746d66	/**< ASCII "fmt " */
#define HEADER_DATA 0x61746164	/**< ASCII "data" */

struct riff_chunk {
	uint32_t chunk_id;
	uint32_t chunk_size;
	uint32_t format;
};

struct fmt_subchunk {
	uint32_t subchunk_id;
	uint32_t subchunk_size;
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
};

struct data_subchunk {
	uint32_t subchunk_id;
	uint32_t subchunk_size;
	uint32_t data[];
};

struct wave {
	struct riff_chunk riff;
	struct fmt_subchunk fmt;
	struct data_subchunk data;
};

#endif /* __WAVE_H__ */
