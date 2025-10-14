// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>


#include <stddef.h>
#include <module/module/interface.h>
#include <sof/audio/audio_stream.h>
#include <openvino/openvino.hpp>

#include "noise_suppression_interface.h"
#define NS_MAX_SOURCE_CHANNELS 2

extern "C" {
	struct ns_data {
		std::shared_ptr<ov::Model> model;
		std::vector<std::pair<std::string, std::string>> state_names;
		ov::InferRequest infer_request[NS_MAX_SOURCE_CHANNELS];
		ov::Shape inp_shape;
		int iter;
	};

	int ov_ns_init(ns_handle *handle) {
		struct ns_data *nd;
		ov::OutputVector inputs, outputs;
		ov::Core core;
		size_t state_size = 0;
		const char* model_name = std::getenv("NOISE_SUPPRESSION_MODEL_NAME");
		std::string device("CPU");
		int i;

		nd = new ns_data();
		if(!nd)
			return -ENOMEM;

		*handle = nd;
		nd->model = core.read_model(model_name);
		inputs = nd->model->inputs();
		outputs = nd->model->outputs();

		/* get state name pairs */
		for (size_t i = 0; i < inputs.size(); i++) {
			std::string inp_state_name = inputs[i].get_any_name();
			if (inp_state_name.find("inp_state_") == std::string::npos)
			    continue;

			std::string out_state_name(inp_state_name);
			out_state_name.replace(0, 3, "out");

			/* find corresponding output state */
			if (outputs.end() == std::find_if(outputs.begin(), outputs.end(), [&out_state_name](const ov::Output<ov::Node>& output) {
				return output.get_any_name() == out_state_name;
			}))
				return -EINVAL;

			nd->state_names.emplace_back(inp_state_name, out_state_name);

			ov::Shape shape = inputs[i].get_shape();
			size_t tensor_size = std::accumulate(shape.begin(), shape.end(), 1,
							     std::multiplies<size_t>());

			state_size += tensor_size;
		}

		if (!state_size)
			return -EINVAL;

		/*
		 * query the list of available devices and use NPU if available, otherwise use
		 * CPU by default
		 */
		std::vector<std::string> available_devices = core.get_available_devices();
		for (auto &s: available_devices) {
			if (!s.compare("NPU")) {
				device.assign("NPU");
				break;
			}
		}

		/* save the infer_request objects for each channel separately */
		ov::CompiledModel compiled_model = core.compile_model(nd->model, device, {});
		for (i = 0; i < NS_MAX_SOURCE_CHANNELS; i++)
			nd->infer_request[i] = compiled_model.create_infer_request();

		nd->inp_shape = nd->model->input("input").get_shape();

		return 0;
	}

	void ov_ns_free(ns_handle handle) {
		struct ns_data *nd = (struct ns_data *)handle;

		delete nd;
	}

	int ov_ns_process(ns_handle handle,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
	{
		struct audio_stream *source = (struct audio_stream *)input_buffers[0].data;
		struct audio_stream *sink = (struct audio_stream *)output_buffers[0].data;
		struct ns_data *nd = (struct ns_data *)handle;
		std::vector<float> inp_wave_fp32, out_wave_fp32;
		/* only 16-bit supported for now */
		int16_t *input_data = (int16_t *)audio_stream_get_rptr(source);
		int16_t *output_data = (int16_t *)audio_stream_get_wptr(sink);
		uint32_t frame_count = input_buffers[0].size;
		float scale = 1.0f / std::numeric_limits<int16_t>::max();
		int i, j, ch;

		/*
		 * The noise suppression model only supports mono, so process each channel
		 * separately
		 */
		inp_wave_fp32.resize(frame_count, 0);
		out_wave_fp32.resize(frame_count, 0);

		for (ch = 0; ch < NS_MAX_SOURCE_CHANNELS; ch++) {
			/* split each channel samples and convert to floating point */
			for (i = ch, j = 0; j < frame_count; i+=2,j++) {
				void *inp = &input_data[i];

				/* wrap if needed */
				if (inp >= source->end_addr)
			                inp = (char *)source->addr +
			                        ((char *)inp - (char *)source->end_addr);

				inp_wave_fp32[j] = (float)(*(int16_t *)inp) * scale;
			}

			ov::Tensor input_tensor(ov::element::f32, nd->inp_shape,
						&inp_wave_fp32[0]);
			nd->infer_request[ch].set_tensor("input", input_tensor);

			for (auto& state_name: nd->state_names) {
				const std::string& inp_state_name = state_name.first;
				const std::string& out_state_name = state_name.second;
				ov::Tensor state_tensor;
				ov::Shape state_shape;

				state_shape = nd->model->input(inp_state_name).get_shape();
				if (nd->iter > 0) {
					/*
					 * set input state by corresponding output state from prev
					 * infer
					 */
					state_tensor =
						nd->infer_request[ch].get_tensor(out_state_name);
				} else {
					state_tensor = ov::Tensor(ov::element::f32, state_shape);
					std::memset(state_tensor.data<float>(), 0,
						    state_tensor.get_byte_size());
				}
				nd->infer_request[ch].set_tensor(inp_state_name, state_tensor);
			}
			nd->infer_request[ch].infer();

			/* process output */
			float* src = nd->infer_request[ch].get_tensor("output").data<float>();
			float* dst = &out_wave_fp32[0];
			std::memcpy(dst, src, frame_count * sizeof(float));

			/* convert back to int and write back to output buffer */
			for (i = 0, j = ch; i < frame_count; i++,j+=2) {
				float v = out_wave_fp32[i];
				void *out = &output_data[j];

				/* wrap if needed */
				if (out >= sink->end_addr)
			                out = (char *)sink->addr +
			                        ((char *)out - (char *)sink->end_addr);

				v = std::clamp(v, -1.0f, +1.0f);
				*(int16_t *)out = (int16_t)(v * std::numeric_limits<int16_t>::max());
			}
		}

		nd->iter++;

		/* return frame_count so the input and output buffers can be updated accordingly */
		return frame_count;
	}
} /* extern "C" */

