// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#include <rtos/sof.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <ipc/stream.h>
#include <tplg_parser/topology.h>

#include "plugin.h"
#include "common.h"

typedef struct snd_sof_pcm {
	snd_pcm_ioplug_t io;
	size_t frame_size;
	struct timespec wait_timeout;
	int capture;
	int events;

	/* PCM flow control */
	struct plug_sem_desc ready[TPLG_MAX_PCM_PIPELINES];
	struct plug_sem_desc done[TPLG_MAX_PCM_PIPELINES];
	/* pipeline IPC tx queues */
	struct plug_mq_desc pipeline_ipc_tx[TPLG_MAX_PCM_PIPELINES];
	 /* pipeline IPC response queues */
	struct plug_mq_desc pipeline_ipc_rx[TPLG_MAX_PCM_PIPELINES];

	struct plug_shm_desc glb_ctx;
	struct plug_shm_desc shm_pcm;

	int frame_us;
} snd_sof_pcm_t;

static int plug_pipeline_set_state(snd_sof_plug_t *plug, int state,
				   struct ipc4_pipeline_set_state *pipe_state,
				   struct tplg_pipeline_info *pipe_info,
				   struct plug_mq_desc *ipc_tx, struct plug_mq_desc *ipc_rx)
{
	struct ipc4_message_reply reply = {{ 0 }};
	int ret;

	pipe_state->primary.r.ppl_id = pipe_info->instance_id;

	ret = plug_mq_cmd_tx_rx(ipc_tx, ipc_rx, pipe_state, sizeof(*pipe_state),
				&reply, sizeof(reply));
	if (ret < 0)
		SNDERR("failed pipeline %d set state %d\n", pipe_info->instance_id, state);

	return ret;
}

static int plug_pipelines_set_state(snd_sof_plug_t *plug, int state)
{
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct ipc4_pipeline_set_state pipe_state = {{ 0 }};
	struct tplg_pipeline_list *pipeline_list;
	int i;

	if (pcm->capture)
		pipeline_list = &plug->pcm_info->capture_pipeline_list;
	else
		pipeline_list = &plug->pcm_info->playback_pipeline_list;

	pipe_state.primary.r.ppl_state = state;
	pipe_state.primary.r.type = SOF_IPC4_GLB_SET_PIPELINE_STATE;
	pipe_state.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	pipe_state.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;

	/*
	 * pipeline list is populated starting from the host to DAI. So traverse the list in
	 * the reverse order for capture to start the source pipeline first.
	 */
	if (pcm->capture) {
		for (i = pipeline_list->count - 1; i >= 0; i--) {
			struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];
			int ret;

			ret = plug_pipeline_set_state(plug, state, &pipe_state, pipe_info,
						      &pcm->pipeline_ipc_tx[i],
						      &pcm->pipeline_ipc_rx[i]);
			if (ret < 0)
				return ret;
		}

		return 0;
	}

	for (i = 0; i < pipeline_list->count; i++) {
		struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];
		int ret;

		ret = plug_pipeline_set_state(plug, state, &pipe_state, pipe_info,
					      &pcm->pipeline_ipc_tx[i], &pcm->pipeline_ipc_rx[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int plug_pcm_start(snd_pcm_ioplug_t *io)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	int err;

	switch (ctx->state) {
	case SOF_PLUGIN_STATE_READY:
		err = plug_pipelines_set_state(plug, SOF_IPC4_PIPELINE_STATE_RUNNING);
		if (err < 0)
			return err;
		break;
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
	{
		struct tplg_pipeline_list *pipeline_list;
		int i, delay;

		if (!pcm->capture)
			break;

		pipeline_list = &plug->pcm_info->capture_pipeline_list;

		/* start the first period copy for capture */
		for (i = pipeline_list->count - 1; i >= 0; i--) {
			sem_post(pcm->ready[i].sem);

			/* wait for sof-pipe reader to consume data or timeout */
			err = clock_gettime(CLOCK_REALTIME, &pcm->wait_timeout);
			if (err == -1) {
				SNDERR("write: cant get time: %s", strerror(errno));
				return -EPIPE;
			}

			/* work out delay TODO: fix ALSA reader */
			delay = pcm->frame_us * io->period_size / 500;
			plug_timespec_add_ms(&pcm->wait_timeout, delay);

			/* wait for sof-pipe writer to produce data or timeout */
			err = sem_timedwait(pcm->done[i].sem, &pcm->wait_timeout);
			if (err == -1) {
				SNDERR("read: waited %d ms for %ld frames fatal timeout: %s",
				       delay, io->period_size, strerror(errno));
				return -errno;
			}
		}
	}
		break;
	case SOF_PLUGIN_STATE_INIT:
	case SOF_PLUGIN_STATE_STREAM_ERROR:
	case SOF_PLUGIN_STATE_DEAD:
	default:
		/* some error */
		SNDERR("pcm start: invalid pipe state: %d", ctx->state);
		return -EINVAL;
	}

	return 0;
}

static int plug_pcm_stop(snd_pcm_ioplug_t *io)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	int err;

	printf("%s %d state %ld\n", __func__, __LINE__, ctx->state);
	switch (ctx->state) {
	case SOF_PLUGIN_STATE_STREAM_ERROR:
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
	{
		err = plug_pipelines_set_state(plug, SOF_IPC4_PIPELINE_STATE_PAUSED);
		if (err < 0)
			return err;
		break;
	}
	case SOF_PLUGIN_STATE_READY:
		/* already stopped */
		break;
	case SOF_PLUGIN_STATE_INIT:
	case SOF_PLUGIN_STATE_DEAD:
	default:
		/* some error */
		SNDERR("pcm stop: invalid pipe state: %d", ctx->state);
		return -EINVAL;
	}

	return 0;
}

/* buffer position up to buffer_size */
static snd_pcm_sframes_t plug_pcm_pointer(snd_pcm_ioplug_t *io)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	snd_pcm_sframes_t ptr = 0;
	int err;

	if (io->state == SND_PCM_STATE_XRUN)
		return -EPIPE;

	if (io->state != SND_PCM_STATE_RUNNING)
		return 0;

	switch (ctx->state) {
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
	case SOF_PLUGIN_STATE_STREAM_ERROR:
		if (pcm->capture)
			ptr =  ctx->wtotal / pcm->frame_size;
		else
			ptr =  ctx->rtotal / pcm->frame_size;
		break;
	case SOF_PLUGIN_STATE_READY:
		/* not running */
		return 0;
	case SOF_PLUGIN_STATE_INIT:
	case SOF_PLUGIN_STATE_DEAD:
	default:
		/* some error */
		SNDERR("pointer: invalid pipe state: %d", ctx->state);
		ptr =  -EPIPE;
	}

	return ptr;
}

/* get the delay for the running PCM; optional; since v1.0.1 */
static int plug_pcm_delay(snd_pcm_ioplug_t *io, snd_pcm_sframes_t *delayp)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	int err = 0;

	switch (ctx->state) {
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
	case SOF_PLUGIN_STATE_READY:
		// TODO: is capture delay correct here ???
		if (pcm->capture)
			*delayp = (ctx->wtotal - ctx->rtotal) / pcm->frame_size;
		else
			*delayp = (ctx->rtotal - ctx->wtotal) / pcm->frame_size;
		/* sanitize delay */
		if (*delayp < pcm->io.period_size || *delayp > io->buffer_size)
			*delayp = pcm->io.period_size;
		return 0;
	case SOF_PLUGIN_STATE_STREAM_ERROR:
		snd_pcm_ioplug_set_state(io, SND_PCM_STATE_XRUN);
		return 0;
	case SOF_PLUGIN_STATE_INIT:
	case SOF_PLUGIN_STATE_DEAD:
	default:
		/* some error */
		SNDERR("delay: invalid pipe state: %d", ctx->state);
		return -EPIPE;
	}
}

/* return frames written */
static snd_pcm_sframes_t plug_pcm_write(snd_pcm_ioplug_t *io, const snd_pcm_channel_area_t *areas,
					snd_pcm_uframes_t offset, snd_pcm_uframes_t size)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	struct tplg_pipeline_list *pipeline_list;
	snd_pcm_sframes_t frames = 0;
	int i;
	ssize_t bytes;
	const char *buf;
	int err, delay;

	pipeline_list = &plug->pcm_info->playback_pipeline_list;

	/* calculate the buffer position and size from application */
	buf = (char *)areas->addr + (areas->first + areas->step * offset) / 8;
	bytes = size * pcm->frame_size;

	/* now check what the pipe has free */
	bytes = MIN(plug_ep_get_free(ctx), bytes);

	frames = bytes / pcm->frame_size;

	if (frames == 0)
		return frames;

	/* write audio data to the pipe */
	memcpy(plug_ep_wptr(ctx), buf, bytes);

	plug_ep_produce(ctx, bytes);

	/* tell the pipelines data is ready starting at the source pipeline */
	for (i = 0; i < pipeline_list->count; i++) {
		struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

		sem_post(pcm->ready[i].sem);

		/* wait for sof-pipe reader to consume data or timeout */
		err = clock_gettime(CLOCK_REALTIME, &pcm->wait_timeout);
		if (err == -1) {
			SNDERR("write: cant get time: %s", strerror(errno));
			return -EPIPE;
		}

		/* work out delay */
		delay = pcm->frame_us * frames / 500;
		plug_timespec_add_ms(&pcm->wait_timeout, delay);

		/* now block caller on pipeline IO to PCM device */
		err = sem_timedwait(pcm->done[i].sem, &pcm->wait_timeout);
		if (err == -1) {
			SNDERR("write: waited %d ms for %ld frames, fatal timeout: %s",
			       delay, frames, strerror(errno));
			return -errno;
		}
	}

	return frames;
}

/* return frames read */
static snd_pcm_sframes_t plug_pcm_read(snd_pcm_ioplug_t *io, const snd_pcm_channel_area_t *areas,
				       snd_pcm_uframes_t offset, snd_pcm_uframes_t size)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	snd_pcm_sframes_t frames;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	struct tplg_pipeline_list *pipeline_list;
	ssize_t bytes;
	char *buf;
	int err, delay, i;

	pipeline_list = &plug->pcm_info->capture_pipeline_list;

	/* calculate the buffer position and size */
	buf = (char *)areas->addr + (areas->first + areas->step * offset) / 8;
	bytes = size * pcm->frame_size;

	/* check what the pipe has avail */
	bytes = MIN(plug_ep_get_avail(ctx), bytes);
	frames = bytes / pcm->frame_size;

	if (!frames)
		return 0;

	/* tell the pipe ready we are ready for next period */
	for (i = pipeline_list->count - 1; i >= 0; i--) {
		sem_post(pcm->ready[i].sem);

		/* wait for sof-pipe reader to consume data or timeout */
		err = clock_gettime(CLOCK_REALTIME, &pcm->wait_timeout);
		if (err == -1) {
			SNDERR("write: cant get time: %s", strerror(errno));
			return -EPIPE;
		}

		/* work out delay TODO: fix ALSA reader */
		delay = pcm->frame_us * frames / 500;
		plug_timespec_add_ms(&pcm->wait_timeout, delay);

		/* wait for sof-pipe writer to produce data or timeout */
		err = sem_timedwait(pcm->done[i].sem, &pcm->wait_timeout);
		if (err == -1) {
			SNDERR("read: waited %d ms for %ld frames fatal timeout: %s",
			       delay, frames, strerror(errno));
			return -errno;
		}
	}

	/* copy audio data from pipe */
	memcpy(buf, plug_ep_rptr(ctx), bytes);
	plug_ep_consume(ctx, bytes);

	return frames;
}

static int plug_pcm_prepare(snd_pcm_ioplug_t *io)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	int err = 0;

	ctx->wtotal = 0;
	ctx->rtotal = 0;
	ctx->rpos = 0;
	ctx->rwrap = 0;
	ctx->wpos = 0;
	ctx->wwrap = 0;

	/* start the pipeline threads
	 *
	 * We do this during prepare so that pipelines can consume/produce
	 * any start threshold worth of data with the pipeline in a running
	 * state
	 */
	switch (ctx->state) {
	case SOF_PLUGIN_STATE_INIT:
		/* set pipelines to PAUSED state to prepare them */
		err = plug_pipelines_set_state(plug, SOF_IPC4_PIPELINE_STATE_PAUSED);
		if (err < 0)
			return err;

		fprintf(stdout, "pipelines complete now\n");

		/* set pipelines to RUNNING state */
		err = plug_pipelines_set_state(plug, SOF_IPC4_PIPELINE_STATE_RUNNING);
		if (err < 0)
			return err;
		break;
	case SOF_PLUGIN_STATE_STREAM_ERROR:
	case SOF_PLUGIN_STATE_DEAD:
		/* some error */
		SNDERR("write: invalid pipe state: %d", ctx->state);
		return -EINVAL;
	case SOF_PLUGIN_STATE_READY:
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
	default:
		/* do nothing */
		break;
	}

	fprintf(stdout, "PCM prepare done\n");

	return err;
}

static int plug_init_shm_ctx(snd_sof_plug_t *plug);

static int plug_pcm_hw_params(snd_pcm_ioplug_t *io, snd_pcm_hw_params_t *params)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx;
	struct tplg_pipeline_list *pipeline_list;
	int i, err;

	pcm->frame_size = (snd_pcm_format_physical_width(io->format) * io->channels) / 8;

	plug->period_size = io->period_size;

	/* used for wait timeouts */
	pcm->frame_us = ceil(1000000.0 / io->rate);

	/* now send IPCs to set up widgets */
	err = plug_set_up_pipelines(plug, pcm->capture);
	if (err < 0) {
		fprintf(stderr, "error setting up pipelines\n");
		return err;
	}

	if (pcm->capture)
		pipeline_list = &plug->pcm_info->capture_pipeline_list;
	else
		pipeline_list = &plug->pcm_info->playback_pipeline_list;

	/* init and open for PCM ready lock for all pipelines */
	for (i = 0; i < pipeline_list->count; i++) {
		struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

		/* init IPC message queue name */
		err = plug_mq_init(&pcm->pipeline_ipc_tx[i], plug->tplg_file, "pcm-tx",
				   pipe_info->instance_id);
		if (err < 0) {
			SNDERR("error: invalid name for pipeline IPC mq %s\n", plug->tplg_file);
			return err;
		}

		/* open the sof-pipe IPC message queue */
		err = plug_mq_open(&pcm->pipeline_ipc_tx[i]);
		if (err < 0) {
			SNDERR("error: failed to open sof-pipe IPC mq %s: %s",
			       pcm->pipeline_ipc_tx[i].queue_name, strerror(err));
			return -errno;
		}

		/* init IPC message queue name */
		err = plug_mq_init(&pcm->pipeline_ipc_rx[i], plug->tplg_file, "pcm-rx",
				   pipe_info->instance_id);
		if (err < 0) {
			SNDERR("error: invalid name for pipeline IPC mq %s\n", plug->tplg_file);
			return err;
		}

		/* open the sof-pipe IPC message queue */
		err = plug_mq_open(&pcm->pipeline_ipc_rx[i]);
		if (err < 0) {
			SNDERR("error: failed to open sof-pipe IPC mq %s: %s",
			       pcm->pipeline_ipc_tx[i].queue_name, strerror(err));
			return -errno;
		}

		/* init name for pipeline ready lock */
		err = plug_lock_init(&pcm->ready[i], plug->tplg_file, "ready",
				     pipe_info->instance_id);
		if (err < 0) {
			SNDERR("error: invalid name for PCM ready lock %s\n",
			       pipe_info->instance_id);
			return err;
		}

		/* init name for pipeline done lock */
		err = plug_lock_init(&pcm->done[i], plug->tplg_file, "done",
				     pipe_info->instance_id);
		if (err < 0) {
			SNDERR("error: invalid name for PCM done lock %s\n",
			       pipe_info->instance_id);
			return err;
		}

		/* open lock "ready" for pipeline audio data */
		err = plug_lock_open(&pcm->ready[i]);
		if (err < 0) {
			SNDERR("error: failed to open sof-pipe ready lock %s: %s",
			       pcm->ready[i].name, strerror(err));
			return -errno;
		}

		/* open lock "done" for pipeline audio data */
		err = plug_lock_open(&pcm->done[i]);
		if (err < 0) {
			SNDERR("error: failed to open sof-pipe done lock %s: %s",
			       pcm->done[i].name, strerror(err));
			return -errno;
		}
	}

	/* init global status shm name */
	err = plug_shm_init(&pcm->glb_ctx, plug->tplg_file, "ctx", 0);
	if (err < 0) {
		SNDERR("error: invalid name for global SHM %s\n", plug->tplg_file);
		return err;
	}

	/* init PCM shm name */
	err = plug_shm_init(&pcm->shm_pcm, plug->tplg_file, "pcm", plug->pcm_id);
	if (err < 0) {
		SNDERR("error: invalid name for PCM SHM %s\n", plug->tplg_file);
		return err;
	}

	/* open the global sof-pipe context via SHM */
	pcm->glb_ctx.size = 128 * 1024;
	err = plug_shm_open(&pcm->glb_ctx);
	if (err < 0) {
		SNDERR("error: failed to open sof-pipe context: %s:%s",
		       pcm->glb_ctx.name, strerror(err));
		return err;
	}

	/* open audio PCM SHM data endpoint */
	err = plug_shm_open(&pcm->shm_pcm);
	if (err < 0) {
		SNDERR("error: failed to open sof-pipe PCM SHM %s: %s",
		       pcm->shm_pcm.name, strerror(err));
		return -errno;
	}

	/* set up the endpoint configs */
	err = plug_init_shm_ctx(plug);
	if (err < 0) {
		SNDERR("error: failed to init sof-pipe ep context: %s:%s",
		       pcm->glb_ctx.name, strerror(err));
		return -err;
	}

	ctx = pcm->shm_pcm.addr;
	ctx->frame_size = pcm->frame_size;

	/* needs to be set here and NOT in SW params as SW params not
	 * called in capture flow ?
	 */
	ctx->buffer_size = io->buffer_size * ctx->frame_size;

	if (!ctx->buffer_size) {
		SNDERR("Invalid buffer_size io buffer_size %d ctx->frame_size %d\n",
		       io->buffer_size, ctx->frame_size);
		return -EINVAL;
	}

	fprintf(stdout, "PCM hw_params done\n");

	return 0;
}

// TODO: why not called for arecord
static int plug_pcm_sw_params(snd_pcm_ioplug_t *io, snd_pcm_sw_params_t *params)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	snd_pcm_uframes_t start_threshold;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	int err;

	/* get the stream start threshold */
	err = snd_pcm_sw_params_get_start_threshold(params, &start_threshold);
	if (err < 0) {
		SNDERR("sw params: failed to get start threshold: %s", strerror(err));
		return err;
	}

	/* TODO: this seems to be ignored or overridden by application params ??? */
	if (start_threshold < io->period_size) {
		start_threshold = io->period_size;
		err = snd_pcm_sw_params_set_start_threshold(pcm->io.pcm,
							    params, start_threshold);
		if (err < 0) {
			SNDERR("sw params: failed to set start threshold %d: %s",
			       start_threshold, strerror(err));
			return err;
		}
	}

	/* keep running as long as we can */
	err = snd_pcm_sw_params_set_avail_min(pcm->io.pcm, params, 1);
	if (err < 0) {
		SNDERR("sw params: failed to set avail min %d: %s", 1, strerror(err));
		return err;
	}

	fprintf(stdout, "sw_params done\n");

	return 0;
}

static int plug_pcm_close(snd_pcm_ioplug_t *io)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_glb_state *ctx = pcm->glb_ctx.addr;
	int err = 0;

	printf("%s %d\n", __func__, __LINE__);

	ctx->state = SOF_PLUGIN_STATE_INIT;

	plug_free_topology(plug);

	free(plug->tplg_file);
	free(plug->module_prv);
	free(plug);

	return err;
}

static int plug_pcm_hw_free(snd_pcm_ioplug_t *io)
{
	struct tplg_pipeline_list *pipeline_list;
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	int ret, i;

	/* reset all pipelines */
	ret = plug_pipelines_set_state(plug, SOF_IPC4_PIPELINE_STATE_RESET);
	if (ret < 0) {
		fprintf(stderr, "failed to reset pipelines\n");
		return ret;
	}

	if (pcm->capture)
		pipeline_list = &plug->pcm_info->capture_pipeline_list;
	else
		pipeline_list = &plug->pcm_info->playback_pipeline_list;

	ret = plug_free_pipelines(plug, pipeline_list, pcm->capture);
	if (ret < 0)
		return ret;

	close(pcm->shm_pcm.fd);
	close(pcm->glb_ctx.fd);

	for (i = 0; i < pipeline_list->count; i++) {
		struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

		mq_close(pcm->pipeline_ipc_tx[pipe_info->instance_id].mq);
		mq_close(pcm->pipeline_ipc_rx[pipe_info->instance_id].mq);
		sem_close(pcm->ready[pipe_info->instance_id].sem);
		sem_close(pcm->done[pipe_info->instance_id].sem);
	}
}

static const snd_pcm_ioplug_callback_t sof_playback_callback = {
	.start = plug_pcm_start,
	.stop = plug_pcm_stop,
	.pointer = plug_pcm_pointer,
	.transfer = plug_pcm_write,
	.delay = plug_pcm_delay,
	.prepare = plug_pcm_prepare,
	.hw_params = plug_pcm_hw_params,
	.hw_free = plug_pcm_hw_free,
	.sw_params = plug_pcm_sw_params,
	.close = plug_pcm_close,
};

static const snd_pcm_ioplug_callback_t sof_capture_callback = {
	.start = plug_pcm_start,
	.stop = plug_pcm_stop,
	.pointer = plug_pcm_pointer,
	.transfer = plug_pcm_read,
	.delay = plug_pcm_delay,
	.prepare = plug_pcm_prepare,
	.hw_params = plug_pcm_hw_params,
	.sw_params = plug_pcm_sw_params,
	.hw_free = plug_pcm_hw_free,
	.close = plug_pcm_close,
};

static const snd_pcm_access_t access_list[] = {
	SND_PCM_ACCESS_RW_INTERLEAVED
};

static const unsigned int formats[] = {
	SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_FLOAT_LE,
	SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_S24_LE,
};

/*
 * Set HW constraints for the SOF plugin. This needs to be quite unrestrictive atm
 * as we really need to parse topology before the HW constraints can be narrowed
 * to a range that will work with the specified pipeline.
 * TODO: Align with topology.
 */
static int plug_hw_constraint(snd_sof_plug_t *plug)
{
	snd_sof_pcm_t *pcm = plug->module_prv;
	snd_pcm_ioplug_t *io = &pcm->io;
	int err;

	err = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_ACCESS,
					    ARRAY_SIZE(access_list),
					    access_list);
	if (err < 0) {
		SNDERR("constraints: failed to set access: %s", strerror(err));
		return err;
	}

	err = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_FORMAT,
					    ARRAY_SIZE(formats), formats);
	if (err < 0) {
		SNDERR("constraints: failed to set format: %s", strerror(err));
		return err;
	}

	err =
	    snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_CHANNELS,
					    1, 8);
	if (err < 0) {
		SNDERR("constraints: failed to set channels: %s", strerror(err));
		return err;
	}

	err = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_RATE,
					      1, 192000);
	if (err < 0) {
		SNDERR("constraints: failed to set rate: %s", strerror(err));
		return err;
	}

	err =
	    snd_pcm_ioplug_set_param_minmax(io,
					    SND_PCM_IOPLUG_HW_BUFFER_BYTES,
					    1, 4 * 1024 * 1024);
	if (err < 0) {
		SNDERR("constraints: failed to set buffer bytes: %s", strerror(err));
		return err;
	}

	err =
	    snd_pcm_ioplug_set_param_minmax(io,
					    SND_PCM_IOPLUG_HW_PERIOD_BYTES,
					    128, 2 * 1024 * 1024);
	if (err < 0) {
		SNDERR("constraints: failed to set period bytes: %s", strerror(err));
		return err;
	}

	err =
	    snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIODS, 1, 4);
	if (err < 0) {
		SNDERR("constraints: failed to set period count: %s", strerror(err));
		return err;
	}

	return 0;
}

/*
 * Register the plugin with ALSA and make available for use.
 * TODO: setup all audio params
 * TODO: setup polling fd for RW or mmap IOs
 */
static int plug_create(snd_sof_plug_t *plug, snd_pcm_t **pcmp, const char *name,
		       snd_pcm_stream_t stream, int mode)
{
	snd_sof_pcm_t *pcm = plug->module_prv;
	int err;

	pcm->io.version = SND_PCM_IOPLUG_VERSION;
	pcm->io.name = "ALSA <-> SOF PCM I/O Plugin";
	pcm->io.poll_fd = pcm->shm_pcm.fd;
	pcm->io.poll_events = POLLIN;
	pcm->io.mmap_rw = 0;

	if (stream == SND_PCM_STREAM_PLAYBACK)
		pcm->io.callback = &sof_playback_callback;
	else
		pcm->io.callback = &sof_capture_callback;

	pcm->io.private_data = plug;

	/* create the plugin */
	err = snd_pcm_ioplug_create(&pcm->io, name, stream, mode);
	if (err < 0) {
		SNDERR("failed to register plugin %s: %s\n", name, strerror(err));
		return err;
	}

	/* set the HW constrainst */
	err = plug_hw_constraint(plug);
	if (err < 0) {
		snd_pcm_ioplug_delete(&pcm->io);
		return err;
	}

	*pcmp = pcm->io.pcm;
	return 0;
}

static int plug_init_shm_ctx(snd_sof_plug_t *plug)
{
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_glb_state *glb = pcm->glb_ctx.addr;
	struct endpoint_hw_config *ep;
	struct plug_cmdline_item *ci;
	struct plug_config *pc;
	int i, j;

	glb->num_ep_configs = 0;

	for (i = 0; i < plug->num_cmdline; i++) {
		bool found = false;

		if (glb->num_ep_configs >= NUM_EP_CONFIGS - 1) {
			SNDERR("error: too many endpoint configs\n");
			return -EINVAL;
		}

		ci = &plug->cmdline[i];

		for (j = 0; j < plug->num_configs; j++) {
			pc = &plug->config[j];
			if (strcmp(pc->name, ci->config_name))
				continue;

			ep = &glb->ep_config[glb->num_ep_configs++];
			ep->buffer_frames = pc->buffer_frames;
			ep->buffer_time = pc->buffer_time;
			ep->channels = pc->channels;
			ep->format = pc->format;
			ep->period_frames = pc->period_frames;
			ep->period_time = pc->period_time;
			ep->rate = pc->rate;
			ep->pipeline = ci->pcm;
			strncpy(ep->card_name, ci->card_name,
				sizeof(ep->card_name));
			strncpy(ep->dev_name, ci->dev_name,
				sizeof(ep->dev_name));
			strncpy(ep->config_name, ci->config_name,
				sizeof(ep->config_name));
			found = true;
			break;
		}

		if (!found) {
			SNDERR("error: config %s not found\n", ci->config_name);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * Complete parent initialisation.
 * 1. Check if pipe already ready by opening SHM context and IPC.
 * 2. TODO: check context state and load topology is needed for core.
 */
static int plug_init_sof_pipe(snd_sof_plug_t *plug, snd_pcm_t **pcmp,
			      const char *name, snd_pcm_stream_t stream, int mode)
{
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_glb_state *ctx;
	struct timespec delay;
	int err, i;

	/* initialize widget, route and pcm lists */
	list_init(&plug->widget_list);
	list_init(&plug->route_list);
	list_init(&plug->pcm_list);

	plug->tplg.tplg_file = plug->tplg_file;

	/* plugin only works with IPC4 */
	plug->tplg.ipc_major = 4;

	/* parse topology file */
	err = plug_parse_topology(plug);
	if (err < 0) {
		fprintf(stderr, "error parsing topology %s\n", plug->tplg.tplg_file);
		return err;
	}

	fprintf(stdout, "topology parsing complete\n");

	/* init IPC message queue name */
	err = plug_mq_init(&plug->ipc_tx, "sof", "ipc-tx", 0);
	if (err < 0) {
		SNDERR("error: invalid name for IPC mq %s\n", plug->tplg_file);
		return err;
	}

	/* open the sof-pipe IPC message queue */
	err = plug_mq_open(&plug->ipc_tx);
	if (err < 0) {
		SNDERR("error: failed to open sof-pipe IPC mq %s: %s",
		       plug->ipc_tx.queue_name, strerror(err));
		return -errno;
	}

	err = plug_mq_init(&plug->ipc_rx, "sof", "ipc-rx", 0);
	if (err < 0) {
		SNDERR("error: invalid name for IPC mq %s\n", plug->tplg_file);
		return err;
	}

	/* open the sof-pipe IPC message queue */
	err = plug_mq_open(&plug->ipc_rx);
	if (err < 0) {
		SNDERR("error: failed to open sof-pipe IPC mq %s: %s",
		       plug->ipc_rx.queue_name, strerror(err));
		return -errno;
	}

	/* now register the plugin */
	err = plug_create(plug, pcmp, name, stream, mode);
	if (err < 0) {
		SNDERR("failed to create plugin: %s", strerror(err));
		return -errno;
	}

	return 0;
}

/*
 * ALSA PCM plugin entry point.
 */
SND_PCM_PLUGIN_DEFINE_FUNC(sof)
{
	snd_sof_plug_t *plug;
	snd_sof_pcm_t *pcm;
	int err;

	fprintf(stdout, "This code is WIP. Cmd args & config will possible change over time\n");
	fprintf(stdout, "\nThe 50-sonf.conf file is parsed for PCM configurations which can\n");
	fprintf(stdout, "be mapped on the cmd line to pipeline endpoints.\n");
	fprintf(stdout, "\ni.e. aplay -Dsof:<topology>:<pcm id><card>:<device>:<config> file.wav\n");
	fprintf(stdout, "\nwhich can be used as\n");
	fprintf(stdout, "\ne.g. aplay -Dsof:bdw-nocodec:1:default:default:48k2c16b -f dat ~/audiodump.wav\n\n");

	/* create context */
	plug = calloc(1, sizeof(*plug));
	if (!plug)
		return -ENOMEM;

	pcm = calloc(1, sizeof(*pcm));
	if (!pcm) {
		free(plug);
		return -ENOMEM;
	}
	plug->module_prv = pcm;

	if (stream == SND_PCM_STREAM_CAPTURE)
		pcm->capture = 1;

	/* parse the ALSA configuration file for sof plugin */
	err = plug_parse_conf(plug, name, root, conf);
	if (err < 0) {
		SNDERR("failed to parse config: %s", strerror(err));
		goto parse_conf_err;
	}

	/* now try and connect to the sof-pipe for this topology */
	err = plug_init_sof_pipe(plug, pcmp, name, stream, mode);
	if (err < 0) {
		SNDERR("failed to complete plugin init: %s", strerror(err));
		goto pipe_error;
	}

	/* everything is good */
	return 0;

pipe_error:
	free(plug->tplg_file);
parse_conf_err:
	free(plug->module_prv);
dev_error:
	free(plug);
	return err;
}

SND_PCM_PLUGIN_SYMBOL(sof);
