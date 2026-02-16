// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//

#include <sof/audio/cadence/xa_type_def.h>
#include <sof/audio/cadence/xa_error_standards.h>
#include <sof/audio/cadence/xa_apicmd_standards.h>
#include <sof/audio/cadence/xa_memory_standards.h>
#include <sof/audio/compress_other/pcm_dec/xa_pcm_dec_api.h>
#include <rtos/string.h>
#include <stdint.h>

/* For XA_API_CMD_GET_MEM_INFO_SIZE */
#define PCM_DEC_IN_BUF_SIZE 16384
#define PCM_DEC_OUT_BUF_SIZE 16384

/* Maximum library-name buffer size advertised to callers of
 * XA_API_CMD_GET_LIB_ID_STRINGS. Must stay <= LIB_NAME_MAX_LEN (30) from
 * sof/audio/module_adapter/module/cadence.h, which is the size of the
 * destination buffer (cadence_codec_data::name).
 */
#define XA_PCM_DEC_LIB_NAME_MAX 30

/* Number of XA_CMD_TYPE_DO_EXECUTE calls after input_over before declaring
 * end-of-stream when the host keeps providing full input buffers. PCM
 * passthrough has no internal state to detect end-of-data, so this is a
 * timeout fallback used when the input buffer never shrinks below the full
 * size. With a 16 KB input buffer at 48 kHz stereo S16, each full buffer is
 * ~85 ms, so ~12 calls bound the trailing silence to ~1 s.
 */
#define PCM_DEC_EOS_FULL_BUF_COUNT 12

/* PCM decoder state structure */
struct xa_pcm_dec_state {
	/* Configuration parameters */
	uint32_t sample_rate;
	uint32_t num_channels;
	uint32_t pcm_width;

	/* State variables */
	uint32_t bytes_consumed;
	uint32_t bytes_produced;
	uint32_t init_done;
	uint32_t exec_done;
	uint32_t input_over;
	uint32_t eos_full_buf_count;

	/* Memory pointers */
	void *input_buf;
	void *output_buf;
	uint32_t output_buf_size;
	uint32_t input_bytes;
};

static const char lib_name[] = "PCM Decoder";

/* Main codec API function */
XA_ERRORCODE xa_pcm_dec(xa_codec_handle_t handle, WORD32 cmd, WORD32 idx, pVOID value)
{
	struct xa_pcm_dec_state *state = (struct xa_pcm_dec_state *)handle;

	/* Commands that don't need initialized state */
	switch (cmd) {
	case XA_API_CMD_GET_API_SIZE:
		*(WORD32 *)value = sizeof(struct xa_pcm_dec_state);
		return XA_NO_ERROR;

	case XA_API_CMD_GET_LIB_ID_STRINGS:
		if (idx == XA_CMD_TYPE_LIB_NAME) {
			memcpy_s(value, XA_PCM_DEC_LIB_NAME_MAX,
				 lib_name, sizeof(lib_name));
			return XA_NO_ERROR;
		}
		return XA_API_FATAL_INVALID_CMD_TYPE;

	case XA_API_CMD_GET_MEMTABS_SIZE:
		/* PCM decoder needs minimal memtabs structure */
		*(WORD32 *)value = 4;
		return XA_NO_ERROR;

	case XA_API_CMD_SET_MEMTABS_PTR:
		/* PCM decoder doesn't use memtabs, just return success */
		return XA_NO_ERROR;

	default:
		break;
	}

	/* All other commands need initialized state */
	if (!handle)
		return XA_PCMDEC_EXECUTE_FATAL_UNINITIALIZED;

	switch (cmd) {
	case XA_API_CMD_INIT:
		switch (idx) {
		case XA_CMD_TYPE_INIT_API_PRE_CONFIG_PARAMS:
			/* Initialize with default values */
			bzero(state, sizeof(*state));
			state->sample_rate = 48000;
			state->num_channels = 2;
			state->pcm_width = 16;
			return XA_NO_ERROR;

		case XA_CMD_TYPE_INIT_API_POST_CONFIG_PARAMS:
			/* Nothing to do here for simple PCM decoder */
			return XA_NO_ERROR;

		case XA_CMD_TYPE_INIT_PROCESS:
			state->init_done = 1;
			return XA_NO_ERROR;

		case XA_CMD_TYPE_INIT_DONE_QUERY:
			*(WORD32 *)value = state->init_done;
			return XA_NO_ERROR;

		default:
			return XA_API_FATAL_INVALID_CMD_TYPE;
		}

	case XA_API_CMD_SET_CONFIG_PARAM:
		switch (idx) {
		case XA_PCM_DEC_CONFIG_PARAM_SAMPLE_RATE:
			state->sample_rate = *(WORD32 *)value;
			return XA_NO_ERROR;

		case XA_PCM_DEC_CONFIG_PARAM_CHANNELS:
			state->num_channels = *(WORD32 *)value;
			return XA_NO_ERROR;

		case XA_PCM_DEC_CONFIG_PARAM_PCM_WIDTH: {
			WORD32 width = *(WORD32 *)value;

			if (width != 8 && width != 16 && width != 24 && width != 32)
				return XA_PCMDEC_CONFIG_NONFATAL_INVALID_PCM_WIDTH;
			state->pcm_width = width;
			return XA_NO_ERROR;
		}

		case XA_PCM_DEC_CONFIG_PARAM_INTERLEAVE:
			return XA_NO_ERROR;

		default:
			return XA_API_FATAL_INVALID_CMD_TYPE;
		}

	case XA_API_CMD_GET_CONFIG_PARAM:
		switch (idx) {
		case XA_PCM_DEC_CONFIG_PARAM_SAMPLE_RATE:
			*(WORD32 *)value = state->sample_rate;
			return XA_NO_ERROR;

		case XA_PCM_DEC_CONFIG_PARAM_CHANNELS:
			*(WORD32 *)value = state->num_channels;
			return XA_NO_ERROR;

		case XA_PCM_DEC_CONFIG_PARAM_PCM_WIDTH:
			*(WORD32 *)value = state->pcm_width;
			return XA_NO_ERROR;

		case XA_PCM_DEC_CONFIG_PARAM_PRODUCED:
			*(WORD32 *)value = state->bytes_produced;
			return XA_NO_ERROR;

		default:
			return XA_API_FATAL_INVALID_CMD_TYPE;
		}

	case XA_API_CMD_GET_N_MEMTABS:
		/* We need 2 memory tables: input and output buffers */
		*(WORD32 *)value = 2;
		return XA_NO_ERROR;

	case XA_API_CMD_GET_MEM_INFO_TYPE:
		if (idx == 0)
			*(WORD32 *)value = XA_MEMTYPE_INPUT;
		else if (idx == 1)
			*(WORD32 *)value = XA_MEMTYPE_OUTPUT;
		else
			return XA_API_FATAL_INVALID_CMD_TYPE;
		return XA_NO_ERROR;

	case XA_API_CMD_GET_MEM_INFO_SIZE:
		if (idx == 0)
			*(WORD32 *)value = PCM_DEC_IN_BUF_SIZE;
		else if (idx == 1)
			*(WORD32 *)value = PCM_DEC_OUT_BUF_SIZE;
		else
			return XA_API_FATAL_INVALID_CMD_TYPE;
		return XA_NO_ERROR;

	case XA_API_CMD_GET_MEM_INFO_ALIGNMENT:
		*(WORD32 *)value = 4; /* 4-byte alignment */
		return XA_NO_ERROR;

	case XA_API_CMD_SET_MEM_PTR:
		if (idx == 0) {
			state->input_buf = value;
		} else if (idx == 1) {
			state->output_buf = value;
			state->output_buf_size = PCM_DEC_OUT_BUF_SIZE;
		} else {
			return XA_API_FATAL_INVALID_CMD_TYPE;
		}
		return XA_NO_ERROR;

	case XA_API_CMD_SET_INPUT_BYTES:
		state->input_bytes = *(WORD32 *)value;
		state->bytes_consumed = 0;
		if (state->input_bytes > 0)
			state->exec_done = 0;
		return XA_NO_ERROR;

	case XA_API_CMD_GET_OUTPUT_BYTES:
		*(WORD32 *)value = state->bytes_produced;
		return XA_NO_ERROR;

	case XA_API_CMD_GET_CURIDX_INPUT_BUF:
		*(WORD32 *)value = state->bytes_consumed;
		return XA_NO_ERROR;

	case XA_API_CMD_INPUT_OVER:
		/* Indicate no more input buffers will be provided */
		state->input_over = 1;
		return XA_NO_ERROR;

	case XA_API_CMD_GET_N_TABLES:
		/* PCM decoder doesn't use tables */
		*(WORD32 *)value = 0;
		return XA_NO_ERROR;

	case XA_API_CMD_GET_TABLE_PTR:
	case XA_API_CMD_SET_TABLE_PTR:
	case XA_API_CMD_GET_TABLE_INFO_SIZE:
	case XA_API_CMD_GET_TABLE_INFO_ALIGNMENT:
	case XA_API_CMD_GET_TABLE_INFO_PRIORITY:
		/* PCM decoder doesn't use tables, return success */
		return XA_NO_ERROR;

	case XA_API_CMD_GET_MEM_INFO_PLACEMENT:
	case XA_API_CMD_GET_MEM_INFO_PRIORITY:
	case XA_API_CMD_SET_MEM_INFO_SIZE:
	case XA_API_CMD_SET_MEM_PLACEMENT:
		/* Return success for optional memory info commands */
		return XA_NO_ERROR;

	case XA_API_CMD_EXECUTE:
		if (idx == XA_CMD_TYPE_DO_EXECUTE) {
			uint32_t to_copy;

			state->bytes_produced = 0;
			state->bytes_consumed = 0;

			/*
			 * Buffers must have been set via XA_API_CMD_SET_MEM_PTR
			 * before EXECUTE. Treat a missing pointer as a fatal
			 * uninitialized error so the caller fails fast instead
			 * of silently dropping audio.
			 */
			if (!state->input_buf || !state->output_buf)
				return XA_PCMDEC_EXECUTE_FATAL_UNINITIALIZED;

			/* No data and source still live: signal the wrapper to
			 * skip this round without treating it as an error.
			 */
			if (state->input_bytes == 0 && !state->input_over)
				return XA_PCMDEC_EXECUTE_NONFATAL_INSUFFICIENT_DATA;

			/* Copy PCM data from input to output */
			to_copy = state->input_bytes;
			if (to_copy > state->output_buf_size)
				to_copy = state->output_buf_size;

			if (to_copy > 0) {
				memcpy_s(state->output_buf, state->output_buf_size,
					 state->input_buf, to_copy);
				state->bytes_produced = to_copy;
				state->bytes_consumed = to_copy;
			}

			/* PCM passthrough has no internal state to flush. Two
			 * end-of-stream paths:
			 * 1. Deterministic: a partial input buffer after
			 *    input_over means the source is drained.
			 * 2. Fallback: if the host keeps delivering full
			 *    buffers (typical when the DMA ring is kept full),
			 *    declare done after a bounded number of calls past
			 *    input_over so playback ends within ~1 s of the
			 *    real end-of-data.
			 */
			if (state->input_over) {
				if (state->input_bytes < PCM_DEC_IN_BUF_SIZE) {
					state->exec_done = 1;
				} else if (++state->eos_full_buf_count >
					   PCM_DEC_EOS_FULL_BUF_COUNT) {
					state->exec_done = 1;
				}
			}

			return XA_NO_ERROR;
		} else if (idx == XA_CMD_TYPE_DONE_QUERY) {
			/* Query if execution is done */
			*(WORD32 *)value = state->exec_done;
			return XA_NO_ERROR;
		}
		return XA_API_FATAL_INVALID_CMD_TYPE;

	default:
		return XA_API_FATAL_INVALID_CMD;
	}
}
