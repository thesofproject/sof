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
#include <stdint.h>
#include <cmocka.h>
#include <stddef.h>

#include <sof/sof.h>
#include <sof/trace.h>
#include <sof/audio/kpb.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include "kpb_mock.h"
#include <sof/list.h>

/*! Local data types */
enum kpb_test_buff_type {
	KPB_SOURCE_BUFFER = 0,
	KPB_SINK_BUFFER,
};

/*! Parameters for test case */
struct test_case {
	int no_buffers;
	int lp_buff_size;
	int hp_buff_size;
	int source_period_bytes;
	int sink_period_bytes;
};

/* Dummy IPC structure, used to create KPB component */
struct sof_ipc_comp_kpb_mock {
	struct sof_ipc_comp comp;
	struct sof_ipc_comp_config config;
	uint32_t size;	/**< size of bespoke data section in bytes */
	uint32_t type;	/**< sof_ipc_effect_type */

	/* reserved for future use */
	uint32_t reserved[7];
	/* sof_kpb_config */
	uint32_t not_used; /**< kpb size in bytes */
	uint32_t caps; /**< SOF_MEM_CAPS_ */
	uint32_t no_channels; /**< no of channels */
	uint32_t history_depth; /**< time of buffering in milliseconds */
	uint32_t sampling_freq; /**< frequency in hertz */
	uint32_t sampling_width; /**< number of bits */
};

/* Dummy component driver & device */
struct comp_driver kpb_drv_mock;
struct comp_dev *kpb_dev_mock;
struct comp_buffer *source;
struct comp_buffer *sink;

/* Dummy memory buffers and data pointers */
void *source_data;
void *sink_data;
void *his_buf_lp;
void *his_buf_hp;

/* Initialize KPB for test */
static int buffering_test_setup(void **state)
{
	(void)state;

	/* Dummy IPC structue to create new KPB component */
	struct sof_ipc_comp_kpb_mock kpb = {
	.comp = {
		.type = SOF_COMP_KPB,
	},
	.config = {
		.hdr = {
			.size = sizeof(struct sof_ipc_comp_config),
		},
	},
	.size = sizeof(struct sof_kpb_config),
	.no_channels = 2,
	.sampling_freq = KPB_SAMPLNG_FREQUENCY,
	.sampling_width = KPB_SAMPLING_WIDTH,
	};

	/* Register KPB component to use its internal functions */
	sys_comp_kpb_init();

	/* Create KPB component */
	kpb_dev_mock = kpb_drv_mock.ops.new((struct sof_ipc_comp *)&kpb);

	/* Was device created properly?  */
	assert_non_null(kpb_dev_mock);
	/* Verify if config was properly set */
	assert_int_equal(((struct comp_data *)
			 kpb_dev_mock->private)->config.sampling_freq,
			 KPB_SAMPLNG_FREQUENCY);

	return 0;
}

static int buffering_test_teardown(void **state)
{
	return 0;
}

/* Mock comp_register here so we can register our components properly */
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
	/*TODO: Perform copy from source to sink */
}

/* Always successful test */
static void null_test_success(void **state)
{
	(void)state;
}

/* Test main function */
int main(void)
{
	struct CMUnitTest tests[2];
	struct test_case internal_buffering = {
		.no_buffers = 2,
		.lp_buff_size = 100,
		.hp_buff_size = 20,
		.source_period_bytes = 120,
		.sink_period_bytes = 120,
	};

	tests[0].name = "Dummy, always successful test";
	tests[0].test_func = null_test_success;
	tests[0].initial_state = NULL;
	tests[0].setup_func = NULL;
	tests[0].teardown_func = NULL;

	tests[1].name = "KPB real time copy and buffering (Double Buffer)";
	tests[1].test_func = kpb_test_buffer_real_time_stream;
	tests[1].initial_state = &internal_buffering;
	tests[1].setup_func = buffering_test_setup;
	tests[1].teardown_func = buffering_test_teardown;

	return cmocka_run_group_tests(tests, NULL, NULL);
}
