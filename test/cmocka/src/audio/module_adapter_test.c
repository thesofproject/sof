// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//

#include "../util.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include "module_adapter.h"

int module_adapter_test_setup(struct processing_module_test_data *test_data)
{
	struct processing_module_test_parameters *parameters = &test_data->parameters;
	struct processing_module *mod;
	struct comp_dev *dev;
	uint32_t size;
	int i;

	/* allocate and set new device */
	mod = test_malloc(sizeof(struct processing_module));
	test_data->mod = mod;
	dev = test_malloc(sizeof(struct comp_dev));
	dev->frames = parameters->frames;
	mod->dev = dev;
	comp_set_drvdata(dev, mod);

	test_data->sinks = test_calloc(test_data->num_sinks, sizeof(struct comp_buffer *));
	test_data->sources = test_calloc(test_data->num_sources, sizeof(struct comp_buffer *));

	test_data->input_buffers = test_calloc(test_data->num_sources,
					       sizeof(struct input_stream_buffer *));
	test_data->output_buffers = test_calloc(test_data->num_sinks,
						sizeof(struct output_stream_buffer *));

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	/* allocate sink buffers */
	size = parameters->frames *
		get_frame_bytes(parameters->sink_format, parameters->channels) *
		parameters->buffer_size_ms;
	for (i = 0; i < test_data->num_sinks; i++) {
		test_data->sinks[i] = create_test_sink(dev, 0, parameters->sink_format,
						       parameters->channels, size);
		test_data->output_buffers[i] = test_malloc(sizeof(struct output_stream_buffer));
		test_data->output_buffers[i]->data = &test_data->sinks[i]->stream;
	}

	/* allocate source buffers */
	size = parameters->frames * get_frame_bytes(parameters->source_format,
	       parameters->channels) * parameters->buffer_size_ms;
	for (i = 0; i < test_data->num_sources; i++) {
		test_data->sources[i] = create_test_source(dev, 0, parameters->source_format,
							   parameters->channels, size);
		test_data->input_buffers[i] = test_malloc(sizeof(struct input_stream_buffer));
		test_data->input_buffers[i]->data = &test_data->sources[i]->stream;
	}

	test_data->verify = parameters->verify;

	return 0;
}

void module_adapter_test_free(struct processing_module_test_data *test_data)
{
	int i;

	for (i = 0; i < test_data->num_sinks; i++) {
		free_test_sink(test_data->sinks[i]);
		test_free(test_data->output_buffers[i]);
	}

	for (i = 0; i < test_data->num_sources; i++) {
		free_test_sink(test_data->sources[i]);
		test_free(test_data->input_buffers[i]);
	}

	test_free(test_data->input_buffers);
	test_free(test_data->output_buffers);
	test_free(test_data->sinks);
	test_free(test_data->sources);
	test_free(test_data->mod->dev);
	test_free(test_data->mod);
}
