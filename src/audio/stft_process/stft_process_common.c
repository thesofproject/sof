// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/auditory.h>
#include <sof/math/matrix.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>

#include "stft_process.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if STFT_DEBUG
extern FILE *stft_debug_fft_in_fh;
extern FILE *stft_debug_fft_out_fh;
extern FILE *stft_debug_ifft_out_fh;

static void debug_print_to_file_real(FILE *fh, struct icomplex32 *c, int n)
{
	for (int i = 0; i < n; i++)
		fprintf(fh, "%d\n", c[i].real);
}

static void debug_print_to_file_complex(FILE *fh, struct icomplex32 *c, int n)
{
	for (int i = 0; i < n; i++)
		fprintf(fh, "%d %d\n", c[i].real, c[i].imag);
}
#endif

LOG_MODULE_REGISTER(stft_process_common, CONFIG_SOF_LOG_LEVEL);

/*
 * The main processing function for STFT_PROCESS
 */

static int stft_prepare_fft(struct stft_process_state *state, int channel)
{
	struct stft_process_buffer *ibuf = &state->ibuf[channel];
	struct stft_process_fft *fft = &state->fft;

	/* Wait for FFT hop size of new data */
	if (ibuf->s_avail < fft->fft_hop_size)
		return 0;

	return 1;
}

static void stft_do_fft(struct stft_process_state *state, int ch)
{
	struct stft_process_fft *fft = &state->fft;

	/* Copy data to FFT input buffer from overlap buffer and from new samples buffer */
	stft_process_fill_fft_buffer(state, ch);

	/* Window function */
	stft_process_apply_window(state);

#if STFT_DEBUG
	debug_print_to_file_real(stft_debug_fft_in_fh, fft->fft_buf, fft->fft_size);
#endif

	/* Compute FFT. A full scale s16 sine input with 2^N samples period in low
	 * part of s32 real part and zero imaginary part gives to output about 0.5
	 * full scale 32 bit output to real and imaginary. The scaling is same for
	 * all FFT sizes.
	 */
	fft_multi_execute_32(fft->fft_plan, false);

#if STFT_DEBUG
	debug_print_to_file_complex(stft_debug_fft_out_fh, fft->fft_out, fft->fft_size);
#endif
}

static void stft_do_ifft(struct stft_process_state *state, int ch)
{
	struct stft_process_fft *fft = &state->fft;

	/* Compute IFFT */
	fft_multi_execute_32(fft->ifft_plan, true);

#if STFT_DEBUG
	debug_print_to_file_complex(stft_debug_ifft_out_fh, fft->fft_buf, fft->fft_size);
#endif

	/* Window function */
	stft_process_apply_window(state);

	/* Copy to output buffer */
	stft_process_overlap_add_ifft_buffer(state, ch);
}

static void stft_do_fft_ifft(const struct processing_module *mod)
{
	struct stft_comp_data *cd = module_get_private_data(mod);
	struct stft_process_state *state = &cd->state;
	int num_fft;
	int ch;

	for (ch = 0; ch < cd->channels; ch++) {
		num_fft = stft_prepare_fft(state, ch);

		if (num_fft) {
			stft_do_fft(state, ch);

			/* stft_process(state) */

			stft_do_ifft(state, ch);
			cd->fft_done = true;
		}
	}
}

#if CONFIG_FORMAT_S32LE
static int stft_process_output_zeros_s32(struct stft_comp_data *cd, struct sof_sink *sink,
					 int frames)
{
	int32_t *y, *y_start, *y_end;
	int samples = frames * cd->channels;
	size_t bytes = samples * sizeof(int32_t);
	int samples_without_wrap;
	int y_size;
	int ret;

	/* Get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	y_end = y_start + y_size;
	while (samples) {
		/* Find out samples to process before first wrap or end of data. */
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, samples);
		memset(y, 0, samples_without_wrap * sizeof(int32_t));
		y += samples_without_wrap;

		/* Check for wrap */
		if (y >= y_end)
			y -= y_size;

		/* Update processed samples count for next loop iteration. */
		samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	sink_commit_buffer(sink, bytes);
	return 0;
}

static int stft_process_s32(const struct processing_module *mod, struct sof_source *source,
			    struct sof_sink *sink, uint32_t frames)
{
	struct stft_comp_data *cd = module_get_private_data(mod);

	/* Get samples from source buffer */
	stft_process_source_s32(cd, source, frames);

	/* Do STFT, processing and inverse STFT */
	stft_do_fft_ifft(mod);

	/* Get samples from source buffer */
	if (cd->fft_done)
		stft_process_sink_s32(cd, sink, frames);
	else
		stft_process_output_zeros_s32(cd, sink, frames);

	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
static int stft_process_output_zeros_s16(struct stft_comp_data *cd, struct sof_sink *sink,
					 int frames)
{
	int16_t *y, *y_start, *y_end;
	int samples = frames * cd->channels;
	size_t bytes = samples * sizeof(int16_t);
	int samples_without_wrap;
	int y_size;
	int ret;

	/* Get pointer to sink data in circular buffer, buffer start and size. */
	ret = sink_get_buffer_s16(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	/* Set helper pointers to buffer end for wrap check. Then loop until all
	 * samples are processed.
	 */
	y_end = y_start + y_size;
	while (samples) {
		/* Find out samples to process before first wrap or end of data. */
		samples_without_wrap = y_end - y;
		samples_without_wrap = MIN(samples_without_wrap, samples);
		memset(y, 0, samples_without_wrap * sizeof(int16_t));
		y += samples_without_wrap;

		/* Check for wrap */
		if (y >= y_end)
			y -= y_size;

		/* Update processed samples count for next loop iteration. */
		samples -= samples_without_wrap;
	}

	/* Update the source and sink for bytes consumed and produced. Return success. */
	sink_commit_buffer(sink, bytes);
	return 0;
}

static int stft_process_s16(const struct processing_module *mod, struct sof_source *source,
			    struct sof_sink *sink, uint32_t frames)
{
	struct stft_comp_data *cd = module_get_private_data(mod);

	/* Get samples from source buffer */
	stft_process_source_s16(cd, source, frames);

	/* Do STFT, processing and inverse STFT */
	stft_do_fft_ifft(mod);

	/* Get samples from source buffer */
	if (cd->fft_done)
		stft_process_sink_s16(cd, sink, frames);
	else
		stft_process_output_zeros_s16(cd, sink, frames);

	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
#endif /* CONFIG_FORMAT_S24LE */

/* This struct array defines the used processing functions for
 * the PCM formats
 */
const struct stft_process_proc_fnmap stft_process_functions[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, stft_process_s16 },
	{ SOF_IPC_FRAME_S32_LE, stft_process_s32 },
#endif
};

/**
 * stft_process_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
stft_process_func stft_process_find_proc_func(enum sof_ipc_frame src_fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < ARRAY_SIZE(stft_process_functions); i++)
		if (src_fmt == stft_process_functions[i].frame_fmt)
			return stft_process_functions[i].stft_process_function;

	return NULL;
}
