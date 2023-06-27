// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * A key phrase buffer component.
 */

/**
 * \file audio/kpb.c
 * \brief Key phrase buffer component implementation
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/kpb.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/clk.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <ipc/topology.h>
#include <ipc4/kpb.h>
#include <user/kpb.h>
#include <user/trace.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#if CONFIG_AMS
#include <sof/lib/ams.h>
#include <sof/lib/ams_msg.h>
#include <ipc4/ams_helpers.h>
#else
#include <sof/lib/notifier.h>
#endif

static const struct comp_driver comp_kpb;

LOG_MODULE_REGISTER(kpb, CONFIG_SOF_LOG_LEVEL);
#if CONFIG_IPC_MAJOR_4
/* A8A0CB32-4A77-4DB1-85C7-53D7EE07BCE6 */
DECLARE_SOF_RT_UUID("kpb", kpb_uuid, 0xA8A0CB32, 0x4A77, 0x4DB1,
		    0x85, 0xC7, 0x53, 0xD7, 0xEE, 0x07, 0xBC, 0xE6);
#else
/* d8218443-5ff3-4a4c-b388-6cfe07b9562e */
DECLARE_SOF_RT_UUID("kpb", kpb_uuid, 0xd8218443, 0x5ff3, 0x4a4c,
		    0xb3, 0x88, 0x6c, 0xfe, 0x07, 0xb9, 0x56, 0x2e);
#endif

DECLARE_TR_CTX(kpb_tr, SOF_UUID(kpb_uuid), LOG_LEVEL_INFO);

/* e50057a5-8b27-4db4-bd79-9a639cee5f50 */
DECLARE_SOF_UUID("kpb-task", kpb_task_uuid, 0xe50057a5, 0x8b27, 0x4db4,
		 0xbd, 0x79, 0x9a, 0x63, 0x9c, 0xee, 0x5f, 0x50);

/* KPB private data, runtime data */
struct comp_data {
	enum kpb_state state; /**< current state of KPB component */
	uint32_t state_log; /**< keeps record of KPB recent states */
#ifndef __ZEPHYR__
	struct k_spinlock lock; /**< locking mechanism for read pointer calculations */
	k_spinlock_key_t key;
#else
	struct k_mutex lock;
#endif
	struct sof_kpb_config config;   /**< component configuration data */
	struct history_data hd; /** data related to history buffer */
	struct task draining_task;
	struct draining_data draining_task_data;
	struct kpb_client clients[KPB_MAX_NO_OF_CLIENTS];
	struct comp_buffer *sel_sink; /**< real time sink (channel selector)*/
	struct comp_buffer *host_sink; /**< draining sink (client) */
	uint32_t kpb_no_of_clients; /**< number of registered clients */
	uint32_t source_period_bytes; /**< source number of period bytes */
	uint32_t sink_period_bytes; /**< sink number of period bytes */
	size_t host_buffer_size; /**< size of host buffer */
	size_t host_period_size; /**< size of history period */
	bool sync_draining_mode; /**< should we synchronize draining with
				   * host?
				   */
	enum comp_copy_type force_copy_type; /**< should we force copy_type on kpb sink? */
#ifdef CONFIG_IPC_MAJOR_4
	struct ipc4_kpb_module_cfg ipc4_cfg;
#endif /* CONFIG_IPC_MAJOR_4 */
	uint32_t num_of_sel_mic;
	uint32_t num_of_in_channels;
	uint32_t offsets[KPB_MAX_MICSEL_CHANNELS];
	struct kpb_micselector_config mic_sel;

#if CONFIG_AMS
	uint32_t kpd_uuid_id;
#endif
};

/*! KPB private functions */
#ifndef CONFIG_AMS
static void kpb_event_handler(void *arg, enum notify_id type, void *event_data);
static int kpb_register_client(struct comp_data *kpb, struct kpb_client *cli);
#endif
static void kpb_init_draining(struct comp_dev *dev, struct kpb_client *cli);
static enum task_state kpb_draining_task(void *arg);
static int kpb_buffer_data(struct comp_dev *dev,
			   const struct comp_buffer __sparse_cache *source, size_t size);
static size_t kpb_allocate_history_buffer(struct comp_data *kpb,
					  size_t hb_size_req);
static void kpb_clear_history_buffer(struct history_buffer *buff);
static void kpb_free_history_buffer(struct history_buffer *buff);
static inline bool kpb_is_sample_width_supported(uint32_t sampling_width);
static void kpb_copy_samples(struct comp_buffer __sparse_cache *sink,
			     struct comp_buffer __sparse_cache *source, size_t size,
			     size_t sample_width, uint32_t channels);
static void kpb_drain_samples(void *source, struct audio_stream __sparse_cache *sink,
			      size_t size, size_t sample_width);
static void kpb_buffer_samples(const struct audio_stream __sparse_cache *source,
			       int offset, void *sink, size_t size,
			       size_t sample_width);
static void kpb_reset_history_buffer(struct history_buffer *buff);
static inline bool validate_host_params(struct comp_dev *dev,
					size_t host_period_size,
					size_t host_buffer_size,
					size_t hb_size_req);
static inline void kpb_change_state(struct comp_data *kpb,
				    enum kpb_state state);

static uint64_t kpb_task_deadline(void *data)
{
	return SOF_TASK_DEADLINE_ALMOST_IDLE;
}

#if CONFIG_AMS

/* Key-phrase detected message*/
static const ams_uuid_t ams_kpd_msg_uuid = AMS_KPD_MSG_UUID;

/* Key-phrase detected notification handler*/
static void kpb_ams_kpd_notification(const struct ams_message_payload *const ams_message_payload,
				     void *ctx)
{
	struct kpb_client *cli_data = (struct kpb_client *)ams_message_payload->message;
	struct comp_dev *dev = ctx;

	comp_dbg(dev, "kpb_ams_kpd_notification()");

	kpb_init_draining(dev, cli_data);
}

#endif /* CONFIG_AMS */

#ifdef __ZEPHYR__

static void kpb_lock(struct comp_data *kpb)
{
	k_mutex_lock(&kpb->lock, K_FOREVER);
}

static void kpb_unlock(struct comp_data *kpb)
{
	k_mutex_unlock(&kpb->lock);
}

static void kpb_lock_init(struct comp_data *kpb)
{
	k_mutex_init(&kpb->lock);
}

#else /* __ZEPHYR__ */

static void kpb_lock(struct comp_data *kpb)
{
	kpb->key = k_spin_lock(&kpb->lock);
}

static void kpb_unlock(struct comp_data *kpb)
{
	k_spin_unlock(&kpb->lock, kpb->key);
}

static void kpb_lock_init(struct comp_data *kpb)
{
	k_spinlock_init(&kpb->lock);
}

#endif /* __ZEPHYR__ */

#if CONFIG_IPC_MAJOR_4
/**
 * \brief Set and verify ipc params.
 * \param[in] dev - component device pointer.
 * \param[in] ipc_config - ipc config pointer.
 * \return: none.
 */
static int kpb_set_verify_ipc_params(struct comp_dev *dev,
				     const struct ipc4_kpb_module_cfg *ipc_config)
{
	struct comp_data *kpb = comp_get_drvdata(dev);

	kpb->config.channels = ipc_config->base_cfg.audio_fmt.channels_count;
	kpb->config.sampling_freq =
		ipc_config->base_cfg.audio_fmt.sampling_frequency;
	kpb->config.sampling_width =
		ipc_config->base_cfg.audio_fmt.valid_bit_depth;
	kpb->ipc4_cfg.base_cfg = ipc_config->base_cfg;

	/* Initialize sinks */
	kpb->sel_sink = NULL;
	kpb->host_sink = NULL;

	if (!kpb_is_sample_width_supported(kpb->config.sampling_width)) {
		comp_err(dev, "kpb_set_verify_ipc_params(): requested sampling width not supported");
		return -EINVAL;
	}

	if (kpb->config.channels > KPB_MAX_SUPPORTED_CHANNELS) {
		comp_err(dev, "kpb_set_verify_ipc_params(): no of channels exceeded the limit");
		return -EINVAL;
	}

	if (kpb->config.sampling_freq != KPB_SAMPLNG_FREQUENCY) {
		comp_err(dev, "kpb_set_verify_ipc_params(): requested sampling frequency not supported");
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Set KPB component stream params.
 * \param[in] dev - component device pointer.
 * \param[in] params - sof ipc stream params pointer.
 * \return: none.
 */
static void kpb_set_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	enum sof_ipc_frame frame_fmt, valid_fmt;

	comp_dbg(dev, "kpb_set_params()");

	memset_s(params, sizeof(*params), 0, sizeof(*params));
	params->channels = kpb->ipc4_cfg.base_cfg.audio_fmt.channels_count;
	params->rate = kpb->ipc4_cfg.base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = kpb->ipc4_cfg.base_cfg.audio_fmt.depth / 8;
	params->sample_valid_bytes =
		kpb->ipc4_cfg.base_cfg.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = kpb->ipc4_cfg.base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = kpb->ipc4_cfg.base_cfg.ibs * KPB_MAX_BUFF_TIME * params->channels;

	params->host_period_bytes = params->channels *
				    params->sample_container_bytes *
				    (params->rate / 1000);

	audio_stream_fmt_conversion(kpb->ipc4_cfg.base_cfg.audio_fmt.depth,
				    kpb->ipc4_cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    kpb->ipc4_cfg.base_cfg.audio_fmt.s_type);

	params->frame_fmt = valid_fmt;
}

/**
 * \brief Set KPB component stream params.
 * \param[in] dev - component device pointer.
 * \param[in] type - sof ipc stream params pointer.
 * \param[in] value - ipc4 base module config pointer.
 * \return: none.
 */
static int kpb_get_attribute(struct comp_dev *dev,
			     uint32_t type,
			     void *value)
{
	struct comp_data *kpb = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = kpb->ipc4_cfg.base_cfg;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Initialize KPB sinks when binding.
 * \param[in] dev - component device pointer.
 * \param[in] data - ipc4 bind/unbind data.
 * \return: none.
 */
static int kpb_bind(struct comp_dev *dev, void *data)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct ipc4_module_bind_unbind *bu;
	struct list_item *blist;
	int buf_id;
	int ret = 0;

	comp_dbg(dev, "kpb_bind()");

	bu = (struct ipc4_module_bind_unbind *)data;
	buf_id = IPC4_COMP_ID(bu->extension.r.src_queue, bu->extension.r.dst_queue);

	/* We're assuming here that KPB Real Time sink (kpb->sel_sink) is
	 * always connected to input pin of Detector pipeline so during IPC4
	 * Bind operation both src_queue and dst_queue will have id = 0
	 * (Detector/MicSel has one input pin). To properly connect KPB sink
	 * with Detector source we're looking for buffer with id=0.
	 */

	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink = container_of(blist, struct comp_buffer, source_list);
		struct comp_buffer __sparse_cache *sink_c = buffer_acquire(sink);
		int sink_buf_id;

		if (!sink_c->sink) {
			ret = -EINVAL;
			buffer_release(sink_c);
			break;
		}

		sink_buf_id = sink_c->id;
		buffer_release(sink_c);

		if (sink_buf_id == buf_id) {
			if (sink_buf_id == 0)
				kpb->sel_sink = sink;
			else
				kpb->host_sink = sink;
		}
	}

	return ret;
}

/**
 * \brief Reset KPB sinks when unbinding.
 * \param[in] dev - component device pointer.
 * \param[in] data - ipc4 bind/unbind data.
 * \return: none.
 */
static int kpb_unbind(struct comp_dev *dev, void *data)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct ipc4_module_bind_unbind *bu;
	int buf_id;

	comp_dbg(dev, "kpb_bind()");

	bu = (struct ipc4_module_bind_unbind *)data;
	buf_id = IPC4_COMP_ID(bu->extension.r.src_queue, bu->extension.r.dst_queue);

	/* Reset sinks when unbinding */
	if (buf_id == 0)
		kpb->sel_sink = NULL;
	else
		kpb->host_sink = NULL;

	return 0;
}

#else /* CONFIG_IPC_MAJOR_4 */
/**
 * \brief Set and verify ipc params.
 * \param[in] dev - component device pointer.
 * \param[in] ipc_config - ipc config pointer type.
 * \return: none.
 */
static int kpb_set_verify_ipc_params(struct comp_dev *dev,
				     const struct ipc_config_process *ipc_config)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	int ret;

	ret = memcpy_s(&kpb->config, sizeof(kpb->config), ipc_config->data,
		       ipc_config->size);

	if (ret) {
		comp_err(dev, "kpb_new(): cannot memcpy_s %d bytes into sof_kpb_config (%d)\n",
			 ipc_config->size, sizeof(kpb->config));
		return -EINVAL;
	}

	/* Initialize sinks */
	kpb->sel_sink = NULL;
	kpb->host_sink = NULL;

	if (!kpb_is_sample_width_supported(kpb->config.sampling_width)) {
		comp_err(dev, "kpb_set_verify_ipc_params(): requested sampling width not supported");
		return -EINVAL;
	}

	if (kpb->config.channels > KPB_MAX_SUPPORTED_CHANNELS) {
		comp_err(dev, "kpb_set_verify_ipc_params(): no of channels exceeded the limit");
		return -EINVAL;
	}

	if (kpb->config.sampling_freq != KPB_SAMPLNG_FREQUENCY) {
		comp_err(dev, "kpb_set_verify_ipc_params(): requested sampling frequency not supported");
		return -EINVAL;
	}

	return 0;
}

static void kpb_set_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{}
#endif /* CONFIG_IPC_MAJOR_4 */

/*
 * \brief Create a key phrase buffer component.
 * \param[in] config - generic ipc component pointer.
 *
 * \return: a pointer to newly created KPB component.
 */
static struct comp_dev *kpb_new(const struct comp_driver *drv,
				const struct comp_ipc_config *config,
				const void *spec)
{
#if CONFIG_IPC_MAJOR_4
	const struct ipc4_kpb_module_cfg *ipc_process = spec;
	size_t ipc_config_size = sizeof(*ipc_process);
	size_t kpb_config_size = sizeof(struct ipc4_kpb_module_cfg);
#else
	const struct ipc_config_process *ipc_process = spec;
	size_t ipc_config_size = ipc_process->size;
	size_t kpb_config_size = sizeof(struct sof_kpb_config);
#endif
	struct task_ops ops = {
		.run = kpb_draining_task,
		.get_deadline = kpb_task_deadline,
	};

	struct comp_dev *dev;
	struct comp_data *kpb;
	int ret;

	comp_cl_info(&comp_kpb, "kpb_new()");

	/* make sure data size is not bigger than config space */
	if (ipc_config_size > kpb_config_size) {
		comp_cl_err(&comp_kpb, "kpb_new(): ipc config size %u too big",
			    ipc_config_size);
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	kpb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*kpb));
	if (!kpb) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, kpb);

	ret = kpb_set_verify_ipc_params(dev, ipc_process);
	if (ret) {
		rfree(dev);
		return NULL;
	}

	kpb_lock_init(kpb);

	/* Initialize draining task */
	schedule_task_init_edf(&kpb->draining_task, /* task structure */
			       SOF_UUID(kpb_task_uuid), /* task uuid */
			       &ops, /* task ops */
			       &kpb->draining_task_data, /* task private data */
			       0, /* core on which we should run */
			       0); /* no flags */

	/* Init basic component data */
	kpb->hd.c_hb = NULL;
	kpb->kpb_no_of_clients = 0;
	kpb->state_log = 0;

#ifdef CONFIG_KPB_FORCE_COPY_TYPE_NORMAL
	kpb->force_copy_type = COMP_COPY_NORMAL;
#else
	kpb->force_copy_type = COMP_COPY_INVALID; /* do not change kpb sink copy type */
#endif

	/* Kpb has been created successfully */
	dev->state = COMP_STATE_READY;
	kpb_change_state(kpb, KPB_STATE_CREATED);

	return dev;
}

/**
 * \brief Allocate history buffer.
 * \param[in] kpb - KPB component data pointer.
 *
 * \return: none.
 */
static size_t kpb_allocate_history_buffer(struct comp_data *kpb,
					  size_t hb_size_req)
{
	struct history_buffer *hb;
	struct history_buffer *new_hb = NULL;
	/*! Total allocation size */
	size_t hb_size = hb_size_req;
	/*! Current allocation size */
	size_t ca_size = hb_size;
	/*! Memory caps priorites for history buffer */
	int hb_mcp[KPB_NO_OF_MEM_POOLS] = {SOF_MEM_CAPS_LP, SOF_MEM_CAPS_HP,
					   SOF_MEM_CAPS_RAM };
	void *new_mem_block = NULL;
	size_t temp_ca_size;
	int i = 0;
	size_t allocated_size = 0;

	comp_cl_info(&comp_kpb, "kpb_allocate_history_buffer()");

	/* Initialize history buffer */
	kpb->hd.c_hb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			       sizeof(struct history_buffer));
	if (!kpb->hd.c_hb)
		return 0;
	kpb->hd.c_hb->next = kpb->hd.c_hb;
	kpb->hd.c_hb->prev = kpb->hd.c_hb;
	hb = kpb->hd.c_hb;

	/* Allocate history buffer/s. KPB history buffer has a size of
	 * KPB_MAX_BUFFER_SIZE, since there is no single memory block
	 * that big, we need to allocate couple smaller blocks which
	 * linked together will form history buffer.
	 */
	while (hb_size > 0 && i < ARRAY_SIZE(hb_mcp)) {
		/* Try to allocate ca_size (current allocation size). At first
		 * attempt it will be equal to hb_size (history buffer size).
		 */
		new_mem_block = rballoc(0, hb_mcp[i], ca_size);

		if (new_mem_block) {
			/* We managed to allocate a block of ca_size.
			 * Now we initialize it.
			 */
			comp_cl_info(&comp_kpb, "kpb new memory block: %d",
				     ca_size);
			allocated_size += ca_size;
			hb->start_addr = new_mem_block;
			hb->end_addr = (char *)new_mem_block +
				ca_size;
			hb->w_ptr = new_mem_block;
			hb->r_ptr = new_mem_block;
			hb->state = KPB_BUFFER_FREE;
			hb_size -= ca_size;
			hb->next = kpb->hd.c_hb;
			/* Do we need another buffer? */
			if (hb_size > 0) {
				/* Yes, we still need at least one more buffer.
				 * Let's first create new container for it.
				 */
				new_hb = rzalloc(SOF_MEM_ZONE_RUNTIME, 0,
						 SOF_MEM_CAPS_RAM,
						 sizeof(struct history_buffer));
				if (!new_hb)
					return 0;
				hb->next = new_hb;
				new_hb->next = kpb->hd.c_hb;
				new_hb->state = KPB_BUFFER_OFF;
				new_hb->prev = hb;
				hb = new_hb;
				kpb->hd.c_hb->prev = new_hb;
				ca_size = hb_size;
				i++;
			}
		} else {
			/* We've failed to allocate ca_size of that hb_mcp
			 * let's try again with some smaller size.
			 * NOTE! If we decrement by some small value,
			 * the allocation will take significant time.
			 * However, bigger values like
			 * HEAP_HP_BUFFER_BLOCK_SIZE will result in lower
			 * accuracy of allocation.
			 */
			temp_ca_size = ca_size - KPB_ALLOCATION_STEP;
			ca_size = (ca_size < temp_ca_size) ? 0 : temp_ca_size;
			if (ca_size == 0) {
				ca_size = hb_size;
				i++;
			}
			continue;
		}
	}

	comp_cl_info(&comp_kpb, "kpb_allocate_history_buffer(): allocated %d bytes",
		     allocated_size);

	return allocated_size;
}

/**
 * \brief Reclaim memory of a history buffer.
 * \param[in] buff - pointer to current history buffer.
 *
 * \return none.
 */
static void kpb_free_history_buffer(struct history_buffer *buff)
{
	struct history_buffer *_buff;
	struct history_buffer *first_buff = buff;

	comp_cl_info(&comp_kpb, "kpb_free_history_buffer()");

	if (!buff)
		return;

	/* Free history buffer/s */
	do {
		/* First reclaim HB internal memory, then HB itself */
		if (buff->start_addr)
			rfree(buff->start_addr);

		_buff = buff->next;
		rfree(buff);
		buff = _buff;
	} while (buff && buff != first_buff);
}

/**
 * \brief Reclaim memory of a key phrase buffer.
 * \param[in] dev - component device pointer.
 *
 * \return none.
 */
static void kpb_free(struct comp_dev *dev)
{
	struct comp_data *kpb = comp_get_drvdata(dev);

	comp_info(dev, "kpb_free()");

#if CONFIG_AMS
	/* Unregister KPB as AMS consumer */
	int ret;

	ret = ams_helper_unregister_consumer(dev, kpb->kpd_uuid_id,
					     kpb_ams_kpd_notification);
	if (ret)
		comp_err(dev, "kpb_free(): AMS unregister error %d", ret);
#else
	/* Unregister KPB from notifications */
	notifier_unregister(dev, NULL, NOTIFIER_ID_KPB_CLIENT_EVT);
#endif/* CONFIG_AMS */

	/* Reclaim memory occupied by history buffer */
	kpb_free_history_buffer(kpb->hd.c_hb);
	kpb->hd.c_hb = NULL;
	kpb->hd.buffer_size = 0;

	/* remove scheduling */
	schedule_task_free(&kpb->draining_task);

	/* change state */
	kpb_change_state(kpb, KPB_STATE_DISABLED);

	/* Free KPB */
	rfree(kpb);
	rfree(dev);
}

/**
 * \brief Trigger a change of KPB state.
 * \param[in] dev - component device pointer.
 * \param[in] cmd - command type.
 * \return none.
 */
static int kpb_trigger(struct comp_dev *dev, int cmd)
{
	comp_info(dev, "kpb_trigger()");

	return comp_set_state(dev, cmd);
}

static int kbp_verify_params(struct comp_dev *dev,
			     struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "kbp_verify_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "kpb_verify_params(): comp_verify_params() failed");
		return ret;
	}

	return 0;
}

/**
 * \brief KPB params.
 * \param[in] dev - component device pointer.
 * \param[in] params - pcm params.
 * \return none.
 */
static int kpb_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	int err;

	if (dev->state == COMP_STATE_PREPARE) {
		comp_err(dev, "kpb_params(): kpb has been already configured.");
		return PPL_STATUS_PATH_STOP;
	}

	kpb_set_params(dev, params);

	err = kbp_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "kpb_params(): pcm params verification failed");
		return -EINVAL;
	}

	kpb->host_buffer_size = params->buffer.size;
	kpb->host_period_size = params->host_period_bytes;
	kpb->config.sampling_width = params->sample_container_bytes * 8;

#if CONFIG_AMS
	kpb->kpd_uuid_id = AMS_INVALID_MSG_TYPE;
#endif

	return 0;
}

/**
 * \brief Prepare key phrase buffer.
 * \param[in] dev - kpb component device pointer.
 *
 * \return integer representing either:
 *	0 -> success
 *	-EINVAL -> failure.
 */
static int kpb_prepare(struct comp_dev *dev)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	int ret = 0;
	int i;
	size_t hb_size_req = KPB_MAX_BUFFER_SIZE(kpb->config.sampling_width, kpb->config.channels);

	comp_dbg(dev, "kpb_prepare()");

	if (kpb->state == KPB_STATE_RESETTING ||
	    kpb->state == KPB_STATE_RESET_FINISHING) {
		comp_cl_err(&comp_kpb, "kpb_prepare(): can not prepare KPB due to ongoing reset, state log %x",
			    kpb->state_log);
		return -EBUSY;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	if (!validate_host_params(dev, kpb->host_period_size,
				  kpb->host_buffer_size, hb_size_req)) {
		return -EINVAL;
	}

	kpb_change_state(kpb, KPB_STATE_PREPARING);

	/* Init private data */
	kpb->kpb_no_of_clients = 0;
	kpb->hd.buffered = 0;

	if (kpb->hd.c_hb && kpb->hd.buffer_size < hb_size_req) {
		/* Host params has changed, we need to allocate new buffer */
		kpb_free_history_buffer(kpb->hd.c_hb);
		kpb->hd.c_hb = NULL;
	}

	if (!kpb->hd.c_hb) {
		/* Allocate history buffer */
		kpb->hd.buffer_size = kpb_allocate_history_buffer(kpb,
								  hb_size_req);

		/* Have we allocated what we requested? */
		if (kpb->hd.buffer_size < hb_size_req) {
			comp_cl_err(&comp_kpb, "kpb_prepare(): failed to allocate space for KPB buffer");
			kpb_free_history_buffer(kpb->hd.c_hb);
			kpb->hd.c_hb = NULL;
			kpb->hd.buffer_size = 0;
			return -EINVAL;
		}
	}
	/* Init history buffer */
	kpb_reset_history_buffer(kpb->hd.c_hb);
	kpb->hd.free = kpb->hd.buffer_size;

	/* Initialize clients data */
	for (i = 0; i < KPB_MAX_NO_OF_CLIENTS; i++) {
		kpb->clients[i].state = KPB_CLIENT_UNREGISTERED;
		kpb->clients[i].r_ptr = NULL;
	}

#if CONFIG_AMS
	/* AMS Register KPB for notification */
	ret = ams_helper_register_consumer(dev, &kpb->kpd_uuid_id,
					   ams_kpd_msg_uuid,
					   kpb_ams_kpd_notification);
#else
	/* Register KPB for notification */
	ret = notifier_register(dev, NULL, NOTIFIER_ID_KPB_CLIENT_EVT,
				kpb_event_handler, 0);
#endif /* CONFIG_AMS */

	if (ret < 0) {
		kpb_free_history_buffer(kpb->hd.c_hb);
		kpb->hd.c_hb = NULL;
		return -ENOMEM;
	}

#ifndef CONFIG_IPC_MAJOR_4
	/* Search for KPB related sinks.
	 * NOTE! We assume here that channel selector component device
	 * is connected to the KPB sinks as well as host device.
	 */
	struct list_item *blist;

	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink = container_of(blist, struct comp_buffer, source_list);
		struct comp_buffer __sparse_cache *sink_c = buffer_acquire(sink);
		enum sof_comp_type type;

		if (!sink_c->sink) {
			ret = -EINVAL;
			buffer_release(sink_c);
			break;
		}

		type = dev_comp_type(sink_c->sink);
		buffer_release(sink_c);

		switch (type) {
		case SOF_COMP_SELECTOR:
			/* We found proper real time sink */
			kpb->sel_sink = sink;
			break;
		case SOF_COMP_HOST:
			/* We found proper host sink */
			kpb->host_sink = sink;
			break;
		default:
			break;
		}
	}
#else
	/* Update number of sel_sink channels.
	 * If OBS is not equal to IBS it means that KPB will work in micselector mode.
	 */
	if (kpb->ipc4_cfg.base_cfg.ibs != kpb->ipc4_cfg.base_cfg.obs) {
		struct list_item *sink_list;
		const uint32_t byte_align = 1;
		const uint32_t frame_align_req = 1;
		uint32_t sink_id;

		list_for_item(sink_list, &dev->bsink_list) {
			struct comp_buffer *sink =
				container_of(sink_list, struct comp_buffer, source_list);
			struct comp_buffer __sparse_cache *sink_c = buffer_acquire(sink);

			audio_stream_init_alignment_constants(byte_align, frame_align_req,
							      &sink_c->stream);
			sink_id = sink_c->id;

			if (sink_id == 0)
				audio_stream_set_channels(&sink_c->stream, kpb->num_of_sel_mic);
			else
				audio_stream_set_channels(&sink_c->stream, kpb->config.channels);

			buffer_release(sink_c);
		}
	}
#endif /* CONFIG_IPC_MAJOR_4 */

	if (!kpb->sel_sink) {
		comp_err(dev, "kpb_prepare(): could not find sink: sel_sink %p",
			 kpb->sel_sink);
		ret = -EIO;
	}

	kpb->sync_draining_mode = true;

	kpb_change_state(kpb, KPB_STATE_RUN);

	return ret;
}

/**
 * \brief Used to pass standard and bespoke commands (with data) to component.
 * \param[in,out] dev - Volume base component device.
 * \param[in] cmd - Command type.
 * \param[in,out] data - Control command data.
 * \return Error code.
 */
static int kpb_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	return 0;
}

/**
 * \brief Resets KPB component.
 * \param[in,out] dev KPB base component device.
 * \return Error code.
 */
static int kpb_reset(struct comp_dev *dev)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	int ret = 0;
	int i;

	comp_cl_info(&comp_kpb, "kpb_reset(): resetting from state %d, state log %x",
		     kpb->state, kpb->state_log);

	switch (kpb->state) {
	case KPB_STATE_BUFFERING:
	case KPB_STATE_DRAINING:
		/* KPB is performing some task now,
		 * terminate it gently.
		 */
		kpb_change_state(kpb, KPB_STATE_RESETTING);
		ret = -EBUSY;
		break;
	case KPB_STATE_DISABLED:
	case KPB_STATE_CREATED:
		/* Nothing to reset */
		ret = comp_set_state(dev, COMP_TRIGGER_RESET);
		break;
	default:
		kpb->hd.buffered = 0;
		kpb->sel_sink = NULL;
		kpb->host_sink = NULL;
		kpb->host_buffer_size = 0;
		kpb->host_period_size = 0;

		for (i = 0; i < KPB_MAX_NO_OF_CLIENTS; i++) {
			kpb->clients[i].state = KPB_CLIENT_UNREGISTERED;
			kpb->clients[i].r_ptr = NULL;
		}

		if (kpb->hd.c_hb) {
			/* Reset history buffer - zero its data, reset pointers
			 * and states.
			 */
			kpb_reset_history_buffer(kpb->hd.c_hb);
		}

#ifndef CONFIG_AMS
		/* Unregister KPB from notifications */
		notifier_unregister(dev, NULL, NOTIFIER_ID_KPB_CLIENT_EVT);
#endif
		/* Finally KPB is ready after reset */
		kpb_change_state(kpb, KPB_STATE_PREPARING);

		ret = comp_set_state(dev, COMP_TRIGGER_RESET);
		break;
	}

	return ret;
}

#ifdef KPB_HIFI3
#if CONFIG_FORMAT_S16LE
static void kpb_micselect_copy16(struct comp_buffer __sparse_cache *sink,
				 struct comp_buffer __sparse_cache *source, size_t size,
				 uint32_t in_channels, uint32_t micsel_channels, uint32_t *offsets)
{
	struct audio_stream __sparse_cache *istream = &source->stream;
	struct audio_stream __sparse_cache *ostream = &sink->stream;
	uint16_t ch;
	size_t i;

	AE_SETCBEGIN0(audio_stream_get_addr(ostream));
	AE_SETCEND0(audio_stream_get_end_addr(ostream));

	buffer_stream_invalidate(source, size);
	const ae_int16 *in_ptr = audio_stream_get_rptr(istream);
	ae_int16x4 d16 = AE_ZERO16();
	const size_t in_offset = in_channels * sizeof(ae_int16);
	const size_t out_offset = micsel_channels * sizeof(ae_int16);
	const size_t samples_per_chan = size / (sizeof(uint16_t) * micsel_channels);
	ae_int16 *out_ptr;

	for (ch = 0; ch < micsel_channels; ch++) {
		const ae_int16 *input_data = (const ae_int16 *)(in_ptr) + offsets[ch];

		out_ptr = audio_stream_get_wptr(ostream);
		out_ptr += ch;
		for (i = 0; i < samples_per_chan; i++) {
			AE_L16_XP(d16, input_data, in_offset);
			AE_S16_0_XC(d16, out_ptr, out_offset);
		}
	}
}
#endif
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static void kpb_micselect_copy32(struct comp_buffer __sparse_cache *sink,
				 struct comp_buffer __sparse_cache *source, size_t size,
				 uint32_t in_channels, uint32_t micsel_channels, uint32_t *offsets)
{
	struct audio_stream __sparse_cache *istream = &source->stream;
	struct audio_stream __sparse_cache *ostream = &sink->stream;
	uint16_t ch;
	size_t i;

	AE_SETCBEGIN0(audio_stream_get_addr(ostream));
	AE_SETCEND0(audio_stream_get_end_addr(ostream));

	buffer_stream_invalidate(source, size);

	const ae_int32 *in_ptr = audio_stream_get_rptr(istream);
	ae_int32x2 d32 = AE_ZERO32();
	const size_t in_offset = in_channels * sizeof(ae_int32);
	const size_t out_offset = micsel_channels * sizeof(ae_int32);
	const size_t samples_per_chan = size / (sizeof(uint32_t) * micsel_channels);
	ae_int32 *out_ptr;

	for (ch = 0; ch < micsel_channels; ch++) {
		const ae_int32 *input_data = (const ae_int32 *)(in_ptr) + offsets[ch];

		out_ptr = audio_stream_get_wptr(ostream);
		out_ptr += ch;
		for (i = 0; i < samples_per_chan; i++) {
			AE_L32_XP(d32, input_data, in_offset);
			AE_S32_L_XC(d32, out_ptr, out_offset);
		}
	}
}
#endif
#else
static void kpb_micselect_copy16(struct comp_buffer __sparse_cache *sink,
				 struct comp_buffer __sparse_cache *source, size_t size,
				 uint32_t in_channels,  uint32_t micsel_channels, uint32_t *offsets)
{
	struct audio_stream __sparse_cache *istream = &source->stream;
	struct audio_stream __sparse_cache *ostream = &sink->stream;

	buffer_stream_invalidate(source, size);
	size_t out_samples;
	uint16_t ch;

	const int16_t *in_data;
	int16_t *out_data;
	const uint32_t samples_per_chan = size / (sizeof(uint16_t) * micsel_channels);

	for (ch = 0; ch < micsel_channels; ch++) {
		out_samples = 0;
		in_data = audio_stream_get_rptr(istream);
		out_data = audio_stream_get_wptr(ostream);

		for (size_t i = 0; i < samples_per_chan * in_channels; i += in_channels) {
			if (&out_data[out_samples + ch]
					>= (int16_t *)audio_stream_get_end_addr(ostream)) {
				out_data = (int16_t *)audio_stream_get_addr(ostream);
				out_samples = 0;
			}
			out_data[out_samples + ch] = in_data[i + offsets[ch]];
			out_samples += micsel_channels;
		}
	}
}

static void kpb_micselect_copy32(struct comp_buffer __sparse_cache *sink,
				 struct comp_buffer __sparse_cache *source, size_t size,
				 uint32_t in_channels, uint32_t micsel_channels, uint32_t *offsets)
{
	struct audio_stream __sparse_cache *istream = &source->stream;
	struct audio_stream __sparse_cache *ostream = &sink->stream;

	buffer_stream_invalidate(source, size);
	size_t out_samples;
	uint16_t ch;
	const int32_t *in_data;
	int32_t *out_data;
	const uint32_t samples_per_chan = size / (sizeof(uint32_t) * micsel_channels);

	for (ch = 0; ch < micsel_channels; ch++) {
		out_samples = 0;
		in_data = audio_stream_get_rptr(istream);
		out_data = audio_stream_get_wptr(ostream);

		for (size_t i = 0; i < samples_per_chan * in_channels; i += in_channels) {
			if (&out_data[out_samples + ch]
					>= (int32_t *)audio_stream_get_end_addr(ostream)) {
				out_data = (int32_t *)audio_stream_get_addr(ostream);
				out_samples = 0;
			}
			out_data[out_samples + ch] = in_data[i + offsets[ch]];
			out_samples += micsel_channels;
		}
	}
}
#endif
static void kpb_micselect_copy(struct comp_dev *dev, struct comp_buffer __sparse_cache *sink_c,
			       struct comp_buffer __sparse_cache *source_c, size_t copy_bytes,
			       uint32_t channels)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	size_t sample_width = kpb->config.sampling_width;
	uint32_t *offsets = kpb->offsets;

	switch (sample_width) {
#if CONFIG_FORMAT_S16LE
	case 16:
		kpb_micselect_copy16(sink_c, source_c, copy_bytes,
				     channels, kpb->num_of_sel_mic, offsets);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	case 24:
		kpb_micselect_copy32(sink_c, source_c, copy_bytes,
				     channels, kpb->num_of_sel_mic, offsets);
		break;
	case 32:
		kpb_micselect_copy32(sink_c, source_c, copy_bytes,
				     channels, kpb->num_of_sel_mic, offsets);
		break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */
	default:
		comp_cl_err(&comp_kpb, "KPB: An attempt to copy not supported format!");
		return;
	}
}
/**
 * \brief Copy real time input stream into sink buffer,
 *	and in the same time buffers that input for
 *	later use by some of clients.
 *
 *\param[in] dev - kpb component device pointer.
 *
 * \return integer representing either:
 *	0 - success
 *	-EINVAL - failure.
 */
static int kpb_copy(struct comp_dev *dev)
{
	int ret = 0;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c = NULL;
	size_t copy_bytes = 0, produced_bytes = 0;
	size_t sample_width = kpb->config.sampling_width;
	struct draining_data *dd = &kpb->draining_task_data;
	uint32_t avail_bytes;
	uint32_t channels = kpb->config.channels;

	comp_dbg(dev, "kpb_copy()");

	if (list_is_empty(&dev->bsource_list)) {
		comp_err(dev, "kpb_copy(): no source.");
		return -EINVAL;
	}

	/* Get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	source_c = buffer_acquire(source);

	/* Validate source */
	if (!audio_stream_get_rptr(&source_c->stream)) {
		comp_err(dev, "kpb_copy(): invalid source pointers.");
		ret = -EINVAL;
		goto out;
	}

	switch (kpb->state) {
	case KPB_STATE_RUN:
		/* In normal RUN state we simply copy to our sink. */
		sink = kpb->sel_sink;
		ret = PPL_STATUS_PATH_STOP;

		if (!sink) {
			comp_err(dev, "kpb_copy(): no sink.");
			ret = -EINVAL;
			break;
		}

		sink_c = buffer_acquire(sink);

		/* Validate sink */
		if (!audio_stream_get_wptr(&sink_c->stream)) {
			comp_err(dev, "kpb_copy(): invalid selector sink pointers.");
			ret = -EINVAL;
			break;
		}

		copy_bytes = audio_stream_get_copy_bytes(&source_c->stream, &sink_c->stream);
		if (!copy_bytes) {
			comp_err(dev, "kpb_copy(): nothing to copy sink->free %d source->avail %d",
				 audio_stream_get_free_bytes(&sink_c->stream),
				 audio_stream_get_avail_bytes(&source_c->stream));
			ret = PPL_STATUS_PATH_STOP;
			break;
		}

		if (kpb->num_of_sel_mic == 0) {
			kpb_copy_samples(sink_c, source_c, copy_bytes, sample_width, channels);
		} else {
			uint32_t avail = audio_stream_get_avail_bytes(&source_c->stream);
			uint32_t free = audio_stream_get_free_bytes(&sink_c->stream);

			copy_bytes = MIN(avail, free * channels / kpb->num_of_sel_mic);
			copy_bytes = ROUND_DOWN(copy_bytes, (sample_width >> 3) * channels);
			unsigned int total_bytes_per_sample =
					(sample_width >> 3) * kpb->num_of_sel_mic;

			produced_bytes = copy_bytes * kpb->num_of_sel_mic / channels;
			produced_bytes = ROUND_DOWN(produced_bytes, total_bytes_per_sample);
			if (!copy_bytes) {
				comp_err(dev, "kpb_copy(): nothing to copy sink->free %d source->avail %d",
					 free,
					 avail);
				ret = PPL_STATUS_PATH_STOP;
				break;
			}
			kpb_micselect_copy(dev, sink_c, source_c, produced_bytes, channels);
		}
		/* Buffer source data internally in history buffer for future
		 * use by clients.
		 */
		if (copy_bytes <= kpb->hd.buffer_size) {
			ret = kpb_buffer_data(dev, source_c, copy_bytes);

			if (ret) {
				comp_err(dev, "kpb_copy(): internal buffering failed.");
				break;
			}
			ret = PPL_STATUS_PATH_STOP;

			/* Update buffered size. NOTE! We only record buffered
			 * data up to the size of history buffer.
			 */
			kpb->hd.buffered += MIN(kpb->hd.buffer_size -
						kpb->hd.buffered,
						copy_bytes);
		} else {
			comp_err(dev, "kpb_copy(): too much data to buffer.");
		}

		if (kpb->num_of_sel_mic == 0)
			comp_update_buffer_produce(sink_c, copy_bytes);
		else
			comp_update_buffer_produce(sink_c, produced_bytes);

		comp_update_buffer_consume(source_c, copy_bytes);

		break;
	case KPB_STATE_HOST_COPY:
		/* In host copy state we only copy to host buffer. */
		sink = kpb->host_sink;

		if (!sink) {
			comp_err(dev, "kpb_copy(): no sink.");
			ret = -EINVAL;
			break;
		}

		sink_c = buffer_acquire(sink);

		/* Validate sink */
		if (!audio_stream_get_wptr(&sink_c->stream)) {
			comp_err(dev, "kpb_copy(): invalid host sink pointers.");
			ret = -EINVAL;
			break;
		}

		copy_bytes = audio_stream_get_copy_bytes(&source_c->stream, &sink_c->stream);
		if (!copy_bytes) {
			comp_err(dev, "kpb_copy(): nothing to copy sink->free %d source->avail %d",
				 audio_stream_get_free_bytes(&sink_c->stream),
				 audio_stream_get_avail_bytes(&source_c->stream));
			/* NOTE! We should stop further pipeline copy due to
			 * no data availability however due to HW bug
			 * (no HOST DMA IRQs) we need to call host copy
			 * anyway so it can update its pointers.
			 */
			break;
		}

		kpb_copy_samples(sink_c, source_c, copy_bytes, sample_width, channels);

		comp_update_buffer_produce(sink_c, copy_bytes);
		comp_update_buffer_consume(source_c, copy_bytes);

		break;
	case KPB_STATE_INIT_DRAINING:
	case KPB_STATE_DRAINING:
		/* In draining and init draining we only buffer data in
		 * the internal history buffer.
		 */
		avail_bytes = audio_stream_get_avail_bytes(&source_c->stream);
		copy_bytes = MIN(avail_bytes, kpb->hd.free);
		ret = PPL_STATUS_PATH_STOP;
		if (copy_bytes) {
			buffer_stream_invalidate(source_c, copy_bytes);
			ret = kpb_buffer_data(dev, source_c, copy_bytes);
			dd->buffered_while_draining += copy_bytes;
			kpb->hd.free -= copy_bytes;

			if (ret) {
				comp_err(dev, "kpb_copy(): internal buffering failed.");
				break;
			}

			comp_update_buffer_consume(source_c, copy_bytes);
		} else {
			comp_warn(dev, "kpb_copy(): buffering skipped (no data to copy, avail %d, free %d",
				  audio_stream_get_avail_bytes(&source_c->stream),
				  kpb->hd.free);
		}

		break;
	default:
		comp_cl_err(&comp_kpb, "kpb_copy(): wrong state (state %d, state log %x)",
			    kpb->state, kpb->state_log);
		ret = -EIO;
		break;
	}

out:
	if (sink_c)
		buffer_release(sink_c);
	buffer_release(source_c);

	return ret;
}

/**
 * \brief Buffer real time data stream in
 *	the internal buffer.
 *
 * \param[in] dev - KPB component data pointer.
 * \param[in] source - pointer to the buffer source.
 *
 */
static int kpb_buffer_data(struct comp_dev *dev,
			   const struct comp_buffer __sparse_cache *source, size_t size)
{
	int ret = 0;
	size_t size_to_copy = size;
	size_t space_avail;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct history_buffer *buff = kpb->hd.c_hb;
	uint32_t offset = 0;
	uint64_t timeout = 0;
	uint64_t current_time;
	enum kpb_state state_preserved = kpb->state;
	size_t sample_width = kpb->config.sampling_width;

	comp_dbg(dev, "kpb_buffer_data()");

	/* We are allowed to buffer data in internal history buffer
	 * only in KPB_STATE_RUN, KPB_STATE_DRAINING or KPB_STATE_INIT_DRAINING
	 * states.
	 */
	if (kpb->state != KPB_STATE_RUN &&
	    kpb->state != KPB_STATE_DRAINING &&
	    kpb->state != KPB_STATE_INIT_DRAINING) {
		comp_err(dev, "kpb_buffer_data(): wrong state! (current state %d, state log %x)",
			 kpb->state, kpb->state_log);
		return PPL_STATUS_PATH_STOP;
	}

	kpb_change_state(kpb, KPB_STATE_BUFFERING);

	timeout = sof_cycle_get_64() + k_ms_to_cyc_ceil64(1);
	/* Let's store audio stream data in internal history buffer */
	while (size_to_copy) {
		/* Reset was requested, it's time to stop buffering and finish
		 * KPB reset.
		 */
		if (kpb->state == KPB_STATE_RESETTING) {
			kpb_change_state(kpb, KPB_STATE_RESET_FINISHING);
			kpb_reset(dev);
			return PPL_STATUS_PATH_STOP;
		}

		/* Are we stuck in buffering? */
		current_time = sof_cycle_get_64();
		if (timeout < current_time) {
			timeout = k_cyc_to_ms_near64(current_time - timeout);
			if (timeout <= UINT_MAX)
				comp_err(dev,
					 "kpb_buffer_data(): timeout of %u [ms] (current state %d, state log %x)",
					 (unsigned int)(timeout), kpb->state,
					 kpb->state_log);
			else
				comp_err(dev,
					 "kpb_buffer_data(): timeout > %u [ms] (current state %d, state log %x)",
					 UINT_MAX, kpb->state,
					 kpb->state_log);
			return -ETIME;
		}

		/* Check how much space there is in current write buffer */
		space_avail = (uintptr_t)buff->end_addr - (uintptr_t)buff->w_ptr;

		if (size_to_copy > space_avail) {
			/* We have more data to copy than available space
			 * in this buffer, copy what's available and continue
			 * with next buffer.
			 */
			kpb_buffer_samples(&source->stream, offset, buff->w_ptr,
					   space_avail, sample_width);
			/* Update write pointer & requested copy size */
			buff->w_ptr = (char *)buff->w_ptr + space_avail;
			size_to_copy = size_to_copy - space_avail;
			/* Update read pointer's offset before continuing
			 * with next buffer.
			 */
			offset += space_avail;
		} else {
			/* Requested size is smaller or equal to the space
			 * available in this buffer. In this scenario simply
			 * copy what was requested.
			 */
			kpb_buffer_samples(&source->stream, offset, buff->w_ptr,
					   size_to_copy, sample_width);
			/* Update write pointer & requested copy size */
			buff->w_ptr = (char *)buff->w_ptr + size_to_copy;
			/* Reset requested copy size */
			size_to_copy = 0;
		}
		/* Have we filled whole buffer? */
		if (buff->w_ptr == buff->end_addr) {
			/* Reset write pointer back to the beginning
			 * of the buffer.
			 */
			buff->w_ptr = buff->start_addr;
			/* If we have more buffers use them */
			if (buff->next && buff->next != buff) {
				/* Mark current buffer FULL */
				buff->state = KPB_BUFFER_FULL;
				/* Use next buffer available on the list
				 * of buffers.
				 */
				buff = buff->next;
				/* Update also component container,
				 * so next time we enter buffering function
				 * we will know right away what is the current
				 * write buffer
				 */
				kpb->hd.c_hb = buff;
			}
			/* Mark buffer as FREE */
			buff->state = KPB_BUFFER_FREE;
		}
	}

	kpb_change_state(kpb, state_preserved);
	return ret;
}

#ifndef CONFIG_AMS
/**
 * \brief Main event dispatcher.
 * \param[in] arg - KPB component internal data.
 * \param[in] type - notification type
 * \param[in] event_data - event specific data.
 * \return none.
 */
static void kpb_event_handler(void *arg, enum notify_id type, void *event_data)
{
	struct comp_dev *dev = arg;
	struct comp_data *kpb = comp_get_drvdata(dev);
	struct kpb_event_data *evd = event_data;
	struct kpb_client *cli = evd->client_data;

	comp_info(dev, "kpb_event_handler(): received event with ID: %d ",
		  evd->event_id);

	switch (evd->event_id) {
	case KPB_EVENT_REGISTER_CLIENT:
		kpb_register_client(kpb, cli);
		break;
	case KPB_EVENT_UNREGISTER_CLIENT:
		/*TODO*/
		break;
	case KPB_EVENT_BEGIN_DRAINING:
		kpb_init_draining(dev, cli);
		break;
	case KPB_EVENT_STOP_DRAINING:
		/*TODO*/
		break;
	default:
		comp_err(dev, "kpb_cmd(): unsupported command");
		break;
	}
}

/**
 * \brief Register clients in the system.
 *
 * \param[in] kpb - kpb device component pointer.
 * \param[in] cli - pointer to KPB client's data.
 *
 * \return integer representing either:
 *	0 - success
 *	-EINVAL - failure.
 */
static int kpb_register_client(struct comp_data *kpb, struct kpb_client *cli)
{
	int ret = 0;

	comp_cl_info(&comp_kpb, "kpb_register_client()");

	if (!cli) {
		comp_cl_err(&comp_kpb, "kpb_register_client(): no client data");
		return -EINVAL;
	}
	/* Do we have a room for a new client? */
	if (kpb->kpb_no_of_clients >= KPB_MAX_NO_OF_CLIENTS ||
	    cli->id >= KPB_MAX_NO_OF_CLIENTS) {
		comp_cl_err(&comp_kpb, "kpb_register_client(): no free room for client = %u ",
			    cli->id);
		ret = -EINVAL;
	} else if (kpb->clients[cli->id].state != KPB_CLIENT_UNREGISTERED) {
		comp_cl_err(&comp_kpb, "kpb_register_client(): client = %u already registered",
			    cli->id);
		ret = -EINVAL;
	} else {
		/* Client accepted, let's store his data */
		kpb->clients[cli->id].id  = cli->id;
		kpb->clients[cli->id].drain_req = cli->drain_req;
		kpb->clients[cli->id].sink = cli->sink;
		kpb->clients[cli->id].r_ptr = NULL;
		kpb->clients[cli->id].state = KPB_CLIENT_BUFFERING;
		kpb->kpb_no_of_clients++;
		ret = 0;
	}

	return ret;
}
#endif /* CONFIG_AMS */

/**
 * \brief Prepare history buffer for draining.
 *
 * \param[in] dev - kpb component data.
 * \param[in] cli - client's data.
 *
 */
static void kpb_init_draining(struct comp_dev *dev, struct kpb_client *cli)
{
	struct comp_data *kpb = comp_get_drvdata(dev);
	bool is_sink_ready = (kpb->host_sink->sink->state == COMP_STATE_ACTIVE);
	size_t sample_width = kpb->config.sampling_width;
	size_t drain_req = cli->drain_req * kpb->config.channels *
			       (kpb->config.sampling_freq / 1000) *
			       (KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8);
	struct history_buffer *buff = kpb->hd.c_hb;
	struct history_buffer *first_buff = buff;
	size_t buffered = 0;
	size_t local_buffered;
	size_t drain_interval;
	size_t host_period_size = kpb->host_period_size;
	size_t bytes_per_ms = KPB_SAMPLES_PER_MS *
			      (KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8) *
			      kpb->config.channels;
	size_t period_bytes_limit;

	comp_info(dev, "kpb_init_draining(): requested draining of %d [ms] from history buffer",
		  cli->drain_req);

	if (kpb->state != KPB_STATE_RUN) {
		comp_err(dev, "kpb_init_draining(): wrong KPB state");
	} else if (cli->id > KPB_MAX_NO_OF_CLIENTS) {
		comp_err(dev, "kpb_init_draining(): wrong client id");
	/* TODO: check also if client is registered */
	} else if (!is_sink_ready) {
		comp_err(dev, "kpb_init_draining(): sink not ready for draining");
	} else if (kpb->hd.buffered < drain_req ||
		   cli->drain_req > KPB_MAX_DRAINING_REQ) {
		comp_cl_err(&comp_kpb, "kpb_init_draining(): not enough data in history buffer");
	} else {
		/* Draining accepted, find proper buffer to start reading
		 * At this point we are guaranteed that there is enough data
		 * in the history buffer. All we have to do now is to calculate
		 * read pointer from which we will start draining.
		 */
		kpb_lock(kpb);

		kpb_change_state(kpb, KPB_STATE_INIT_DRAINING);

		/* Set history buffer size so new data won't overwrite those
		 * staged for draining.
		 */
		kpb->hd.free = kpb->hd.buffer_size - drain_req;

		/* Find buffer to start draining from */
		do {
			/* Calculate how much data we have stored in
			 * current buffer.
			 */
			buff->r_ptr = buff->start_addr;
			if (buff->state == KPB_BUFFER_FREE) {
				local_buffered = (uintptr_t)buff->w_ptr -
						 (uintptr_t)buff->start_addr;
				buffered += local_buffered;
			} else if (buff->state == KPB_BUFFER_FULL) {
				local_buffered = (uintptr_t)buff->end_addr -
						 (uintptr_t)buff->start_addr;
				buffered += local_buffered;
			} else {
				comp_err(dev, "kpb_init_draining(): incorrect buffer label");
			}
			/* Check if this is already sufficient to start draining
			 * if not, go to previous buffer and continue
			 * calculations.
			 */
			if (drain_req > buffered) {
				if (buff->prev == first_buff) {
					/* We went full circle and still don't
					 * have sufficient data for draining.
					 * That means we need to look up the
					 * first buffer again. Our read pointer
					 * is somewhere between write pointer
					 * and buffer's end address.
					 */
					buff = buff->prev;
					buffered += (uintptr_t)buff->end_addr -
						    (uintptr_t)buff->w_ptr;
					buff->r_ptr = (char *)buff->w_ptr +
						      (buffered - drain_req);
					break;
				}
				buff = buff->prev;
			} else if (drain_req == buffered) {
				buff->r_ptr = buff->start_addr;
				break;
			} else {
				buff->r_ptr = (char *)buff->start_addr +
					      (buffered - drain_req);
				break;
			}

		} while (buff != first_buff);

		kpb_unlock(kpb);

		/* Should we drain in synchronized mode (sync_draining_mode)?
		 * Note! We have already verified host params during
		 * kpb_prepare().
		 */
		if (kpb->sync_draining_mode) {
			/* Calculate time in clock ticks each draining event
			 * shall take place. This time will be used to
			 * synchronize us with application interrupts.
			 */
			drain_interval = k_ms_to_cyc_ceil64(host_period_size / bytes_per_ms) /
					 KPB_DRAIN_NUM_OF_PPL_PERIODS_AT_ONCE;
			period_bytes_limit = host_period_size;
			comp_info(dev, "kpb_init_draining(): sync_draining_mode selected with interval %u [uS].",
				  (unsigned int)k_cyc_to_us_near64(drain_interval));
		} else {
			/* Unlimited draining */
			drain_interval = 0;
			period_bytes_limit = 0;
			comp_info(dev, "kpb_init_draining: unlimited draining speed selected.");
		}

		comp_info(dev, "kpb_init_draining(), schedule draining task");

		/* Add one-time draining task into the scheduler. */
		kpb->draining_task_data.sink = kpb->host_sink;
		kpb->draining_task_data.hb = buff;
		kpb->draining_task_data.drain_req = drain_req;
		kpb->draining_task_data.sample_width = sample_width;
		kpb->draining_task_data.drain_interval = drain_interval;
		kpb->draining_task_data.pb_limit = period_bytes_limit;
		kpb->draining_task_data.dev = dev;
		kpb->draining_task_data.sync_mode_on = kpb->sync_draining_mode;

		/* save current sink copy type */
		comp_get_attribute(kpb->host_sink->sink, COMP_ATTR_COPY_TYPE,
				   &kpb->draining_task_data.copy_type);

		if (kpb->force_copy_type != COMP_COPY_INVALID)
			comp_set_attribute(kpb->host_sink->sink, COMP_ATTR_COPY_TYPE,
					   &kpb->force_copy_type);

		/* Pause selector copy. */
		kpb->sel_sink->sink->state = COMP_STATE_PAUSED;

		/* Schedule draining task */
		schedule_task(&kpb->draining_task, 0, 0);
	}
}

/**
 * \brief Draining task.
 *
 * \param[in] arg - pointer keeping drainig data previously prepared
 * by kpb_init_draining().
 *
 * \return none.
 */
static enum task_state kpb_draining_task(void *arg)
{
	struct draining_data *draining_data = (struct draining_data *)arg;
	struct comp_buffer __sparse_cache *sink = buffer_acquire(draining_data->sink);
	struct history_buffer *buff = draining_data->hb;
	size_t drain_req = draining_data->drain_req;
	size_t sample_width = draining_data->sample_width;
	size_t size_to_read;
	size_t size_to_copy;
	bool move_buffer = false;
	uint32_t drained = 0;
	uint64_t draining_time_start;
	uint64_t draining_time_end;
	uint64_t draining_time_ms;
	uint64_t drain_interval = draining_data->drain_interval;
	uint64_t next_copy_time = 0;
	uint64_t current_time;
	size_t period_bytes = 0;
	size_t period_bytes_limit = draining_data->pb_limit;
	size_t period_copy_start = sof_cycle_get_64();
	size_t time_taken;
	size_t *rt_stream_update = &draining_data->buffered_while_draining;
	struct comp_data *kpb = comp_get_drvdata(draining_data->dev);
	bool sync_mode_on = draining_data->sync_mode_on;
	bool pm_is_active;

	comp_cl_info(&comp_kpb, "kpb_draining_task(), start.");

	pm_is_active = pm_runtime_is_active(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

	if (!pm_is_active)
		pm_runtime_disable(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

	/* Change KPB internal state to DRAINING */
	kpb_change_state(kpb, KPB_STATE_DRAINING);

	draining_time_start = sof_cycle_get_64();

	while (drain_req > 0) {
		/* Have we received reset request? */
		if (kpb->state == KPB_STATE_RESETTING) {
			kpb_change_state(kpb, KPB_STATE_RESET_FINISHING);
			kpb_reset(draining_data->dev);
			goto out;
		}
		/* Are we ready to drain further or host still need some time
		 * to read the data already provided?
		 */
		if (sync_mode_on &&
		    next_copy_time > sof_cycle_get_64()) {
			period_bytes = 0;
			period_copy_start = sof_cycle_get_64();
			continue;
		} else if (next_copy_time == 0) {
			period_copy_start = sof_cycle_get_64();
		}

		size_to_read = (uintptr_t)buff->end_addr - (uintptr_t)buff->r_ptr;

		if (size_to_read > audio_stream_get_free_bytes(&sink->stream)) {
			if (audio_stream_get_free_bytes(&sink->stream) >= drain_req)
				size_to_copy = drain_req;
			else
				size_to_copy = audio_stream_get_free_bytes(&sink->stream);
		} else {
			if (size_to_read > drain_req) {
				size_to_copy = drain_req;
			} else {
				size_to_copy = size_to_read;
				move_buffer = true;
			}
		}

		kpb_drain_samples(buff->r_ptr, &sink->stream, size_to_copy,
				  sample_width);

		buff->r_ptr = (char *)buff->r_ptr + (uint32_t)size_to_copy;
		drain_req -= size_to_copy;
		drained += size_to_copy;
		period_bytes += size_to_copy;
		kpb->hd.free += MIN(kpb->hd.buffer_size -
				    kpb->hd.free, size_to_copy);

		if (move_buffer) {
			buff->r_ptr = buff->start_addr;
			buff = buff->next;
			move_buffer = false;
		}

		if (size_to_copy) {
			comp_update_buffer_produce(sink, size_to_copy);
			comp_copy(sink->sink);
		} else if (!audio_stream_get_free_bytes(&sink->stream)) {
			/* There is no free space in sink buffer.
			 * Call .copy() on sink component so it can
			 * process its data further.
			 */
			comp_copy(sink->sink);
		}

		if (sync_mode_on && period_bytes >= period_bytes_limit) {
			current_time = sof_cycle_get_64();
			time_taken = current_time - period_copy_start;
			next_copy_time = current_time + drain_interval -
					 time_taken;
		}

		if (drain_req == 0) {
		/* We have finished draining of requested data however
		 * while we were draining real time stream could provided
		 * new data which needs to be copy to host.
		 */
			comp_cl_info(&comp_kpb, "kpb: update drain_req by %d",
				     *rt_stream_update);
			kpb_lock(kpb);
			drain_req += *rt_stream_update;
			*rt_stream_update = 0;
			if (!drain_req && kpb->state == KPB_STATE_DRAINING) {
			/* Draining is done. Now switch KPB to copy real time
			 * stream to client's sink. This state is called
			 * "draining on demand"
			 * Note! If KPB state changed during draining due to
			 * i.e reset request we should not change that state.
			 */
				kpb_change_state(kpb, KPB_STATE_HOST_COPY);
			}
			kpb_unlock(kpb);
		}
	}

out:
	draining_time_end = sof_cycle_get_64();

	buffer_release(sink);

	/* Reset host-sink copy mode back to its pre-draining value */
	sink = buffer_acquire(kpb->host_sink);
	comp_set_attribute(sink->sink, COMP_ATTR_COPY_TYPE,
			   &kpb->draining_task_data.copy_type);
	buffer_release(sink);

	draining_time_ms = k_cyc_to_ms_near64(draining_time_end - draining_time_start);
	if (draining_time_ms <= UINT_MAX)
		comp_cl_info(&comp_kpb, "KPB: kpb_draining_task(), done. %u drained in %u ms",
			     drained, (unsigned int)draining_time_ms);
	else
		comp_cl_info(&comp_kpb, "KPB: kpb_draining_task(), done. %u drained in > %u ms",
			     drained, UINT_MAX);

	return SOF_TASK_STATE_COMPLETED;
}

#ifdef KPB_HIFI3
static void kpb_convert_24b_to_32b(const void *linear_source, int ioffset,
				   struct audio_stream __sparse_cache *sink, int ooffset,
				   unsigned int n_samples)
{
	int ssize = audio_stream_sample_bytes(sink);
	uint8_t *in = (uint8_t *)linear_source + ioffset * ssize;
	uint8_t *out = audio_stream_wrap(sink, (uint8_t *)audio_stream_get_wptr(sink) +
					 ooffset * ssize);
	ae_int32x2 *buf_end;
	ae_int32x2 *buf;

	buf = (ae_int32x2 *)(audio_stream_get_addr(sink));
	buf_end = audio_stream_get_end_addr(sink);
	ae_int32x2 *out_ptr = (ae_int32x2 *)buf;

	AE_SETCBEGIN0(buf);
	AE_SETCEND0(buf_end);
	out_ptr = (ae_int32x2 *)out;

	ae_valign align_in = AE_LA64_PP(in);
	int i = 0;
	ae_int24x2 d24 = AE_ZERO24();

	if (!IS_ALIGNED((uintptr_t)out_ptr, 8)) {
		AE_LA24_IP(d24, align_in, in);
		ae_int32x2 d320 = d24;
		int higher = AE_MOVAD32_H(d320);
		*(ae_int32 *)(out_ptr) = higher << 8;
		out_ptr = (ae_int32x2 *)(out + 4);
		++i;
	}
	/* process two samples in single iteration to increase performance */
	while (i < (int)n_samples - 1) {
		AE_LA24X2_IP(d24, align_in, in);
		ae_int32x2 d320 = d24;

		d320 = AE_SLAI32(d320, 8);
		AE_S32X2_XC(d320, out_ptr, 8);
		i += 2;
	}
	if (i != (int)n_samples) {
		AE_LA24X2_IP(d24, align_in, in);
		ae_int32x2 d320 = d24;
		int higher = AE_MOVAD32_H(d320);
		*(ae_int32 *)(out_ptr) = higher << 8;
	}
}
#else
static void kpb_convert_24b_to_32b(const void *source, int ioffset,
				   struct audio_stream __sparse_cache *sink,
				   int ooffset, unsigned int samples)
{
	int ssize = audio_stream_sample_bytes(sink);
	uint8_t *src = (uint8_t *)source + ioffset * 3;
	int32_t *dst = audio_stream_wrap(sink, (uint8_t *)audio_stream_get_wptr(sink) +
					 ooffset * ssize);
	int processed;
	int nmax, i, n;

	for (processed = 0; processed < samples; processed += n) {
		dst = audio_stream_wrap(sink, dst);
		n = samples - processed;
		nmax = KPB_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(sink, dst));
		n = MIN(n, nmax);
		for (i = 0; i < n; i += 1) {
			*dst = (src[2] << 16) | (src[1] << 8) | src[0];
			dst++;
			src += 3;
		}
	}
}
#endif
/**
 * \brief Drain data samples safe, according to configuration.
 *
 * \param[in] sink - pointer to sink buffer.
 * \param[in] source - pointer to source buffer.
 * \param[in] size - requested copy size in bytes.
 *
 * \return none.
 */
static void kpb_drain_samples(void *source, struct audio_stream __sparse_cache *sink,
			      size_t size, size_t sample_width)
{
	unsigned int samples;

	switch (sample_width) {
#if CONFIG_FORMAT_S16LE
	case 16:
		samples = KPB_BYTES_TO_S16_SAMPLES(size);
		audio_stream_copy_from_linear(source, 0, sink, 0, samples);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	case 24:
		samples = size / ((sample_width >> 3) * audio_stream_get_channels(sink));
		kpb_convert_24b_to_32b(source, 0, sink, 0, samples);
		break;
	case 32:
		samples = KPB_BYTES_TO_S32_SAMPLES(size);
		audio_stream_copy_from_linear(source, 0, sink, 0, samples);
		break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */
	default:
		comp_cl_err(&comp_kpb, "KPB: An attempt to copy not supported format!");
		return;
	}
}

#ifdef KPB_HIFI3
static void kpb_convert_32b_to_24b(const struct audio_stream __sparse_cache *source, int ioffset,
				   void *linear_sink, int ooffset, unsigned int n_samples)
{
	int ssize = audio_stream_sample_bytes(source);
	uint8_t *in = audio_stream_wrap(source, (uint8_t *)audio_stream_get_rptr(source) +
					ioffset * ssize);
	uint8_t *out = (uint8_t *)linear_sink + ooffset * ssize;

	const ae_f24x2 *sin = (const ae_f24x2 *)in;
	ae_f24x2 *sout = (ae_f24x2 *)out;

	ae_f24x2 vs = AE_ZERO24();
	ae_valign align_out = AE_ZALIGN64();

	if (!IS_ALIGNED((uintptr_t)sin, 8)) {
		AE_L32F24_XC(vs, (const ae_f24 *)sin, 4);
		AE_SA24_IP(vs, align_out, sout);
		n_samples--;
	}

	unsigned int size = n_samples >> 1;
	size_t i;

	for (i = 0; i < size; i++) {
		AE_L32X2F24_XC(vs, sin, 8);
		AE_SA24X2_IP(vs, align_out, sout);
	}
	AE_SA64POS_FP(align_out, sout);

	if (n_samples & 1) {
		AE_L32X2F24_XC(vs, sin, 4);
		ae_f24 tmp = AE_MOVAD32_H(AE_MOVINT24X2_FROMF24X2(vs));

		AE_SA24_IP(tmp, align_out, sout);
		AE_SA64POS_FP(align_out, sout);
	}
}
#else
static void kpb_convert_32b_to_24b(const struct audio_stream __sparse_cache *source, int ioffset,
				   void *sink, int ooffset, unsigned int samples)
{
	int ssize = audio_stream_sample_bytes(source);
	int32_t *src = audio_stream_wrap(source, (uint8_t *)audio_stream_get_rptr(source) +
					 ioffset * ssize);
	uint8_t *dst = (uint8_t *)sink + ooffset * 3;
	int processed;
	int nmax, i, n;

	for (processed = 0; processed < samples; processed += n) {
		src = audio_stream_wrap(source, src);
		n = samples - processed;
		nmax = KPB_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(source, src));
		n = MIN(n, nmax);
		for (i = 0; i < n; i += 1) {
			dst[0] = *src & 0xFF;
			dst[1] = (*src >> 8) & 0xFF;
			dst[2] = (*src >> 16) & 0xFF;
			dst += 3;
			src++;
		}
	}
}
#endif
/**
 * \brief Buffers data samples safe, according to configuration.
 * \param[in,out] source Pointer to source buffer.
 * \param[in] offset Start offset of source buffer in bytes.
 * \param[in,out] sink Pointer to sink buffer.
 * \param[in] size Requested copy size in bytes.
 * \param[in] sample_width Sample size.
 */
static void kpb_buffer_samples(const struct audio_stream __sparse_cache *source,
			       int offset, void *sink, size_t size,
			       size_t sample_width)
{
	unsigned int samples_count;
	int samples_offset;

	switch (sample_width) {
#if CONFIG_FORMAT_S16LE
	case 16:
		samples_count = KPB_BYTES_TO_S16_SAMPLES(size);
		samples_offset = KPB_BYTES_TO_S16_SAMPLES(offset);
		audio_stream_copy_to_linear(source, samples_offset,
					    sink, 0, samples_count);
		break;
#endif
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	case 24:
		samples_count =  size / ((sample_width >> 3) * audio_stream_get_channels(source));
		samples_offset = offset / ((sample_width >> 3) * audio_stream_get_channels(source));
		kpb_convert_32b_to_24b(source, samples_offset,
				       sink, 0, samples_count);
		break;
	case 32:
		samples_count = KPB_BYTES_TO_S32_SAMPLES(size);
		samples_offset = KPB_BYTES_TO_S32_SAMPLES(offset);
		audio_stream_copy_to_linear(source, samples_offset,
					    sink, 0, samples_count);
		break;
#endif
	default:
		comp_cl_err(&comp_kpb, "KPB: An attempt to copy not supported format!");
		return;
	}
}

/**
 * \brief Initialize history buffer by zeroing its memory.
 * \param[in] buff - pointer to current history buffer.
 *
 * \return: none.
 */
static void kpb_clear_history_buffer(struct history_buffer *buff)
{
	struct history_buffer *first_buff = buff;
	void *start_addr;
	size_t size;

	comp_cl_info(&comp_kpb, "kpb_clear_history_buffer()");

	do {
		start_addr = buff->start_addr;
		size = (uintptr_t)buff->end_addr - (uintptr_t)start_addr;

		bzero(start_addr, size);

		buff = buff->next;
	} while (buff != first_buff);
}

static inline bool kpb_is_sample_width_supported(uint32_t sampling_width)
{
	bool ret;

	switch (sampling_width) {
#if CONFIG_FORMAT_S16LE
	case 16:
	/* FALLTHROUGH */
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case 24:
	/* FALLTHROUGH */
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case 32:
#endif /* CONFIG_FORMAT_S32LE */
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

#ifdef KPB_HIFI3
static void kpb_copy_24b_in_32b(const struct audio_stream __sparse_cache *source, uint32_t ioffset,
				struct audio_stream __sparse_cache *sink, uint32_t ooffset,
				uint32_t n_samples)
{
	int ssize = audio_stream_sample_bytes(source); /* src fmt == sink fmt */
	uint8_t *in = audio_stream_wrap(source, (uint8_t *)audio_stream_get_rptr(source) +
					ioffset * ssize);
	uint8_t *out = audio_stream_wrap(sink, (uint8_t *)audio_stream_get_wptr(sink) +
					 ooffset * ssize);

	const ae_int32x2 *sin = (const ae_int32x2 *)in;
	ae_int32x2 *sout = (ae_int32x2 *)out;
	ae_int32x2 vs = AE_ZERO32();

	if (!IS_ALIGNED((uintptr_t)sin, 8)) {
		AE_L32_IP(vs, (const ae_int32 *)sin, 4);
		AE_S32_L_IP(vs, (ae_int32 *)sout, 4);
		n_samples--;
	}
	ae_valign align_out = AE_ZALIGN64();
	size_t size = n_samples >> 1;
	size_t i;

	for (i = 0; i < size; i++) {
		AE_L32X2_IP(vs, sin, 8);
		AE_SA32X2_IP(vs, align_out, sout);
	}
	AE_SA64POS_FP(align_out, sout);
	if (n_samples & 1) {
		vs = AE_L32_I((const ae_int32 *)sin, 0);
		AE_S32_L_I(vs, (ae_int32 *)sout, 0);
	}
}
#else
static void kpb_copy_24b_in_32b(const struct audio_stream __sparse_cache *source,
				uint32_t ioffset, struct audio_stream __sparse_cache *sink,
				uint32_t ooffset, uint32_t samples)
{
	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dst = audio_stream_get_wptr(sink);
	int processed;
	int nmax, i, n;

	src += ioffset;
	dst += ooffset;
	for (processed = 0; processed < samples; processed += n) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		n = samples - processed;
		nmax = KPB_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(source, src));
		n = MIN(n, nmax);
		nmax = KPB_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(sink, dst));
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*dst = *src << 8;
			src++;
			dst++;
		}
	}
}
#endif
/**
 * \brief Copy data samples safe, according to configuration.
 *
 * \param[in] sink - pointer to sink buffer.
 * \param[in] source - pointer to source buffer.
 * \param[in] size - requested copy size in bytes.
 *
 * \return none.
 */
static void kpb_copy_samples(struct comp_buffer __sparse_cache *sink,
			     struct comp_buffer __sparse_cache *source, size_t size,
			     size_t sample_width, uint32_t channels)
{
	struct audio_stream __sparse_cache *istream = &source->stream;
	struct audio_stream __sparse_cache *ostream = &sink->stream;
	unsigned int samples;

	buffer_stream_invalidate(source, size);
	switch (sample_width) {
#if CONFIG_FORMAT_S16LE
	case 16:
		audio_stream_copy(istream, 0, ostream, 0, KPB_BYTES_TO_S16_SAMPLES(size));
		break;
#endif
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	case 24:
		samples = size / ((sample_width >> 3) * channels);
		kpb_copy_24b_in_32b(istream, 0, ostream, 0, samples);
		break;
	case 32:
		audio_stream_copy(istream, 0, ostream, 0, KPB_BYTES_TO_S32_SAMPLES(size));
		break;
#endif
	default:
		comp_cl_err(&comp_kpb, "KPB: An attempt to copy not supported format!");
		return;
	}

	buffer_stream_writeback(sink, size);
}

/**
 * \brief Reset history buffer.
 * \param[in] buff - pointer to current history buffer.
 *
 * \return none.
 */
static void kpb_reset_history_buffer(struct history_buffer *buff)
{
	struct history_buffer *first_buff = buff;

	comp_cl_info(&comp_kpb, "kpb_reset_history_buffer()");

	if (!buff)
		return;

	kpb_clear_history_buffer(buff);

	do {
		buff->w_ptr = buff->start_addr;
		buff->r_ptr = buff->start_addr;
		buff->state = KPB_BUFFER_FREE;

		buff = buff->next;

	} while (buff != first_buff);
}

static inline bool validate_host_params(struct comp_dev *dev,
					size_t host_period_size,
					size_t host_buffer_size,
					size_t hb_size_req)
{
	/* The aim of this function is to perform basic check of host params
	 * and reject them if they won't allow for stable draining.
	 * Note however that this is highly recommended for host buffer to
	 * be at least twice the history buffer size. This will quarantee
	 * "safe" draining.
	 * By safe we mean no XRUNs(host was unable to read data on time),
	 * or loss of data due to host delayed read. The later condition
	 * is very likely after wake up from power state like d0ix.
	 */
	struct comp_data *kpb = comp_get_drvdata(dev);
	size_t sample_width = kpb->config.sampling_width;
	size_t bytes_per_ms = KPB_SAMPLES_PER_MS *
			      (KPB_SAMPLE_CONTAINER_SIZE(sample_width) / 8) *
			      kpb->config.channels;
	size_t pipeline_period_size = (dev->pipeline->period / 1000)
					* bytes_per_ms;

	if (!host_period_size || !host_buffer_size) {
		/* Wrong host params */
		comp_err(dev, "kpb: host_period_size (%d) cannot be 0 and host_buffer_size (%d) cannot be 0",
			 host_period_size, host_buffer_size);
		return false;
	} else if (HOST_BUFFER_MIN_SIZE(hb_size_req, kpb->config.channels) >
		   host_buffer_size) {
		/* Host buffer size is too small - history data
		 * may get overwritten.
		 */
		comp_warn(dev, "kpb: host_buffer_size (%d) must be at least %d",
			  host_buffer_size,
			  HOST_BUFFER_MIN_SIZE(hb_size_req, kpb->config.channels));
	} else if (kpb->sync_draining_mode) {
		/* Sync draining allowed. Check if we can perform draining
		 * with current settings.
		 * In this mode we copy host period size to host
		 * (to avoid overwrite of buffered data by real time stream
		 * this period shall be bigger than pipeline period) and
		 * give host some time to read it. Therefore, in worst
		 * case scenario, we copy one period of real time data + some
		 * of buffered data.
		 */
		if ((host_period_size / KPB_DRAIN_NUM_OF_PPL_PERIODS_AT_ONCE) <
		    pipeline_period_size) {
			comp_err(dev, "kpb: host_period_size (%d) must be at least %d * %d",
				 host_period_size,
				 KPB_DRAIN_NUM_OF_PPL_PERIODS_AT_ONCE,
				 pipeline_period_size);
			return false;
		}
	}

	return true;
}

/**
 * \brief Change KPB state and log this change internally.
 * \param[in] kpb - KPB component data pointer.
 * \param[in] state - current KPB state.
 *
 * \return none.
 */
static inline void kpb_change_state(struct comp_data *kpb,
				    enum kpb_state state)
{
	comp_cl_dbg(&comp_kpb, "kpb_change_state(): from %d to %d",
		    kpb->state, state);
	kpb->state = state;
	kpb->state_log = (kpb->state_log << 4) | state;
}

static int kpb_set_micselect(struct comp_dev *dev, const void *data,
			     int max_data_size)
{
	const struct kpb_micselector_config *mic_sel = data;
	struct comp_data *kpb = comp_get_drvdata(dev);
	const size_t mic_cnt = kpb->config.channels - KPB_REFERENCE_SUPPORT_CHANNELS;
	const uint8_t valid_mask = KPB_COUNT_TO_BITMASK(mic_cnt);
	size_t i;

	if ((valid_mask & mic_sel->mask) == 0) {
		comp_err(dev, "error: invalid micselector bit mask");
		return -EINVAL;
	}
	 /* selected mics counter */
	size_t num_of_sel_mic = 0;

	for (i = 0; i < mic_cnt; i++) {
		if (KPB_IS_BIT_SET(mic_sel->mask, i)) {
			kpb->offsets[num_of_sel_mic] = i;
			num_of_sel_mic++;
		}
	}
	kpb->num_of_sel_mic = num_of_sel_mic;
	kpb->num_of_in_channels = kpb->config.channels;
	kpb->mic_sel.mask = mic_sel->mask;
	return 0;
}

static int kpb_set_large_config(struct comp_dev *dev, uint32_t param_id,
				bool first_block,
				bool last_block,
				uint32_t data_offset,
				const char *data)
{
	comp_info(dev, "kpb_set_large_config()");

	switch (param_id) {
	case KP_BUF_CLIENT_MIC_SELECT:
		return kpb_set_micselect(dev, data, data_offset);
	default:
		return -EINVAL;
	}
}

static const struct comp_driver comp_kpb = {
	.type = SOF_COMP_KPB,
	.uid = SOF_RT_UUID(kpb_uuid),
	.tctx = &kpb_tr,
	.ops = {
		.create		= kpb_new,
		.free		= kpb_free,
		.cmd		= kpb_cmd,
		.trigger	= kpb_trigger,
		.copy		= kpb_copy,
		.prepare	= kpb_prepare,
		.reset		= kpb_reset,
		.params		= kpb_params,
		.set_large_config = kpb_set_large_config,
#ifdef CONFIG_IPC_MAJOR_4
		.get_attribute	= kpb_get_attribute,
		.bind		= kpb_bind,
		.unbind		= kpb_unbind,
#endif /* CONFIG_IPC_MAJOR_4 */
	},
};

static SHARED_DATA struct comp_driver_info comp_kpb_info = {
	.drv = &comp_kpb,
};

UT_STATIC void sys_comp_kpb_init(void)
{
	comp_register(platform_shared_get(&comp_kpb_info,
					  sizeof(comp_kpb_info)));
}

DECLARE_MODULE(sys_comp_kpb_init);
SOF_MODULE_INIT(kpb, sys_comp_kpb_init);
