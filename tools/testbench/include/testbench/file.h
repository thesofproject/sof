/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#ifndef _TESTBENCH_FILE_H
#define _TESTBENCH_FILE_H

#include <stdint.h>

#define FILE_MAX_COPIES_TIMEOUT		3

/**< Convert with right shift a bytes count to samples count */
#define FILE_BYTES_TO_S16_SAMPLES(s)	((s) >> 1)
#define FILE_BYTES_TO_S32_SAMPLES(s)	((s) >> 2)

/* bfc7488c-75aa-4ce8-9dbed8da08a698c2 */
static const struct sof_uuid tb_file_uuid = {
	0xbfc7488c, 0x75aa, 0x4ce8, {0x9d, 0xbe, 0xd8, 0xda, 0x08, 0xa6, 0x98, 0xc2}
};

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
	uint64_t cycles_count;
	FILE *rfh, *wfh; /* read/write file handle */
	char *fn;
	int copy_count;
	int n;
	enum file_mode mode;
	enum file_format f_format;
	bool reached_eof;
	bool write_failed;
	bool copy_timeout;
};

struct sof_file_config {
	uint32_t rate;
	uint32_t channels;
	char *fn;
	uint32_t mode;
	uint32_t frame_fmt;
	uint32_t direction;	/**< SOF_IPC_STREAM_ */
} __attribute__((packed, aligned(4)));

struct file_comp_data;

/* file comp data */
struct file_comp_data {
	struct sof_file_config config;
	struct file_state fs;
	int sample_container_bytes;
	int (*file_func)(struct file_comp_data *cd, struct audio_stream *sink,
			 struct audio_stream *source, uint32_t frames);

	/* maximum limits */
	int max_samples;
	int max_copies;
	int max_frames;
	int copies_timeout_count;
};

void sys_comp_module_file_interface_init(void);

/* Get file comp data from copier data */
static inline struct file_comp_data *get_file_comp_data(struct copier_data *ccd)
{
	struct file_comp_data *cd = (struct file_comp_data *)ccd->ipcgtw_data;

	return cd;
}

/* Set file comp data to copier data */
static inline void file_set_comp_data(struct copier_data *ccd, struct file_comp_data *cd)
{
	ccd->ipcgtw_data = (struct ipcgtw_data *)cd;
}

#endif /* _TESTBENCH_FILE */
