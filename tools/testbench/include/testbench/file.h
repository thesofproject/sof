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

/**< Convert with right shift a bytes count to samples count */
#define FILE_BYTES_TO_S16_SAMPLES(s)	((s) >> 1)
#define FILE_BYTES_TO_S32_SAMPLES(s)	((s) >> 2)

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
	bool reached_eof;
	bool write_failed;
	int n;
	enum file_mode mode;
	enum file_format f_format;
};

/* file comp data */
struct file_comp_data {
	struct file_state fs;
	enum sof_ipc_frame frame_fmt;
	uint32_t channels;
	uint32_t rate;
	int sample_container_bytes;
	int (*file_func)(struct comp_dev *dev, struct audio_stream *sink,
			 struct audio_stream *source, uint32_t frames);

};

#endif
