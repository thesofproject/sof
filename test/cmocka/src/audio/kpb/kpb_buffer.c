/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 * Test KPB internal buffering mechanism.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdint.h>
#include <stddef.h>

#include <sof/sof.h>
#include <sof/trace.h>
#include <sof/audio/kpb.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include "kpb_mock.h"
#include <sof/list.h>

/* local data types */
enum kpb_test_type {
	KPB_TEST_COPY_TWO_BUFFERS = 0,
	KPB_TEST_COPY_ONE_BUFFER,
};

enum kpb_test_buff_type {
	KPB_SOURCE_BUFFER = 0,
	KPB_SINK_BUFFER,
};

struct test_case {
	enum kpb_test_type type;
	int no_buffers;
	int lp_buff_size;
	int hp_buff_size;
	int source_period_bytes;
	int sink_period_bytes;
};

/* dummy component driver & device */
struct comp_driver kpb_drv_mock;
struct comp_dev *kpb_dev_mock;
struct comp_buffer *source;
struct comp_buffer *sink;

/* dummy memory buffers and data pointers */
void *source_data;
void *sink_data;
void *his_buf_lp;
void *his_buf_hp;

/* local function declarations */
static struct comp_buffer *mock_comp_buffer(void **state,
					    enum kpb_test_buff_type buff_type);
static struct comp_data *mock_comp_kpb(void **state, void *hp_buf,
				       void *lp_buf);
static struct comp_dev *mock_comp_dev(void);

static int buffering_test_setup(void **state)
{
	struct test_case *test_case_data = (struct test_case *)*state;
	int lp_buff_size = test_case_data->lp_buff_size;
	int hp_buff_size = test_case_data->hp_buff_size;
	int entire_buff_size = hp_buff_size + lp_buff_size;
	/* allocate memory for components */
	if (test_case_data->no_buffers == 2) {
		his_buf_lp = test_malloc(lp_buff_size);
		his_buf_hp = test_malloc(hp_buff_size);
	} else if (test_case_data->type == KPB_TEST_COPY_ONE_BUFFER) {
		his_buf_hp = test_malloc(hp_buff_size);
	} else {
		return -1;
	}

	source_data = test_malloc(entire_buff_size);
	sink_data = test_malloc(entire_buff_size);
	kpb_dev_mock = mock_comp_dev();

	/* create mocks */
	kpb_dev_mock->private = mock_comp_kpb(state, his_buf_hp, his_buf_lp);
	source = mock_comp_buffer(state, KPB_SOURCE_BUFFER);
	sink = mock_comp_buffer(state, KPB_SINK_BUFFER);

	/* mount coponents for test */
	source->source = kpb_dev_mock;
	source->sink = kpb_dev_mock;
	source->r_ptr = source_data;
	sink->w_ptr = sink_data;
	kpb_dev_mock->bsource_list.next = &source->sink_list;
	kpb_dev_mock->bsink_list.next = &sink->source_list;
	((struct comp_data *)kpb_dev_mock->private)->rt_sink = sink;

	return 0;
}

static int buffering_test_teardown(void **state)
{
	/* free components memory */
	struct test_case *test_case_data = (struct test_case *)*state;
	struct comp_data *cd = comp_get_drvdata(kpb_dev_mock);

	test_free(cd);
	test_free(source_data);
	test_free(sink_data);
	test_free(source);
	test_free(sink);

	if (test_case_data->type == KPB_TEST_COPY_TWO_BUFFERS) {
		test_free(his_buf_lp);
		test_free(his_buf_hp);
	} else if (test_case_data->type == KPB_TEST_COPY_ONE_BUFFER) {
		test_free(his_buf_hp);
	} else {
		return -1;
	}

	test_free(kpb_dev_mock);
	return 0;
}

static struct comp_dev *mock_comp_dev(void)
{
	struct comp_dev *dev = test_malloc(sizeof(struct comp_dev));

	dev->is_dma_connected = 0;
	return dev;
}

static struct comp_data *mock_comp_kpb(void **state, void *hp_buf,
				       void *lp_buf)
{
	struct comp_data *kpb = test_malloc(sizeof(struct comp_data));
	struct test_case *test_case_data = (struct test_case *)*state;

	kpb->source_period_bytes = test_case_data->source_period_bytes;
	kpb->sink_period_bytes = test_case_data->sink_period_bytes;

	if (test_case_data->type == KPB_TEST_COPY_TWO_BUFFERS) {
		int lp_buff_size = test_case_data->lp_buff_size;
		int hp_buff_size = test_case_data->hp_buff_size;
		/* LPSRAM buffer init */
		kpb->his_buf_lp.id = KPB_LP;
		kpb->his_buf_lp.state = KPB_BUFFER_FREE;
		kpb->his_buf_lp.w_ptr = lp_buf;
		kpb->his_buf_lp.end_addr = kpb->his_buf_lp.w_ptr + lp_buff_size;
		kpb->his_buf_lp.sta_addr = lp_buf;
		/* HPSRAM buffer init */
		kpb->his_buf_hp.id = KPB_HP;
		kpb->his_buf_hp.state = KPB_BUFFER_FREE;
		kpb->his_buf_hp.w_ptr = hp_buf;
		kpb->his_buf_hp.end_addr = kpb->his_buf_hp.w_ptr + hp_buff_size;
		kpb->his_buf_hp.sta_addr = hp_buf;
	} else if (test_case_data->type == KPB_TEST_COPY_ONE_BUFFER) {
		int hp_buff_size = test_case_data->hp_buff_size;
		/* HPSRAM buffer init only */
		kpb->his_buf_hp.id = KPB_HP;
		kpb->his_buf_hp.state = KPB_BUFFER_FREE;
		kpb->his_buf_hp.w_ptr = hp_buf;
		kpb->his_buf_hp.end_addr = kpb->his_buf_hp.w_ptr + hp_buff_size;
		kpb->his_buf_hp.sta_addr = hp_buf;
	} else {
		return NULL;
	}

	return kpb;
}

static struct comp_buffer *mock_comp_buffer(void **state,
					    enum kpb_test_buff_type buff_type)
{
	struct test_case *test_case_data = (struct test_case *)*state;
	struct comp_buffer *buffer = test_malloc(sizeof(struct comp_buffer));

	buffer->size = test_case_data->lp_buff_size +
		       test_case_data->hp_buff_size;

	switch (buff_type) {
	case KPB_SOURCE_BUFFER:
		buffer->avail = buffer->size;
		buffer->w_ptr = &buffer->w_ptr; /* setup to NULL */
		buffer->addr = buffer->r_ptr;
		buffer->end_addr = buffer->r_ptr + buffer->size;
		break;
	case KPB_SINK_BUFFER:
		buffer->free = buffer->size;
		buffer->r_ptr = &buffer->r_ptr;
		buffer->addr = buffer->w_ptr;
		buffer->end_addr = buffer->w_ptr + buffer->size;
		break;
	}

	buffer->cb = NULL;

	return buffer;
}

/* Mocking comp_register here so we can register our components properly */
int comp_register(struct comp_driver *drv)
{
	void *dst;

	switch (drv->type) {
	case SOF_COMP_KPB:
		dst = &kpb_drv_mock;
		memcpy(dst, drv, sizeof(struct comp_driver));
		break;
	default:
		return -1;
	}
	return 0;
}

/* A test case that copies real time stream into KPB internal buffer
 * and to real time sink.
 */
static void kpb_test_buffer_real_time_stream(void **state)
{
	int i, ret = 0;
	struct test_case *test_case_data = (struct test_case *)*state;
	int buffer_size = test_case_data->hp_buff_size +
			  test_case_data->lp_buff_size;
	char pattern = 0xAA;
	struct comp_buffer *source_test;
	struct comp_buffer *sink_test;
	char *r_ptr;
	/* register KPB component to use its internal functions */
	sys_comp_kpb_init();
	assert_int_equal(kpb_drv_mock.type, SOF_COMP_KPB);
	assert_non_null(kpb_drv_mock.ops.copy);

	source_test = list_first_item(&kpb_dev_mock->bsource_list,
				      struct comp_buffer,
				      sink_list);
	sink_test = list_first_item(&kpb_dev_mock->bsink_list,
				    struct comp_buffer,
				    source_list);
	assert_ptr_equal(source, source_test);
	assert_ptr_equal(sink, sink_test);

	/* fiil source buffer with test data */
	r_ptr = (char *)source_test->r_ptr;
	for (i = 0; i < buffer_size; i++)
		(*r_ptr++) = pattern;

	/* perform kpb_copy test */
	ret = kpb_drv_mock.ops.copy(kpb_dev_mock);

	assert_int_equal(ret, 0);
	/* verify if source was copied to sink */
	assert_memory_equal(source_data,
			    sink_data,
			    test_case_data->sink_period_bytes);

	if (test_case_data->type == KPB_TEST_COPY_TWO_BUFFERS) {
	/* verify if LPSRAM internal buffer was filled properly */
		assert_memory_equal(source_data,
				    his_buf_lp,
				    test_case_data->lp_buff_size);
	/* verify if HPSRAM internal buffer was filled properly */
		assert_memory_equal(source_data,
				    his_buf_hp,
				    test_case_data->hp_buff_size);
	} else if (test_case_data->type == KPB_TEST_COPY_ONE_BUFFER) {
	/* verify if HPSRAM internal buffer was filled properly */
		assert_memory_equal(source_data,
				    his_buf_hp,
				    test_case_data->hp_buff_size);
	}
}

/* always successful test */
static void null_test_success(void **state)
{
	(void)state;
}

/* test main function */
int main(void)
{
	struct CMUnitTest tests[3];
	struct test_case internal_double_buffering = {
		.type = KPB_TEST_COPY_TWO_BUFFERS,
		.no_buffers = 2,
		.lp_buff_size = 100,
		.hp_buff_size = 20,
		.source_period_bytes = 120,
		.sink_period_bytes = 120,
	};
	struct test_case internal_single_buffering = {
		.type = KPB_TEST_COPY_ONE_BUFFER,
		.no_buffers = 1,
		.lp_buff_size = 0,
		.hp_buff_size = 120,
		.source_period_bytes = 120,
		.sink_period_bytes = 120,
	};

	tests[0].name = "dummy, always successful test";
	tests[0].test_func = null_test_success;
	tests[0].initial_state = NULL;
	tests[0].setup_func = NULL;
	tests[0].teardown_func = NULL;

	tests[1].name = "KPB real time copy and buffering (Double Buffer)";
	tests[1].test_func = kpb_test_buffer_real_time_stream;
	tests[1].initial_state = &internal_double_buffering;
	tests[1].setup_func = buffering_test_setup;
	tests[1].teardown_func = buffering_test_teardown;

	tests[2].name = "KPB real time copy and buffering (Single Buffer)";
	tests[2].test_func = kpb_test_buffer_real_time_stream;
	tests[2].initial_state = &internal_single_buffering;
	tests[2].setup_func = buffering_test_setup;
	tests[2].teardown_func = buffering_test_teardown;

	return cmocka_run_group_tests(tests, NULL, NULL);
}
