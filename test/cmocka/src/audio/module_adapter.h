// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//

struct processing_module_test_parameters {
	uint32_t channels;
	uint32_t frames;
	uint32_t buffer_size_ms;
	uint32_t source_format;
	uint32_t sink_format;
	void (*verify)(struct processing_module *mod, struct comp_buffer *sink,
		       struct comp_buffer *source);
};

struct processing_module_test_data {
	struct processing_module *mod;
	struct comp_buffer **sinks;
	struct comp_buffer **sources;
	struct input_stream_buffer **input_buffers;
	struct output_stream_buffer **output_buffers;
	uint32_t num_sources;
	uint32_t num_sinks;
	struct processing_module_test_parameters parameters;
	void (*verify)(struct processing_module *mod, struct comp_buffer *sink,
		       struct comp_buffer *source);
};

int module_adapter_test_setup(struct processing_module_test_data *test_data);
void module_adapter_test_free(struct processing_module_test_data *test_data);
