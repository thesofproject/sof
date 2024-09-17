/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 */

#ifndef _TESTBENCH_COMMON_TEST_H
#define _TESTBENCH_COMMON_TEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <rtos/sof.h>
#include <sof/audio/component_ext.h>
#include <sof/math/numbers.h>
#include <sof/audio/format.h>

#include <sof/lib/uuid.h>

#define TB_DEBUG_MSG_LEN		1024
#define TB_MAX_LIB_NAME_LEN		1024

#define TB_MAX_INPUT_FILE_NUM		16
#define TB_MAX_OUTPUT_FILE_NUM		16

/* number of widgets types supported in testbench */
#define TB_NUM_WIDGETS_SUPPORTED	16

struct tplg_context;

/*
 * Global testbench data.
 *
 * TODO: some items are topology and pipeline specific and need moved out
 * into per pipeline data and per topology data structures.
 */
struct testbench_prm {
	long long total_cycles;
	char *tplg_file; /* topology file to use */
	char *input_file[TB_MAX_INPUT_FILE_NUM]; /* input file names */
	char *output_file[TB_MAX_OUTPUT_FILE_NUM]; /* output file names */
	int input_file_num; /* number of input files */
	int output_file_num; /* number of output files */
	char *bits_in; /* input bit format */
	int pipelines[TB_MAX_OUTPUT_FILE_NUM]; /* output file names */
	int pipeline_num;
	struct tplg_context *ctx;

	int fr_id;
	int fw_id;

	int max_pipeline_id;
	int copy_iterations;
	bool copy_check;
	bool quiet;
	int dynamic_pipeline_iterations;
	int num_vcores;
	int tick_period_us;
	int pipeline_duration_ms;
	int real_time;
	FILE *file;
	char *pipeline_string;
	int output_file_index;
	int input_file_index;

	struct tplg_comp_info *info;
	int info_index;
	int info_elems;

	/*
	 * input and output sample rate parameters
	 * By default, these are calculated from pipeline frames_per_sched
	 * and period but they can also be overridden via input arguments
	 * to the testbench.
	 */
	uint32_t fs_in;
	uint32_t fs_out;
	uint32_t channels_in;
	uint32_t channels_out;
	enum sof_ipc_frame frame_fmt;
};

extern int debug;

int tb_parse_topology(struct testbench_prm *tb, struct tplg_context *ctx);

int edf_scheduler_init(void);

int tb_setup(struct sof *sof, struct testbench_prm *tp);
void tb_free(struct sof *sof);

int tb_pipeline_start(struct ipc *ipc, struct pipeline *p);

int tb_pipeline_params(struct testbench_prm *tp, struct ipc *ipc, struct pipeline *p,
		       struct tplg_context *ctx);

int tb_pipeline_stop(struct ipc *ipc, struct pipeline *p);

int tb_pipeline_reset(struct ipc *ipc, struct pipeline *p);

void debug_print(char *message);

void tb_gettime(struct timespec *td);

void tb_getcycles(uint64_t *cycles);

#endif /* _TESTBENCH_COMMON_TEST_H */
