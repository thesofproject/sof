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
#include <sof/audio/module_adapter/module/generic.h>
#include "../module_adapter.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include "comp_mock.h"

#define MIX_TEST_SAMPLES 32

struct mix_test_case {
	int num_sources;
	int num_chans;
	const char *name;
};

#define TEST_CASE(_num_sources, _num_chans) \
	{ \
		.num_sources = (_num_sources), \
		.num_chans = (_num_chans), \
		.name = ("test_audio_mixer_copy_" \
			 #_num_sources "_srcs_" \
			 #_num_chans "ch"), \
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

static int test_setup(void **state)
{
	struct mix_test_case *tc = *((struct mix_test_case **)state);
	struct processing_module_test_parameters test_parameters = {
		tc->num_chans, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, NULL
	};
	struct processing_module_test_data *test_data;
	struct module_data *mod_data;
	struct mixer_data *md;


	/* allocate and set new processing module, device and mixer data */
	test_data = test_malloc(sizeof(*test_data));
	test_data->parameters = test_parameters;
	test_data->num_sources = tc->num_sources;
	test_data->num_sinks = 1;
	module_adapter_test_setup(test_data);

	mod_data = &test_data->mod->priv;
	md = test_malloc(sizeof(*md));
	mod_data->private = md;

	md->mix_func = mixer_get_processing_function(test_data->mod->dev, test_data->sinks[0]);

	*state = test_data;

	return 0;
}

static int test_teardown(void **state)
{
	struct processing_module_test_data *test_data = *state;
	struct mixer_data *md = module_get_private_data(test_data->mod);

	test_free(md);
	module_adapter_test_free(test_data);
	test_free(test_data);

	return 0;
}

static void test_audio_mixer_copy(void **state)
{
	int src_idx;
	int smp;
	struct processing_module_test_data *tc = *state;
	struct processing_module *mod = tc->mod;
	struct mixer_data *md = module_get_private_data(tc->mod);
	const struct audio_stream *sources_stream[PLATFORM_MAX_STREAMS];

	for (src_idx = 0; src_idx < tc->num_sources; ++src_idx) {
		uint32_t *samples = tc->sources[src_idx]->stream.addr;

		for (smp = 0; smp < tc->sources[src_idx]->stream.size / sizeof(int32_t); ++smp) {
			double rad = M_PI / (180.0 / (smp * (src_idx + 1)));

			samples[smp] = ((sin(rad) + 1) / 2) * (0xFFFFFFFF / 2);
		}

		audio_stream_produce(&tc->sources[src_idx]->stream,
				     tc->sources[src_idx]->stream.size / sizeof(int32_t));

		sources_stream[src_idx] = &tc->sources[src_idx]->stream;
	}

	md->mix_func(mod->dev, &tc->sinks[0]->stream, sources_stream, tc->num_sources,
		     mod->dev->frames);

	for (smp = 0; smp < tc->sinks[0]->stream.size / sizeof(int32_t); ++smp) {
		uint64_t sum = 0;

		for (src_idx = 0; src_idx < tc->num_sources; ++src_idx) {
			assert_non_null(tc->sources[src_idx]);

			uint32_t *samples = tc->sources[src_idx]->stream.addr;

			sum += samples[smp];
		}

		sum = sat_int32(sum);

		uint32_t *out_samples = tc->sinks[0]->stream.addr;

		assert_int_equal(out_samples[smp], sum);
	}
}

int main(void)
{
	struct CMUnitTest tests[ARRAY_SIZE(mix_test_cases)];

	int i;
	int cur_test_case = 0;

	for (i = 0; i < ARRAY_SIZE(tests); (++i, ++cur_test_case)) {
		tests[i].test_func = test_audio_mixer_copy;
		tests[i].initial_state = &mix_test_cases[cur_test_case];
		tests[i].setup_func = test_setup;
		tests[i].teardown_func = test_teardown;
		tests[i].name = mix_test_cases[cur_test_case].name;
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
