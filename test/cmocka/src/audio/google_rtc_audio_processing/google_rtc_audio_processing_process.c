// SPDX-License-Identifier: BSD-3-Clause
//
// Author: Lionel Koenig <lionelk@google.com>

#include "../../util.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <cmocka.h>
#include <sof/audio/component.h>
#include <ipc/topology.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/uuid.h>

void sys_comp_google_rtc_audio_processing_init(void);
extern const struct sof_uuid google_rtc_audio_processing_uuid;

static void verify_s16_to_s16(struct comp_dev *dev, struct comp_buffer *mic,
			      struct comp_buffer *ref, struct comp_buffer *out);

static struct sof sof;
struct sof *sof_get(void)
{
	return &sof;
}

struct google_rtc_audio_processing_test_parameters {
	int period_size;
	int num_capture_channels;
	int num_output_channels;
	int num_aec_reference_channels;
	int sample_rate_hz;
	int num_periods;
};

struct google_rtc_audio_processing_test_state {
	struct google_rtc_audio_processing_test_parameters *parameters;
	struct pipeline *capture_pipeline;
	struct pipeline *render_pipeline;
	struct comp_dev *dev;
	struct comp_buffer *mic;
	struct comp_buffer *ref;
	struct comp_buffer *out;
	void (*verify)(struct comp_dev *dev, struct comp_buffer *mic,
		       struct comp_buffer *ref, struct comp_buffer *out);
};

static int test_setup(void **state)
{
	pipeline_posn_init(sof_get());

	struct google_rtc_audio_processing_test_parameters *parameters = *state;
	struct google_rtc_audio_processing_test_state
		*google_rtc_audio_processing_state =
			test_malloc(sizeof(*google_rtc_audio_processing_state));
	int rc;
	struct sof_ipc_comp_process *component =
		test_malloc(sizeof(struct sof_ipc_comp_process) +
			    sizeof(struct sof_ipc_comp_ext));
	component->comp.hdr.size = sizeof(struct sof_ipc_comp_process) +
				   sizeof(struct sof_ipc_comp_ext);
	component->comp.ext_data_length = sizeof(struct sof_ipc_comp_ext);
	component->comp.type = SOF_COMP_NONE;
	component->comp.core = 0;
	component->comp.id = 1;
	component->comp.pipeline_id = 5;
	component->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	struct sof_ipc_comp_ext *comp_ext =
		(struct sof_ipc_comp_ext *)((uint8_t *)component +
					    component->comp.hdr.size -
					    component->comp.ext_data_length);

	component->type = SOF_COMP_NONE;
	memcpy_s(comp_ext->uuid, UUID_SIZE,
		 SOF_RT_UUID(google_rtc_audio_processing_uuid), UUID_SIZE);

	google_rtc_audio_processing_state->parameters = parameters;

	google_rtc_audio_processing_state->capture_pipeline =
		pipeline_new(42, 1, 987);
	google_rtc_audio_processing_state->render_pipeline =
		pipeline_new(43, 1, 987);

	/* allocate new state */
	google_rtc_audio_processing_state->dev =
		comp_new((struct sof_ipc_comp *)component);
	assert_non_null(google_rtc_audio_processing_state->dev);
	test_free(component);
	google_rtc_audio_processing_state->dev->pipeline =
		google_rtc_audio_processing_state->capture_pipeline;

	google_rtc_audio_processing_state->mic = create_test_source(
		google_rtc_audio_processing_state->dev, 0, SOF_IPC_FRAME_S16_LE,
		parameters->num_capture_channels, parameters->period_size);
	google_rtc_audio_processing_state->mic->source->pipeline =
		google_rtc_audio_processing_state->capture_pipeline;
	google_rtc_audio_processing_state->ref =
		create_test_source(google_rtc_audio_processing_state->dev, 0,
				   SOF_IPC_FRAME_S16_LE,
				   parameters->num_aec_reference_channels,
				   parameters->period_size);
	google_rtc_audio_processing_state->ref->source->pipeline =
		google_rtc_audio_processing_state->render_pipeline;
	google_rtc_audio_processing_state->out = create_test_sink(
		google_rtc_audio_processing_state->dev, 0, SOF_IPC_FRAME_S16_LE,
		parameters->num_output_channels, parameters->period_size);
	google_rtc_audio_processing_state->out->sink->pipeline =
		google_rtc_audio_processing_state->capture_pipeline;
	google_rtc_audio_processing_state->mic->stream.rate =
		parameters->sample_rate_hz;
	google_rtc_audio_processing_state->ref->stream.rate =
		parameters->sample_rate_hz;
	google_rtc_audio_processing_state->out->stream.rate =
		parameters->sample_rate_hz;
	google_rtc_audio_processing_state->mic->source->ipc_config.type =
		SOF_COMP_DAI;
	google_rtc_audio_processing_state->ref->source->ipc_config.type =
		SOF_COMP_DEMUX;

	rc = comp_trigger(google_rtc_audio_processing_state->dev,
			  COMP_TRIGGER_PREPARE);
	assert_return_code(rc, 0);
	rc = comp_prepare(google_rtc_audio_processing_state->dev);
	assert_return_code(rc, 0);
	rc = comp_trigger(google_rtc_audio_processing_state->dev,
			  COMP_TRIGGER_PRE_START);
	assert_return_code(rc, 0);
	rc = comp_trigger(google_rtc_audio_processing_state->dev,
			  COMP_TRIGGER_START);
	assert_return_code(rc, 0);

	/* assigns verification function */
	google_rtc_audio_processing_state->verify = verify_s16_to_s16;

	/* assign test state */
	*state = google_rtc_audio_processing_state;
	return 0;
}

static int test_teardown(void **state)
{
	struct google_rtc_audio_processing_test_state
		*google_rtc_audio_processing_state = *state;
	struct comp_dev *dev = google_rtc_audio_processing_state->dev;
	int rc;

	rc = comp_trigger(dev, COMP_TRIGGER_STOP);
	assert_return_code(rc, 0);
	rc = comp_trigger(dev, COMP_TRIGGER_RESET);
	assert_return_code(rc, 0);
	rc = comp_reset(dev);
	assert_return_code(rc, 0);

	comp_free(dev);

	free_test_sink(google_rtc_audio_processing_state->mic);
	free_test_sink(google_rtc_audio_processing_state->ref);
	free_test_source(google_rtc_audio_processing_state->out);

	pipeline_free(google_rtc_audio_processing_state->capture_pipeline);
	pipeline_free(google_rtc_audio_processing_state->render_pipeline);

	test_free(google_rtc_audio_processing_state);

	return 0;
}

#if CONFIG_FORMAT_S16LE
static void fill_source_s16(struct google_rtc_audio_processing_test_state
				    *google_rtc_audio_processing_state)
{
	int16_t *mic = NULL;
	int16_t *ref = NULL;
	uint32_t i;
	uint32_t length = audio_stream_get_free_frames(
		&google_rtc_audio_processing_state->mic->stream);
	uint32_t length_bytes = audio_stream_get_free_bytes(
		&google_rtc_audio_processing_state->mic->stream);

	for (i = 0; i < length; ++i) {
		mic = audio_stream_write_frag_s16(
			&google_rtc_audio_processing_state->mic->stream, i);
		ref = audio_stream_write_frag_s16(
			&google_rtc_audio_processing_state->ref->stream, i);
		*mic = *ref = 0;
	}
	comp_update_buffer_produce(google_rtc_audio_processing_state->mic,
				   length_bytes);
	comp_update_buffer_produce(google_rtc_audio_processing_state->ref,
				   length_bytes);
}

static void verify_s16_to_s16(struct comp_dev *dev, struct comp_buffer *mic,
			      struct comp_buffer *ref, struct comp_buffer *out)
{
	// This unit tests only verify that everything compiles and run.
}
#endif /* CONFIG_FORMAT_S16LE */

static void test_google_rtc_audio_processing(void **state)
{
	struct google_rtc_audio_processing_test_state
		*google_rtc_audio_processing_state = *state;
	int i = 0;
	uint32_t out_available_bytes = 0;
	int rc;

	for (i = 0;
	     i < google_rtc_audio_processing_state->parameters->num_periods;
	     ++i) {
		fill_source_s16(google_rtc_audio_processing_state);
		rc = comp_copy(google_rtc_audio_processing_state->dev);
		assert_return_code(rc, 0);

		google_rtc_audio_processing_state->verify(
			google_rtc_audio_processing_state->dev,
			google_rtc_audio_processing_state->mic,
			google_rtc_audio_processing_state->ref,
			google_rtc_audio_processing_state->out);

		out_available_bytes = audio_stream_get_avail_bytes(
			&google_rtc_audio_processing_state->out->stream);
		comp_update_buffer_consume(
			google_rtc_audio_processing_state->out,
			out_available_bytes);
	}
}

static int setup_group(void **state)
{
	sys_comp_init(sof_get());
	sys_comp_google_rtc_audio_processing_init();
	return 0;
}

int main(void)
{
	struct google_rtc_audio_processing_test_parameters parameters[] = {
		{
			.period_size = 48,
			.num_capture_channels = 1,
			.num_output_channels = 1,
			.num_aec_reference_channels = 2,
			.sample_rate_hz = 48000,
			.num_periods = 10,
		},
		{
			.period_size = 480,
			.num_capture_channels = 1,
			.num_output_channels = 1,
			.num_aec_reference_channels = 4,
			.sample_rate_hz = 48000,
			.num_periods = 10,
		},
		{
			.period_size = 48,
			.num_capture_channels = 1,
			.num_output_channels = 1,
			.num_aec_reference_channels = 4,
			.sample_rate_hz = 48000,
			.num_periods = 10,
		},
	};
	struct CMUnitTest tests[ARRAY_SIZE(parameters)];
	int test_index;

	for (test_index = 0; test_index < ARRAY_SIZE(parameters);
	     ++test_index) {
		tests[test_index].name = "test_google_rtc_audio_processing";
		tests[test_index].test_func = test_google_rtc_audio_processing;
		tests[test_index].setup_func = test_setup;
		tests[test_index].teardown_func = test_teardown;
		tests[test_index].initial_state = &parameters[test_index];
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup_group, NULL);
}
