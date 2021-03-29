/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_AUDIO_PIPELINE_H__
#define __SOF_AUDIO_PIPELINE_H__

#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/audio/pipeline-trace.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

struct comp_buffer;
struct comp_dev;
struct ipc;
struct ipc_msg;
struct sof_ipc_buffer;
struct sof_ipc_pcm_params;
struct task;

/*
 * This flag disables firmware-side xrun recovery.
 * It should remain enabled in the situation when the
 * recovery is delegated to the outside of firmware.
 */
#define NO_XRUN_RECOVERY 1


/* Pipeline status to stop execution of current path */
#define PPL_STATUS_PATH_STOP	1

/* pipeline connection directions */
#define PPL_CONN_DIR_COMP_TO_BUFFER	0
#define PPL_CONN_DIR_BUFFER_TO_COMP	1

/* pipeline processing directions */
#define PPL_DIR_DOWNSTREAM	0
#define PPL_DIR_UPSTREAM	1

#define PPL_POSN_OFFSETS \
	(MAILBOX_STREAM_SIZE / sizeof(struct sof_ipc_stream_posn))

/*
 * Audio pipeline.
 */
struct pipeline {
	struct sof_ipc_pipe_new ipc_pipe;

	/* runtime status */
	int32_t xrun_bytes;		/* last xrun length */
	uint32_t status;		/* pipeline status */
	struct tr_ctx tctx;		/* trace settings */

	/* scheduling */
	struct task *pipe_task;		/* pipeline processing task */

	/* component that drives scheduling in this pipe */
	struct comp_dev *sched_comp;
	/* source component for this pipe */
	struct comp_dev *source_comp;
	/* sink component for this pipe */
	struct comp_dev *sink_comp;

	struct list_item list;	/**< list in walk context */

	/* position update */
	uint32_t posn_offset;		/* position update array offset*/
	struct ipc_msg *msg;
};

/* static pipeline */
extern struct pipeline *pipeline_static;

struct pipeline_walk_context {
	int (*comp_func)(struct comp_dev *cd, struct comp_buffer *buffer,
			 struct pipeline_walk_context *ctx, int dir);
	void *comp_data;
	void (*buff_func)(struct comp_buffer *buffer, void *data);
	void *buff_data;
	/**< pipelines to be scheduled after trigger walk */
	struct list_item pipelines;
	/*
	 * If this flag is set, pipeline_for_each_comp() will skip all
	 * incompletely initialised components, i.e. those, whose .pipeline ==
	 * NULL. Such components should not be skipped during initialisation
	 * and clean up, but they should be skipped during streaming.
	 */
	bool skip_incomplete;
};

/* generic pipeline data used by pipeline_comp_* functions */
struct pipeline_data {
	struct comp_dev *start;
	struct sof_ipc_pcm_params *params;
	struct sof_ipc_stream_posn *posn;
	struct pipeline *p;
	int cmd;
};

struct pipeline_posn {
	bool posn_offset[PPL_POSN_OFFSETS];	/**< available offsets */
	spinlock_t lock;			/**< lock mechanism */
};

/**
 * \brief Retrieves pipeline position structure.
 * \return Pointer to pipeline position structure.
 */
static inline struct pipeline_posn *pipeline_posn_get(void)
{
	return sof_get()->pipeline_posn;
}

/**
 * \brief Initializes pipeline position structure.
 * \param[in,out] sof Pointer to sof structure.
 */
void pipeline_posn_init(struct sof *sof);

/**
 * \brief Retrieves first free pipeline position offset.
 * \param[in,out] posn_offset Pipeline position offset to be set.
 * \return Error code.
 */
static inline int pipeline_posn_offset_get(uint32_t *posn_offset)
{
	struct pipeline_posn *pipeline_posn = pipeline_posn_get();
	int ret = -EINVAL;
	uint32_t i;

	spin_lock(&pipeline_posn->lock);

	for (i = 0; i < PPL_POSN_OFFSETS; ++i) {
		if (!pipeline_posn->posn_offset[i]) {
			*posn_offset = i * sizeof(struct sof_ipc_stream_posn);
			pipeline_posn->posn_offset[i] = true;
			ret = 0;
			break;
		}
	}

	platform_shared_commit(pipeline_posn, sizeof(*pipeline_posn));

	spin_unlock(&pipeline_posn->lock);

	return ret;
}

/**
 * \brief Frees pipeline position offset.
 * \param[in] posn_offset Pipeline position offset to be freed.
 */
static inline void pipeline_posn_offset_put(uint32_t posn_offset)
{
	struct pipeline_posn *pipeline_posn = pipeline_posn_get();
	int i = posn_offset / sizeof(struct sof_ipc_stream_posn);

	spin_lock(&pipeline_posn->lock);

	pipeline_posn->posn_offset[i] = false;

	platform_shared_commit(pipeline_posn, sizeof(*pipeline_posn));

	spin_unlock(&pipeline_posn->lock);
}

/* checks if two pipelines have the same scheduling component */
static inline bool pipeline_is_same_sched_comp(struct pipeline *current,
					       struct pipeline *previous)
{
	return current->sched_comp == previous->sched_comp;
}

/* checks if pipeline is scheduled with timer */
static inline bool pipeline_is_timer_driven(struct pipeline *p)
{
	return p->ipc_pipe.time_domain == SOF_TIME_DOMAIN_TIMER;
}

/* checks if pipeline is scheduled on this core */
static inline bool pipeline_is_this_cpu(struct pipeline *p)
{
	return p->ipc_pipe.core == cpu_get_id();
}

/**
 * Retrieves pipeline id from pipeline.
 * @param p pipeline.
 * @return pipeline id.
 */
static inline uint32_t pipeline_id(struct pipeline *p)
{
	return p->ipc_pipe.pipeline_id;
}

/* pipeline creation and destruction */
struct pipeline *pipeline_new(struct sof_ipc_pipe_new *pipe_desc,
	struct comp_dev *cd);
int pipeline_free(struct pipeline *p);

/* insert/remove component in pipeline */
int pipeline_connect(struct comp_dev *comp, struct comp_buffer *buffer,
		     int dir);
void pipeline_disconnect(struct comp_dev *comp, struct comp_buffer *buffer, int dir);

/* complete the pipeline */
int pipeline_complete(struct pipeline *p, struct comp_dev *source,
		      struct comp_dev *sink);

/* pipeline parameters */
int pipeline_params(struct pipeline *p, struct comp_dev *cd,
		    struct sof_ipc_pcm_params *params);

/* prepare the pipeline for usage */
int pipeline_prepare(struct pipeline *p, struct comp_dev *cd);

/* reset the pipeline and free resources */
int pipeline_reset(struct pipeline *p, struct comp_dev *host_cd);

/* perform selected cache command on pipeline */
void pipeline_cache(struct pipeline *p, struct comp_dev *dev, int cmd);

/* trigger pipeline - atomic */
int pipeline_trigger(struct pipeline *p, struct comp_dev *host_cd, int cmd);

/* static pipeline creation */
int init_static_pipeline(struct ipc *ipc);

/* pipeline creation */
int init_pipeline(void);

/* recover the pipeline from a XRUN condition */
int pipeline_xrun_recover(struct pipeline *p);

/* init pipeline scheduler task */
int pipeline_comp_task_init(struct pipeline *p);

/* copy data on pipeline */
int pipeline_copy(struct pipeline *p);

/* perform xrun recovery */
int pipeline_xrun_handle_trigger(struct pipeline *p, int cmd);

/* trigger pipeline's scheduling component  */
void pipeline_comp_trigger_sched_comp(struct pipeline *p,
				      struct comp_dev *comp,
				      struct pipeline_walk_context *ctx);

/* schedule all triggered pipelines */
void pipeline_schedule_triggered(struct pipeline_walk_context *ctx,
				 int cmd);

/* walk pipeline */
int pipeline_for_each_comp(struct comp_dev *current,
			   struct pipeline_walk_context *ctx, int dir);

/* schedule a copy operation for this pipeline */
void pipeline_schedule_copy(struct pipeline *p, uint64_t start);

/* get time pipeline timestamps from host to dai */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host_dev,
			    struct sof_ipc_stream_posn *posn);

/* notify host that we have XRUN */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes);

#endif /* __SOF_AUDIO_PIPELINE_H__ */
