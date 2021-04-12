// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <stdarg.h>
#include <stddef.h>
#include <math.h>
#include <setjmp.h>
#include <stdint.h>
#include <malloc.h>
#include <cmocka.h>
#include <sof/list.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/schedule.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/mixer.h>

#include "comp_mock.h"

#define MIX_TEST_SAMPLES 32

struct comp_driver drv_mock;

struct comp_driver mixer_drv_mock;
struct comp_dev *mixer_dev_mock;

struct comp_dev *post_mixer_comp;
struct comp_buffer *post_mixer_buf;

/* Mocking comp_register here so we can register our components properly */
int comp_register(struct comp_driver_info *info)
{
	void *dst;
	int err;
	switch (info->drv->type) {
	case SOF_COMP_MIXER:
		dst = &mixer_drv_mock;
		err = memcpy_s(dst, sizeof(mixer_drv_mock),
			       info->drv, sizeof(struct comp_driver));
		break;

	case SOF_COMP_MOCK:
		dst = &drv_mock;
		err = memcpy_s(dst, sizeof(drv_mock), info->drv,
			       sizeof(struct comp_driver));
		break;

	default:
		return -ENOTSUP;
	}

	return err;
}

struct source {
	struct comp_dev *comp;
	struct comp_buffer *buf;
};

struct mix_test_case {
	int num_sources;
	int num_chans;
	const char *name;
	struct source *sources;
};

#define TEST_CASE(_num_sources, _num_chans) \
	{ \
		.num_sources = (_num_sources), \
		.num_chans = (_num_chans), \
		.name = ("test_audio_mixer_copy_" \
			 #_num_sources "_srcs_" \
			 #_num_chans "ch"), \
		.sources = NULL \
	}

static struct mix_test_case mix_test_cases[] = {
	TEST_CASE(1, 2),
	TEST_CASE(1, 4),
	TEST_CASE(1, 8),
	TEST_CASE(2, 2),
	TEST_CASE(2, 4),
	TEST_CASE(2, 8),
	TEST_CASE(3, 2),
	TEST_CASE(4, 2),
	TEST_CASE(6, 2),
	TEST_CASE(8, 2)
};

static struct sof_ipc_comp mock_comp = {
	.type = SOF_COMP_MOCK
};

static struct comp_dev *create_comp(struct sof_ipc_comp *comp,
				    struct comp_driver *drv)
{
	struct comp_dev *cd = drv->ops.create(drv, comp);

	assert_non_null(cd);

	memcpy_s(COMP_GET_IPC(cd, sof_ipc_comp), sizeof(struct sof_ipc_comp),
		 comp, sizeof(*comp));
	cd->drv = drv;
	list_init(&cd->bsource_list);
	list_init(&cd->bsink_list);

	return cd;
}

static void destroy_comp(struct comp_driver *drv, struct comp_dev *dev)
{
	drv->ops.free(dev);
}

static void init_buffer_pcm_params(struct comp_buffer *buf, int num_chans)
{
	buf->stream.channels = num_chans;
	buf->stream.frame_fmt = SOF_IPC_FRAME_S32_LE;
}

static void create_sources(struct mix_test_case *tc)
{
	int src_idx;

	tc->sources = malloc(tc->num_sources * sizeof(struct source));

	for (src_idx = 0; src_idx < tc->num_sources; ++src_idx) {
		struct source *src = &tc->sources[src_idx];

		struct sof_ipc_buffer buf = {
			.size = (MIX_TEST_SAMPLES * sizeof(uint32_t)) *
				tc->num_chans
		};

		src->comp = create_comp(&mock_comp, &drv_mock);
		src->buf = buffer_new(&buf);
		init_buffer_pcm_params(src->buf, tc->num_chans);

		src->buf->source = src->comp;
		src->buf->sink = mixer_dev_mock;

		list_item_prepend(&src->buf->source_list,
				  &src->comp->bsink_list);
		list_item_prepend(&src->buf->sink_list,
				  &mixer_dev_mock->bsource_list);
	}
}

static void destroy_sources(struct mix_test_case *tc)
{
	int src_idx;

	for (src_idx = 0; src_idx < tc->num_sources; ++src_idx)
		destroy_comp(&drv_mock, tc->sources[src_idx].comp);

	free(tc->sources);
}

static void activate_periph_comps(struct mix_test_case *tc)
{
	int src_idx;

	for (src_idx = 0; src_idx < tc->num_sources; ++src_idx)
		tc->sources[src_idx].comp->state = COMP_STATE_ACTIVE;

	post_mixer_comp->state = COMP_STATE_ACTIVE;
}

static int test_group_setup(void **state)
{
	sys_comp_mixer_init();
	sys_comp_mock_init();

	return 0;
}

static int test_setup(void **state)
{
	static struct sof_ipc_comp_mixer mixer = {
		.comp = {
			.type = SOF_COMP_MIXER,
		},
		.config = {
			.hdr = {
				.size = sizeof(struct sof_ipc_comp_config)
			}
		}
	};

	mixer_dev_mock = create_comp((struct sof_ipc_comp *)&mixer,
				     &mixer_drv_mock);

	struct mix_test_case *tc = *((struct mix_test_case **)state);

	if (tc) {
		struct sof_ipc_buffer buf = {
			.size = (MIX_TEST_SAMPLES * sizeof(uint32_t)) *
				tc->num_chans
		};

		post_mixer_buf = buffer_new(&buf);

		create_sources(tc);
		post_mixer_comp = create_comp(&mock_comp, &drv_mock);

		post_mixer_buf->source = mixer_dev_mock;
		post_mixer_buf->sink = post_mixer_comp;
		init_buffer_pcm_params(post_mixer_buf, tc->num_chans);

		list_item_prepend(&post_mixer_buf->source_list,
				  &mixer_dev_mock->bsink_list);
		list_item_prepend(&post_mixer_buf->sink_list,
				  &post_mixer_comp->bsource_list);

		mixer_drv_mock.ops.prepare(mixer_dev_mock);

		mixer_dev_mock->state = COMP_STATE_ACTIVE;
		activate_periph_comps(tc);

		mixer_dev_mock->frames = MIX_TEST_SAMPLES;
	}

	return 0;
}

static int test_teardown(void **state)
{
	destroy_comp(&mixer_drv_mock, mixer_dev_mock);

	struct mix_test_case *tc = *((struct mix_test_case **)state);

	if (tc) {
		buffer_free(post_mixer_buf);
		destroy_sources(tc);
	}

	return 0;
}

/*
 * Tests
 */

static void test_audio_mixer_new(void **state)
{
	assert_non_null(mixer_dev_mock->priv_data);
}

static void test_audio_mixer_prepare_no_sources(void **state)
{
	int downstream = mixer_drv_mock.ops.prepare(mixer_dev_mock);

	assert_int_equal(downstream, 0);
}

static void test_audio_mixer_copy(void **state)
{
	int src_idx;
	int smp;
	struct mix_test_case *tc = *((struct mix_test_case **)state);

	for (src_idx = 0; src_idx < tc->num_sources; ++src_idx) {
		uint32_t *samples = tc->sources[src_idx].buf->stream.addr;

		for (smp = 0; smp < MIX_TEST_SAMPLES; ++smp) {
			double rad = M_PI / (180.0 / (smp * (src_idx + 1)));

			samples[smp] = ((sin(rad) + 1) / 2) * (0xFFFFFFFF / 2);
		}

		audio_stream_produce(&tc->sources[src_idx].buf->stream,
				     sizeof(uint32_t) * MIX_TEST_SAMPLES);
	}

	mixer_drv_mock.ops.copy(mixer_dev_mock);

	for (smp = 0; smp < MIX_TEST_SAMPLES; ++smp) {
		uint64_t sum = 0;

		for (src_idx = 0; src_idx < tc->num_sources; ++src_idx) {
			assert_non_null(tc->sources[src_idx].buf);

			uint32_t *samples =
				tc->sources[src_idx].buf->stream.addr;

			sum += samples[smp];
		}

		sum = sat_int32(sum);

		uint32_t *out_samples = post_mixer_buf->stream.addr;

		assert_int_equal(out_samples[smp], sum);
	}
}

int main(void)
{
	struct CMUnitTest tests[ARRAY_SIZE(mix_test_cases) + 2];

	int i;
	int cur_test_case = 0;

	tests[0].test_func = test_audio_mixer_new;
	tests[0].initial_state = NULL;
	tests[0].setup_func = test_setup;
	tests[0].teardown_func = test_teardown;
	tests[0].name = "test_audio_mixer_new";

	tests[1].test_func = test_audio_mixer_prepare_no_sources;
	tests[1].initial_state = NULL;
	tests[1].setup_func = test_setup;
	tests[1].teardown_func = test_teardown;
	tests[1].name = "test_audio_mixer_prepare_no_sources";

	for (i = 2; i < ARRAY_SIZE(tests); (++i, ++cur_test_case)) {
		tests[i].test_func = test_audio_mixer_copy;
		tests[i].initial_state = &mix_test_cases[cur_test_case];
		tests[i].setup_func = test_setup;
		tests[i].teardown_func = test_teardown;
		tests[i].name = mix_test_cases[cur_test_case].name;
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, test_group_setup, NULL);
}
