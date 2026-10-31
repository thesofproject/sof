// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file crossover_blob_ztest.c
 * @brief Run-time configuration update tests for the crossover component.
 *
 * comp_get_data_blob() frees the blob in use when it hands out a new one, so a
 * blob the component cannot use has to be refused when it is set, while the
 * blob in use is still there. These tests run the real crossover.c on top of
 * the real data blob handler and module adapter, under ASan in CI.
 */

#include <zephyr/ztest.h>

#include <kernel/header.h>
#include <rtos/sof.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/common.h>
#include <sof/list.h>
#include <ipc/control.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crossover/crossover.h"
#include "crossover/crossover_user.h"

#define TEST_CHANNELS		2
#define TEST_FRAMES		48
#define TEST_NUM_SINKS		CROSSOVER_3WAY_NUM_SINKS
/* Room for the periods of all the process calls of one test */
#define TEST_BUFFER_BYTES	(TEST_FRAMES * TEST_CHANNELS * sizeof(int32_t) * 4)
/* The IPC3 crossover identifies its sinks by their pipeline ids */
#define TEST_FIRST_PIPELINE_ID	1

struct test_data {
	struct comp_dev *dev;
	struct comp_buffer *source;
	struct comp_buffer *sink[TEST_NUM_SINKS];
	struct sof_source *sources[1];
	struct sof_sink *sinks[TEST_NUM_SINKS];
};

/**
 * @brief Create a blob routing its bands to the test sinks, in order.
 */
static struct sof_crossover_config *create_config(uint32_t num_sinks)
{
	const size_t num_biquads = num_sinks == CROSSOVER_2WAY_NUM_SINKS ? 2 : 6;
	const size_t size = sizeof(struct sof_crossover_config) +
		num_biquads * sizeof(struct sof_eq_iir_biquad);
	struct sof_crossover_config *config = calloc(1, size);
	size_t i;

	zassert_not_null(config);

	config->size = size;
	config->num_sinks = num_sinks;
	for (i = 0; i < num_sinks; i++)
		config->assign_sink[i] = TEST_FIRST_PIPELINE_ID + i;

	for (i = 0; i < num_biquads; i++) {
		config->coef[i].b0 = 1 << 30;
		config->coef[i].output_gain = 1 << 14;
	}

	return config;
}

/**
 * @brief Send one complete configuration blob through the module API.
 */
static int send_config(struct processing_module *mod, const struct sof_crossover_config *config)
{
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;
	const size_t size = config->size;
	struct sof_ipc_ctrl_data *cdata;
	int ret;

	cdata = calloc(1, sizeof(*cdata) + sizeof(struct sof_abi_hdr) + size);
	zassert_not_null(cdata);

	cdata->cmd = SOF_CTRL_CMD_BINARY;
	cdata->num_elems = size;
	cdata->data[0].size = size;
	memcpy_s(cdata->data[0].data, size, config, size);

	ret = ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE, size,
				     (const uint8_t *)cdata, size, NULL, 0);

	free(cdata);
	return ret;
}

/**
 * @brief Create a zeroed S32 test buffer owned by the given pipeline.
 */
static struct comp_buffer *create_test_buffer(uint32_t pipeline_id)
{
	struct sof_ipc_buffer desc = {
		.comp = {
			.pipeline_id = pipeline_id,
		},
		.size = TEST_BUFFER_BYTES,
	};
	struct comp_buffer *buffer = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	zassert_not_null(buffer);

	memset(buffer->stream.addr, 0, TEST_BUFFER_BYTES);
	audio_stream_set_frm_fmt(&buffer->stream, SOF_IPC_FRAME_S32_LE);
	audio_stream_set_channels(&buffer->stream, TEST_CHANNELS);

	return buffer;
}

/**
 * @brief Create the IPC description used to instantiate the crossover module.
 */
static struct sof_ipc_comp_process *create_comp_ipc(void)
{
	struct sof_ipc_comp_process *ipc;
	const size_t ipc_size = sizeof(*ipc);
	const struct sof_uuid uuid = SOF_REG_UUID(crossover);

	ipc = calloc(1, ipc_size + SOF_UUID_SIZE);
	zassert_not_null(ipc);

	memcpy_s(ipc + 1, SOF_UUID_SIZE, &uuid, SOF_UUID_SIZE);
	ipc->comp.hdr.size = ipc_size + SOF_UUID_SIZE;
	ipc->comp.type = SOF_COMP_MODULE_ADAPTER;
	ipc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	ipc->comp.ext_data_length = SOF_UUID_SIZE;

	return ipc;
}

/**
 * @brief Create a 3-way crossover with a config, prepared and streaming.
 */
static struct test_data *create_test_data(void)
{
	struct sof_crossover_config *config = create_config(TEST_NUM_SINKS);
	struct sof_ipc_comp_process *ipc = create_comp_ipc();
	struct test_data *td = calloc(1, sizeof(*td));
	struct processing_module *mod;
	int i;

	zassert_not_null(td);

	td->dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);
	zassert_not_null(td->dev);
	td->dev->frames = TEST_FRAMES;
	mod = comp_mod(td->dev);

	td->source = create_test_buffer(0);
	td->source->source = calloc(1, sizeof(*td->source->source));
	zassert_not_null(td->source->source);
	list_item_append(&td->source->sink_list, &td->dev->bsource_list);
	td->sources[0] = audio_buffer_get_source(&td->source->audio_buffer);

	for (i = 0; i < TEST_NUM_SINKS; i++) {
		td->sink[i] = create_test_buffer(TEST_FIRST_PIPELINE_ID + i);
		td->sink[i]->sink = calloc(1, sizeof(*td->sink[i]->sink));
		zassert_not_null(td->sink[i]->sink);
		list_item_append(&td->sink[i]->source_list, &td->dev->bsink_list);
		td->sinks[i] = audio_buffer_get_sink(&td->sink[i]->audio_buffer);
	}

	zassert_ok(send_config(mod, config));
	free(config);
	zassert_ok(module_prepare(mod, td->sources, 1, td->sinks, TEST_NUM_SINKS));

	td->dev->state = COMP_STATE_ACTIVE;
	td->source->source->state = COMP_STATE_ACTIVE;
	for (i = 0; i < TEST_NUM_SINKS; i++)
		td->sink[i]->sink->state = COMP_STATE_ACTIVE;

	return td;
}

/**
 * @brief Release everything allocated by create_test_data().
 */
static void destroy_test_data(struct test_data *td)
{
	int i;

	free(td->source->source);
	buffer_free(td->source);
	for (i = 0; i < TEST_NUM_SINKS; i++) {
		free(td->sink[i]->sink);
		buffer_free(td->sink[i]);
	}
	comp_free(td->dev);
	free(td);
}

/**
 * @brief Run one period through the component, as an LL or a DP task would.
 */
static void process_one_period(struct test_data *td)
{
	const uint32_t bytes = TEST_FRAMES * TEST_CHANNELS * sizeof(int32_t);
	const uint32_t sink_bytes = audio_stream_get_avail_bytes(&td->sink[0]->stream);
	int i;

	audio_stream_produce(&td->source->stream, bytes);
	zassert_ok(module_process_sink_src(comp_mod(td->dev), td->sources, 1, td->sinks,
					   TEST_NUM_SINKS));

	for (i = 0; i < TEST_NUM_SINKS; i++)
		zassert_equal(audio_stream_get_avail_bytes(&td->sink[i]->stream),
			      sink_bytes + bytes, "sink %d got no output", i);
}

/**
 * @brief A blob that has to be refused must leave the blob in use untouched.
 */
static void check_config_refused(struct sof_crossover_config *config)
{
	struct test_data *td = create_test_data();
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_crossover_config *current_config = cd->config;

	zassert_equal(send_config(mod, config), -EINVAL);
	zassert_false(comp_is_new_data_blob_available(cd->model_handler));

	process_one_period(td);
	process_one_period(td);

	zassert_equal_ptr(cd->config, current_config);
	zassert_equal(cd->config->num_sinks, TEST_NUM_SINKS);

	destroy_test_data(td);
	free(config);
}

/**
 * @brief An out of range num_sinks must not reach the processing path.
 */
ZTEST(crossover_blob_suite, test_crossover_refuses_invalid_num_sinks)
{
	struct sof_crossover_config *config = create_config(TEST_NUM_SINKS);

	config->num_sinks = SOF_CROSSOVER_MAX_STREAMS + 1;
	check_config_refused(config);
}

/**
 * @brief A valid blob with fewer sinks must not change a running filter.
 *
 * The sink assignment check stops at the first sink the blob does not name, so
 * this is the change that gets past it.
 */
ZTEST(crossover_blob_suite, test_crossover_refuses_fewer_sinks)
{
	check_config_refused(create_config(CROSSOVER_2WAY_NUM_SINKS));
}

/**
 * @brief A valid blob with more sinks must not change a running filter.
 */
ZTEST(crossover_blob_suite, test_crossover_refuses_more_sinks)
{
	check_config_refused(create_config(CROSSOVER_4WAY_NUM_SINKS));
}

/**
 * @brief A valid blob for the same sink count is still applied at run time.
 */
ZTEST(crossover_blob_suite, test_crossover_applies_same_num_sinks)
{
	struct test_data *td = create_test_data();
	struct sof_crossover_config *config = create_config(TEST_NUM_SINKS);
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_data *cd = module_get_private_data(mod);

	config->coef[0].output_gain = 1 << 13;

	zassert_ok(send_config(mod, config));
	zassert_true(comp_is_new_data_blob_available(cd->model_handler));

	process_one_period(td);

	zassert_false(comp_is_new_data_blob_available(cd->model_handler));
	zassert_equal(cd->config->coef[0].output_gain, 1 << 13);

	destroy_test_data(td);
	free(config);
}

/**
 * @brief Initialize the SOF component registry for the suite.
 */
static void *crossover_blob_suite_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_crossover_interface_init();

	return NULL;
}

ZTEST_SUITE(crossover_blob_suite, NULL, crossover_blob_suite_setup, NULL, NULL, NULL);
