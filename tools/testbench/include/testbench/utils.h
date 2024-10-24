/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 */

#ifndef _TESTBENCH_UTILS_H
#define _TESTBENCH_UTILS_H

#include <tplg_parser/topology.h>
#include <stdint.h>
#include <stdbool.h>


#include <sof/lib/uuid.h>

#define TB_DEBUG_MSG_LEN		1024
#define TB_MAX_LIB_NAME_LEN		1024

#define TB_MAX_INPUT_FILE_NUM		16
#define TB_MAX_OUTPUT_FILE_NUM		16
#define TB_MAX_PIPELINES_NUM		16

/* number of widgets types supported in testbench */
#define TB_NUM_WIDGETS_SUPPORTED	16

struct tplg_context;

struct file_comp_lookup {
	int id;
	int instance_id;
	int pipeline_id;
	struct file_state *state;
};

#if CONFIG_IPC_MAJOR_4

#define TB_NAME_SIZE		256
#define TB_MAX_CONFIG_COUNT	2
#define TB_MAX_CONFIG_NAME_SIZE	64
#define TB_MAX_VOLUME_SIZE	120
#define TB_MAX_DATA_SIZE	512
#define TB_MAX_CTLS		16

struct tb_mq_desc {
	char queue_name[TB_NAME_SIZE];
};

struct tb_config {
	char name[TB_MAX_CONFIG_NAME_SIZE];
	unsigned long buffer_frames;
	unsigned long buffer_time;
	unsigned long period_frames;
	unsigned long period_time;
	int rate;
	int channels;
	unsigned long format;
};

struct tb_ctl {
	unsigned int module_id;
	unsigned int instance_id;
	unsigned int type;
	unsigned int volume_table[TB_MAX_VOLUME_SIZE];
	unsigned int index;
	char data[TB_MAX_DATA_SIZE];
	union {
		struct snd_soc_tplg_mixer_control mixer_ctl;
		struct snd_soc_tplg_enum_control enum_ctl;
		struct snd_soc_tplg_bytes_control bytes_ctl;
	};
};

struct tb_glb_state {
	char magic[8];			/* SOF_MAGIC */
	uint32_t num_ctls;		/* number of ctls */
	size_t size;			/* size of this structure in bytes */
	struct tb_ctl *ctl;
};
#endif

/*
 * Global testbench data.
 *
 * TODO: some items are topology and pipeline specific and need moved out
 * into per pipeline data and per topology data structures.
 */
struct testbench_prm {
	long long total_cycles;
	int pipelines[TB_MAX_PIPELINES_NUM];
	struct file_comp_lookup fr[TB_MAX_INPUT_FILE_NUM];
	struct file_comp_lookup fw[TB_MAX_OUTPUT_FILE_NUM];
	char *input_file[TB_MAX_INPUT_FILE_NUM]; /* input file names */
	char *output_file[TB_MAX_OUTPUT_FILE_NUM]; /* output file names */
	char *tplg_file; /* topology file to use */
	char *bits_in; /* input bit format */
	int input_file_num; /* number of input files */
	int output_file_num; /* number of output files */
	int pipeline_num;
	int copy_iterations;
	bool copy_check;
	int trace_level;
	int dynamic_pipeline_iterations;
	int tick_period_us;
	int pipeline_duration_ms;
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

	/* topology */
	struct tplg_context tplg;

#if CONFIG_IPC_MAJOR_4
	struct list_item widget_list;
	struct list_item route_list;
	struct list_item pcm_list;
	struct list_item pipeline_list;
	int instance_ids[SND_SOC_TPLG_DAPM_LAST];
	struct tb_mq_desc ipc_tx;
	struct tb_mq_desc ipc_rx;
	int pcm_id;	// TODO: This needs to be cleaned up
	struct tplg_pcm_info *pcm_info;
	struct tb_config config[TB_MAX_CONFIG_COUNT];
	int num_configs;
	int period_frames;
	struct tb_glb_state glb_ctx;
#endif
};

extern int debug;

int tb_find_file_components(struct testbench_prm *tp);
int tb_free_all_pipelines(struct testbench_prm *tp);
int tb_load_topology(struct testbench_prm *tp);
int tb_parse_topology(struct testbench_prm *tp);
int tb_pipeline_params(struct testbench_prm *tp, struct ipc *ipc, struct pipeline *p);
int tb_pipeline_reset(struct ipc *ipc, struct pipeline *p);
int tb_pipeline_start(struct ipc *ipc, struct pipeline *p);
int tb_pipeline_stop(struct ipc *ipc, struct pipeline *p);
int tb_set_reset_state(struct testbench_prm *tp);
int tb_set_running_state(struct testbench_prm *tp);
int tb_set_up_all_pipelines(struct testbench_prm *tp);
int tb_setup(struct sof *sof, struct testbench_prm *tp);
bool tb_is_pipeline_enabled(struct testbench_prm *tp, int pipeline_id);
bool tb_schedule_pipeline_check_state(struct testbench_prm *tp);
void tb_debug_print(char *message);
void tb_free(struct sof *sof);
void tb_free_topology(struct testbench_prm *tp);
void tb_getcycles(uint64_t *cycles);
void tb_gettime(struct timespec *td);
void tb_show_file_stats(struct testbench_prm *tp, int pipeline_id);

#endif /* _TESTBENCH_UTILS_H */
