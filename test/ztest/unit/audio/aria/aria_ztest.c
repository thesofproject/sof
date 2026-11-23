// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file aria_ztest.c
 * @brief Aria internal ring buffer tests.
 *
 * Aria keeps the last smpl_group_cnt frames in an internal ring and outputs
 * them delayed by that amount, scaled by its gain. These tests run the real
 * module through the module adapter with buffer sizes and short periods that
 * make the ring wrap at a different point than the sink buffer, and check the
 * output frame by frame.
 */

#include <zephyr/ztest.h>

#include <rtos/sof.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/stream.h>
#include <ipc4/base-config.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "aria.h"

#define TEST_RATE    48000
#define TEST_PERIODS 40
/* aria ramps its gain across the gain states, compare once it has settled */
#define TEST_SETTLED (2 * ARIA_MAX_GAIN_STATES)

struct aria_fixture {
	struct comp_dev *dev;
	struct comp_buffer *source;
	struct comp_buffer *sink;
	struct sof_source *sources[1];
	struct sof_sink *sinks[1];
};

/**
 * @brief Test sample for a frame and channel, small enough to keep full gain.
 * @param frame Input frame index.
 * @param ch Channel index.
 * @return S24 sample value.
 */
static int32_t test_sample(size_t frame, unsigned int ch)
{
	return (int32_t)(((frame & 0xfff) << 4) | ch);
}

/**
 * @brief Allocate a buffer and connect it to the aria component.
 * @param dev Aria component.
 * @param source True to connect as the aria source, false for the sink.
 * @param size Buffer size in bytes.
 * @param channels Channels count.
 * @return Connected buffer.
 */
static struct comp_buffer *create_test_buffer(struct comp_dev *dev, bool source, size_t size,
					      unsigned int channels)
{
	struct sof_ipc_buffer desc = {
		.size = size,
	};
	struct comp_buffer *buffer = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	zassert_not_null(buffer, "Failed to allocate test buffer");
	memset(audio_stream_get_addr(&buffer->stream), 0, size);
	audio_stream_set_frm_fmt(&buffer->stream, SOF_IPC_FRAME_S32_LE);
	audio_stream_set_valid_fmt(&buffer->stream, SOF_IPC_FRAME_S24_4LE);
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
 * @brief Create an aria instance from an IPC4 init payload and connect its buffers.
 * @param channels Channels count in the base config.
 * @param ibs Input buffer size in bytes, sets the 1 ms chunk.
 * @param source_bytes Source buffer size in bytes.
 * @param sink_bytes Sink buffer size in bytes.
 * @param attenuation Aria attenuation parameter.
 * @return Fixture, not yet prepared.
 */
static struct aria_fixture *create_fixture(unsigned int channels, uint32_t ibs,
					   size_t source_bytes, size_t sink_bytes,
					   uint32_t attenuation)
{
	const struct sof_uuid uuid = SOF_REG_UUID(aria);
	const struct comp_driver *driver = comp_driver_find(&uuid, SOF_COMP_MODULE_ADAPTER);
	struct ipc4_aria_module_cfg cfg = {
		.base_cfg =
			{
				.ibs = ibs,
				.obs = ibs,
				.audio_fmt =
					{
						.sampling_frequency = TEST_RATE,
						.depth = IPC4_DEPTH_32BIT,
						.ch_map = 0x76543210,
						.ch_cfg = IPC4_CHANNEL_CONFIG_INVALID,
						.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
						.channels_count = channels,
						.valid_bit_depth = IPC4_DEPTH_24BIT,
						.s_type = IPC4_TYPE_SIGNED_INTEGER,
					},
			},
		.attenuation = attenuation,
	};
	struct ipc_config_process spec = {
		.size = sizeof(cfg),
		.data = (const uint8_t *)&cfg,
	};
	struct comp_ipc_config ipc_cfg = {
		.id = 1,
		.proc_domain = COMP_PROCESSING_DOMAIN_LL,
		.type = SOF_COMP_MODULE_ADAPTER,
		.ipc_config_size = spec.size,
	};
	struct aria_fixture *fixture;

	zassert_not_null(driver, "Aria driver not registered");
	fixture = calloc(1, sizeof(*fixture));
	zassert_not_null(fixture);
	fixture->dev = driver->ops.create(driver, &ipc_cfg, &spec);
	zassert_not_null(fixture->dev, "Failed to create aria");

	fixture->source = create_test_buffer(fixture->dev, true, source_bytes, channels);
	fixture->sink = create_test_buffer(fixture->dev, false, sink_bytes, channels);
	fixture->sources[0] = audio_buffer_get_source(&fixture->source->audio_buffer);
	fixture->sinks[0] = audio_buffer_get_sink(&fixture->sink->audio_buffer);

	return fixture;
}

/**
 * @brief Prepare the aria instance of a fixture with its connected buffers.
 * @param fixture Fixture to prepare.
 * @return Status from the aria prepare handler.
 */
static int prepare_fixture(struct aria_fixture *fixture)
{
	const struct module_interface *ops = fixture->dev->drv->adapter_ops;

	return ops->prepare(comp_mod(fixture->dev), fixture->sources, 1, fixture->sinks, 1);
}

/**
 * @brief Free all resources owned by an aria fixture.
 * @param fixture Fixture to free.
 */
static void destroy_fixture(struct aria_fixture *fixture)
{
	free(fixture->source->source);
	buffer_free(fixture->source);
	free(fixture->sink->sink);
	buffer_free(fixture->sink);
	fixture->dev->drv->ops.free(fixture->dev);
	free(fixture);
}

/**
 * @brief Run aria period by period and check the delayed, scaled output.
 * @param channels Channels count.
 * @param chunk_frames Frames per 1 ms chunk, ibs in frames.
 * @param source_frames Source buffer size in frames.
 * @param sink_frames Sink buffer size in frames.
 * @param first_frames Frames available in the first period.
 * @param attenuation Aria attenuation parameter.
 */
static void run_aria(unsigned int channels, size_t chunk_frames, size_t source_frames,
		     size_t sink_frames, size_t first_frames, uint32_t attenuation)
{
	const size_t frame_bytes = channels * sizeof(int32_t);
	struct aria_fixture *fixture = create_fixture(channels, chunk_frames * frame_bytes,
						      source_frames * frame_bytes,
						      sink_frames * frame_bytes, attenuation);
	struct processing_module *mod = comp_mod(fixture->dev);
	const struct module_interface *ops = fixture->dev->drv->adapter_ops;
	struct audio_stream *source = &fixture->source->stream;
	struct audio_stream *sink = &fixture->sink->stream;
	size_t in_frame = 0;
	size_t out_frame = 0;

	zassert_ok(prepare_fixture(fixture));

	for (int period = 0; period < TEST_PERIODS; period++) {
		size_t frames = period ? chunk_frames : first_frames;
		int32_t *w = audio_stream_get_wptr(source);
		int32_t *r;
		size_t avail;

		for (size_t f = 0; f < frames; f++) {
			for (unsigned int ch = 0; ch < channels; ch++) {
				*w = test_sample(in_frame + f, ch);
				w = audio_stream_wrap(source, w + 1);
			}
		}
		audio_stream_produce(source, frames * frame_bytes);
		in_frame += frames;

		zassert_ok(ops->process(mod, fixture->sources, 1, fixture->sinks, 1));

		avail = audio_stream_get_avail_frames(sink);
		zassert_equal(avail, frames, "period %d: %zu frames out, %zu in", period, avail,
			      frames);

		r = audio_stream_get_rptr(sink);
		for (size_t f = 0; f < avail; f++, out_frame++) {
			for (unsigned int ch = 0; ch < channels; ch++) {
				int32_t expected = test_sample(out_frame - chunk_frames, ch)
						   << attenuation;

				if (period >= TEST_SETTLED) {
					zassert_equal(*r, expected,
						      "frame %zu ch %u: got %#x expected %#x",
						      out_frame, ch, *r, expected);
				}
				r = audio_stream_wrap(sink, r + 1);
			}
		}
		audio_stream_consume(sink, avail * frame_bytes);
	}

	destroy_fixture(fixture);
}

/**
 * @brief Initialize the aria component registry for the suite.
 * @return Suite fixture, unused.
 */
static void *aria_suite_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_aria_interface_init();
	return NULL;
}

/*
 * A short first period leaves the read position inside the ring, and with a
 * 70 frame sink the sink wraps before the ring in later periods. The ring
 * wrap distance must follow the read pointer across the sink wrap.
 */
ZTEST(aria_suite, test_sink_wrap_before_ring_wrap)
{
	run_aria(2, 48, 96, 70, 20, 1);
}

/*
 * 3 channels and a 47 frame chunk give an odd number of samples per chunk.
 * The ring must still hold whole frames, so no frame straddles its end and
 * channels stay in place.
 */
ZTEST(aria_suite, test_odd_samples_per_chunk)
{
	run_aria(3, 47, 94, 94, 47, 1);
}

/*
 * Odd samples per chunk, a short first period and a sink that wraps before
 * the ring in the same run.
 */
ZTEST(aria_suite, test_odd_samples_sink_wrap_before_ring_wrap)
{
	run_aria(3, 47, 94, 70, 20, 1);
}

/*
 * With attenuation 0 aria bypasses the gain and copies the delayed input
 * from the ring with cir_buf_copy(), which must wrap the same way.
 */
ZTEST(aria_suite, test_bypass_sink_wrap_before_ring_wrap)
{
	run_aria(3, 47, 94, 70, 20, 0);
}

/*
 * The get_data paths write whole frames up to the sink buffer end, so a
 * source or sink buffer that is not a whole number of frames is rejected at
 * prepare instead of overrunning the sink on the wrap.
 */
ZTEST(aria_suite, test_rejects_partial_frame_buffers)
{
	const size_t frame_bytes = 3 * sizeof(int32_t);
	struct aria_fixture *fixture;

	fixture = create_fixture(3, 47 * frame_bytes, 94 * frame_bytes,
				 70 * frame_bytes + sizeof(int32_t), 1);
	zassert_equal(prepare_fixture(fixture), -EINVAL);
	destroy_fixture(fixture);

	fixture = create_fixture(3, 47 * frame_bytes, 94 * frame_bytes + sizeof(int32_t),
				 70 * frame_bytes, 1);
	zassert_equal(prepare_fixture(fixture), -EINVAL);
	destroy_fixture(fixture);
}

ZTEST_SUITE(aria_suite, NULL, aria_suite_setup, NULL, NULL, NULL);
