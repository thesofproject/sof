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
#include <rtos/sof.h>
#include <tplg_parser/topology.h>
#include <sof/audio/component_ext.h>
#include <sof/math/numbers.h>
#include <sof/audio/format.h>
#include <sof/lib/uuid.h>

#define DEBUG_MSG_LEN		1024
#define MAX_LIB_NAME_LEN	1024

#define MAX_INPUT_FILE_NUM	16
#define MAX_OUTPUT_FILE_NUM	16
#define MAX_PIPELINES_NUM	16


/* number of widgets types supported in testbench */
#define NUM_WIDGETS_SUPPORTED	16

#define TB_NAME_SIZE	256

#define TB_MAX_CONFIG	128

struct tb_mq_desc {
	/* IPC message queue */
	//mqd_t mq;
	//struct mq_attr attr;
	char queue_name[TB_NAME_SIZE];
};

struct tb_config {
	char name[44];
	unsigned long buffer_frames;
	unsigned long buffer_time;
	unsigned long period_frames;
	unsigned long period_time;
	int rate;
	int channels;
	unsigned long format;
};

struct file_comp_lookup {
	int id;
	int instance_id;
	int pipeline_id;
	struct file_state *state;
};

/*
 * Global testbench data.
 *
 * TODO: some items are topology and pipeline specific and need moved out
 * into per pipeline data and per topology data structures.
 */
struct testbench_prm {
	long long total_cycles;
	int pipelines[MAX_PIPELINES_NUM];
	struct file_comp_lookup fr[MAX_INPUT_FILE_NUM];
	struct file_comp_lookup fw[MAX_OUTPUT_FILE_NUM];
	char *input_file[MAX_INPUT_FILE_NUM]; /* input file names */
	char *output_file[MAX_OUTPUT_FILE_NUM]; /* output file names */
	char *tplg_file; /* topology file to use */
	char *bits_in; /* input bit format */
	int input_file_num; /* number of input files */
	int output_file_num; /* number of output files */
	int pipeline_num;
	int copy_iterations;
	bool copy_check;
	bool quiet;
	int dynamic_pipeline_iterations;
	int num_vcores;
	int tick_period_us;
	int pipeline_duration_ms;
	int real_time;
	//FILE *file;
	char *pipeline_string;
	int output_file_index;
	int input_file_index;

	//struct tplg_comp_info *info;
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
	int ipc_version;

	/* topology */
	struct tplg_context tplg;
	struct list_item widget_list;
	struct list_item route_list;
	struct list_item pcm_list;
	struct list_item pipeline_list;
	int instance_ids[SND_SOC_TPLG_DAPM_LAST];
	struct tb_mq_desc ipc_tx;
	struct tb_mq_desc ipc_rx;

	int pcm_id;	// TODO: This needs to be cleaned up
	struct tplg_pcm_info *pcm_info;

	struct tb_config config[TB_MAX_CONFIG];
	int num_configs;

	size_t period_size;
};

extern int debug;

int tb_parse_topology(struct testbench_prm *tb);

int edf_scheduler_init(void);

int tb_setup(struct sof *sof, struct testbench_prm *tp);
void tb_free(struct sof *sof);

int tb_pipeline_start(struct ipc *ipc, struct pipeline *p);

int tb_pipeline_params(struct testbench_prm *tp, struct ipc *ipc, struct pipeline *p);

int tb_pipeline_stop(struct ipc *ipc, struct pipeline *p);

int tb_pipeline_reset(struct ipc *ipc, struct pipeline *p);

void debug_print(char *message);

void tb_gettime(struct timespec *td);

void tb_getcycles(uint64_t *cycles);

int tb_load_topology(struct testbench_prm *tb);

int tb_set_up_all_pipelines(struct testbench_prm *tb);

void tb_show_file_stats(struct testbench_prm *tp, int pipeline_id);

bool tb_schedule_pipeline_check_state(struct testbench_prm *tp);

bool tb_is_pipeline_enabled(struct testbench_prm *tb, int pipeline_id);

int tb_find_file_components(struct testbench_prm *tb);

#endif
