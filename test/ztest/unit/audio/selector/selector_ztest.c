// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <zephyr/ztest.h>

#include <rtos/sof.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/selector.h>
#include <sof/ipc/common.h>
#include <ipc/stream.h>
#include <ipc4/base-config.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TEST_RATE     48000
#define TEST_FRAMES   4
#define TEST_BUF_SIZE 1024

struct selector_fixture {
	struct comp_dev *dev;
	struct comp_buffer *source;
	struct comp_buffer *sink;
	struct sof_source *sources[1];
	struct sof_sink *sinks[1];
};

struct selector_extended_config {
	struct ipc4_base_module_cfg base_cfg;
	uint16_t nb_input_pins;
	uint16_t nb_output_pins;
	uint8_t reserved[12];
	struct ipc4_input_pin_format input_pin;
	struct ipc4_output_pin_format output_pin;
} __packed __aligned(4);

/**
 * @brief Build an IPC4 signed-interleaved test format.
 */
static struct ipc4_audio_format test_format(uint32_t depth, uint32_t valid, uint32_t channels)
{
	return (struct ipc4_audio_format){
		.sampling_frequency = TEST_RATE,
		.depth = depth,
		.ch_map = 0x76543210,
		.ch_cfg = channels == 1 ? IPC4_CHANNEL_CONFIG_MONO : IPC4_CHANNEL_CONFIG_STEREO,
		.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
		.channels_count = channels,
		.valid_bit_depth = valid,
		.s_type = IPC4_TYPE_SIGNED_INTEGER,
	};
}

/**
 * @brief Initialize a lightweight module for direct processing-function tests.
 */
static void init_processing_module(struct processing_module *mod, struct comp_data *cd,
				   enum sof_ipc_frame source, enum sof_ipc_frame source_valid,
				   enum sof_ipc_frame sink, enum sof_ipc_frame sink_valid,
				   unsigned int source_channels, unsigned int sink_channels)
{
	memset(mod, 0, sizeof(*mod));
	memset(cd, 0, sizeof(*cd));
	mod->priv.private = cd;
	cd->source_format = source;
	cd->source_valid_format = source_valid;
	cd->sink_format = sink;
	cd->sink_valid_format = sink_valid;
	cd->config.in_channels_count = source_channels;
	cd->config.out_channels_count = sink_channels;
}

/**
 * @brief Resolve and require a selector processing function.
 */
static sel_func get_processing_function(struct processing_module *mod)
{
	sel_func function = sel_get_processing_function(mod);

	zassert_not_null(function, "No processing function for test format");
	return function;
}

/**
 * @brief Allocate and connect a real SOF component buffer.
 */
static struct comp_buffer *create_test_buffer(struct comp_dev *dev, bool source,
					      enum sof_ipc_frame frame_fmt,
					      enum sof_ipc_frame valid_fmt, unsigned int channels)
{
	struct sof_ipc_buffer desc = {
		.size = TEST_BUF_SIZE,
	};
	struct comp_buffer *buffer = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	zassert_not_null(buffer, "Failed to allocate test buffer");
	memset(audio_stream_get_addr(&buffer->stream), 0, TEST_BUF_SIZE);
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_valid_fmt(&buffer->stream, valid_fmt);
	audio_stream_set_rate(&buffer->stream, TEST_RATE);
	audio_stream_set_channels(&buffer->stream, channels);

	if (source) {
		buffer->source = calloc(1, sizeof(*buffer->source));
		zassert_not_null(buffer->source);
		buffer->source->state = COMP_STATE_PREPARE;
		list_item_append(&buffer->sink_list, &dev->bsource_list);
	} else {
		buffer->sink = calloc(1, sizeof(*buffer->sink));
		zassert_not_null(buffer->sink);
		buffer->sink->state = COMP_STATE_PREPARE;
		list_item_append(&buffer->source_list, &dev->bsink_list);
	}

	return buffer;
}

/**
 * @brief Disconnect and free a test component buffer.
 */
static void free_test_buffer(struct comp_buffer *buffer, bool source)
{
	if (!buffer) {
		return;
	}

	if (source) {
		free(buffer->source);
	} else {
		free(buffer->sink);
	}
	buffer_free(buffer);
}

/**
 * @brief Initialize a selector coefficient matrix as an identity transform.
 */
static void set_identity_coefficients(struct ipc4_selector_coeffs_config *coeffs,
				      unsigned int channels)
{
	memset(coeffs, 0, sizeof(*coeffs));
	for (unsigned int i = 0; i < channels; i++) {
		coeffs->coeffs[i][i] = SEL_COEF_ONE_Q10;
	}
}

/**
 * @brief Send one complete coefficient configuration through the module API.
 */
static int send_coefficients(struct processing_module *mod,
			     const struct ipc4_selector_coeffs_config *coeffs)
{
	const struct module_interface *ops = mod->dev->drv->adapter_ops;

	return ops->set_configuration(mod, IPC4_SELECTOR_COEFFS_CONFIG_ID,
				      MODULE_CFG_FRAGMENT_SINGLE, sizeof(*coeffs),
				      (const uint8_t *)coeffs, sizeof(*coeffs), NULL, 0);
}

/**
 * @brief Create an initialized selector with connected source and sink buffers.
 */
static struct selector_fixture *create_fixture_with_payload(const struct ipc4_audio_format *input,
							    const struct ipc4_audio_format *output,
							    uint32_t direction,
							    bool configured_source,
							    bool extended_payload)
{
	const struct sof_uuid uuid = SOF_REG_UUID(selector4);
	const struct comp_driver *driver = comp_driver_find(&uuid, SOF_COMP_MODULE_ADAPTER);
	struct sof_selector_avs_ipc4_config module_cfg = {
		.base_cfg =
			{
				.ibs = TEST_BUF_SIZE,
				.obs = TEST_BUF_SIZE,
				.audio_fmt = *input,
			},
		.output_format = *output,
	};
	struct selector_extended_config extended_cfg = {
		.base_cfg =
			{
				.ibs = TEST_BUF_SIZE,
				.obs = TEST_BUF_SIZE,
				.audio_fmt = *input,
			},
		.nb_input_pins = 1,
		.nb_output_pins = 1,
		.input_pin =
			{
				.ibs = TEST_BUF_SIZE,
				.audio_fmt = *input,
			},
		.output_pin =
			{
				.obs = TEST_BUF_SIZE,
				.audio_fmt = *output,
			},
	};
	struct ipc_config_process spec = {
		.size = extended_payload ? sizeof(extended_cfg) : sizeof(module_cfg),
		.data = extended_payload ? (const uint8_t *)&extended_cfg
					 : (const uint8_t *)&module_cfg,
	};
	struct comp_ipc_config ipc_cfg = {
		.id = 1,
		.proc_domain = COMP_PROCESSING_DOMAIN_LL,
		.type = SOF_COMP_MODULE_ADAPTER,
		.ipc_config_size = spec.size,
	};
	struct selector_fixture *fixture;
	struct processing_module *mod;

	zassert_not_null(driver, "Selector driver not registered");
	fixture = calloc(1, sizeof(*fixture));
	zassert_not_null(fixture);
	fixture->dev = driver->ops.create(driver, &ipc_cfg, &spec);
	zassert_not_null(fixture->dev, "Failed to create selector");
	fixture->dev->frames = TEST_FRAMES;
	fixture->dev->period = 1000;
	fixture->dev->direction = direction;
	mod = comp_mod(fixture->dev);
	mod->stream_params = calloc(1, sizeof(*mod->stream_params));
	zassert_not_null(mod->stream_params);

	fixture->source = create_test_buffer(
		fixture->dev, true,
		input->depth == IPC4_DEPTH_16BIT ? SOF_IPC_FRAME_S16_LE : SOF_IPC_FRAME_S32_LE,
		configured_source ? SOF_IPC_FRAME_S16_LE
				  : (enum sof_ipc_frame)((input->valid_bit_depth >> 3) - 2),
		input->channels_count);
	fixture->sink = create_test_buffer(
		fixture->dev, false,
		output->depth == IPC4_DEPTH_16BIT ? SOF_IPC_FRAME_S16_LE : SOF_IPC_FRAME_S32_LE,
		(enum sof_ipc_frame)((output->valid_bit_depth >> 3) - 2), output->channels_count);
	fixture->sources[0] = audio_buffer_get_source(&fixture->source->audio_buffer);
	fixture->sinks[0] = audio_buffer_get_sink(&fixture->sink->audio_buffer);

	if (configured_source) {
		struct sof_ipc_stream_params params = {
			.frame_fmt = SOF_IPC_FRAME_S32_LE,
			.rate = TEST_RATE,
			.channels = input->channels_count,
		};

		zassert_ok(buffer_set_params(fixture->source, &params, BUFFER_UPDATE_FORCE));
		zassert_equal(audio_stream_get_valid_fmt(&fixture->source->stream),
			      SOF_IPC_FRAME_S16_LE);
	}

	zassert_not_null(mod);
	return fixture;
}

/**
 * @brief Create a selector using the base-plus-output-format IPC4 payload.
 */
static struct selector_fixture *create_fixture(const struct ipc4_audio_format *input,
					       const struct ipc4_audio_format *output,
					       uint32_t direction, bool configured_source)
{
	return create_fixture_with_payload(input, output, direction, configured_source, false);
}

/**
 * @brief Free all resources owned by a selector fixture.
 */
static void destroy_fixture(struct selector_fixture *fixture)
{
	if (!fixture) {
		return;
	}

	free_test_buffer(fixture->source, true);
	free_test_buffer(fixture->sink, false);
	if (fixture->dev) {
		free(comp_mod(fixture->dev)->stream_params);
		fixture->dev->drv->ops.free(fixture->dev);
	}
	free(fixture);
}

/**
 * @brief Initialize the selector component registry for the suite.
 */
static void *selector_suite_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_selector_interface_init();
	return NULL;
}

ZTEST(selector_suite, test_dispatch_supported_and_invalid_formats)
{
	struct processing_module mod;
	struct comp_data cd;

	init_processing_module(&mod, &cd, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,
			       SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, 1, 1);
	zassert_not_null(sel_get_processing_function(&mod));

	cd.source_format = SOF_IPC_FRAME_S32_LE;
	cd.source_valid_format = SOF_IPC_FRAME_S24_4LE;
	cd.sink_format = SOF_IPC_FRAME_S16_LE;
	cd.sink_valid_format = SOF_IPC_FRAME_S16_LE;
	zassert_not_null(sel_get_processing_function(&mod));

	cd.source_valid_format = SOF_IPC_FRAME_S32_LE;
	zassert_is_null(sel_get_processing_function(&mod), "S32-to-S16 must not alias S24-to-S16");

	cd.source_format = SOF_IPC_FRAME_S32_LE;
	cd.source_valid_format = SOF_IPC_FRAME_S24_4LE;
	cd.sink_format = SOF_IPC_FRAME_S32_LE;
	cd.sink_valid_format = SOF_IPC_FRAME_S32_LE;
	zassert_is_null(sel_get_processing_function(&mod),
			"Unsupported S24-to-S32 must be rejected");
}

ZTEST(selector_suite, test_s24_and_s32_same_container_use_distinct_processing)
{
	const int32_t source = 0x00600000;
	int32_t output = 0;
	struct cir_buf_source source_view = {
		.ptr = &source,
		.buf_start = &source,
		.buf_end = &source + 1,
	};
	struct cir_buf_sink sink_view = {
		.ptr = &output,
		.buf_start = &output,
		.buf_end = &output + 1,
	};
	struct processing_module mod;
	struct comp_data cd;

	init_processing_module(&mod, &cd, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
			       SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, 1, 1);
	cd.coeffs_config.coeffs[0][0] = 2 * SEL_COEF_ONE_Q10;
	get_processing_function (&mod)(&mod, &source_view, &sink_view, 1);
	zassert_equal(output, 0x007fffff, "S24 processing did not clip to 24 bits");

	cd.source_valid_format = SOF_IPC_FRAME_S32_LE;
	cd.sink_valid_format = SOF_IPC_FRAME_S32_LE;
	output = 0;
	get_processing_function (&mod)(&mod, &source_view, &sink_view, 1);
	zassert_equal(output, 0x00c00000, "S32 processing clipped as S24");
}

ZTEST(selector_suite, test_s16_to_s24_unity_and_matrix)
{
	const int16_t source[] = {0, 1, -1, INT16_MAX, INT16_MIN};
	const int32_t expected[] = {0, 256, -256, 8388352, -8388608};
	struct cir_buf_source source_view = {
		.ptr = source,
		.buf_start = source,
		.buf_end = source + ARRAY_SIZE(source),
	};
	int32_t output[ARRAY_SIZE(source)] = {0};
	struct cir_buf_sink sink_view = {
		.ptr = output,
		.buf_start = output,
		.buf_end = output + ARRAY_SIZE(output),
	};
	struct processing_module mod;
	struct comp_data cd;
	sel_func function;

	init_processing_module(&mod, &cd, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,
			       SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, 1, 1);
	cd.coeffs_config.coeffs[0][0] = SEL_COEF_ONE_Q10;
	function = get_processing_function(&mod);
	function(&mod, &source_view, &sink_view, ARRAY_SIZE(source));
	zassert_mem_equal(output, expected, sizeof(expected));

	const int16_t matrix_source[] = {1000, 250};
	int32_t matrix_output = 0;

	cd.config.in_channels_count = 2;
	cd.coeffs_config.coeffs[0][0] = 512;
	cd.coeffs_config.coeffs[0][1] = -1024;
	source_view.ptr = matrix_source;
	source_view.buf_start = matrix_source;
	source_view.buf_end = matrix_source + ARRAY_SIZE(matrix_source);
	sink_view.ptr = &matrix_output;
	sink_view.buf_start = &matrix_output;
	sink_view.buf_end = &matrix_output + 1;
	function(&mod, &source_view, &sink_view, 1);
	zassert_equal(matrix_output, 64000);
}

ZTEST(selector_suite, test_s16_to_s24_saturates_wide_sum)
{
	int16_t source[SEL_SOURCE_CHANNELS_MAX];
	int32_t output = 0;
	struct processing_module mod;
	struct comp_data cd;
	struct cir_buf_source source_view = {
		.ptr = source,
		.buf_start = source,
		.buf_end = source + ARRAY_SIZE(source),
	};
	struct cir_buf_sink sink_view = {
		.ptr = &output,
		.buf_start = &output,
		.buf_end = &output + 1,
	};

	init_processing_module(&mod, &cd, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,
			       SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SEL_SOURCE_CHANNELS_MAX,
			       1);
	for (unsigned int i = 0; i < ARRAY_SIZE(source); i++) {
		source[i] = INT16_MIN;
		cd.coeffs_config.coeffs[0][i] = INT16_MIN;
	}

	get_processing_function (&mod)(&mod, &source_view, &sink_view, 1);
	zassert_equal(output, 8388607, "Positive overflow changed polarity");

	for (unsigned int i = 0; i < ARRAY_SIZE(source); i++) {
		cd.coeffs_config.coeffs[0][i] = INT16_MAX;
	}
	get_processing_function (&mod)(&mod, &source_view, &sink_view, 1);
	zassert_equal(output, -8388608);
}

ZTEST(selector_suite, test_s24_to_s16_unity_and_sign_extension)
{
	const int32_t source[] = {0, 256, -256, 0x007fffff, 0x00800000};
	const int16_t expected[] = {0, 1, -1, INT16_MAX, INT16_MIN};
	struct cir_buf_source source_view = {
		.ptr = source,
		.buf_start = source,
		.buf_end = source + ARRAY_SIZE(source),
	};
	int16_t output[ARRAY_SIZE(source)] = {0};
	struct cir_buf_sink sink_view = {
		.ptr = output,
		.buf_start = output,
		.buf_end = output + ARRAY_SIZE(output),
	};
	struct processing_module mod;
	struct comp_data cd;

	init_processing_module(&mod, &cd, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
			       SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, 1, 1);
	cd.coeffs_config.coeffs[0][0] = SEL_COEF_ONE_Q10;
	get_processing_function (&mod)(&mod, &source_view, &sink_view, ARRAY_SIZE(source));
	zassert_mem_equal(output, expected, sizeof(expected));
}

ZTEST(selector_suite, test_s24_to_s16_rounding_boundaries)
{
	const int32_t source[] = {127, 128, -128, -129};
	const int16_t expected[] = {0, 1, 0, -1};
	struct cir_buf_source source_view = {
		.ptr = source,
		.buf_start = source,
		.buf_end = source + ARRAY_SIZE(source),
	};
	int16_t output[ARRAY_SIZE(source)] = {0};
	struct cir_buf_sink sink_view = {
		.ptr = output,
		.buf_start = output,
		.buf_end = output + ARRAY_SIZE(output),
	};
	struct processing_module mod;
	struct comp_data cd;

	init_processing_module(&mod, &cd, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
			       SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, 1, 1);
	cd.coeffs_config.coeffs[0][0] = SEL_COEF_ONE_Q10;
	get_processing_function (&mod)(&mod, &source_view, &sink_view, ARRAY_SIZE(source));
	zassert_mem_equal(output, expected, sizeof(expected));
}

ZTEST(selector_suite, test_conversion_wraps_source_and_sink)
{
	int16_t source[] = {3, 4, 1, 2};
	int32_t output[] = {0x55555555, 0x55555555, 0x55555555, 0x55555555};
	struct cir_buf_source source_view = {
		.ptr = &source[2],
		.buf_start = source,
		.buf_end = source + ARRAY_SIZE(source),
	};
	struct cir_buf_sink sink_view = {
		.ptr = &output[3],
		.buf_start = output,
		.buf_end = output + ARRAY_SIZE(output),
	};
	struct processing_module mod;
	struct comp_data cd;

	init_processing_module(&mod, &cd, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,
			       SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, 1, 1);
	cd.coeffs_config.coeffs[0][0] = SEL_COEF_ONE_Q10;
	get_processing_function (&mod)(&mod, &source_view, &sink_view, 4);
	zassert_equal(output[3], 256);
	zassert_equal(output[0], 512);
	zassert_equal(output[1], 768);
	zassert_equal(output[2], 1024);
}

ZTEST(selector_suite, test_s24_to_s16_wraps_source_and_sink)
{
	int32_t source[] = {768, 1024, 256, 512};
	int16_t output[] = {INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX};
	struct cir_buf_source source_view = {
		.ptr = &source[2],
		.buf_start = source,
		.buf_end = source + ARRAY_SIZE(source),
	};
	struct cir_buf_sink sink_view = {
		.ptr = &output[3],
		.buf_start = output,
		.buf_end = output + ARRAY_SIZE(output),
	};
	struct processing_module mod;
	struct comp_data cd;

	init_processing_module(&mod, &cd, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
			       SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, 1, 1);
	cd.coeffs_config.coeffs[0][0] = SEL_COEF_ONE_Q10;
	get_processing_function (&mod)(&mod, &source_view, &sink_view, 4);
	zassert_equal(output[3], 1);
	zassert_equal(output[0], 2);
	zassert_equal(output[1], 3);
	zassert_equal(output[2], 4);
}

ZTEST(selector_suite, test_prepare_retains_configured_s32_valid_format)
{
	struct ipc4_audio_format format = test_format(IPC4_DEPTH_32BIT, IPC4_DEPTH_32BIT, 2);
	struct selector_fixture *fixture =
		create_fixture(&format, &format, SOF_IPC_STREAM_PLAYBACK, true);
	struct processing_module *mod = comp_mod(fixture->dev);
	struct comp_data *cd = module_get_private_data(mod);
	const struct module_interface *ops = fixture->dev->drv->adapter_ops;
	struct ipc4_selector_coeffs_config coeffs;
	int ret;

	set_identity_coefficients(&coeffs, 2);
	zassert_ok(send_coefficients(mod, &coeffs));
	ret = ops->prepare(mod, fixture->sources, 1, fixture->sinks, 1);
	zassert_equal(ret, 0, "prepare %d channels %u/%u formats %d/%d -> %d/%d periods %u/%u", ret,
		      cd->config.in_channels_count, cd->config.out_channels_count,
		      cd->source_format, cd->source_valid_format, cd->sink_format,
		      cd->sink_valid_format, cd->source_period_bytes, cd->sink_period_bytes);
	zassert_equal(cd->source_valid_format, SOF_IPC_FRAME_S32_LE);
	zassert_not_null(cd->sel_func);
	zassert_true(cd->passthrough);
	destroy_fixture(fixture);
}

ZTEST(selector_suite, test_extended_payload_capture_propagates_asymmetric_channels)
{
	struct ipc4_audio_format input = test_format(IPC4_DEPTH_16BIT, IPC4_DEPTH_16BIT, 2);
	struct ipc4_audio_format output = test_format(IPC4_DEPTH_16BIT, IPC4_DEPTH_16BIT, 1);
	struct selector_fixture *fixture =
		create_fixture_with_payload(&input, &output, SOF_IPC_STREAM_CAPTURE, false, true);
	struct processing_module *mod = comp_mod(fixture->dev);
	const struct module_interface *ops = fixture->dev->drv->adapter_ops;
	struct ipc4_selector_coeffs_config coeffs = {0};

	coeffs.coeffs[0][0] = SEL_COEF_ONE_Q10;
	zassert_ok(send_coefficients(mod, &coeffs));
	zassert_ok(ops->prepare(mod, fixture->sources, 1, fixture->sinks, 1));
	zassert_equal(audio_stream_get_channels(&fixture->source->stream), 2);
	zassert_equal(audio_stream_get_channels(&fixture->sink->stream), 1);
	zassert_equal(audio_stream_get_frm_fmt(&fixture->source->stream), SOF_IPC_FRAME_S16_LE);
	zassert_equal(audio_stream_get_frm_fmt(&fixture->sink->stream), SOF_IPC_FRAME_S16_LE);
	destroy_fixture(fixture);
}

ZTEST(selector_suite, test_nonidentity_coefficients_disable_same_format_passthrough)
{
	struct ipc4_audio_format format = test_format(IPC4_DEPTH_16BIT, IPC4_DEPTH_16BIT, 1);
	struct selector_fixture *fixture =
		create_fixture(&format, &format, SOF_IPC_STREAM_PLAYBACK, false);
	struct processing_module *mod = comp_mod(fixture->dev);
	struct comp_data *cd = module_get_private_data(mod);
	const struct module_interface *ops = fixture->dev->drv->adapter_ops;
	struct ipc4_selector_coeffs_config coeffs = {0};
	int16_t *source = audio_stream_get_wptr(&fixture->source->stream);
	int16_t *sink = audio_stream_get_wptr(&fixture->sink->stream);

	coeffs.coeffs[0][0] = SEL_COEF_ONE_Q10 / 2;
	zassert_ok(send_coefficients(mod, &coeffs));
	zassert_ok(ops->prepare(mod, fixture->sources, 1, fixture->sinks, 1));
	zassert_false(cd->passthrough);

	source[0] = 1000;
	audio_stream_produce(&fixture->source->stream, sizeof(*source));
	zassert_ok(ops->process(mod, fixture->sources, 1, fixture->sinks, 1));
	zassert_equal(sink[0], 500);
	destroy_fixture(fixture);
}

ZTEST(selector_suite, test_prepare_conversion_disables_passthrough_and_processes)
{
	struct ipc4_audio_format input = test_format(IPC4_DEPTH_16BIT, IPC4_DEPTH_16BIT, 1);
	struct ipc4_audio_format output = test_format(IPC4_DEPTH_32BIT, IPC4_DEPTH_24BIT, 1);
	struct selector_fixture *fixture =
		create_fixture(&input, &output, SOF_IPC_STREAM_PLAYBACK, false);
	struct processing_module *mod = comp_mod(fixture->dev);
	struct comp_data *cd = module_get_private_data(mod);
	const struct module_interface *ops = fixture->dev->drv->adapter_ops;
	struct ipc4_selector_coeffs_config coeffs;
	int16_t *source = audio_stream_get_wptr(&fixture->source->stream);
	int32_t *sink = audio_stream_get_wptr(&fixture->sink->stream);
	int ret;

	set_identity_coefficients(&coeffs, 1);
	zassert_ok(send_coefficients(mod, &coeffs));
	ret = ops->prepare(mod, fixture->sources, 1, fixture->sinks, 1);
	zassert_equal(ret, 0, "prepare %d channels %u/%u formats %d/%d -> %d/%d periods %u/%u", ret,
		      cd->config.in_channels_count, cd->config.out_channels_count,
		      cd->source_format, cd->source_valid_format, cd->sink_format,
		      cd->sink_valid_format, cd->source_period_bytes, cd->sink_period_bytes);
	zassert_false(cd->passthrough);
	zassert_equal(cd->source_valid_format, SOF_IPC_FRAME_S16_LE);
	zassert_equal(cd->sink_valid_format, SOF_IPC_FRAME_S24_4LE);

	for (int i = 0; i < TEST_FRAMES; i++) {
		source[i] = i - 2;
	}
	audio_stream_produce(&fixture->source->stream, TEST_FRAMES * sizeof(*source));
	zassert_ok(ops->process(mod, fixture->sources, 1, fixture->sinks, 1));
	for (int i = 0; i < TEST_FRAMES; i++) {
		zassert_equal(sink[i], (i - 2) * 256);
	}
	zassert_equal(audio_stream_get_avail_bytes(&fixture->source->stream), 0);
	zassert_equal(audio_stream_get_avail_bytes(&fixture->sink->stream),
		      TEST_FRAMES * sizeof(*sink));
	destroy_fixture(fixture);
}

ZTEST_SUITE(selector_suite, NULL, selector_suite_setup, NULL, NULL, NULL);
