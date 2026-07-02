// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Piotr Hoppe <piotr.hoppe@intel.com>

/**
 * @file crossover_test_mocks.c
 * @brief Trivial source/sink API stubs for the crossover unit tests.
 *
 * The crossover_generic.c translation unit also contains the per-format
 * processing functions (crossover_s16/s24/s32_default and the passthrough),
 * which reference the source/sink API. The split functions under test never
 * call them, but the linker still needs the referenced symbols defined.
 *
 * The stubs are provided by this separate translation unit so that any number
 * of test executables can link them via CMake without risking multiple
 * definitions. The prototypes come from the real source/sink API headers.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>

size_t source_get_frame_bytes(struct sof_source *source)
{
	(void)source;
	return 0;
}

int source_get_data_s16(struct sof_source *source, size_t req_size, int16_t const **data_ptr,
			int16_t const **buffer_start, int *buffer_samples)
{
	(void)source; (void)req_size; (void)data_ptr; (void)buffer_start; (void)buffer_samples;
	return 0;
}

int source_get_data_s32(struct sof_source *source, size_t req_size, int32_t const **data_ptr,
			int32_t const **buffer_start, int *buffer_samples)
{
	(void)source; (void)req_size; (void)data_ptr; (void)buffer_start; (void)buffer_samples;
	return 0;
}

int source_release_data(struct sof_source *source, size_t free_size)
{
	(void)source; (void)free_size;
	return 0;
}

int sink_get_buffer_s16(struct sof_sink *sink, size_t req_size, int16_t **data_ptr,
			int16_t **buffer_start, int *buffer_samples)
{
	(void)sink; (void)req_size; (void)data_ptr; (void)buffer_start; (void)buffer_samples;
	return 0;
}

int sink_get_buffer_s32(struct sof_sink *sink, size_t req_size, int32_t **data_ptr,
			int32_t **buffer_start, int *buffer_samples)
{
	(void)sink; (void)req_size; (void)data_ptr; (void)buffer_start; (void)buffer_samples;
	return 0;
}

int sink_commit_buffer(struct sof_sink *sink, size_t commit_size)
{
	(void)sink; (void)commit_size;
	return 0;
}

int source_to_sink_copy(struct sof_source *source, struct sof_sink *sink, bool free, size_t size)
{
	(void)source; (void)sink; (void)free; (void)size;
	return 0;
}
