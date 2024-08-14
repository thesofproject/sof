// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <rtos/symbol.h>

#include <sof/audio/audio_stream.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/buffer.h>

static uint32_t audio_stream_frame_align_get(const uint32_t byte_align,
					     const uint32_t frame_align_req,
					     uint32_t frame_size)
{
	/* Figure out how many frames are needed to meet the byte_align alignment requirements */
	uint32_t frame_num = byte_align / gcd(byte_align, frame_size);

	/** return the lcm of frame_num and frame_align_req*/
	return frame_align_req * frame_num / gcd(frame_num, frame_align_req);
}


void audio_stream_recalc_align(struct audio_stream *stream)
{
	const uint32_t byte_align = stream->byte_align_req;
	const uint32_t frame_align_req = stream->frame_align_req;
	uint32_t process_size;
	uint32_t frame_size = audio_stream_frame_bytes(stream);

	stream->runtime_stream_params.align_frame_cnt =
			audio_stream_frame_align_get(byte_align, frame_align_req, frame_size);
	process_size = stream->runtime_stream_params.align_frame_cnt * frame_size;
	stream->runtime_stream_params.align_shift_idx	=
			(is_power_of_2(process_size) ? 31 : 32) - clz(process_size);
}

void audio_stream_set_align(const uint32_t byte_align,
					   const uint32_t frame_align_req,
					   struct audio_stream *stream)
{
	stream->byte_align_req = byte_align;
	stream->frame_align_req = frame_align_req;
	audio_stream_recalc_align(stream);
}
EXPORT_SYMBOL(audio_stream_set_align);
void audio_stream_init(struct audio_stream *audio_stream, void *buff_addr, uint32_t size)
{
	audio_stream->size = size;
	audio_stream->addr = buff_addr;
	audio_stream->end_addr = (char *)audio_stream->addr + size;

	audio_stream_set_align(1, 1, audio_stream);
	audio_stream_reset(audio_stream);
}
