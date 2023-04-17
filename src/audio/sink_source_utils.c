// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/audio/sink_source_utils.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <rtos/string.h>
#include <sof/math/numbers.h>
#include <limits.h>

int source_to_sink_copy(struct source __sparse_cache *source, struct sink __sparse_cache *sink,
			bool free, size_t size)
{
	uint8_t __sparse_cache *src_ptr;
	uint8_t __sparse_cache *src_begin;
	uint8_t __sparse_cache *src_end;
	uint8_t __sparse_cache *dst_ptr;
	uint8_t __sparse_cache *dst_begin;
	uint8_t __sparse_cache *dst_end;
	size_t src_size, dst_size;
	int ret;

	if (!size)
		return 0;
	if (size > source_get_data_available(source))
		return -EFBIG;
	if (size > sink_get_free_size(sink))
		return -ENOSPC;

	ret = source_get_data(source, size,
			      (void __sparse_cache **)&src_ptr,
			      (void __sparse_cache **)&src_begin,
			      &src_size);
	if (ret)
		return ret;

	ret = sink_get_buffer(sink, size,
			      (void __sparse_cache **)&dst_ptr,
			      (void __sparse_cache **)&dst_begin,
			      &dst_size);
	if (ret) {
		source_put_data(source, 0);
		return ret;
	}

	src_end = src_begin + src_size;
	dst_end = dst_begin + dst_size;
	while (size) {
		uint32_t src_to_buf_overlap = (uintptr_t)src_end - (uintptr_t)src_ptr;
		uint32_t dst_to_buf_overlap = (uintptr_t)dst_end - (uintptr_t)dst_ptr;
		uint32_t to_copy = MIN(src_to_buf_overlap, dst_to_buf_overlap);

		to_copy = MIN(to_copy, size);
		ret = memcpy_s((__sparse_force void *)dst_ptr, dst_to_buf_overlap,
			       (__sparse_force void *)src_ptr, to_copy);
		assert(!ret);

		size -= to_copy;
		src_ptr += to_copy;
		dst_ptr += to_copy;
		if (to_copy == src_to_buf_overlap)
			src_ptr = src_begin;
		if (to_copy == dst_to_buf_overlap)
			dst_ptr = dst_begin;
	}

	source_put_data(source, free ? INT_MAX : 0);
	sink_commit_buffer(sink, INT_MAX);
	return 0;
}
