/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 */

#ifndef _COMMON_TEST_H
#define _COMMON_TEST_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include <sof/sof.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>

#define DEBUG_MSG_LEN		256
#define MAX_LIB_NAME_LEN	256

/* number of widgets types supported in testbench */
#define NUM_WIDGETS_SUPPORTED	4

struct testbench_prm {
	char *tplg_file; /* topology file to use */
	char *input_file; /* input file name */
	char *output_file; /* output file name */
	char *bits_in; /* input bit format */
	/*
	 * input and output sample rate parameters
	 * By default, these are calculated from pipeline frames_per_sched
	 * and period but they can also be overridden via input arguments
	 * to the testbench.
	 */
	uint32_t fs_in;
	uint32_t fs_out;
	uint32_t channels;
	int fr_id;
	int fw_id;
	int sched_id;
	enum sof_ipc_frame frame_fmt;
};

struct shared_lib_table {
	char *comp_name;
	char library_name[MAX_LIB_NAME_LEN];
	uint32_t widget_type;
	int register_drv;
	void *handle;
};

extern int debug;

int edf_scheduler_init(void);

void sys_comp_file_init(void);

void sys_comp_filewrite_init(void);

int tb_pipeline_setup(struct sof *sof);

int tb_pipeline_start(struct ipc *ipc, struct sof_ipc_pipe_new *ipc_pipe,
		      struct testbench_prm *tp);

int tb_pipeline_params(struct ipc *ipc, struct sof_ipc_pipe_new *ipc_pipe,
		       struct testbench_prm *tp);

void debug_print(char *message);

int get_index_by_name(char *comp_name,
		      struct shared_lib_table *lib_table);

int get_index_by_type(uint32_t comp_type,
		      struct shared_lib_table *lib_table);

int parse_topology(struct sof *sof, struct shared_lib_table *library_table,
		   struct testbench_prm *tp, char *pipeline_msg);
#endif
