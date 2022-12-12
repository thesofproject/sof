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
	struct plug_sem_desc ready;
	struct plug_sem_desc done;

	struct plug_shm_desc glb_ctx;
	struct plug_shm_desc shm_pcm;

	struct plug_mq_desc ipc;
	int frame_us;

} snd_sof_pcm_t;

static int plug_pcm_start(snd_pcm_ioplug_t * io)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	struct sof_ipc_stream stream = {0};
	int err;

	switch (ctx->state) {
	case SOF_PLUGIN_STATE_READY:
		/* trigger start */
		stream.hdr.size = sizeof(stream);
		stream.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | SOF_IPC_STREAM_TRIG_START;
		stream.comp_id = ctx->comp_id;

		err = plug_mq_cmd(&pcm->ipc, &stream, sizeof(stream), &stream, sizeof(stream));
		if (err < 0) {
			SNDERR("error: can't trigger START the PCM\n");
			return err;
		}

		// HACK - start the read for capture
		if (pcm->capture)
			ctx->rtotal = pcm->io.period_size * pcm->frame_size;
		break;
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
		/* do nothing */
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

static int plug_pcm_stop(snd_pcm_ioplug_t * io)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	struct sof_ipc_stream stream = {0};
	int err;

	switch (ctx->state) {
	case SOF_PLUGIN_STATE_READY:
	case SOF_PLUGIN_STATE_STREAM_ERROR:
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
		stream.hdr.size = sizeof(stream);
		stream.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | SOF_IPC_STREAM_TRIG_STOP;
		stream.comp_id = ctx->comp_id;

		err = plug_mq_cmd(&pcm->ipc, &stream, sizeof(stream), &stream, sizeof(stream));
		if (err < 0) {
			SNDERR("error: can't trigger STOP the PCM\n");
			return err;
		}
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
	case SOF_PLUGIN_STATE_READY:
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
	case SOF_PLUGIN_STATE_STREAM_ERROR:
		if (pcm->capture)
			ptr =  ctx->rtotal / pcm->frame_size;
		else
			ptr =  ctx->wtotal / pcm->frame_size;
		break;
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
static snd_pcm_sframes_t plug_pcm_write(snd_pcm_ioplug_t *io,
				     const snd_pcm_channel_area_t *areas,
				     snd_pcm_uframes_t offset,
				     snd_pcm_uframes_t size)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	snd_pcm_sframes_t frames = 0;
	ssize_t bytes;
	const char *buf;
	int err, delay;

	/* calculate the buffer position and size from application */
	buf = (char *)areas->addr + (areas->first + areas->step * offset) / 8;
	bytes = size * pcm->frame_size;

	/* now check what the pipe has free */
	bytes = MIN(plug_ep_get_free(ctx), bytes);
	frames = bytes / pcm->frame_size;

	if (frames == 0)
		return frames;

	/* write audio data to pipe */
	memcpy(plug_ep_wptr(ctx), buf, bytes);
	plug_ep_produce(ctx, bytes);

	/* tell the pipe data is ready */
	sem_post(pcm->ready.sem);

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
	err = sem_timedwait(pcm->done.sem, &pcm->wait_timeout);
	if (err == -1) {
		SNDERR("write: waited %d ms for %ld frames, fatal timeout: %s",
			delay, frames, strerror(errno));
		return -EPIPE;
	}

	return frames;
}

/* return frames read */
static snd_pcm_sframes_t plug_pcm_read(snd_pcm_ioplug_t *io,
				    const snd_pcm_channel_area_t *areas,
				    snd_pcm_uframes_t offset,
				    snd_pcm_uframes_t size)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	snd_pcm_sframes_t frames = 0;
	ssize_t bytes;
	char *buf;
	int err, delay;

	/* tell the pipe ready we are ready */
	sem_post(pcm->ready.sem);

	/* calculate the buffer position and size */
	buf = (char *)areas->addr + (areas->first + areas->step * offset) / 8;
	bytes = size * pcm->frame_size;

	/* now check what the pipe has avail */
	bytes = MIN(plug_ep_get_avail(ctx), bytes);
	frames = bytes / pcm->frame_size;

	if (frames == 0)
		return frames;

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
	err = sem_timedwait(pcm->done.sem, &pcm->wait_timeout);
	if (err == -1) {
		SNDERR("read: waited %d ms for %ld frames fatal timeout: %s",
			delay, frames, strerror(errno));
		return -EPIPE;
	}

	/* write audio data to pipe */
	memcpy(buf, plug_ep_rptr(ctx), bytes);
	plug_ep_consume(ctx, bytes);

	return frames;
}

static int plug_pcm_prepare(snd_pcm_ioplug_t * io)
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

	/* start the pipeline thread */
	/* check the ctx->state here - run the pipeline if its not active */
	switch (ctx->state) {
	case SOF_PLUGIN_STATE_INIT:
	case SOF_PLUGIN_STATE_STREAM_ERROR:
	case SOF_PLUGIN_STATE_DEAD:
		/* some error */
		SNDERR("write: invalid pipe state: %d", ctx->state);
		return -EINVAL;
	case SOF_PLUGIN_STATE_READY:
		/* trigger start */
		err = plug_pcm_start(io);
		if (err < 0) {
			SNDERR("prepare: cant start pipe thread: %d", err);
			return err;
		}
		break;
	case SOF_PLUGIN_STATE_STREAM_RUNNING:
	default:
		/* do nothing */
		break;
	}

	return err;
}

static int plug_pcm_hw_params(snd_pcm_ioplug_t * io,
			   snd_pcm_hw_params_t * params)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_endpoint *ctx = pcm->shm_pcm.addr;
	struct sof_ipc_pcm_params ipc_params = {0};
	struct sof_ipc_pcm_params_reply params_reply = {0};
	struct ipc_comp_dev *pcm_dev;
	struct comp_dev *cd;
	int err = 0;

	/* set plug params */
	ipc_params.comp_id = ctx->comp_id;
	ipc_params.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED; // TODO:
	ipc_params.params.rate = io->rate;
	ipc_params.params.channels = io->channels;

	switch (io->format) {
	case SND_PCM_FORMAT_S16_LE:
		ipc_params.params.frame_fmt = SOF_IPC_FRAME_S16_LE;
		ipc_params.params.sample_container_bytes = 2;
		ipc_params.params.sample_valid_bytes = 2;
		break;
	case SND_PCM_FORMAT_S24_LE:
		ipc_params.params.frame_fmt = SOF_IPC_FRAME_S24_4LE;
		ipc_params.params.sample_container_bytes = 4;
		ipc_params.params.sample_valid_bytes = 3;
		break;
	case SND_PCM_FORMAT_S32_LE:
		ipc_params.params.frame_fmt = SOF_IPC_FRAME_S32_LE;
		ipc_params.params.sample_container_bytes = 4;
		ipc_params.params.sample_valid_bytes = 4;
		break;
	default:
		SNDERR("SOF: Unsupported format %s\n",
			snd_pcm_format_name(io->format));
		return -EINVAL;
	}

	pcm->frame_size = ctx->frame_size =
	    (snd_pcm_format_physical_width(io->format) * io->channels) / 8;

	ipc_params.params.host_period_bytes = io->period_size * pcm->frame_size;

	/* Set pipeline params direction from scheduling component */
	ipc_params.params.direction = io->stream;
	ipc_params.params.hdr.size = sizeof(ipc_params.params);
	ipc_params.hdr.size = sizeof(ipc_params);
	ipc_params.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | SOF_IPC_STREAM_PCM_PARAMS;

	err = plug_mq_cmd(&pcm->ipc, &ipc_params, sizeof(ipc_params),
			   &params_reply, sizeof(params_reply));
	if (err < 0) {
		SNDERR("error: can't set PCM params\n");
		return err;
	}

	if (params_reply.rhdr.error) {
		SNDERR("error: hw_params failed");
		err = -EINVAL;
	}

	/* needs to be set here and NOT in SW params as SW params not
	 * called in capture flow ? */
	ctx->buffer_size = io->buffer_size * ctx->frame_size;

	/* used for wait timeouts */
	pcm->frame_us = ceil(1000000.0 / io->rate);

	return err;
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
		SNDERR("sw params: failed to set avail min %d: %s",
			1, strerror(err));
		return err;
	}

	return 0;
}

static int plug_pcm_close(snd_pcm_ioplug_t * io)
{
	snd_sof_plug_t *plug = io->private_data;
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct plug_shm_glb_state *ctx = pcm->glb_ctx.addr;
	int err = 0;
	printf("%s %d\n", __func__, __LINE__);

	plug_pcm_stop(io);

	// set shm state

	// close mq

	//close shm

	// close locks

	return err;
}

static const snd_pcm_ioplug_callback_t sof_playback_callback = {
	.start = plug_pcm_start,
	.stop = plug_pcm_stop,
	.pointer = plug_pcm_pointer,
	.transfer = plug_pcm_write,
	.delay = plug_pcm_delay,
	.prepare = plug_pcm_prepare,
	.hw_params = plug_pcm_hw_params,
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
static int plug_hw_constraint(snd_sof_plug_t * plug)
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
	    snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIODS,
					   1, 4);
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

	if (stream == SND_PCM_STREAM_PLAYBACK) {
		pcm->io.callback = &sof_playback_callback;
	} else {
		pcm->io.callback = &sof_capture_callback;
	}
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
	int i, j, found;

	for (i = 0; i < plug->num_cmdline; i++) {

		if (glb->num_ep_configs >= NUM_EP_CONFIGS - 1) {
			SNDERR("error: too many endpoint configs\n");
			return -EINVAL;
		}

		ci = &plug->cmdline[i];
		found = 0;

		for (j = 0; j < plug->num_configs; j++) {

			pc = &plug->config[j];

			ep = &glb->ep_config[glb->num_ep_configs++];
			ep->buffer_frames = pc->buffer_frames;
			ep->buffer_time = pc->buffer_time;
			ep->channels = pc->channels;
			ep->format = pc->format;
			ep->period_frames = pc->period_frames;
			ep->period_time = pc->period_time;
			ep->rate = pc->rate;
			ep->pipeline = ci->pipe;
			strncpy(ep->card_name, ci->card_name,
				sizeof(ep->card_name));
			strncpy(ep->dev_name, ci->dev_name,
				sizeof(ep->dev_name));
			strncpy(ep->config_name, ci->config_name,
				sizeof(ep->config_name));
			found = 1;
			break;
		}

		if (!found) {
			SNDERR("error: config %s not found\n", ci->config_name);
		//	return -EINVAL;
		}
	}

	return 0;
}

/*
 * Complete parent initialisation.
 * 1. Check if pipe already ready by opening SHM context and IPC.
 * 2. TODO: check context state and load topology is needed for core.
 * 3. Open SHM and locks for this PCM plugin.
 */
static int plug_init_sof_pipe(snd_sof_plug_t *plug, snd_pcm_t **pcmp,
							  const char *name, snd_pcm_stream_t stream,
							  int mode)
{
	snd_sof_pcm_t *pcm = plug->module_prv;
	struct timespec delay;
	struct plug_shm_glb_state *ctx;
	int err;

	/* init name for PCM ready lock */
	err = plug_lock_init(&pcm->ready, plug->tplg_file, "ready",
				plug->tplg_pipeline);
	if (err < 0) {
		SNDERR("error: invalid name for PCM ready lock %s\n",
				plug->tplg_file);
		return err;
	}

	/* init name for PCM done lock */
	err = plug_lock_init(&pcm->done, plug->tplg_file, "done",
				plug->tplg_pipeline);
	if (err < 0) {
		SNDERR("error: invalid name for PCM done lock %s\n",
				plug->tplg_file);
		return err;
	}

	/* init IPC message queue name */
	err = plug_mq_init(&pcm->ipc, plug->tplg_file, "pcm", plug->tplg_pipeline);
	if (err < 0) {
		SNDERR("error: invalid name for IPC mq %s\n", plug->tplg_file);
		return err;
	}

	/* init global status shm name */
	err = plug_shm_init(&pcm->glb_ctx, plug->tplg_file, "ctx", 0);
	if (err < 0) {
		SNDERR("error: invalid name for global SHM %s\n", plug->tplg_file);
		return err;
	}

	/* init PCM shm name */
	err = plug_shm_init(&pcm->shm_pcm, plug->tplg_file, "pcm",
				plug->tplg_pipeline);
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

	/* open the sof-pipe IPC message queue */
	err = plug_mq_open(&pcm->ipc);
	if (err < 0) {
		SNDERR("error: failed to open sof-pipe IPC mq %s: %s",
				pcm->ipc.queue_name, strerror(err));
		return -errno;
	}

	/* open lock "ready" for PCM audio data */
	err = plug_lock_open(&pcm->ready);
	if (err < 0) {
		SNDERR("error: failed to open sof-pipe ready lock %s: %s",
				pcm->ready.name, strerror(err));
		return -errno;
	}

	/* open lock "done" for PCM audio data */
	err = plug_lock_open(&pcm->done);
	if (err < 0) {
		SNDERR("error: failed to open sof-pipe done lock %s: %s",
				pcm->done.name, strerror(err));
		return -errno;
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
	fprintf(stdout, "\ni.e. aplay -Dsof:<topology>:<pcm pipeline>:<dai pipeline>:<card>:<device>:<config> file.wav\n");
	fprintf(stdout, "\nwhich can be used as\n");
	fprintf(stdout, "\ne.g. aplay -Dsof:bdw-nocodec:1:1:default:default:48k2c16b -f dat ~/audiodump.wav\n\n");

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
		goto pipe_error;
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
	free(plug->device);
dev_error:
	free(plug);
	return err;
}

SND_PCM_PLUGIN_SYMBOL(sof);
