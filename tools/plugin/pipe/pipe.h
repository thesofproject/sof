/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_PLUGIN_PIPE_H__
#define __SOF_PLUGIN_PIPE_H__

#include <stdatomic.h>
#include <stdint.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>

#include <alsa/asoundlib.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>
#include "common.h"

struct sof_pipe;

#define MAX_MODULE_ID	256
#define MAX_PIPE_THREADS	128
#define MAX_PIPELINES	32

struct pipethread_data {
	pthread_t pcm_thread;
	pthread_t ipc_thread;
	struct sof_pipe *sp;
	struct plug_mq_desc ipc_tx_mq;
	struct plug_mq_desc ipc_rx_mq;
	struct pipeline *pcm_pipeline;
	/* PCM flow control */
	struct plug_sem_desc ready;
	struct plug_sem_desc done;
	atomic_int pipe_users;
};

struct sof_pipe_module {
	void *handle;
	char uuid[SOF_UUID_SIZE];
	int module_id;
};

struct sof_pipe {
	const char *alsa_name;
	char topology_name[NAME_SIZE];
	int realtime;
	int use_P_core;
	int use_E_core;
	int capture;
	int file_mode;
	int pipe_thread_count;

	struct sigaction action;

	/* SHM for stream context sync */
	struct plug_shm_desc shm_context;
	struct plug_shm_glb_state *glb;

	FILE *log;
	pthread_mutex_t ipc_lock;
	struct tplg_context tplg;
	struct tplg_comp_info *comp_list;
	struct list_item widget_list; /* list of widgets */
	struct list_item route_list; /* list of widget connections */
	struct list_item pcm_list; /* list of PCMs */
	int info_elems;
	int info_index;

	/* IPC */
	pthread_t ipc_pcm_thread;
	struct plug_mq_desc ipc_tx_mq; /* queue used by plugin to send IPCs */
	struct plug_mq_desc ipc_rx_mq; /* queue used by plugin to receive the IPC response */

	/* module SO handles */
	struct sof_pipe_module module[MAX_MODULE_ID];
	int mod_idx;

	/* pipeline context */
	struct pipethread_data pipeline_ctx[MAX_PIPELINES];
};

/* global needed for signal handler */
extern struct sof_pipe *_sp;

int pipe_thread_new(struct sof_pipe *sp, struct pipeline *p);
int pipe_thread_free(struct sof_pipe *sp, int pipeline_id);
int pipe_thread_start(struct sof_pipe *sp, struct pipeline *p);
int pipe_thread_stop(struct sof_pipe *sp, struct pipeline *p);
int pipe_sof_setup(struct sof *sof);
int pipe_kcontrol_cb_new(struct snd_soc_tplg_ctl_hdr *tplg_ctl,
			 void *comp, void *arg);

/* set pipeline to realtime priority */
int pipe_set_rt(struct sof_pipe *sp);

/* set ipc thread to low priority */
int pipe_set_ipc_lowpri(struct sof_pipe *sp);

int pipe_ipc_process(struct sof_pipe *sp, struct plug_mq_desc *tx_mq, struct plug_mq_desc *rx_mq);

int pipe_ipc_new(struct sof_pipe *sp, int pri, int core);
void pipe_ipc_free(struct sof_pipe *sp);

int pipe_set_affinity(struct sof_pipe *sp);

int pipe_ipc_message(struct sof_pipe *sp, void *mailbox, size_t bytes);

int pipe_ipc_do(struct sof_pipe *sp, void *mailbox, size_t bytes);

/*
 * Topology
 */
int plug_parse_topology(struct sof_pipe *sp, struct tplg_context *ctx);

int plug_load_widget(struct sof_pipe *sp, struct tplg_context *ctx);

int plug_register_graph(struct sof_pipe *sp, struct tplg_context *ctx,
			struct tplg_comp_info *temp_comp_list,
			char *pipeline_string,
			int count, int num_comps, int pipeline_id);

#endif
