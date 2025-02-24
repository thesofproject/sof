// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

/*
 * IPC (InterProcessor Communication) provides a method of two way
 * communication between the host processor and the DSP. The IPC used here
 * utilises a shared mailbox and door bell between the host and DSP.
 *
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/gdb/gdb.h>
#include <rtos/panic.h>
#include <rtos/idc.h>
#include <rtos/interrupt.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
#include <sof/lib/agent.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/pm_runtime.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <rtos/string_macro.h>
#if CONFIG_TRACE
#include <sof/trace/dma-trace.h>
#endif
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/dai.h>
#include <ipc/debug.h>
#include <ipc/header.h>
#include <ipc/pm.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <ipc/probe.h>
#include <sof/probe/probe.h>

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Command format errors during fuzzing are reported for virtually all
 * commands, and the resulting flood of logging becomes a severe
 * performance penalty (i.e. we get a lot less fuzzing done per CPU
 * cycle).
 */
#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
#define ipc_cmd_err(...)
#else
#define ipc_cmd_err(...) tr_err(__VA_ARGS__)
#endif

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_CAVS
#include <ipc/header-intel-cavs.h>
#include <cavs/drivers/sideband-ipc.h>
#define CAVS_IPC_TYPE_S(x)		((x) & CAVS_IPC_TYPE_MASK)
#endif

#define iGS(x) ((x) & SOF_GLB_TYPE_MASK)
#define iCS(x) ((x) & SOF_CMD_TYPE_MASK)

/* FIXME: assert() is most likely turned off in production builds
 * https://open-std.org/jtc1/sc22/wg14/www/docs/n1967.htm
 */
#define _IPC_COPY_CMD(rx, tx, rx_size)					\
	do {								\
		int ___ret;						\
		if (rx_size > tx->size) {				\
			___ret = memcpy_s(rx, rx_size, tx, tx->size);	\
			assert(!___ret);				\
			bzero((char *)rx + tx->size, rx_size - tx->size);\
			tr_dbg(&ipc_tr, "ipc: hdr 0x%x rx (%zu) > tx (%d)",\
			       rx->cmd, rx_size, tx->size);		\
		} else if (tx->size > rx_size) {			\
			___ret = memcpy_s(rx, rx_size, tx, rx_size);	\
			assert(!___ret);				\
			tr_warn(&ipc_tr, "ipc: hdr 0x%x tx (%d) > rx (%zu)",\
				rx->cmd, tx->size, rx_size);		\
		} else	{						\
			___ret = memcpy_s(rx, rx_size, tx, rx_size);	\
			assert(!___ret);				\
		}							\
	} while (0)

/* copies whole message from Tx to Rx, follows above ABI rules */
#define IPC_COPY_CMD(rx, tx) \
	_IPC_COPY_CMD(((struct sof_ipc_cmd_hdr *)&rx),			\
			((struct sof_ipc_cmd_hdr *)tx),			\
			sizeof(rx))

struct ipc_cmd_hdr *mailbox_validate(void)
{
	struct sof_ipc_cmd_hdr *hdr = ipc_get()->comp_data;

	/* read component values from the inbox */
	mailbox_hostbox_read(hdr, SOF_IPC_MSG_MAX_SIZE, 0, sizeof(*hdr));

	/* validate component header */
	if (hdr->size < sizeof(*hdr) || hdr->size > SOF_IPC_MSG_MAX_SIZE) {
#ifndef CONFIG_ARCH_POSIX_LIBFUZZER
		ipc_cmd_err(&ipc_tr, "ipc: invalid size 0x%x", hdr->size);
#endif
		return NULL;
	}

	/* read rest of component data */
	mailbox_hostbox_read(hdr + 1, SOF_IPC_MSG_MAX_SIZE - sizeof(*hdr),
			     sizeof(*hdr), hdr->size - sizeof(*hdr));

	return ipc_to_hdr(hdr);
}

/*
 * Stream IPC Operations.
 */

#if CONFIG_HOST_PTABLE
/* check if a pipeline is hostless when walking downstream */
static bool is_hostless_downstream(struct comp_dev *current)
{
	struct comp_buffer *buffer;

	/* check if current is a HOST comp */
	if (current->ipc_config.type == SOF_COMP_HOST ||
	    current->ipc_config.type == SOF_COMP_SG_HOST)
		return false;

	/* check if the pipeline has a HOST comp downstream */
	comp_dev_for_each_consumer(current, buffer) {
		/* don't go downstream if this component is not connected */
		if (!comp_buffer_get_sink_component(buffer))
			continue;

		/* dont go downstream if this comp belongs to another pipe */
		if (comp_buffer_get_sink_component(buffer)->ipc_config.pipeline_id !=
			current->ipc_config.pipeline_id)
			continue;

		/* return if there's a host comp downstream */
		if (!is_hostless_downstream(comp_buffer_get_sink_component(buffer)))
			return false;
	}

	return true;
}

/* check if a pipeline is hostless when walking upstream */
static bool is_hostless_upstream(struct comp_dev *current)
{
	struct comp_buffer *buffer;

	/* check if current is a HOST comp */
	if (current->ipc_config.type == SOF_COMP_HOST ||
	    current->ipc_config.type == SOF_COMP_SG_HOST)
		return false;

	/* check if the pipeline has a HOST comp upstream */
	comp_dev_for_each_producer(current, buffer) {
		/* don't go upstream if this component is not connected */
		if (!comp_buffer_get_source_component(buffer))
			continue;

		/* dont go upstream if this comp belongs to another pipeline */
		if (comp_buffer_get_source_component(buffer)->ipc_config.pipeline_id !=
		    current->ipc_config.pipeline_id)
			continue;

		/* return if there is a host comp upstream */
		if (!is_hostless_upstream(comp_buffer_get_source_component(buffer)))
			return false;
	}

	return true;
}
#endif

/* allocate a new stream */
static int ipc_stream_pcm_params(uint32_t stream)
{
#if CONFIG_HOST_PTABLE
	struct dma_sg_elem_array elem_array;
	uint32_t ring_size;
	enum comp_copy_type copy_type = COMP_COPY_ONE_SHOT;
	struct comp_dev *cd;
	uint32_t direction;
#endif
	struct ipc *ipc = ipc_get();
	struct sof_ipc_pcm_params pcm_params;
	struct sof_ipc_pcm_params_reply reply;
	struct ipc_comp_dev *pcm_dev;
	struct sof_ipc_stream_posn posn;
	int err, reset_err;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(pcm_params, ipc->comp_data);

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp_by_id(ipc, pcm_params.comp_id);
	if (!pcm_dev) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", pcm_params.comp_id);
		return -ENODEV;
	}

	/* check core */
	if (!cpu_is_me(pcm_dev->core))
		return ipc_process_on_core(pcm_dev->core, false);

	tr_dbg(&ipc_tr, "ipc: comp %d -> params", pcm_params.comp_id);

	/* sanity check comp */
	if (!pcm_dev->cd->pipeline) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d pipeline not found",
			    pcm_params.comp_id);
		return -EINVAL;
	}

	/* sanity check for pcm_params size */
	if (pcm_params.hdr.size !=
	    sizeof(pcm_params) + pcm_params.params.ext_data_length) {
		ipc_cmd_err(&ipc_tr, "pcm_params invalid size, hdr.size=%d, ext_data_len=%d",
			    pcm_params.hdr.size, pcm_params.params.ext_data_length);
		return -EINVAL;
	}

	/* sanity check for pcm_params.params size */
	if (pcm_params.params.hdr.size !=
	    sizeof(pcm_params.params) + pcm_params.params.ext_data_length) {
		ipc_cmd_err(&ipc_tr, "pcm_params.params invalid size, hdr.size=%d, ext_data_len=%d",
			    pcm_params.params.hdr.size, pcm_params.params.ext_data_length);
		return -EINVAL;
	}

	if (sizeof(pcm_params) + pcm_params.params.ext_data_length > SOF_IPC_MSG_MAX_SIZE) {
		ipc_cmd_err(&ipc_tr, "pcm_params ext_data_length invalid size %d max allowed %zu",
			    pcm_params.params.ext_data_length,
			    SOF_IPC_MSG_MAX_SIZE - sizeof(pcm_params));
		return -EINVAL;
	}

	switch (pcm_dev->cd->pipeline->status) {
	case COMP_STATE_ACTIVE:
	case COMP_STATE_PRE_ACTIVE:
		/*
		 * IPC4 has a use-case when a PCM parameter change request can
		 * be sent on an active pipeline, ignore it
		 */
		pipe_dbg(pcm_dev->cd->pipeline,
			 "ipc: ignore PCM param change request on an active pipeline");
		return 0;
	}

#if CONFIG_HOST_PTABLE
	cd = pcm_dev->cd;

	/*
	 * walk in both directions to check if the pipeline is hostless
	 * skip page table set up if it is
	 */
	if (is_hostless_downstream(cd) && is_hostless_upstream(cd))
		goto pipe_params;

	err = comp_get_attribute(cd, COMP_ATTR_COPY_DIR, &direction);
	if (err < 0)
		goto error;

	err = ipc_process_host_buffer(ipc, &pcm_params.params.buffer,
				      direction,
				      &elem_array,
				      &ring_size);
	if (err < 0)
		goto error;

	err = comp_set_attribute(cd, COMP_ATTR_HOST_BUFFER, &elem_array);
	if (err < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d host buffer failed %d",
			    pcm_params.comp_id, err);
		goto error;
	}

	/* TODO: should be extracted to platform specific code */
	err = comp_set_attribute(cd, COMP_ATTR_COPY_TYPE, &copy_type);
	if (err < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d setting copy type failed %d",
			    pcm_params.comp_id, err);
		goto error;
	}

pipe_params:
#endif

	/* configure pipeline audio params */
	err = pipeline_params(pcm_dev->cd->pipeline, pcm_dev->cd,
			(struct sof_ipc_pcm_params *)ipc_get()->comp_data);
	if (err < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d comp %d params failed %d",
			    pcm_dev->cd->pipeline->pipeline_id,
			    pcm_params.comp_id, err);
		goto error;
	}

	/* prepare pipeline audio params */
	err = pipeline_prepare(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (err < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d comp %d prepare failed %d",
			    pcm_dev->cd->pipeline->pipeline_id,
			    pcm_params.comp_id, err);
		goto error;
	}

	/* write component values to the outbox */
	reply.rhdr.hdr.size = sizeof(reply);
	reply.rhdr.hdr.cmd = stream;
	reply.rhdr.error = 0;
	reply.comp_id = pcm_params.comp_id;
	reply.posn_offset = pcm_dev->cd->pipeline->posn_offset;

	/* reset position value before send ipc */
	memset(&posn, 0, sizeof(posn));
	mailbox_stream_write(reply.posn_offset, &posn, sizeof(posn));

	mailbox_hostbox_write(0, &reply, sizeof(reply));
	return 1;

error:
	reset_err = pipeline_reset(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (reset_err < 0)
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d comp %d reset failed %d",
			    pcm_dev->cd->pipeline->pipeline_id,
			    pcm_params.comp_id, reset_err);
	return err;
}

/* free stream resources */
static int ipc_stream_pcm_free(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_stream free_req;
	struct ipc_comp_dev *pcm_dev;
	int ret;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(free_req, ipc->comp_data);

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp_by_id(ipc, free_req.comp_id);
	if (!pcm_dev) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", free_req.comp_id);
		return -ENODEV;
	}

	/* check core */
	if (!cpu_is_me(pcm_dev->core))
		return ipc_process_on_core(pcm_dev->core, false);

	tr_dbg(&ipc_tr, "ipc: comp %d -> free", free_req.comp_id);

	/* sanity check comp */
	if (!pcm_dev->cd->pipeline) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d pipeline not found",
			    free_req.comp_id);
		return -EINVAL;
	}

	/* reset the pipeline */
	ret = pipeline_reset(pcm_dev->cd->pipeline, pcm_dev->cd);

	return ret;
}

/* get stream position */
static int ipc_stream_position(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_stream stream;
	struct sof_ipc_stream_posn posn;
	struct ipc_comp_dev *pcm_dev;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(stream, ipc->comp_data);

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp_by_id(ipc, stream.comp_id);
	if (!pcm_dev) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", stream.comp_id);
		return -ENODEV;
	}

	/* check core */
	if (!cpu_is_me(pcm_dev->core))
		return ipc_process_on_core(pcm_dev->core, false);

	tr_info(&ipc_tr, "ipc: comp %d -> position", stream.comp_id);

	memset(&posn, 0, sizeof(posn));

	/* set message fields - TODO; get others */
	posn.rhdr.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | SOF_IPC_STREAM_POSITION |
			    stream.comp_id;
	posn.rhdr.hdr.size = sizeof(posn);
	posn.comp_id = stream.comp_id;

	if (!pcm_dev->cd->pipeline) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d pipeline not found",
			    stream.comp_id);
		return -EINVAL;
	}

	/* get the stream positions and timestamps */
	pipeline_get_timestamp(pcm_dev->cd->pipeline, pcm_dev->cd, &posn);

	/* copy positions to stream region */
	mailbox_stream_write(pcm_dev->cd->pipeline->posn_offset,
			     &posn, sizeof(posn));

	return 1;
}

static int ipc_stream_trigger(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *pcm_dev;
	struct sof_ipc_stream stream;
	uint32_t ipc_command = iCS(header);
	uint32_t cmd;
	int ret;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(stream, ipc->comp_data);

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp_by_id(ipc, stream.comp_id);
	if (!pcm_dev) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", stream.comp_id);
		return -ENODEV;
	}

	/* check core */
	if (!cpu_is_me(pcm_dev->core))
		return ipc_process_on_core(pcm_dev->core, false);

	tr_dbg(&ipc_tr, "ipc: comp %d -> trigger cmd 0x%x",
	       stream.comp_id, ipc_command);

	switch (ipc_command) {
	case SOF_IPC_STREAM_TRIG_START:
		cmd = COMP_TRIGGER_PRE_START;
		break;
	case SOF_IPC_STREAM_TRIG_STOP:
		cmd = COMP_TRIGGER_STOP;
		break;
	case SOF_IPC_STREAM_TRIG_PAUSE:
		cmd = COMP_TRIGGER_PAUSE;
		break;
	case SOF_IPC_STREAM_TRIG_RELEASE:
		cmd = COMP_TRIGGER_PRE_RELEASE;
		break;
	/* XRUN is special case- TODO */
	case SOF_IPC_STREAM_TRIG_XRUN:
		return 0;
	default:
		ipc_cmd_err(&ipc_tr, "ipc: invalid trigger cmd 0x%x", ipc_command);
		return -ENODEV;
	}

	if (!pcm_dev->cd->pipeline) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d pipeline not found",
			    stream.comp_id);
		return -EINVAL;
	}

	if (pcm_dev->type != COMP_TYPE_COMPONENT) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d not stream (type %d)",
			    stream.comp_id, pcm_dev->type);
		return -EINVAL;
	}

	/*
	 * Trigger the component: timer domain pipelines offload some trigger
	 * operations in their pipeline tasks, in which case IPC response to
	 * the host can be delayed. DMA domain pipelines always trigger
	 * synchronously.
	 */
	if (pipeline_is_timer_driven(pcm_dev->cd->pipeline)) {
		k_spinlock_key_t key;

		key = k_spin_lock(&ipc->lock);
		ipc->task_mask |= IPC_TASK_IN_THREAD;
		k_spin_unlock(&ipc->lock, key);

		ret = pipeline_trigger(pcm_dev->cd->pipeline, pcm_dev->cd, cmd);
		if (ret <= 0) {
			key = k_spin_lock(&ipc->lock);
			ipc->task_mask &= ~IPC_TASK_IN_THREAD;
			k_spin_unlock(&ipc->lock, key);
		}
	} else {
		ret = pipeline_trigger_run(pcm_dev->cd->pipeline, pcm_dev->cd, cmd);
	}

	if (ret < 0)
		ipc_cmd_err(&ipc_tr, "ipc: comp %d trigger 0x%x failed %d",
			    stream.comp_id, ipc_command, ret);


	return ret;
}

static int ipc_glb_stream_message(uint32_t header)
{
	uint32_t cmd = iCS(header);

	switch (cmd) {
	case SOF_IPC_STREAM_PCM_PARAMS:
		return ipc_stream_pcm_params(header);
	case SOF_IPC_STREAM_PCM_FREE:
		return ipc_stream_pcm_free(header);
	case SOF_IPC_STREAM_TRIG_START:
	case SOF_IPC_STREAM_TRIG_STOP:
	case SOF_IPC_STREAM_TRIG_PAUSE:
	case SOF_IPC_STREAM_TRIG_RELEASE:
	case SOF_IPC_STREAM_TRIG_DRAIN:
	case SOF_IPC_STREAM_TRIG_XRUN:
		return ipc_stream_trigger(header);
	case SOF_IPC_STREAM_POSITION:
		return ipc_stream_position(header);
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown stream cmd 0x%x", cmd);
		return -EINVAL;
	}
}

/*
 * DAI IPC Operations.
 */

static void build_dai_config(struct sof_ipc_dai_config *config,
			     struct ipc_config_dai *config_dai)
{
	memset(config_dai, 0, sizeof(*config_dai));

	config_dai->dai_index = config->dai_index;
	config_dai->direction = -1;
	config_dai->format = config->format;
	config_dai->group_id = config->group_id;
	config_dai->type = config->type;
}

static int ipc_dai_config_set(struct sof_ipc_dai_config *config,
			      struct ipc_config_dai *config_dai)
{
	struct dai *dai;
	int ret;

	/* get DAI */
	dai = dai_get(config->type, config->dai_index, 0 /* existing only */);
	if (!dai) {
		ipc_cmd_err(&ipc_tr, "ipc: dai %d,%d not found", config->type,
			    config->dai_index);
		return -ENODEV;
	}

	/* configure DAI */
	ret = dai_set_config(dai, config_dai, config);
	dai_put(dai); /* free ref immediately */
	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: dai %d,%d config failed %d", config->type,
			    config->dai_index, ret);
		return ret;
	}

	return 0;
}

/*
 * DAI config occurs in several steps (and can be optimised)
 * 1) IPC arrived from host
 * 2) Primary core configures the DAI driver HW config via drv->set_config()
 * 3) Target core then calls comp->dai_config()
 * 4) Stream params IPC then calls dai_params() which calls
 *    ipc_dai_data_config() followed by dai_verify_params() to validate
 *    stream params with physical DAI HW config.
 */
static int ipc_msg_dai_config(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct ipc_config_dai config_dai;
	struct sof_ipc_dai_config config;
	int ret;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(config, ipc->comp_data);

	tr_info(&ipc_tr, "ipc: dai %d.%d -> config ", config.type,
		config.dai_index);

	/* set common configuration */
	build_dai_config(&config, &config_dai);

	/* only primary core configures dai */
	if (cpu_get_id() == PLATFORM_PRIMARY_CORE_ID) {
		ret = ipc_dai_config_set(&config, &config_dai);
		if (ret < 0)
			return ret;
	}

	/* send params to all DAI components who use that physical DAI */
	return ipc_comp_dai_config(ipc, &config_dai, ipc->comp_data);
}

static int ipc_glb_dai_message(uint32_t header)
{
	uint32_t cmd = iCS(header);

	switch (cmd) {
	case SOF_IPC_DAI_CONFIG:
		return ipc_msg_dai_config(header);
	case SOF_IPC_DAI_LOOPBACK:
		//return ipc_comp_set_value(header, COMP_CMD_LOOPBACK);
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown DAI cmd 0x%x", cmd);
		return -EINVAL;
	}
}

/*
 * PM IPC Operations.
 */

static int ipc_pm_context_size(uint32_t header)
{
	struct sof_ipc_pm_ctx pm_ctx;

	tr_info(&ipc_tr, "ipc: pm -> size");

	bzero(&pm_ctx, sizeof(pm_ctx));

	/* TODO: calculate the context and size of host buffers required */

	/* write the context to the host driver */
	//mailbox_hostbox_write(0, &pm_ctx, sizeof(pm_ctx));

	return 0;
}

static int ipc_pm_context_save(uint32_t header)
{
	//struct sof_ipc_pm_ctx *pm_ctx = _ipc->comp_data;

	tr_info(&ipc_tr, "ipc: pm -> save");

	sa_exit(sof_get());

	/* do platform specific suspending */
	platform_context_save(sof_get());

#if !defined(CONFIG_LIBRARY)
	/* TODO: check we are inactive - all streams are suspended */

	/* TODO: mask ALL platform interrupts except DMA */

	/* mask all DSP interrupts */
	arch_irq_lock();

	/* TODO: mask ALL platform interrupts inc DMA */

	/* TODO: clear any outstanding platform IRQs - TODO refine */

	/* TODO: stop ALL timers */
	platform_timer_stop(timer_get());

	/* TODO: disable SSP and DMA HW */

	/* TODO: save the context */
	//reply.entries_no = 0;

	/* write the context to the host driver */
	//mailbox_hostbox_write(0, pm_ctx, sizeof(*pm_ctx));
#endif
	ipc_get()->pm_prepare_D3 = 1;

	return 0;
}

static int ipc_pm_context_restore(uint32_t header)
{
	//struct sof_ipc_pm_ctx *pm_ctx = _ipc->comp_data;

	tr_info(&ipc_tr, "ipc: pm -> restore");

	ipc_get()->pm_prepare_D3 = 0;

	/* restore context placeholder */
	//mailbox_hostbox_write(0, pm_ctx, sizeof(*pm_ctx));

	return 0;
}

static int ipc_pm_core_enable(uint32_t header)
{
	struct sof_ipc_pm_core_config pm_core_config;
	int ret;
	int i = 0;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(pm_core_config, ipc_get()->comp_data);

	/* check if core enable mask is valid */
	if (pm_core_config.enable_mask > MASK(CONFIG_CORE_COUNT - 1, 0)) {
		ipc_cmd_err(&ipc_tr, "ipc: CONFIG_CORE_COUNT: %d < core enable mask: %d",
			    CONFIG_CORE_COUNT, pm_core_config.enable_mask);
		return -EINVAL;
	}

	tr_info(&ipc_tr, "ipc: pm core mask 0x%x -> enable",
		pm_core_config.enable_mask);

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		if (i != PLATFORM_PRIMARY_CORE_ID) {
			if (pm_core_config.enable_mask & (1 << i)) {
				ret = cpu_enable_core(i);
				if (ret < 0) {
					ipc_cmd_err(&ipc_tr, "Failed to enable core %d", i);
					return ret;
				}
			} else {
				cpu_disable_core(i);
			}
		}
	}

	return 0;
}

static int ipc_pm_gate(uint32_t header)
{
	struct sof_ipc_pm_gate pm_gate;

	IPC_COPY_CMD(pm_gate, ipc_get()->comp_data);

	tr_info(&ipc_tr, "ipc: pm gate flags 0x%x", pm_gate.flags);

	/* pause dma trace firstly if needed */
	if (pm_gate.flags & SOF_PM_NO_TRACE)
		trace_off();

	if (pm_gate.flags & SOF_PM_PPG) {
		pm_runtime_disable(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);
	} else {
		/* before we enable pm runtime and perform D0->D0ix flow
		 * (primary core powers off secondary cores in
		 * platform_pg_int_handler) we have to prepare all secondary
		 * cores data for powering off (disable interrupt, perform
		 * cache writeback).
		 */
		pm_runtime_enable(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);
	}

	/* resume dma trace if needed */
	if (!(pm_gate.flags & SOF_PM_NO_TRACE))
		trace_on();

	return 0;
}

static int ipc_glb_pm_message(uint32_t header)
{
	uint32_t cmd = iCS(header);

	switch (cmd) {
	case SOF_IPC_PM_CTX_SAVE:
		return ipc_pm_context_save(header);
	case SOF_IPC_PM_CTX_RESTORE:
		return ipc_pm_context_restore(header);
	case SOF_IPC_PM_CTX_SIZE:
		return ipc_pm_context_size(header);
	case SOF_IPC_PM_CORE_ENABLE:
		return ipc_pm_core_enable(header);
	case SOF_IPC_PM_GATE:
		return ipc_pm_gate(header);
	case SOF_IPC_PM_CLK_SET:
	case SOF_IPC_PM_CLK_GET:
	case SOF_IPC_PM_CLK_REQ:
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown pm cmd 0x%x", cmd);
		return -EINVAL;
	}
}

#if CONFIG_TRACE
/*
 * Debug IPC Operations.
 */
static void ipc_dma_trace_free(uint32_t header)
{
	struct dma_trace_data *dmat = dma_trace_data_get();

	dma_trace_disable(dmat);
}

static int ipc_dma_trace_config(uint32_t header)
{
#if CONFIG_HOST_PTABLE
	struct dma_sg_elem_array elem_array;
	uint32_t ring_size;
#endif
	struct dma_trace_data *dmat = dma_trace_data_get();
	struct ipc *ipc = ipc_get();
	struct sof_ipc_dma_trace_params_ext params;
	int err;

	if (!dmat) {
		mtrace_printf(LOG_LEVEL_ERROR,
			      "ipc_dma_trace_config failed: dmat not initialized");
		return -ENOMEM;
	}

	/* copy message with ABI safe method */
	IPC_COPY_CMD(params, ipc->comp_data);

	if (iCS(header) == SOF_IPC_TRACE_DMA_PARAMS_EXT)
		/* As version 5.12 Linux sends the monotonic
		 *  ktime_get(). Search for
		 *  "SOF_IPC_TRACE_DMA_PARAMS_EXT" in your particular
		 *  kernel version.
		 */
		dmat->time_delta = k_ns_to_cyc_near64(params.timestamp_ns) - sof_cycle_get_64();
	else
		dmat->time_delta = 0;

#if CONFIG_HOST_PTABLE
	err = ipc_process_host_buffer(ipc, &params.buffer,
				      SOF_IPC_STREAM_CAPTURE,
				      &elem_array,
				      &ring_size);
	if (err < 0)
		goto processing_error;

	err = dma_trace_host_buffer(dmat, &elem_array, ring_size);
	if (err < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: trace failed to set host buffers %d",
			    err);
		goto error;
	}
#else
	/* stream tag of capture stream for DMA trace */
	dmat->stream_tag = params.stream_tag;

	/* host buffer size for DMA trace */
	dmat->host_size = params.buffer.size;
#endif

	err = dma_trace_enable(dmat);
	if (err < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: failed to enable trace %d", err);
		goto error;
	}

	return 0;

error:
#if CONFIG_HOST_PTABLE
	dma_sg_free(&elem_array);

processing_error:
#endif

	return err;
}

static int ipc_trace_filter_update(uint32_t header)
{
	struct sof_ipc_trace_filter_elem *next_elem;
	struct sof_ipc_trace_filter_elem *elem;
	struct sof_ipc_trace_filter_elem *end;
	struct sof_ipc_trace_filter *packet;
	struct trace_filter filter;
	struct ipc *ipc = ipc_get();
	int ret = 0;
	int cnt;

	packet = ipc->comp_data;

	/* validation, packet->hdr.size has already been compared with SOF_IPC_MSG_MAX_SIZE */
	if (sizeof(*packet) + sizeof(*elem) * packet->elem_cnt != packet->hdr.size) {
		ipc_cmd_err(&ipc_tr, "trace_filter_update failed, elem_cnt %d is inconsistent with hdr.size %d",
			    packet->elem_cnt, packet->hdr.size);
		return -EINVAL;
	}

	tr_info(&ipc_tr, "ipc: trace_filter_update received, size %d elems",
		packet->elem_cnt);

	elem = packet->elems;
	end = &packet->elems[packet->elem_cnt];

	/* read each filter set and update selected components trace settings */
	while (elem != end) {
		next_elem = trace_filter_fill(elem, end, &filter);
		if (!next_elem)
			return -EINVAL;

		cnt = trace_filter_update(&filter);
		if (cnt < 0) {
			ipc_cmd_err(&ipc_tr, "trace_filter_update failed for UUID key 0x%X, comp %d.%d and log level %d",
				    filter.uuid_id, filter.pipe_id, filter.comp_id,
				    filter.log_level);
			ret = cnt;
		} else {
			tr_info(&ipc_tr, "trace_filter_update for UUID key 0x%X, comp %d.%d affected %d components",
				filter.uuid_id, filter.pipe_id, filter.comp_id,
				cnt);
		}

		elem = next_elem;
	}

	return ret;
}

static int ipc_glb_trace_message(uint32_t header)
{
	uint32_t cmd = iCS(header);

	tr_info(&ipc_tr, "ipc: debug cmd 0x%x", cmd);

	switch (cmd) {
	case SOF_IPC_TRACE_DMA_PARAMS:
	case SOF_IPC_TRACE_DMA_PARAMS_EXT:
		return ipc_dma_trace_config(header);
	case SOF_IPC_TRACE_DMA_FREE:
		ipc_dma_trace_free(header);
		return 0;
	case SOF_IPC_TRACE_FILTER_UPDATE:
		return ipc_trace_filter_update(header);
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown debug cmd 0x%x", cmd);
		return -EINVAL;
	}
}
#else
static int ipc_glb_trace_message(uint32_t header)
{
	/* Return success, as the protocol provides no way to inform
	 * the kernel that we don't support dtrace.  It will just see
	 * no output.
	 */
	return 0;
}
#endif

static int ipc_glb_gdb_debug(uint32_t header)
{
	/* no furher information needs to be extracted form header */
	(void) header;

#if CONFIG_GDB_DEBUG
	gdb_init_debug_exception();
	gdb_init();
	/* TODO: this asm should be in arch/include/debug/debug.h
	 * with a generic name and trigger debug exception
	 */
	asm volatile("_break 0, 0");
	return 0;
#else
	return -EINVAL;
#endif

}

#if CONFIG_PROBE
static inline int ipc_probe_init(uint32_t header)
{
	struct sof_ipc_probe_dma_add_params *params = ipc_get()->comp_data;
	int dma_provided = params->num_elems;

	tr_dbg(&ipc_tr, "ipc_probe_init()");

	if (dma_provided > 1 || dma_provided < 0) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_init(): Invalid amount of extraction DMAs specified = %d",
			    dma_provided);
		return -EINVAL;
	}

	return probe_init(dma_provided ? params->probe_dma : NULL);
}

static inline int ipc_probe_deinit(uint32_t header)
{
	tr_dbg(&ipc_tr, "ipc_probe_deinit()");

	return probe_deinit();
}

static inline int ipc_probe_dma_add(uint32_t header)
{
	struct sof_ipc_probe_dma_add_params *params = ipc_get()->comp_data;
	int dmas_count = params->num_elems;

	tr_dbg(&ipc_tr, "ipc_probe_dma_add()");

	if (dmas_count > CONFIG_PROBE_DMA_MAX) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_dma_add(): Invalid amount of injection DMAs specified = %d. Max is "
			    STRINGIFY(CONFIG_PROBE_DMA_MAX) ".",
			    dmas_count);
		return -EINVAL;
	}

	if (dmas_count <= 0) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_dma_add(): Inferred amount of incjection DMAs in payload is %d. This could indicate corrupt size reported in header or invalid IPC payload.",
			    dmas_count);
		return -EINVAL;
	}

	return probe_dma_add(dmas_count, params->probe_dma);
}

static inline int ipc_probe_dma_remove(uint32_t header)
{
	struct sof_ipc_probe_dma_remove_params *params = ipc_get()->comp_data;
	int tags_count = params->num_elems;

	tr_dbg(&ipc_tr, "ipc_probe_dma_remove()");

	if (tags_count > CONFIG_PROBE_DMA_MAX) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_dma_remove(): Invalid amount of injection DMAs specified = %d. Max is "
			    STRINGIFY(CONFIG_PROBE_DMA_MAX) ".",
			    tags_count);
		return -EINVAL;
	}

	if (tags_count <= 0) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_dma_remove(): Inferred amount of incjection DMAs in payload is %d. This could indicate corrupt size reported in header or invalid IPC payload.",
			    tags_count);
		return -EINVAL;
	}

	return probe_dma_remove(tags_count, params->stream_tag);
}

static inline int ipc_probe_point_add(uint32_t header)
{
	struct sof_ipc_probe_point_add_params *params = ipc_get()->comp_data;
	int probes_count = params->num_elems;

	tr_dbg(&ipc_tr, "ipc_probe_point_add()");

	if (probes_count > CONFIG_PROBE_POINTS_MAX) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_point_add(): Invalid amount of Probe Points specified = %d. Max is "
			    STRINGIFY(CONFIG_PROBE_POINT_MAX) ".",
			    probes_count);
		return -EINVAL;
	}

	if (probes_count <= 0) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_point_add(): Inferred amount of Probe Points in payload is %d. This could indicate corrupt size reported in header or invalid IPC payload.",
			    probes_count);
		return -EINVAL;
	}

	return probe_point_add(probes_count, params->probe_point);
}

static inline int ipc_probe_point_remove(uint32_t header)
{
	struct sof_ipc_probe_point_remove_params *params = ipc_get()->comp_data;
	int probes_count = params->num_elems;

	tr_dbg(&ipc_tr, "ipc_probe_point_remove()");

	if (probes_count > CONFIG_PROBE_POINTS_MAX) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_point_remove(): Invalid amount of Probe Points specified = %d. Max is "
			    STRINGIFY(CONFIG_PROBE_POINT_MAX) ".",
			    probes_count);
		return -EINVAL;
	}

	if (probes_count <= 0) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_point_remove(): Inferred amount of Probe Points in payload is %d. This could indicate corrupt size reported in header or invalid IPC payload.",
			    probes_count);
		return -EINVAL;
	}
	return probe_point_remove(probes_count, params->buffer_id);
}

static int ipc_probe_info(uint32_t header)
{
	uint32_t cmd = iCS(header);
	struct sof_ipc_probe_info_params *params = ipc_get()->comp_data;
	int ret;

	tr_dbg(&ipc_tr, "ipc_probe_get_data()");

	switch (cmd) {
	case SOF_IPC_PROBE_DMA_INFO:
		ret = probe_dma_info(params, SOF_IPC_MSG_MAX_SIZE);
		break;
	case SOF_IPC_PROBE_POINT_INFO:
		ret = probe_point_info(params, SOF_IPC_MSG_MAX_SIZE);
		break;
	default:
		ipc_cmd_err(&ipc_tr, "ipc_probe_info(): Invalid probe INFO command = %u",
			    cmd);
		ret = -EINVAL;
	}

	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "ipc_probe_info(): cmd %u failed", cmd);
		return ret;
	}

	/* write data to the outbox */
	if (params->rhdr.hdr.size <= MAILBOX_HOSTBOX_SIZE &&
	    params->rhdr.hdr.size <= SOF_IPC_MSG_MAX_SIZE) {
		params->rhdr.error = ret;
		mailbox_hostbox_write(0, params, params->rhdr.hdr.size);
		ret = 1;
	} else {
		ipc_cmd_err(&ipc_tr, "ipc_probe_get_data(): probes module returned too much payload for cmd %u - returned %d bytes, max %d",
			    cmd, params->rhdr.hdr.size,
			    MIN(MAILBOX_HOSTBOX_SIZE, SOF_IPC_MSG_MAX_SIZE));
		ret = -EINVAL;
	}

	return ret;
}

static int ipc_glb_probe(uint32_t header)
{
	uint32_t cmd = iCS(header);

	tr_dbg(&ipc_tr, "ipc: probe cmd 0x%x", cmd);

	switch (cmd) {
	case SOF_IPC_PROBE_INIT:
		return ipc_probe_init(header);
	case SOF_IPC_PROBE_DEINIT:
		return ipc_probe_deinit(header);
	case SOF_IPC_PROBE_DMA_ADD:
		return ipc_probe_dma_add(header);
	case SOF_IPC_PROBE_DMA_REMOVE:
		return ipc_probe_dma_remove(header);
	case SOF_IPC_PROBE_POINT_ADD:
		return ipc_probe_point_add(header);
	case SOF_IPC_PROBE_POINT_REMOVE:
		return ipc_probe_point_remove(header);
	case SOF_IPC_PROBE_DMA_INFO:
	case SOF_IPC_PROBE_POINT_INFO:
		return ipc_probe_info(header);
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown probe cmd 0x%x", cmd);
		return -EINVAL;
	}
}
#else
static inline int ipc_glb_probe(uint32_t header)
{
	ipc_cmd_err(&ipc_tr, "ipc_glb_probe(): Probes not enabled by Kconfig.");

	return -EINVAL;
}
#endif

/*
 * Topology IPC Operations.
 */

/* get/set component values or runtime data */
static int ipc_comp_value(uint32_t header, uint32_t cmd)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *comp_dev;
	struct sof_ipc_ctrl_data *data = ipc->comp_data;
	int ret;

	/* get the component */
	comp_dev = ipc_get_comp_by_id(ipc, data->comp_id);
	if (!comp_dev) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", data->comp_id);
		return -ENODEV;
	}

	/* check core */
	if (!cpu_is_me(comp_dev->core))
		return ipc_process_on_core(comp_dev->core, false);

	tr_dbg(&ipc_tr, "ipc: comp %d -> cmd %d", data->comp_id, data->cmd);

	/* get component values */
	ret = comp_cmd(comp_dev->cd, cmd, data, SOF_IPC_MSG_MAX_SIZE);
	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d cmd %u failed %d", data->comp_id,
			    data->cmd, ret);
		return ret;
	}

	/* write component values to the outbox */
	if (data->rhdr.hdr.size <= MAILBOX_HOSTBOX_SIZE &&
	    data->rhdr.hdr.size <= SOF_IPC_MSG_MAX_SIZE) {
		mailbox_hostbox_write(0, data, data->rhdr.hdr.size);
		ret = 1;
	} else {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d cmd %u returned %d bytes max %d",
			    data->comp_id, data->cmd, data->rhdr.hdr.size,
			    MIN(MAILBOX_HOSTBOX_SIZE, SOF_IPC_MSG_MAX_SIZE));
		ret = -EINVAL;
	}

	return ret;
}

static int ipc_glb_comp_message(uint32_t header)
{
	uint32_t cmd = iCS(header);

	switch (cmd) {
	case SOF_IPC_COMP_SET_VALUE:
		return ipc_comp_value(header, COMP_CMD_SET_VALUE);
	case SOF_IPC_COMP_GET_VALUE:
		return ipc_comp_value(header, COMP_CMD_GET_VALUE);
	case SOF_IPC_COMP_SET_DATA:
		return ipc_comp_value(header, COMP_CMD_SET_DATA);
	case SOF_IPC_COMP_GET_DATA:
		return ipc_comp_value(header, COMP_CMD_GET_DATA);
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown comp cmd 0x%x", cmd);
		return -EINVAL;
	}
}

static int ipc_glb_tplg_comp_new(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_comp *comp = ipc->comp_data;
	struct sof_ipc_comp_reply reply = {
		.rhdr.hdr = {
			.cmd = header,
			.size = sizeof(reply),
		},
	};
	int ret;

	/* check core */
	if (!cpu_is_me(comp->core))
		return ipc_process_on_core(comp->core, false);

	tr_dbg(&ipc_tr, "ipc: pipe %d comp %d -> new (type %d)",
	       comp->pipeline_id, comp->id, comp->type);

	/* register component */
	ret = ipc_comp_new(ipc, ipc_to_comp_new(comp));
	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d comp %d creation failed %d",
			    comp->pipeline_id, comp->id, ret);
		return ret;
	}

	/* write component values to the outbox */
	mailbox_hostbox_write(0, &reply, sizeof(reply));

	return 1;
}

static int ipc_glb_tplg_buffer_new(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_buffer ipc_buffer;
	struct sof_ipc_comp_reply reply = {
		.rhdr.hdr = {
			.cmd = header,
			.size = sizeof(reply),
		},
	};
	int ret;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(ipc_buffer, ipc->comp_data);

	/* check core */
	if (!cpu_is_me(ipc_buffer.comp.core))
		return ipc_process_on_core(ipc_buffer.comp.core, false);

	tr_dbg(&ipc_tr, "ipc: pipe %d buffer %d -> new (0x%x bytes)",
	       ipc_buffer.comp.pipeline_id, ipc_buffer.comp.id,
	       ipc_buffer.size);

	ret = ipc_buffer_new(ipc, (struct sof_ipc_buffer *)ipc->comp_data);
	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d buffer %d creation failed %d",
			    ipc_buffer.comp.pipeline_id,
			    ipc_buffer.comp.id, ret);
		return ret;
	}

	/* write component values to the outbox */
	mailbox_hostbox_write(0, &reply, sizeof(reply));

	return 1;
}

static int ipc_glb_tplg_pipe_new(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_pipe_new ipc_pipeline;
	struct sof_ipc_comp_reply reply = {
		.rhdr.hdr = {
			.cmd = header,
			.size = sizeof(reply),
		},
	};
	int ret;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(ipc_pipeline, ipc->comp_data);

	/* check core */
	if (!cpu_is_me(ipc_pipeline.core))
		return ipc_process_on_core(ipc_pipeline.core, false);

	tr_dbg(&ipc_tr, "ipc: pipe %d -> new", ipc_pipeline.pipeline_id);

	ret = ipc_pipeline_new(ipc, ipc->comp_data);
	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d creation failed %d",
			    ipc_pipeline.pipeline_id, ret);
		return ret;
	}

	/* write component values to the outbox */
	mailbox_hostbox_write(0, &reply, sizeof(reply));

	return 1;
}

static int ipc_glb_tplg_pipe_complete(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_pipe_ready ipc_pipeline;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(ipc_pipeline, ipc->comp_data);

	return ipc_pipeline_complete(ipc, ipc_pipeline.comp_id);
}

static int ipc_glb_tplg_comp_connect(uint32_t header)
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_pipe_comp_connect connect;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(connect, ipc->comp_data);

	return ipc_comp_connect(ipc, ipc->comp_data);
}

static int ipc_glb_tplg_free(uint32_t header,
		int (*free_func)(struct ipc *ipc, uint32_t id))
{
	struct ipc *ipc = ipc_get();
	struct sof_ipc_free ipc_free_msg;
	int ret;

	/* copy message with ABI safe method */
	IPC_COPY_CMD(ipc_free_msg, ipc->comp_data);

	tr_info(&ipc_tr, "ipc: comp %d -> free", ipc_free_msg.id);

	/* free the object */
	ret = free_func(ipc, ipc_free_msg.id);

	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d free failed %d",
			    ipc_free_msg.id, ret);
	}

	return ret;
}

static int ipc_glb_tplg_message(uint32_t header)
{
	uint32_t cmd = iCS(header);

	switch (cmd) {
	case SOF_IPC_TPLG_COMP_NEW:
		return ipc_glb_tplg_comp_new(header);
	case SOF_IPC_TPLG_COMP_FREE:
		return ipc_glb_tplg_free(header, ipc_comp_free);
	case SOF_IPC_TPLG_COMP_CONNECT:
		return ipc_glb_tplg_comp_connect(header);
	case SOF_IPC_TPLG_PIPE_NEW:
		return ipc_glb_tplg_pipe_new(header);
	case SOF_IPC_TPLG_PIPE_COMPLETE:
		return ipc_glb_tplg_pipe_complete(header);
	case SOF_IPC_TPLG_PIPE_FREE:
		return ipc_glb_tplg_free(header, ipc_pipeline_free);
	case SOF_IPC_TPLG_BUFFER_NEW:
		return ipc_glb_tplg_buffer_new(header);
	case SOF_IPC_TPLG_BUFFER_FREE:
		return ipc_glb_tplg_free(header, ipc_buffer_free);
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown tplg header 0x%x", header);
		return -EINVAL;
	}
}

#if CONFIG_DEBUG_MEMORY_USAGE_SCAN
static int fill_mem_usage_elems(enum mem_zone zone, enum sof_ipc_dbg_mem_zone ipc_zone,
				int elem_number, struct sof_ipc_dbg_mem_usage_elem *elems)
{
	struct mm_info info;
	int ret;
	int i;

	for (i = 0; i < elem_number; ++i) {
		ret = heap_info(zone, i, &info);
		elems[i].zone = ipc_zone;
		elems[i].id = i;
		elems[i].used = ret < 0 ? UINT32_MAX : info.used;
		elems[i].free = ret < 0 ? 0 : info.free;
	}

	return elem_number;
}

#if CONFIG_CORE_COUNT > 1
#define PLATFORM_HEAP_SYSTEM_SHARED_CNT (PLATFORM_HEAP_SYSTEM_SHARED + PLATFORM_HEAP_RUNTIME_SHARED)
#else
#define PLATFORM_HEAP_SYSTEM_SHARED_CNT 0
#endif

static int ipc_glb_test_mem_usage(uint32_t header)
{
	/* count number heaps */
	int elem_cnt = PLATFORM_HEAP_SYSTEM + PLATFORM_HEAP_SYSTEM_RUNTIME +
		       PLATFORM_HEAP_RUNTIME + PLATFORM_HEAP_BUFFER +
		       PLATFORM_HEAP_SYSTEM_SHARED_CNT;
	size_t size = sizeof(struct sof_ipc_dbg_mem_usage) +
		      elem_cnt * sizeof(struct sof_ipc_dbg_mem_usage_elem);
	struct sof_ipc_dbg_mem_usage_elem *elems;
	struct sof_ipc_dbg_mem_usage *mem_usage;

	mem_usage = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, 0, size);
	if (!mem_usage)
		return -ENOMEM;

	mem_usage->rhdr.hdr.cmd = header;
	mem_usage->rhdr.hdr.size = size;
	mem_usage->num_elems = elem_cnt;

	/* fill list of elems */
	elems = mem_usage->elems;
	elems += fill_mem_usage_elems(SOF_MEM_ZONE_SYS, SOF_IPC_MEM_ZONE_SYS,
				      PLATFORM_HEAP_SYSTEM, elems);
	elems += fill_mem_usage_elems(SOF_MEM_ZONE_SYS_RUNTIME, SOF_IPC_MEM_ZONE_SYS_RUNTIME,
				      PLATFORM_HEAP_SYSTEM_RUNTIME, elems);
	elems += fill_mem_usage_elems(SOF_MEM_ZONE_RUNTIME, SOF_IPC_MEM_ZONE_RUNTIME,
				      PLATFORM_HEAP_RUNTIME, elems);
	/* cppcheck-suppress unreadVariable */
	elems += fill_mem_usage_elems(SOF_MEM_ZONE_BUFFER, SOF_IPC_MEM_ZONE_BUFFER,
				      PLATFORM_HEAP_BUFFER, elems);
#if CONFIG_CORE_COUNT > 1
	elems += fill_mem_usage_elems(SOF_MEM_ZONE_SYS_SHARED, SOF_IPC_MEM_ZONE_SYS_SHARED,
				      PLATFORM_HEAP_SYSTEM_SHARED, elems);
	elems += fill_mem_usage_elems(SOF_MEM_ZONE_RUNTIME_SHARED, SOF_IPC_MEM_ZONE_RUNTIME_SHARED,
				      PLATFORM_HEAP_RUNTIME_SHARED, elems);
#endif

	/* write component values to the outbox */
	mailbox_hostbox_write(0, mem_usage, mem_usage->rhdr.hdr.size);

	rfree(mem_usage);
	return 1;
}
#endif

static int ipc_glb_debug_message(uint32_t header)
{
	uint32_t cmd = iCS(header);

	switch (cmd) {
#if CONFIG_DEBUG_MEMORY_USAGE_SCAN
	case SOF_IPC_DEBUG_MEM_USAGE:
		return ipc_glb_test_mem_usage(header);
#endif
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown debug header 0x%x", header);
		return -EINVAL;
	}
}

#if CONFIG_DEBUG
static int ipc_glb_test_message(uint32_t header)
{
	uint32_t cmd = iCS(header);

	switch (cmd) {
	case SOF_IPC_TEST_IPC_FLOOD:
		return 0; /* just return so next IPC can be sent */
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown test header 0x%x", header);
		return -EINVAL;
	}
}
#endif

#if CONFIG_CAVS
static struct ipc_cmd_hdr *ipc_cavs_read_set_d0ix(struct ipc_cmd_hdr *hdr)
{
	struct sof_ipc_pm_gate *cmd = ipc_get()->comp_data;
	uint32_t *chdr = (uint32_t *)hdr;

	cmd->hdr.cmd = SOF_IPC_GLB_PM_MSG | SOF_IPC_PM_GATE;
	cmd->hdr.size = sizeof(*cmd);
	cmd->flags = chdr[1] & CAVS_IPC_MOD_SETD0IX_BIT_MASK;

	return ipc_to_hdr(&cmd->hdr);
}

/*
 * Read a compact IPC message or return NULL for normal message.
 */
struct ipc_cmd_hdr *ipc_compact_read_msg(void)
{
	uint32_t chdr[2];
	struct ipc_cmd_hdr *hdr = (struct ipc_cmd_hdr *)chdr;
	int words;

	words = ipc_platform_compact_read_msg(hdr, 2);
	if (!words)
		return mailbox_validate();

	/* if there is no cAVS module IPC in regs go the previous path */
	if (!(chdr[0] & CAVS_IPC_MSG_TGT))
		return mailbox_validate();

	switch (CAVS_IPC_TYPE_S(chdr[0])) {
	case CAVS_IPC_MOD_SET_D0IX:
		return ipc_cavs_read_set_d0ix(hdr);
	}

	return NULL;
}
#endif

/* prepare the message using ABI major layout */
struct ipc_cmd_hdr *ipc_prepare_to_send(const struct ipc_msg *msg)
{
	static uint32_t hdr[2];

	hdr[0] = msg->header;
	hdr[1] = 0;

	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);

	return ipc_to_hdr(hdr);
}

void ipc_boot_complete_msg(struct ipc_cmd_hdr *header, uint32_t data)
{
	header->dat[0] = SOF_IPC_FW_READY;
	header->dat[1] = data;
}

void ipc_send_panic_notification(void)
{
	/* nothing to do */
}

static int ipc_fw_ready(void)
{
#ifdef CONFIG_IMX93_A55
	/* VERY IMPORTANT:
	 *	* due to how the FW is started, i.MX93 has the
	 *	following flow (please note that the host driver
	 *	is blacklisted and inserted later on):
	 *
	 *	1) Linux kernel boots and user space becomes
	 *	available.
	 *	2) FW is started (through Jailhouse) => SOF is running
	 *	4) Host driver module is inserted.
	 *	5) Host platform driver sends SOF_IPC_FW_READY and
	 *	expects SOF to send the SOF_IPC_FW_READY
	 *	message, the window regions and the reply
	 *	header in the following order:
	 *		1) reply structure
	 *		2) sof_ipc_fw_ready structure
	 *		3) windows structure
	 *	(all of the above information is written
	 *	contiguously in the hostbox)
	 */
	return platform_boot_complete(0);
#else
	/* any other platform should not receive SOF_IPC_FW_READY from host */
	return -EINVAL;
#endif /* CONFIG_IMX93_A55 */
}

/*
 * Global IPC Operations.
 */

void ipc_cmd(struct ipc_cmd_hdr *_hdr)
{
	struct sof_ipc_cmd_hdr *hdr = ipc_from_hdr(_hdr);
	struct ipc *ipc = ipc_get();
	struct sof_ipc_reply reply;
	uint32_t type = 0;
	int ret;

	if (!hdr) {
		ipc_cmd_err(&ipc_tr, "ipc: invalid IPC header.");
		ret = -EINVAL;
		goto out;
	}

	if (cpu_is_primary(cpu_get_id())) {
		/* A new IPC from the host, delivered to the primary core */
		ipc->core = PLATFORM_PRIMARY_CORE_ID;
		tr_info(&ipc_tr, "ipc: new cmd 0x%x", hdr->cmd);
	}

	type = iGS(hdr->cmd);

	switch (type) {
	case SOF_IPC_GLB_REPLY:
		ret = 0;
		break;
	case SOF_IPC_GLB_COMPOUND:
		ret = -EINVAL;	/* TODO */
		break;
	case SOF_IPC_GLB_TPLG_MSG:
		ret = ipc_glb_tplg_message(hdr->cmd);
		break;
	case SOF_IPC_GLB_PM_MSG:
		ret = ipc_glb_pm_message(hdr->cmd);
		break;
	case SOF_IPC_GLB_COMP_MSG:
		ret = ipc_glb_comp_message(hdr->cmd);
		break;
	case SOF_IPC_GLB_STREAM_MSG:
		ret = ipc_glb_stream_message(hdr->cmd);
		break;
	case SOF_IPC_GLB_DAI_MSG:
		ret = ipc_glb_dai_message(hdr->cmd);
		break;
	case SOF_IPC_GLB_TRACE_MSG:
		ret = ipc_glb_trace_message(hdr->cmd);
		break;
	case SOF_IPC_GLB_GDB_DEBUG:
		ret = ipc_glb_gdb_debug(hdr->cmd);
		break;
	case SOF_IPC_GLB_PROBE:
		ret = ipc_glb_probe(hdr->cmd);
		break;
	case SOF_IPC_GLB_DEBUG:
		ret = ipc_glb_debug_message(hdr->cmd);
		break;
	case SOF_IPC_FW_READY:
		ret = ipc_fw_ready();
		break;
#if CONFIG_DEBUG
	case SOF_IPC_GLB_TEST:
		ret = ipc_glb_test_message(hdr->cmd);
		break;
#endif
	default:
		ipc_cmd_err(&ipc_tr, "ipc: unknown command type %u", type);
		ret = -EINVAL;
		break;
	}

out:
	tr_dbg(&ipc_tr, "ipc: last request 0x%x returned %d", type, ret);

	/* if ret > 0, reply created and copied by cmd() */
	if (ret <= 0) {
		/* send std error/ok reply */
		reply.error = ret;

		reply.hdr.cmd = SOF_IPC_GLB_REPLY;
		reply.hdr.size = sizeof(reply);
		mailbox_hostbox_write(0, &reply, sizeof(reply));
	}
}
