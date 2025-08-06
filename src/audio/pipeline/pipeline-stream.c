// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/lib/dai.h>
#include <rtos/wait.h>
#include <sof/list.h>
#include <rtos/interrupt.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <rtos/clk.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/module.h>
#include <rtos/kernel.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/ipc/notification_pool.h>

#ifdef CONFIG_IPC_MAJOR_4
#include <ipc4/notification.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sof/audio/module_adapter/module/generic.h>

#include "../audio/copier/copier.h"

LOG_MODULE_DECLARE(pipe, CONFIG_SOF_LOG_LEVEL);

/*
 * Check whether pipeline is incapable of acquiring data for capture.
 *
 * If capture START/RELEASE trigger originated on dailess pipeline and reached
 * inactive pipeline as it's source, then we indicate that it's blocked.
 *
 * @param rsrc - component from remote pipeline serving as source to relevant
 *		 pipeline
 * @param ctx - trigger walk context
 * @param dir - trigger direction
 */
static inline bool
pipeline_should_report_enodata_on_trigger(struct comp_dev *rsrc,
					  struct pipeline_walk_context *ctx,
					  int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	struct comp_dev *pipe_source = ppl_data->start->pipeline->source_comp;

	/* In IPC3, FW propagates triggers to connected pipelines, so
	 * it can have determistic logic to conclude no data is
	 * available.
	 * In IPC4, host controls state of each pipeline separately,
	 * so FW cannot reliably detect case of no data based on
	 * observing state of src->pipeline here.
	 */
#if CONFIG_IPC_MAJOR_4
	return false;
#endif

	/* only applies to capture pipelines */
	if (dir != SOF_IPC_STREAM_CAPTURE)
		return false;

	/* only applicable on trigger start/release */
	if (ppl_data->cmd != COMP_TRIGGER_START &&
	    ppl_data->cmd != COMP_TRIGGER_RELEASE)
		return false;

	/* only applies for dailess pipelines */
	if (pipe_source && dev_comp_type(pipe_source) == SOF_COMP_DAI)
		return false;

	/* source pipeline may not be active since priority is not higher than current one */
	if (rsrc->pipeline->priority <= ppl_data->start->pipeline->priority)
		return false;

	/* if component on which we depend to provide data is inactive, then the
	 * pipeline has no means of providing data
	 */
	if (rsrc->state != COMP_STATE_ACTIVE)
		return true;

	return false;
}

void pipeline_comp_copy_error_notify(const struct comp_dev *component, int err)
{
#ifdef CONFIG_IPC_MAJOR_4
	struct ipc_msg *notify;

	notify = ipc_notification_pool_get(IPC4_RESOURCE_EVENT_SIZE);
	if (!notify)
		return;

	process_data_error_notif_msg_init(notify, component->ipc_config.id, err);
	ipc_msg_send(notify, notify->tx_data, false);
#endif
}

static int pipeline_comp_copy(struct comp_dev *current,
			      struct comp_buffer *calling_buf,
			      struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	bool is_single_ppl = comp_is_single_pipeline(current, ppl_data->start);
	int err;

	pipe_dbg(current->pipeline, "pipeline_comp_copy(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	if (!is_single_ppl) {
		pipe_dbg(current->pipeline,
			 "pipeline_comp_copy(), current is from another pipeline and can't be scheduled together");
		return 0;
	}

	if (!comp_is_active(current)) {
		pipe_dbg(current->pipeline, "pipeline_comp_copy(), current is not active");
		return 0;
	}

	/* copy to downstream immediately */
	if (dir == PPL_DIR_DOWNSTREAM) {
		err = comp_copy(current);
		if (err < 0) {
			pipeline_comp_copy_error_notify(current, err);
			return err;
		}
		if (err == PPL_STATUS_PATH_STOP)
			return err;
	}

	err = pipeline_for_each_comp(current, ctx, dir);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	if (dir == PPL_DIR_UPSTREAM) {
		err = comp_copy(current);
		if (err < 0)
			pipeline_comp_copy_error_notify(current, err);
	}

	return err;
}

/* Copy data across all pipeline components.
 * For capture pipelines it always starts from source component
 * and continues downstream and for playback pipelines it first
 * copies sink component itself and then goes upstream.
 */
int pipeline_copy(struct pipeline *p)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_copy,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	struct comp_dev *start;
	uint32_t dir;
	int ret;

	if (p->source_comp->direction == SOF_IPC_STREAM_PLAYBACK) {
		dir = PPL_DIR_UPSTREAM;
		start = p->sink_comp;
	} else {
		dir = PPL_DIR_DOWNSTREAM;
		start = p->source_comp;
	}

	data.start = start;
	data.p = p;

	ret = walk_ctx.comp_func(start, NULL, &walk_ctx, dir);
	if (ret < 0)
		pipe_err(p, "ret = %d, start->comp.id = %u, dir = %u",
			 ret, dev_comp_id(start), dir);

	return ret;
}

#if CONFIG_LIBRARY && !CONFIG_LIBRARY_STATIC
/* trigger pipeline immediately in IPC context. TODO: Add support for XRUN */
int pipeline_trigger(struct pipeline *p, struct comp_dev *host, int cmd)
{
	int ret;

	pipe_info(p, "pipe trigger cmd %d", cmd);

	p->trigger.aborted = false;

	ret = pipeline_trigger_run(p, host, cmd);
	if (ret < 0)
		return ret;

	switch (cmd) {
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_PRE_RELEASE:
		p->status = COMP_STATE_ACTIVE;
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_PAUSE:
		p->status = COMP_STATE_PAUSED;
		break;
	default:
		break;
	}

	return  0;
}

#else /* CONFIG_LIBRARY */

/* only collect scheduling components */
static int pipeline_comp_list(struct comp_dev *current,
			      struct comp_buffer *calling_buf,
			      struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	bool is_single_ppl = comp_is_single_pipeline(current, ppl_data->start);
	bool is_same_sched = pipeline_is_same_sched_comp(current->pipeline,
							 ppl_data->start->pipeline);

	/*
	 * We walk connected pipelines only if they have the same scheduling
	 * component and we aren't using IPC4. With IPC4 each pipeline receives
	 * commands separately so we don't need to trigger them together
	 */
	if (!is_single_ppl && (!is_same_sched || IPC4_MOD_ID(current->ipc_config.id))) {
		pipe_dbg(current->pipeline,
			 "pipeline_comp_list(), current is from another pipeline");
		return 0;
	}

	/* Add scheduling components to the list */
	pipeline_comp_trigger_sched_comp(current->pipeline, current, ctx);

	return pipeline_for_each_comp(current, ctx, dir);
}

/* build a list of connected pipelines' scheduling components and trigger them */
static int pipeline_trigger_list(struct pipeline *p, struct comp_dev *host, int cmd)
{
	struct pipeline_data data = {
		.start = host,
		.cmd = cmd,
	};
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_list,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	int ret;

	list_init(&walk_ctx.pipelines);

	ret = walk_ctx.comp_func(host, NULL, &walk_ctx, host->direction);
	if (ret < 0) {
		pipe_err(p, "ret = %d, host->comp.id = %u, cmd = %d",
			 ret, dev_comp_id(host), cmd);
	} else {
		if (cmd == COMP_TRIGGER_PRE_START) {
			struct list_item *list;
			struct pipeline *current, *upstream = NULL;

			/* Make sure the first pipeline has the highest priority */
			list_for_item(list, &walk_ctx.pipelines) {
				current = list_item(list, struct pipeline, list);

				if (current->sched_comp->direction == SOF_IPC_STREAM_PLAYBACK) {
					current->sched_prev = upstream;
					if (upstream)
						upstream->sched_next = current;
				} else {
					current->sched_next = upstream;
					if (upstream)
						upstream->sched_prev = current;
				}

				upstream = current;
			}
		}
		pipeline_schedule_triggered(&walk_ctx, cmd);
	}

	return ret;
}

static void pipeline_trigger_xrun(struct pipeline *p, struct comp_dev **host)
{
	/*
	 * XRUN can happen on a pipeline, not directly attached to the host,
	 * find the original one
	 */
	do {
		/* Check the opposite direction */
		int dir = (*host)->direction == PPL_DIR_DOWNSTREAM ? PPL_DIR_UPSTREAM :
			PPL_DIR_DOWNSTREAM;
		struct list_item *buffer_list = comp_buffer_list(*host, dir);
		struct list_item *clist;
		bool found = false;

		if (list_is_empty(buffer_list))
			/* Reached the original host */
			break;

		list_for_item(clist, buffer_list) {
			struct comp_buffer *buffer = buffer_from_list(clist, dir);
			struct comp_dev *buffer_comp = buffer_get_comp(buffer, dir);

			switch (buffer_comp->pipeline->status) {
			case COMP_STATE_ACTIVE:
			case COMP_STATE_PREPARE:
				found = true;
				break;
			}

			if (found) {
				*host = (*host)->direction == PPL_DIR_DOWNSTREAM ?
					buffer_comp->pipeline->source_comp :
					buffer_comp->pipeline->sink_comp;
				break;
			}
		}

		if (!found) {
			/* No active pipeline found! Should never occur. */
			pipe_err(p, "No active pipeline found to link to pipeline %u!",
				 (*host)->pipeline->pipeline_id);
			break;
		}
	} while (true);
}

#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
static struct ipc4_base_module_cfg *ipc4_get_base_cfg(struct comp_dev *comp)
{
	if (comp->drv->type != SOF_COMP_MODULE_ADAPTER)
		return comp_get_drvdata(comp);

	struct processing_module *mod = comp_mod(comp);
	struct module_data *md = &mod->priv;

	return &md->cfg.base_cfg;
}

static void pipeline_cps_rebalance(struct pipeline *p, bool starting)
{
	unsigned int core_kcps[CONFIG_CORE_COUNT];
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	const unsigned int clk_max_khz = CLK_MAX_CPU_HZ / 1000;

	for (unsigned int i = 0; i < CONFIG_CORE_COUNT; i++)
		core_kcps[i] = i == PLATFORM_PRIMARY_CORE_ID ? PRIMARY_CORE_BASE_CPS_USAGE :
			SECONDARY_CORE_BASE_CPS_USAGE;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		struct comp_dev *comp = icd->cd;

		/*
		 * When a pipeline is started, its components have state PREPARE, when
		 * a pipeline is terminated, its components still have state ACTIVE
		 */
		if ((comp->state == COMP_STATE_ACTIVE &&
		     (starting || comp->pipeline != p)) ||
		    ((comp->state == COMP_STATE_PREPARE || comp->state == COMP_STATE_PAUSED) &&
		     starting && comp->pipeline == p)) {
			struct ipc4_base_module_cfg *cd = ipc4_get_base_cfg(comp);

			if (cd->cpc && core_kcps[icd->core] < clk_max_khz)
				core_kcps[icd->core] += cd->cpc;
			else
				core_kcps[icd->core] = clk_max_khz;
		}
	}

	for (int i = 0; i < arch_num_cpus(); i++) {
		int delta_kcps = core_kcps[i] - core_kcps_get(i);

		tr_dbg(pipe, "Proposed KCPS consumption: %d, core: %d, delta: %d",
		       core_kcps[i], i, delta_kcps);
		if (delta_kcps)
			core_kcps_adjust(i, delta_kcps);
	}
}
#endif /* CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL */

/* trigger pipeline in IPC context */
int pipeline_trigger(struct pipeline *p, struct comp_dev *host, int cmd)
{
	int ret;
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	bool trigger_first = false;
	uint32_t flags = 0;
#endif
	pipe_info(p, "pipe trigger cmd %d", cmd);

	p->trigger.aborted = false;

	switch (cmd) {
	case COMP_TRIGGER_PAUSE:
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
		trigger_first = true;
		COMPILER_FALLTHROUGH;
#endif
	case COMP_TRIGGER_STOP:
		if (p->status == COMP_STATE_PAUSED || p->xrun_bytes) {
			/* The task isn't running, trigger inline */
			ret = pipeline_trigger_run(p, host, cmd);
			return ret < 0 ? ret : 0;
		}

		COMPILER_FALLTHROUGH;
	case COMP_TRIGGER_XRUN:
		if (cmd == COMP_TRIGGER_XRUN)
			pipeline_trigger_xrun(p, &host);

		COMPILER_FALLTHROUGH;
	case COMP_TRIGGER_PRE_RELEASE:
	case COMP_TRIGGER_PRE_START:
		/* Add all connected pipelines to the list and trigger them all */
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
		flags = irq_lock();
		/* setup walking ctx for removing consumption */
		if (!trigger_first)
			pipeline_cps_rebalance(p, true);
#endif
		ret = pipeline_trigger_list(p, host, cmd);
		if (ret < 0) {
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
			irq_unlock(flags);
#endif
			return ret;
		}
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
		if (trigger_first)
			pipeline_cps_rebalance(p, false);
		irq_unlock(flags);
#endif
		/* IPC response will be sent from the task, unless it was paused */
		return PPL_STATUS_SCHEDULED;
	}

	return 0;
}
#endif /* CONFIG_LIBRARY */

/* Runs in IPC or in pipeline task context */
static int pipeline_comp_trigger(struct comp_dev *current,
				 struct comp_buffer *calling_buf,
				 struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	bool is_single_ppl = comp_is_single_pipeline(current, ppl_data->start);
	bool is_same_sched;
	int err;

	pipe_dbg(current->pipeline,
		 "pipeline_comp_trigger(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	switch (ppl_data->cmd) {
	case COMP_TRIGGER_PRE_RELEASE:
	case COMP_TRIGGER_PRE_START:
		if (comp_get_endpoint_type(current) == COMP_ENDPOINT_DAI) {
			/*
			 * Initialization delay is only used with SSP, where we
			 * don't use more than one DAI per copier
			 */
			struct dai_data *dd;
#if CONFIG_IPC_MAJOR_3
			dd = comp_get_drvdata(current);
#elif CONFIG_IPC_MAJOR_4
			struct processing_module *mod = comp_mod(current);
			struct copier_data *cd = module_get_private_data(mod);

			dd = cd->dd[0];
#else
#error Unknown IPC major version
#endif
			ppl_data->delay_ms = dai_get_init_delay_ms(dd->dai);
		}
		break;
	default:
		return -EINVAL;
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_RELEASE:
	case COMP_TRIGGER_START:
		break;
	}

	is_same_sched = pipeline_is_same_sched_comp(current->pipeline,
						    ppl_data->start->pipeline);

	/* trigger should propagate to the connected pipelines,
	 * which need to be scheduled together
	 *
	 * IPC4 has a SET_PIPELINE_STATE for each pipeline, so FW
	 * should not propagate triggers on its own.
	 * IPC3 has commands only for graph edges, so propagation is
	 * needed in many cases.
	 */
	if (!is_single_ppl && (!is_same_sched || IS_ENABLED(CONFIG_IPC_MAJOR_4))) {
		pipe_dbg(current->pipeline,
			 "pipeline_comp_trigger(), current is from another pipeline");

		if (pipeline_should_report_enodata_on_trigger(current, ctx, dir))
			return -ENODATA;

		return 0;
	}

	current->pipeline->trigger.pending = false;

	/* send command to the component and update pipeline state */
	err = comp_trigger(current, ppl_data->cmd);
	switch (err) {
	case 0:
		break;
	case PPL_STATUS_PATH_STOP:
		current->pipeline->trigger.aborted = true;
		COMPILER_FALLTHROUGH;
	case PPL_STATUS_PATH_TERMINATE:
		return PPL_STATUS_PATH_STOP;
	default:
		return err;
	}

	switch (ppl_data->cmd) {
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
		if (pipeline_is_timer_driven(current->pipeline))
			current->pipeline->status = COMP_STATE_PAUSED;
	}

	/*
	 * Add scheduling components to the list. This is only needed for
	 * asynchronous flows.
	 */
	if (!pipeline_is_timer_driven(current->pipeline))
		pipeline_comp_trigger_sched_comp(current->pipeline, current, ctx);

	return pipeline_for_each_comp(current, ctx, dir);
}

/* actually execute pipeline trigger, including components: either in IPC or in task context */
int pipeline_trigger_run(struct pipeline *p, struct comp_dev *host, int cmd)
{
	struct pipeline_data data = {
		.start = host,
		.cmd = cmd,
	};
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_trigger,
		.comp_data = &data,
		.skip_incomplete = true,
	};
	int ret;

	pipe_dbg(p, "execute trigger cmd %d on pipe %u", cmd, p->pipeline_id);

	list_init(&walk_ctx.pipelines);
	p->trigger.aborted = false;

	/* handle pipeline global checks before going into each components */
	if (p->xrun_bytes) {
		ret = pipeline_xrun_handle_trigger(p, cmd);
		if (ret < 0) {
			pipe_err(p, "xrun handle: ret = %d", ret);
			return ret;
		}

		if (ret == PPL_STATUS_PATH_STOP)
			/* no further action needed*/
			return pipeline_is_timer_driven(p);
	}

	ret = walk_ctx.comp_func(host, NULL, &walk_ctx, host->direction);
	if (ret < 0) {
		pipe_err(p, "ret = %d, host->comp.id = %u, cmd = %d",
			 ret, dev_comp_id(host), cmd);
		goto out;
	}

	switch (cmd) {
	case COMP_TRIGGER_PRE_START:
		data.cmd = COMP_TRIGGER_START;
		break;
	case COMP_TRIGGER_PRE_RELEASE:
		data.cmd = COMP_TRIGGER_RELEASE;
	}

	if (data.cmd != cmd) {
		if (data.delay_ms && pipeline_is_timer_driven(p)) {
			/* The task will skip .delay periods before processing the next command */
			p->trigger.delay = (data.delay_ms * 1000 + p->period - 1) / p->period;
			p->trigger.cmd = data.cmd;

			return 0;
		}

		list_init(&walk_ctx.pipelines);

		if (data.delay_ms)
			k_msleep(data.delay_ms);

		ret = walk_ctx.comp_func(host, NULL, &walk_ctx, host->direction);
		if (ret < 0)
			pipe_err(p, "ret = %d, host->comp.id = %u, cmd = %d",
				 ret, dev_comp_id(host), cmd);
		else if (ret == PPL_STATUS_PATH_STOP)
			ret = 0;

		if (pipeline_is_timer_driven(p))
			return ret;
	}

out:
	/*
	 * When called from the pipeline task, pipeline_comp_trigger() will not
	 * add pipelines to the list, so pipeline_schedule_triggered() will have
	 * no effect.
	 */
	pipeline_schedule_triggered(&walk_ctx, cmd);

	return ret;
}

/* Get the timestamps for host and first active DAI found. */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host,
			    struct sof_ipc_stream_posn *posn)
{
	struct comp_dev *dai;

	platform_host_timestamp(host, posn);

	if (host->direction == SOF_IPC_STREAM_PLAYBACK)
		dai = pipeline_get_dai_comp(host->pipeline->pipeline_id, PPL_DIR_DOWNSTREAM);
	else
		dai = pipeline_get_dai_comp(host->pipeline->pipeline_id, PPL_DIR_UPSTREAM);

	if (!dai) {
		pipe_dbg(p, "DAI position update failed");
		return;
	}

	platform_dai_timestamp(dai, posn);

	/* set timestamp resolution */
	posn->timestamp_ns = p->period * 1000;
}
