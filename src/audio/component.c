// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component_ext.h>
#include <sof/common.h>
#include <sof/debug/telemetry/performance_monitor.h>
#include <rtos/panic.h>
#include <rtos/interrupt.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <rtos/sof.h>
#include <rtos/string.h>
#include <rtos/symbol.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__XCC__)
#include <xtensa/config/core-isa.h>
# if XCHAL_HAVE_HIFI5
#  define STREAMCOPY_HIFI5
# elif XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4
#  define STREAMCOPY_HIFI3
#endif
#endif

LOG_MODULE_REGISTER(component, CONFIG_SOF_LOG_LEVEL);

static SHARED_DATA struct comp_driver_list cd;

/* 7c42ce8b-0108-43d0-9137-56d660478c5f */
DECLARE_SOF_UUID("component", comp_uuid, 0x7c42ce8b, 0x0108, 0x43d0,
		 0x91, 0x37, 0x56, 0xd6, 0x60, 0x47, 0x8c, 0x5f);

DECLARE_TR_CTX(comp_tr, SOF_UUID(comp_uuid), LOG_LEVEL_INFO);

int comp_register(struct comp_driver_info *drv)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	k_spinlock_key_t key;

	key = k_spin_lock(&drivers->lock);
	list_item_prepend(&drv->list, &drivers->list);
	k_spin_unlock(&drivers->lock, key);

	return 0;
}
EXPORT_SYMBOL(comp_register);

void comp_unregister(struct comp_driver_info *drv)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	k_spinlock_key_t key;

	key = k_spin_lock(&drivers->lock);
	list_item_del(&drv->list);
	k_spin_unlock(&drivers->lock, key);
}

/* NOTE: Keep the component state diagram up to date:
 * sof-docs/developer_guides/firmware/components/images/comp-dev-states.pu
 */

int comp_set_state(struct comp_dev *dev, int cmd)
{
	int requested_state = comp_get_requested_state(cmd);

	if (dev->state == requested_state) {
		comp_info(dev, "comp_set_state(), state already set to %u",
			  dev->state);
#ifdef CONFIG_IPC_MAJOR_4
		return 0;
#else
		return COMP_STATUS_STATE_ALREADY_SET;
#endif
	}

	switch (cmd) {
	case COMP_TRIGGER_START:
		if (dev->state != COMP_STATE_PRE_ACTIVE) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_START",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_RELEASE:
		if (dev->state != COMP_STATE_PRE_ACTIVE) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_RELEASE",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_STOP:
		if (dev->state != COMP_STATE_ACTIVE &&
		    dev->state != COMP_STATE_PAUSED) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_STOP",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_PAUSE:
		/* only support pausing for running */
		if (dev->state != COMP_STATE_ACTIVE) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_PAUSE",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_RESET:
		/* reset always succeeds */
		if (dev->state == COMP_STATE_ACTIVE)
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_RESET",
				 dev->state);
		else if (dev->state == COMP_STATE_PAUSED)
			comp_info(dev, "comp_set_state(): state = %u, COMP_TRIGGER_RESET",
				  dev->state);
		break;
	case COMP_TRIGGER_PREPARE:
		if (dev->state != COMP_STATE_READY) {
			comp_err(dev, "comp_set_state(): wrong state = %u, COMP_TRIGGER_PREPARE",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_PRE_START:
		if (dev->state != COMP_STATE_PREPARE) {
			comp_err(dev,
				 "comp_set_state(): wrong state = %u, COMP_TRIGGER_PRE_START",
				 dev->state);
			return -EINVAL;
		}
		break;
	case COMP_TRIGGER_PRE_RELEASE:
		if (dev->state != COMP_STATE_PAUSED) {
			comp_err(dev,
				 "comp_set_state(): wrong state = %u, COMP_TRIGGER_PRE_RELEASE",
				 dev->state);
			return -EINVAL;
		}
		break;
	default:
		return 0;
	}

	dev->state = requested_state;

	return 0;
}
EXPORT_SYMBOL(comp_set_state);

void sys_comp_init(struct sof *sof)
{
	sof->comp_drivers = platform_shared_get(&cd, sizeof(cd));

	list_init(&sof->comp_drivers->list);
	k_spinlock_init(&sof->comp_drivers->lock);
}

void comp_get_copy_limits(struct comp_buffer *source,
			  struct comp_buffer *sink,
			  struct comp_copy_limits *cl)
{
	cl->frames = audio_stream_avail_frames(&source->stream, &sink->stream);
	cl->source_frame_bytes = audio_stream_frame_bytes(&source->stream);
	cl->sink_frame_bytes = audio_stream_frame_bytes(&sink->stream);
	cl->source_bytes = cl->frames * cl->source_frame_bytes;
	cl->sink_bytes = cl->frames * cl->sink_frame_bytes;
}

void comp_get_copy_limits_frame_aligned(const struct comp_buffer *source,
					const struct comp_buffer *sink,
					struct comp_copy_limits *cl)
{
	cl->frames = audio_stream_avail_frames_aligned(&source->stream, &sink->stream);
	cl->source_frame_bytes = audio_stream_frame_bytes(&source->stream);
	cl->sink_frame_bytes = audio_stream_frame_bytes(&sink->stream);
	cl->source_bytes = cl->frames * cl->source_frame_bytes;
	cl->sink_bytes = cl->frames * cl->sink_frame_bytes;
}

#if defined(STREAMCOPY_HIFI5)

#include <xtensa/tie/xt_hifi5.h>

int audio_stream_copy(const struct audio_stream *source, uint32_t ioffset,
		      struct audio_stream *sink, uint32_t ooffset, uint32_t samples)
{
	int ssize = audio_stream_sample_bytes(source); /* src fmt == sink fmt */
	ae_int16x8 *src = (ae_int16x8 *)((int8_t *)audio_stream_get_rptr(source) + ioffset * ssize);
	ae_int16x8 *dst = (ae_int16x8 *)((int8_t *)audio_stream_get_wptr(sink) + ooffset * ssize);
	int shorts = samples * ssize >> 1;
	int shorts_src;
	int shorts_dst;
	int shorts_copied;
	int left, m, i;
	ae_int16x4 in_sample1;
	ae_int16x4 in_sample2;
	ae_valignx2 inu;
	ae_valignx2 outu = AE_ZALIGN128();

	/* copy with 16bit as the minimum unit since the minimum sample size is 16 bit*/
	while (shorts > 0) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		shorts_src = audio_stream_samples_without_wrap_s16(source, src);
		shorts_dst = audio_stream_samples_without_wrap_s16(sink, dst);
		shorts_copied = AE_MIN32(shorts_src, shorts_dst);
		shorts_copied = AE_MIN32(shorts, shorts_copied);
		m = shorts_copied >> 3;
		left = shorts_copied & 0x07;
		inu = AE_LA128_PP(src);
		/* copy 4 * 16bit(8 bytes)per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4X2_IP(in_sample1, in_sample2, inu, src);
			AE_SA16X4X2_IP(in_sample1, in_sample2, outu, dst);
		}
		AE_SA128POS_FP(outu, dst);

		/* process the left bits that less than 4 * 16 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample1, (ae_int16 *)src, sizeof(ae_int16));
			AE_S16_0_IP(in_sample1, (ae_int16 *)dst, sizeof(ae_int16));
		}
		shorts -= shorts_copied;
	}
	return samples;
}

void cir_buf_copy(void *src, void *src_addr, void *src_end, void *dst,
		  void *dst_addr, void *dst_end, size_t byte_size)
{
	size_t bytes = byte_size;
	size_t bytes_src;
	size_t bytes_dst;
	size_t bytes_copied;
	size_t short_copied;
	int left, m, i;
	ae_int16x4 in_sample1, in_sample2;
	ae_valignx2 inu;
	ae_valignx2 outu = AE_ZALIGN128();
	ae_int16x8 *in = (ae_int16x8 *)src;
	ae_int16x8 *out = (ae_int16x8 *)dst;

	while (bytes) {
		bytes_src = cir_buf_bytes_without_wrap(in, src_end);
		bytes_dst = cir_buf_bytes_without_wrap(out, dst_end);
		bytes_copied = MIN(bytes_src, bytes_dst);
		bytes_copied = MIN(bytes, bytes_copied);
		short_copied = bytes_copied >> 1;

		m = short_copied >> 3;
		left = short_copied & 0x07;
		inu = AE_LA128_PP(in);
		/* copy 2 * 4 * 16bit(16 bytes)per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4X2_IP(in_sample1, in_sample2, inu, in);
			AE_SA16X4X2_IP(in_sample1, in_sample2, outu, out);
		}
		AE_SA128POS_FP(outu, out);

		/* process the left bits that less than 2 * 4 * 16 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample1, (ae_int16 *)in, sizeof(ae_int16));
			AE_S16_0_IP(in_sample1, (ae_int16 *)out, sizeof(ae_int16));
		}

		bytes -= bytes_copied;
		in = cir_buf_wrap(in, src_addr, src_end);
		out = cir_buf_wrap(out, dst_addr, dst_end);
	}
}

#elif defined(STREAMCOPY_HIFI3)

#include <xtensa/tie/xt_hifi3.h>

int audio_stream_copy(const struct audio_stream *source, uint32_t ioffset,
		      struct audio_stream *sink, uint32_t ooffset, uint32_t samples)
{
	int ssize = audio_stream_sample_bytes(source); /* src fmt == sink fmt */
	ae_int16x4 *src = (ae_int16x4 *)((int8_t *)audio_stream_get_rptr(source) + ioffset * ssize);
	ae_int16x4 *dst = (ae_int16x4 *)((int8_t *)audio_stream_get_wptr(sink) + ooffset * ssize);
	int shorts = samples * ssize >> 1;
	int shorts_src;
	int shorts_dst;
	int shorts_copied;
	int left, m, i;
	ae_int16x4 in_sample = AE_ZERO16();
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();

	/* copy with 16bit as the minimum unit since the minimum sample size is 16 bit*/
	while (shorts > 0) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		shorts_src = audio_stream_samples_without_wrap_s16(source, src);
		shorts_dst = audio_stream_samples_without_wrap_s16(sink, dst);
		shorts_copied = AE_MIN32(shorts_src, shorts_dst);
		shorts_copied = AE_MIN32(shorts, shorts_copied);
		m = shorts_copied >> 2;
		left = shorts_copied & 0x03;
		inu = AE_LA64_PP(src);
		/* copy 4 * 16bit(8 bytes)per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4_IP(in_sample, inu, src);
			AE_SA16X4_IP(in_sample, outu, dst);
		}
		AE_SA64POS_FP(outu, dst);

		/* process the left bits that less than 4 * 16 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample, (ae_int16 *)src, sizeof(ae_int16));
			AE_S16_0_IP(in_sample, (ae_int16 *)dst, sizeof(ae_int16));
		}
		shorts -= shorts_copied;
	}
	return samples;
}

void cir_buf_copy(void *src, void *src_addr, void *src_end, void *dst,
		  void *dst_addr, void *dst_end, size_t byte_size)
{
	size_t bytes = byte_size;
	size_t bytes_src;
	size_t bytes_dst;
	size_t bytes_copied;
	size_t short_copied;

	int left, m, i;
	ae_int16x4 in_sample = AE_ZERO16();
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu = AE_ZALIGN64();
	ae_int16x4 *in = (ae_int16x4 *)src;
	ae_int16x4 *out = (ae_int16x4 *)dst;

	while (bytes) {
		bytes_src = cir_buf_bytes_without_wrap(in, src_end);
		bytes_dst = cir_buf_bytes_without_wrap(out, dst_end);
		bytes_copied = MIN(bytes_src, bytes_dst);
		bytes_copied = MIN(bytes, bytes_copied);
		short_copied = bytes_copied >> 1;
		m = short_copied >> 2;
		left = short_copied & 0x03;
		inu = AE_LA64_PP(in);
		/* copy 4 * 16bit(8 bytes)per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4_IP(in_sample, inu, in);
			AE_SA16X4_IP(in_sample, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left bits that less than 4 * 16 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample, (ae_int16 *)in, sizeof(ae_int16));
			AE_S16_0_IP(in_sample, (ae_int16 *)out, sizeof(ae_int16));
		}

		bytes -= bytes_copied;
		in = cir_buf_wrap(in, src_addr, src_end);
		out = cir_buf_wrap(out, dst_addr, dst_end);
	}
}

#else

int audio_stream_copy(const struct audio_stream *source, uint32_t ioffset,
		      struct audio_stream *sink, uint32_t ooffset, uint32_t samples)
{
	int ssize = audio_stream_sample_bytes(source); /* src fmt == sink fmt */
	uint8_t *src = audio_stream_wrap(source, (uint8_t *)audio_stream_get_rptr(source) +
					 ioffset * ssize);
	uint8_t *snk = audio_stream_wrap(sink, (uint8_t *)audio_stream_get_wptr(sink) +
					 ooffset * ssize);
	size_t bytes = samples * ssize;
	size_t bytes_src;
	size_t bytes_snk;
	size_t bytes_copied;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		bytes_copied = MIN(bytes_src, bytes_snk);
		bytes_copied = MIN(bytes, bytes_copied);
		memcpy(snk, src, bytes_copied);
		bytes -= bytes_copied;
		src = audio_stream_wrap(source, src + bytes_copied);
		snk = audio_stream_wrap(sink, snk + bytes_copied);
	}

	return samples;
}

void cir_buf_copy(void *src, void *src_addr, void *src_end, void *dst,
		  void *dst_addr, void *dst_end, size_t byte_size)
{
	size_t bytes = byte_size;
	size_t bytes_src;
	size_t bytes_dst;
	size_t bytes_copied;
	uint8_t *in = (uint8_t *)src;
	uint8_t *out = (uint8_t *)dst;

	while (bytes) {
		bytes_src = cir_buf_bytes_without_wrap(in, src_end);
		bytes_dst = cir_buf_bytes_without_wrap(out, dst_end);
		bytes_copied = MIN(bytes_src, bytes_dst);
		bytes_copied = MIN(bytes, bytes_copied);
		memcpy_s(out, bytes_copied, in, bytes_copied);
		bytes -= bytes_copied;
		in = cir_buf_wrap(in + bytes_copied, src_addr, src_end);
		out = cir_buf_wrap(out + bytes_copied, dst_addr, dst_end);
	}
}

#endif
EXPORT_SYMBOL(audio_stream_copy);

void audio_stream_copy_from_linear(const void *linear_source, int ioffset,
				   struct audio_stream *sink, int ooffset,
				   unsigned int samples)
{
	int ssize = audio_stream_sample_bytes(sink); /* src fmt == sink fmt */
	uint8_t *src = (uint8_t *)linear_source + ioffset * ssize;
	uint8_t *snk = audio_stream_wrap(sink, (uint8_t *)audio_stream_get_wptr(sink) +
					 ooffset * ssize);
	size_t bytes = samples * ssize;
	size_t bytes_snk;
	size_t bytes_copied;

	while (bytes) {
		bytes_snk = audio_stream_bytes_without_wrap(sink, snk);
		bytes_copied = MIN(bytes, bytes_snk);
		memcpy(snk, src, bytes_copied);
		bytes -= bytes_copied;
		src += bytes_copied;
		snk = audio_stream_wrap(sink, snk + bytes_copied);
	}
}

void audio_stream_copy_to_linear(const struct audio_stream *source, int ioffset,
				 void *linear_sink, int ooffset, unsigned int samples)
{
	int ssize = audio_stream_sample_bytes(source); /* src fmt == sink fmt */
	uint8_t *src = audio_stream_wrap(source, (uint8_t *)audio_stream_get_rptr(source) +
					 ioffset * ssize);
	uint8_t *snk = (uint8_t *)linear_sink + ooffset * ssize;
	size_t bytes = samples * ssize;
	size_t bytes_src;
	size_t bytes_copied;

	while (bytes) {
		bytes_src = audio_stream_bytes_without_wrap(source, src);
		bytes_copied = MIN(bytes, bytes_src);
		memcpy(snk, src, bytes_copied);
		bytes -= bytes_copied;
		src = audio_stream_wrap(source, src + bytes_copied);
		snk += bytes_copied;
	}
}

/** See comp_ops::copy */
int comp_copy(struct comp_dev *dev)
{
	int ret = 0;

	assert(dev->drv->ops.copy);

	/* copy only if we are the owner of component OR this is DP component
	 *
	 * DP components (modules) require two stage processing:
	 *
	 *   LL_mod -> [comp_buffer -> dp_queue] -> dp_mod -> [dp_queue -> comp_buffer] -> LL_mod
	 *
	 *  - in first step (it means - now) the pipeline must copy source data from comp_buffer
	 *    to dp_queue and result data from dp_queue to comp_buffer
	 *
	 *  - second step will be performed by a thread specific to the DP module - DP module
	 *    will take data from input dpQueue (using source API) , process it
	 *    and put in output DP queue (using sink API)
	 *
	 * this allows the current pipeline structure to see a DP module as a "normal" LL
	 *
	 * to be removed when pipeline 2.0 is ready
	 */
	if (cpu_is_me(dev->ipc_config.core) ||
	    dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
#if CONFIG_PERFORMANCE_COUNTERS
		perf_cnt_init(&dev->pcd);
#endif

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
		const uint32_t begin_stamp = (uint32_t)sof_cycle_get_64();
#endif

		ret = dev->drv->ops.copy(dev);

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
		const uint32_t cycles_consumed = (uint32_t)sof_cycle_get_64() - begin_stamp;

		comp_update_performance_data(dev, cycles_consumed);
#endif

#if CONFIG_PERFORMANCE_COUNTERS
		perf_cnt_stamp(&dev->pcd, perf_trace_null, dev);
		perf_cnt_average(&dev->pcd, comp_perf_avg_info, dev);
#endif
	}

	return ret;
}

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
void comp_init_performance_data(struct comp_dev *dev)
{
	struct perf_data_item_comp *item = dev->perf_data.perf_data_item;

	if (item)
		perf_data_item_comp_init(item, dev->ipc_config.id, 0);
}

/* returns true if budget violation occurred */
static bool update_peak_of_measured_cpc(struct comp_dev *dev, size_t measured_cpc)
{
	if (measured_cpc <= dev->perf_data.peak_of_measured_cpc)
		return false;
	dev->perf_data.peak_of_measured_cpc = measured_cpc;
	return measured_cpc > dev->cpc;
}

bool comp_update_performance_data(struct comp_dev *dev, uint32_t cycles_used)
{
	struct perf_data_item_comp *item = dev->perf_data.perf_data_item;

	if (perf_meas_get_state() == IPC4_PERF_MEASUREMENTS_STARTED) {
		/* we divide by ibs so we need to check if its set */
		if (item && dev->ibs != 0) {
			item->total_iteration_count++;
			if (item->total_iteration_count == 0) {
				/* We can't allow count to overflow to 0. Overflow will also make
				 * some of the results incorrect. We don't want to crash in this
				 * case, so we just log it. We also reset cycles counter to make
				 * avg correct again.
				 */
				item->total_iteration_count = 1;
				item->total_cycles_consumed = 0;
				tr_err(&ipc_tr,
				       "overflow for module %#x, performance measurement incorrect",
				       dev_comp_id(dev));
			}
			item->total_cycles_consumed += cycles_used;
			item->item.avg_kcps = item->total_cycles_consumed * dev->ll_chunk_size
				/ (dev->ibs * item->total_iteration_count);
			item->item.peak_kcps =
				MAX(item->item.peak_kcps, (cycles_used * dev->ll_chunk_size)
				/ dev->ibs);
		}
	}
	return update_peak_of_measured_cpc(dev, cycles_used);
}
#endif

#if CONFIG_IPC_MAJOR_4
static uint32_t get_sample_group_size_in_bytes(const struct ipc4_audio_format fmt)
{
	return (fmt.depth >> 3) * fmt.channels_count;
}

static uint32_t get_one_ms_in_bytes(const struct ipc4_audio_format fmt)
{
	/* TODO Reference Firmware also has systick multiplier and divider in this equation */
	return get_sample_group_size_in_bytes(fmt) *
			SOF_DIV_ROUND_UP(fmt.sampling_frequency, 1000);
}
#endif

void comp_update_ibs_obs_cpc(struct comp_dev *dev)
{
#if CONFIG_IPC_MAJOR_4
	int ret;
	struct ipc4_base_module_cfg dev_cfg;

	ret = comp_get_attribute(dev, COMP_ATTR_BASE_CONFIG, &dev_cfg);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to get base config for module %#x",
		       dev_comp_id(dev));
		/* set neutral values */
		dev->ll_chunk_size = 0;
		dev->cpc = 0;
		dev->obs = 0;
		dev->ibs = 0;
	}
	dev->ll_chunk_size = get_one_ms_in_bytes(dev_cfg.audio_fmt);
	dev->obs = dev_cfg.obs;
	dev->ibs = dev_cfg.ibs;
	dev->cpc = dev_cfg.cpc;
#else
	/* set neutral values */
	dev->ll_chunk_size = 0;
	dev->cpc = 0;
	dev->obs = 0;
	dev->ibs = 0;
#endif
}

