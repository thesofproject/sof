/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#ifndef _FILE_H
#define _FILE_H

/* file component modes */
enum file_mode {
	FILE_READ = 0,
	FILE_WRITE,
	FILE_DUPLEX,
};

enum file_format {
	FILE_TEXT = 0,
	FILE_RAW,
};

/* file component state */
struct file_state {
	char *fn;
	FILE *rfh, *wfh; /* read/write file handle */
	int reached_eof;
	int n;
	enum file_mode mode;
	enum file_format f_format;
};

/* file comp data */
struct file_comp_data {
	uint32_t period_bytes;
	uint32_t channels;
	uint32_t frame_bytes;
	uint32_t rate;
	struct file_state fs;
	int sample_container_bytes;
	enum sof_ipc_frame frame_fmt;
	int (*file_func)(struct comp_dev *dev, struct audio_stream *sink,
			 struct audio_stream *source, uint32_t frames);

};

/* file IO ipc comp */
struct sof_ipc_comp_file {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	uint32_t rate;
	uint32_t channels;
	char *fn;
	enum file_mode mode;
	enum sof_ipc_frame frame_fmt;
} __attribute__((packed, aligned(4)));
#endif
