/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#ifndef SOF_ZEPHYR_TEST_AUDIO_HELPER_H
#define SOF_ZEPHYR_TEST_AUDIO_HELPER_H

#include <zephyr/ztest.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/pipeline.h>
#include <rtos/alloc.h>

static inline void test_audio_helper_setup_buffers(struct comp_dev *comp, uint32_t buf_size, struct sof_ipc_stream_params *params)
{
	struct comp_buffer *source_buf;
	struct comp_buffer *sink_buf;

	source_buf = buffer_alloc(NULL, buf_size, 0, 0, false);
	zassert_not_null(source_buf, "Source buffer allocation failed");
	buffer_set_params(source_buf, params, true);
	pipeline_connect(comp, source_buf, PPL_CONN_DIR_BUFFER_TO_COMP);

	sink_buf = buffer_alloc(NULL, buf_size, 0, 0, false);
	zassert_not_null(sink_buf, "Sink buffer allocation failed");
	buffer_set_params(sink_buf, params, true);
	pipeline_connect(comp, sink_buf, PPL_CONN_DIR_COMP_TO_BUFFER);
	
	/* Fill source buffer with some dummy data so copy() has something to process */
	audio_stream_produce(&source_buf->stream, buf_size / 2);

	comp->period = 1000;
	if (comp->drv->ops.bind) {
		uint32_t dummy_ipc4_data[8] = {0};
		struct bind_info binfo;
		memset(&binfo, 0, sizeof(binfo));
		binfo.ipc4_data = (void *)dummy_ipc4_data;

		binfo.bind_type = COMP_BIND_TYPE_SOURCE;
		binfo.source = audio_buffer_get_source(&source_buf->audio_buffer);
		comp->drv->ops.bind(comp, &binfo);

		binfo.bind_type = COMP_BIND_TYPE_SINK;
		binfo.sink = audio_buffer_get_sink(&sink_buf->audio_buffer);
		comp->drv->ops.bind(comp, &binfo);
	}

	if (comp->drv->ops.params) {
		comp->drv->ops.params(comp, params);
	}
}

static inline void test_audio_helper_free_buffers(struct comp_dev *comp)
{
	struct list_item *clist, *tmp;
	list_for_item_safe(clist, tmp, &comp->bsource_list) {
		struct comp_buffer *buf = container_of(clist, struct comp_buffer, sink_list);
		list_item_del(clist);
		buffer_free(buf);
	}
	list_for_item_safe(clist, tmp, &comp->bsink_list) {
		struct comp_buffer *buf = container_of(clist, struct comp_buffer, source_list);
		list_item_del(clist);
		buffer_free(buf);
	}
}

static inline void test_audio_helper_process(struct comp_dev *comp)
{
	int ret;
	
	comp->state = COMP_STATE_PREPARE;
	if (comp->drv->ops.prepare) {
		ret = comp->drv->ops.prepare(comp);
		zassert_equal(ret, 0, "prepare failed: %d", ret);
	}
	comp->state = COMP_STATE_ACTIVE;

	if (comp->drv->ops.copy) {
		ret = comp->drv->ops.copy(comp);
		zassert_true(ret >= 0, "copy failed: %d", ret);
	}

	comp->state = COMP_STATE_READY;
}

#endif /* SOF_ZEPHYR_TEST_AUDIO_HELPER_H */
