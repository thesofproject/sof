// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmocka.h>
#include <kernel/header.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/control.h>

#include "drc/drc.h"
#include "../../util.h"
#include "../../../include/cmocka_chirp_2ch.h"
#include "cmocka_drc_coef.h"

/* Maximum absolute value of a signed 24 bit sample */
#define S24_MAX_ABS 0x800000

struct buffer_state {
	int idx;
};

static struct buffer_state buffer_fill_data;
static struct buffer_state buffer_verify_data;

struct test_parameters {
	uint32_t channels;
	uint32_t frames;
	uint32_t buffer_size_mult;
	uint32_t source_format;
	uint32_t sink_format;
	const uint32_t *config;   /* sof_abi_hdr wrapped DRC config blob */
	bool passthrough;   /* true when config has params.enabled == 0 */
};

struct test_data {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct test_parameters *params;
	bool continue_loop;
	int diff_count;     /* enabled mode: output samples that differ from input */
};

static int setup_group(void **state)
{
	sys_comp_init(sof_get());
	sys_comp_module_drc_interface_init();
	return 0;
}

static struct sof_ipc_comp_process *create_drc_comp_ipc(struct test_data *td)
{
	struct sof_ipc_comp_process *ipc;
	size_t ipc_size = sizeof(struct sof_ipc_comp_process);
	const struct sof_uuid uuid = SOF_REG_UUID(drc);

	ipc = calloc(1, ipc_size + SOF_UUID_SIZE);
	memcpy_s(ipc + 1, SOF_UUID_SIZE, &uuid, SOF_UUID_SIZE);
	ipc->comp.hdr.size = ipc_size + SOF_UUID_SIZE;
	ipc->comp.type = SOF_COMP_MODULE_ADAPTER;
	ipc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	ipc->size = 0;
	ipc->comp.ext_data_length = SOF_UUID_SIZE;
	return ipc;
}

static int drc_send_config(struct processing_module *mod, const uint32_t *config)
{
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;
	const struct sof_abi_hdr *blob = (const struct sof_abi_hdr *)config;
	size_t cdata_size = sizeof(struct sof_ipc_ctrl_data) + sizeof(struct sof_abi_hdr) +
		blob->size;
	struct sof_ipc_ctrl_data *cdata;
	int ret;

	cdata = calloc(1, cdata_size);
	if (!cdata)
		return -ENOMEM;

	cdata->cmd = SOF_CTRL_CMD_BINARY;
	cdata->num_elems = blob->size;
	cdata->data[0].magic = blob->magic;
	cdata->data[0].type = blob->type;
	cdata->data[0].size = blob->size;
	cdata->data[0].abi = blob->abi;
	memcpy_s(cdata->data[0].data, blob->size, blob->data, blob->size);

	ret = ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE,
				     blob->size, (const uint8_t *)cdata,
				     blob->size, NULL, 0);

	free(cdata);
	return ret;
}

static void prepare_buffers(struct test_data *td)
{
	struct test_parameters *p = td->params;
	size_t src_size = p->frames * get_frame_bytes(p->source_format, p->channels) *
		p->buffer_size_mult;
	size_t sink_size = p->frames * get_frame_bytes(p->sink_format, p->channels) *
		p->buffer_size_mult;

	td->source = create_test_source(td->dev, 0, p->source_format, p->channels, src_size);
	td->sink = create_test_sink(td->dev, 0, p->sink_format, p->channels, sink_size);
}

static int setup(void **state)
{
	struct test_parameters *params = *state;
	struct processing_module *mod;
	struct test_data *td;
	struct sof_ipc_comp_process *ipc;
	struct comp_dev *dev;
	int ret;

	td = test_malloc(sizeof(*td));
	if (!td)
		return -EINVAL;

	td->params = test_malloc(sizeof(*params));
	if (!td->params) {
		test_free(td);
		return -EINVAL;
	}

	memcpy_s(td->params, sizeof(*td->params), params, sizeof(*params));
	buffer_fill_data.idx = 0;
	buffer_verify_data.idx = 0;
	td->diff_count = 0;
	td->continue_loop = true;

	ipc = create_drc_comp_ipc(td);
	dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);
	if (!dev) {
		test_free(td->params);
		test_free(td);
		return -EINVAL;
	}

	td->dev = dev;
	dev->frames = params->frames;
	mod = comp_mod(dev);

	ret = drc_send_config(mod, params->config);
	if (ret) {
		comp_free(td->dev);
		test_free(td->params);
		test_free(td);
		return ret;
	}

	prepare_buffers(td);

	ret = module_prepare(mod, NULL, 0, NULL, 0);
	if (ret) {
 		free_test_source(td->source);
 		free_test_sink(td->sink);
 		comp_free(td->dev);
 		test_free(td->params);
 		test_free(td);
		return ret;
	}

	*state = td;
	return 0;
}

static int teardown(void **state)
{
	struct test_data *td = *state;

	test_free(td->params);
	free_test_source(td->source);
	free_test_sink(td->sink);
	comp_free(td->dev);
	test_free(td);
	return 0;
}

static void make_views(struct test_data *td, struct cir_buf_source *source_buf,
		       struct cir_buf_sink *sink_buf)
{
	struct audio_stream *ss = &td->source->stream;
	struct audio_stream *ds = &td->sink->stream;

	source_buf->buf_start = audio_stream_get_addr(ss);
	source_buf->buf_end = audio_stream_get_end_addr(ss);
	source_buf->ptr = audio_stream_get_addr(ss);

	sink_buf->buf_start = audio_stream_get_addr(ds);
	sink_buf->buf_end = audio_stream_get_end_addr(ds);
	sink_buf->ptr = audio_stream_get_addr(ds);
}

#if CONFIG_FORMAT_S16LE
static int fill_source_s16(struct test_data *td, int frames)
{
	int16_t *x = audio_stream_get_addr(&td->source->stream);
	int samples = frames * td->params->channels;
	int i;

	for (i = 0; i < samples; i++) {
		x[i] = sat_int16(Q_SHIFT_RND(chirp_2ch[buffer_fill_data.idx++], 31, 15));
		if (buffer_fill_data.idx == CHIRP_2CH_LENGTH) {
			td->continue_loop = false;
			i++;
			break;
		}
	}
	return i / td->params->channels;
}

static void verify_sink_s16(struct test_data *td, int frames)
{
	int16_t *y = audio_stream_get_addr(&td->sink->stream);
	int samples = frames * td->params->channels;
	int32_t ref;
	int32_t out;
	int i;

	for (i = 0; i < samples; i++) {
		out = y[i];
		ref = sat_int16(Q_SHIFT_RND(chirp_2ch[buffer_verify_data.idx++], 31, 15));
		if (td->params->passthrough)
			assert_int_equal(out, ref);
		else if (out != ref)
			td->diff_count++;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static int fill_source_s24(struct test_data *td, int frames)
{
	int32_t *x = audio_stream_get_addr(&td->source->stream);
	int samples = frames * td->params->channels;
	int i;

	for (i = 0; i < samples; i++) {
		x[i] = sat_int24(Q_SHIFT_RND(chirp_2ch[buffer_fill_data.idx++], 31, 23));
		if (buffer_fill_data.idx == CHIRP_2CH_LENGTH) {
			td->continue_loop = false;
			i++;
			break;
		}
	}
	return i / td->params->channels;
}

static void verify_sink_s24(struct test_data *td, int frames)
{
	int32_t *y = audio_stream_get_addr(&td->sink->stream);
	int samples = frames * td->params->channels;
	int32_t ref;
	int32_t out;
	int i;

	for (i = 0; i < samples; i++) {
		out = (y[i] << 8) >> 8; /* Make sure there's no 24 bit overflow */
		ref = sat_int24(Q_SHIFT_RND(chirp_2ch[buffer_verify_data.idx++], 31, 23));
		/* DRC must never produce a value outside of the 24 bit range */
		assert_true(out < S24_MAX_ABS && out >= -S24_MAX_ABS);
		if (td->params->passthrough)
			assert_int_equal(out, ref);
		else if (out != ref)
			td->diff_count++;
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static int fill_source_s32(struct test_data *td, int frames)
{
	int32_t *x = audio_stream_get_addr(&td->source->stream);
	int samples = frames * td->params->channels;
	int i;

	for (i = 0; i < samples; i++) {
		x[i] = chirp_2ch[buffer_fill_data.idx++];
		if (buffer_fill_data.idx == CHIRP_2CH_LENGTH) {
			td->continue_loop = false;
			i++;
			break;
		}
	}
	return i / td->params->channels;
}

static void verify_sink_s32(struct test_data *td, int frames)
{
	int32_t *y = audio_stream_get_addr(&td->sink->stream);
	int samples = frames * td->params->channels;
	int32_t ref;
	int32_t out;
	int i;

	for (i = 0; i < samples; i++) {
		out = y[i];
		ref = chirp_2ch[buffer_verify_data.idx++];
		if (td->params->passthrough)
			assert_int_equal(out, ref);
		else if (out != ref)
			td->diff_count++;
	}
}
#endif /* CONFIG_FORMAT_S32LE */

static int fill_source(struct test_data *td, int frames)
{
	switch (td->params->source_format) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		return fill_source_s16(td, frames);
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		return fill_source_s24(td, frames);
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		return fill_source_s32(td, frames);
#endif
	default:
		assert(0);
		return 0;
	}
}

static void verify_sink(struct test_data *td, int frames)
{
	switch (td->params->sink_format) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		verify_sink_s16(td, frames);
		break;
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		verify_sink_s24(td, frames);
		break;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		verify_sink_s32(td, frames);
		break;
#endif
	default:
		assert(0);
		break;
	}
}

static void test_audio_drc(void **state)
{
	struct test_data *td = *state;
	struct processing_module *mod = comp_mod(td->dev);
	struct drc_comp_data *cd = module_get_private_data(mod);
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;
	int frames;

	while (td->continue_loop) {
		frames = fill_source(td, td->params->frames);
		if (frames <= 0)
			break;

		make_views(td, &source_buf, &sink_buf);
		cd->drc_func(mod, &source_buf, &sink_buf, frames);

		verify_sink(td, frames);
	}

	/* An enabled DRC must actually change the signal, otherwise the
	 * processing function was never exercised (e.g. silently fell back
	 * to pass-through).
	 */
	if (!td->params->passthrough)
		assert_true(td->diff_count > 0);
}

static struct test_parameters parameters[] = {
#if CONFIG_FORMAT_S16LE
	{ 2, 48, 2, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, drc_coef_pass_2ch, true },
	{ 2, 48, 2, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, drc_coef_enabled_2ch, false },
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ 2, 48, 2, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, drc_coef_pass_2ch, true },
	{ 2, 48, 2, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, drc_coef_enabled_2ch, false },
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ 2, 48, 2, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, drc_coef_pass_2ch, true },
	{ 2, 48, 2, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, drc_coef_enabled_2ch, false },
#endif /* CONFIG_FORMAT_S32LE */
};

int main(void)
{
	int i;

	struct CMUnitTest tests[ARRAY_SIZE(parameters)];

	for (i = 0; i < ARRAY_SIZE(parameters); i++) {
		tests[i].name = "test_audio_drc";
		tests[i].test_func = test_audio_drc;
		tests[i].setup_func = setup;
		tests[i].teardown_func = teardown;
		tests[i].initial_state = &parameters[i];
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup_group, NULL);
}
