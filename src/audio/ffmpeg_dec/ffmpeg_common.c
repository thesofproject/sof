// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Shared buffer, extradata, ID3v2, and IPC4 EOS helpers across FFmpeg
// decoder, encoder, and audio-filter modules.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/source_api.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

#if CONFIG_IPC_MAJOR_4
#include <sof/audio/audio_buffer.h>
#include <sof/ipc/msg.h>
#include <ipc4/module.h>
#include <ipc4/notification.h>
#endif

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

static inline size_t ffmpeg_common_min(size_t a, size_t b)
{
	return a < b ? a : b;
}

#if CONFIG_IPC_MAJOR_4
__cold struct ipc_msg *ffmpeg_dec_eos_notification_init(struct processing_module *mod)
{
	struct comp_ipc_config *ipc_config = &mod->dev->ipc_config;
	struct sof_ipc4_notify_module_data *msg_data;
	union ipc4_notification_header primary;
	struct ipc_msg *msg;

	primary.dat = 0;
	primary.r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
	primary.r.type = SOF_IPC4_GLB_NOTIFICATION;
	primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;

	msg = mod_ipc_msg_w_ext_init(mod, primary.dat, 0, sizeof(*msg_data));
	if (!msg)
		return NULL;

	msg_data = (struct sof_ipc4_notify_module_data *)msg->tx_data;
	msg_data->instance_id = IPC4_INST_ID(ipc_config->id);
	msg_data->module_id = IPC4_MOD_ID(ipc_config->id);
	msg_data->event_id = SOF_IPC4_NOTIFY_MODULE_EVENTID_COMPR_MAGIC_VAL;
	msg_data->event_data_size = 0;

	return msg;
}

void ffmpeg_dec_signal_eos(struct processing_module *mod, struct sof_sink *sink)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	if (cd->eos_sent || !cd->eos_msg)
		return;

	ipc_msg_send(cd->eos_msg, NULL, false);
	cd->eos_sent = true;
	audio_buffer_set_eos(sof_audio_buffer_from_sink(sink));
	comp_info(mod->dev, "EOS: compress drain complete");
}
#endif /* CONFIG_IPC_MAJOR_4 */

int ffmpeg_dec_store_extradata(struct processing_module *mod,
			       const uint8_t *data, size_t size)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (!size)
		return 0;

	if (size > FFMPEG_DEC_MAX_EXTRADATA) {
		comp_err(dev, "extradata too large: %zu", size);
		return -EINVAL;
	}

	memcpy_s(cd->extradata, sizeof(cd->extradata), data, size);
	cd->extradata_size = size;
	comp_info(dev, "stored %zu bytes of codec extradata", size);
	return 0;
}

void ffmpeg_dec_copy_from_circular(void *dst, const void *src,
				   const void *buf_start, size_t buf_size,
				   size_t bytes)
{
	size_t to_end = (size_t)((const uint8_t *)buf_start + buf_size -
				 (const uint8_t *)src);

	if (to_end >= bytes) {
		memcpy_s(dst, bytes, src, bytes);
		return;
	}
	memcpy_s(dst, to_end, src, to_end);
	memcpy_s((uint8_t *)dst + to_end, bytes - to_end,
		 buf_start, bytes - to_end);
}

void ffmpeg_dec_copy_to_circular(void *dst, void *buf_start,
				 size_t buf_size, const void *src,
				 size_t bytes)
{
	size_t to_end = (size_t)((uint8_t *)buf_start + buf_size -
				 (uint8_t *)dst);

	if (to_end >= bytes) {
		memcpy_s(dst, bytes, src, bytes);
		return;
	}
	memcpy_s(dst, to_end, src, to_end);
	memcpy_s(buf_start, bytes - to_end,
		 (const uint8_t *)src + to_end, bytes - to_end);
}

int ffmpeg_dec_skip_id3v2(struct processing_module *mod, struct sof_source *src)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	const void *sp, *sstart;
	size_t sbytes, avail, req;
	int ret;

	/* Drain any pending skip bytes from a previous chunk */
	if (cd->skip_bytes > 0) {
		avail = source_get_data_available(src);
		if (!avail)
			return -ENODATA;
		size_t to_skip = ffmpeg_common_min(avail, cd->skip_bytes);

		source_drop_data(src, to_skip);
		cd->skip_bytes -= to_skip;
		if (cd->skip_bytes > 0)
			return -ENODATA;
	}

	avail = source_get_data_available(src);
	if (avail < 4)
		return -ENODATA;

	req = ffmpeg_common_min(avail, cd->in_buf_size);
	ret = source_get_data(src, req, &sp, &sstart, &sbytes);
	if (ret)
		return ret;
	ffmpeg_dec_copy_from_circular(cd->in_buf, sp, sstart, sbytes, req);
	source_release_data(src, 0);

	/* ID3v2 tag: 10-byte header with 28-bit syncsafe length */
	if (memcmp(cd->in_buf, "ID3", 3) == 0) {
		uint32_t tag;
		size_t total_tag;

		if (req < 10)
			return -ENODATA;

		tag = ((uint32_t)(cd->in_buf[6] & 0x7f) << 21) |
		      ((uint32_t)(cd->in_buf[7] & 0x7f) << 14) |
		      ((uint32_t)(cd->in_buf[8] & 0x7f) << 7) |
		      (uint32_t)(cd->in_buf[9] & 0x7f);
		total_tag = 10 + tag + ((cd->in_buf[5] & 0x10) ? 10 : 0);
		comp_info(dev, "ID3v2 tag detected: %zu bytes", total_tag);

		if (avail >= total_tag) {
			source_drop_data(src, total_tag);
			return 1;
		}

		source_drop_data(src, avail);
		cd->skip_bytes = total_tag - avail;
		return -ENODATA;
	}

	return 0;
}
