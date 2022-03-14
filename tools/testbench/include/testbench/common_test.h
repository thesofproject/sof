/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 */

#ifndef _COMMON_TEST_H
#define _COMMON_TEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <sof/sof.h>
#include <sof/audio/component_ext.h>
#include <sof/math/numbers.h>
#include <sof/audio/format.h>

#include <sof/lib/uuid.h>

#define DEBUG_MSG_LEN		1024
#define MAX_LIB_NAME_LEN	1024

#define MAX_INPUT_FILE_NUM	16
#define MAX_OUTPUT_FILE_NUM	16

/* number of widgets types supported in testbench */
#define NUM_WIDGETS_SUPPORTED	15

struct tplg_context;

/*
 * Global testbench data.
 *
 * TODO: some items are topology and pipeline specific and need moved out
 * into per pipeline data and per topology data structures.
 */
struct testbench_prm {
	char *tplg_file; /* topology file to use */
	char *input_file[MAX_INPUT_FILE_NUM]; /* input file names */
	char *output_file[MAX_OUTPUT_FILE_NUM]; /* output file names */
	int input_file_num; /* number of input files */
	int output_file_num; /* number of output files */
	char *bits_in; /* input bit format */
	int pipelines[MAX_OUTPUT_FILE_NUM]; /* output file names */
	int pipeline_num;

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

	/* global cmd line args that can override topology */
	enum sof_ipc_frame cmd_frame_fmt;
	uint32_t cmd_fs_in;
	uint32_t cmd_fs_out;
	uint32_t cmd_channels_in;
	uint32_t cmd_channels_out;
};

struct shared_lib_table {
	char *comp_name;
	char library_name[MAX_LIB_NAME_LEN];
	uint32_t widget_type;
	struct sof_uuid *uid;
	int register_drv;
	void *handle;
};

extern struct shared_lib_table lib_table[];

extern int debug;

int edf_scheduler_init(void);

void sys_comp_file_init(void);

void sys_comp_filewrite_init(void);

int tb_setup(struct sof *sof, struct testbench_prm *tp);
void tb_free(struct sof *sof);

int tb_pipeline_start(struct ipc *ipc, struct pipeline *p);

int tb_pipeline_params(struct ipc *ipc, struct pipeline *p,
		       struct tplg_context *ctx);

int tb_pipeline_stop(struct ipc *ipc, struct pipeline *p);

int tb_pipeline_reset(struct ipc *ipc, struct pipeline *p);

void debug_print(char *message);

int get_index_by_name(char *comp_name,
		      struct shared_lib_table *lib_table);

int get_index_by_type(uint32_t comp_type,
		      struct shared_lib_table *lib_table);

int get_index_by_uuid(struct sof_ipc_comp_ext *comp_ext,
		      struct shared_lib_table *lib_table);
#endif
