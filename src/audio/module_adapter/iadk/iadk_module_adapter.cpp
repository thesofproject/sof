// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@linux.intel.com>

#include <iadk_module_adapter.h>
#include <system_error.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef intel_adsp::ProcessingModuleInterface::ErrorCode::Type IntelErrorCode;

namespace dsp_fw
{

  IadkModuleAdapter::IadkModuleAdapter(intel_adsp::ProcessingModuleInterface& processing_module,
				       void *comp_dev_instance,
				       uint32_t module_id,
				       uint32_t instance_id,
				       uint32_t core_id,
				       size_t module_size)
				       :processing_module_(processing_module)
{
}

int IadkModuleAdapter::IadkModuleAdapter_Init(void)
{
	return processing_module_.Init();
}

int IadkModuleAdapter::IadkModuleAdapter_Prepare(void)
{
	return 0;
}

uint32_t IadkModuleAdapter::IadkModuleAdapter_Process(struct input_stream_buffer *input_buffers,
						      int num_input_buffers,
						      struct output_stream_buffer *output_buffers,
						      int num_output_buffers)
{
	uint32_t ret = 0;
	if ((num_input_buffers > 0) && (num_output_buffers > 0)) {
		intel_adsp::InputStreamBuffer input_stream_buffers[INPUT_PIN_COUNT];
		intel_adsp::OutputStreamBuffer output_stream_buffers[OUTPUT_PIN_COUNT];
		for (int i = 0; i < (int)num_input_buffers; i++) {
			intel_adsp::InputStreamFlags flags = {};
			flags.end_of_stream = input_buffers[i].end_of_stream;
			const intel_adsp::InputStreamBuffer isb_data(
				(uint8_t *)input_buffers[i].data,
				input_buffers[i].size,
				flags);
			new (&input_stream_buffers[i]) intel_adsp::InputStreamBuffer(isb_data);
		}

		for (int i = 0; i < (int)num_output_buffers; i++) {
			const intel_adsp::OutputStreamBuffer osb_data(
					(uint8_t *)output_buffers[i].data,
					output_buffers[i].size);
			new (&output_stream_buffers[i]) intel_adsp::OutputStreamBuffer(osb_data);
		}

		ret = processing_module_.Process(input_stream_buffers, output_stream_buffers);

		for (int i = 0; i < (int)num_input_buffers; i++) {
			input_buffers[i].consumed = input_buffers[i].size;
		}

		for (int i = 0; i < (int)num_output_buffers; i++) {
			output_buffers[i].size = output_stream_buffers[i].size;
		}
	}
	return ret;
}

AdspErrorCode
IadkModuleAdapter::IadkModuleAdapter_SetConfiguration(uint32_t config_id,
						      enum module_cfg_fragment_position pos,
						      uint32_t data_offset_size,
						      const uint8_t *fragment_buffer,
						      size_t fragment_size,
						      uint8_t *response,
						      size_t &response_size)
{
	intel_adsp::ConfigurationFragmentPosition fragment_position =
			(intel_adsp::ConfigurationFragmentPosition::Enum) pos;

	return processing_module_.SetConfiguration(config_id, fragment_position,
						   data_offset_size, fragment_buffer,
						   fragment_size, response, response_size);
}

AdspErrorCode
IadkModuleAdapter::IadkModuleAdapter_GetConfiguration(uint32_t config_id,
						      enum module_cfg_fragment_position pos,
						      uint32_t data_offset_size,
						      uint8_t *fragment_buffer,
						      size_t fragment_size)
{
	intel_adsp::ConfigurationFragmentPosition fragment_position =
			(intel_adsp::ConfigurationFragmentPosition::Enum) pos;

	return processing_module_.GetConfiguration(config_id, fragment_position,
						   data_offset_size, fragment_buffer,
						   fragment_size);
}


void IadkModuleAdapter::IadkModuleAdapter_SetProcessingMode(enum module_processing_mode sof_mode)
{
	intel_adsp::ProcessingMode mode;
	sof_mode == MODULE_PROCESSING_NORMAL ?
		(mode = intel_adsp::ProcessingMode::NORMAL) :
		(mode = intel_adsp::ProcessingMode::BYPASS);
	processing_module_.SetProcessingMode(mode);
}


void IadkModuleAdapter::IadkModuleAdapter_Reset(void)
{
	processing_module_.Reset();
}

enum module_processing_mode IadkModuleAdapter::IadkModuleAdapter_GetProcessingMode(void)
{
	enum module_processing_mode sof_mode;
	intel_adsp::ProcessingMode mode = processing_module_.GetProcessingMode();
	mode == intel_adsp::ProcessingMode::NORMAL ?
		(sof_mode = MODULE_PROCESSING_NORMAL) : (sof_mode = MODULE_PROCESSING_BYPASS);
	return sof_mode;
}

/* C wrappers for C++  ProcessingModuleInterface() methods. */
int IadkModuleAdapter::IadkModuleAdapter_Free(void)
{
	return processing_module_.Delete();
}


int iadk_wrapper_init(void *md)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_Init();
}

int iadk_wrapper_prepare(void *md)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_Prepare();
}

int iadk_wrapper_set_processing_mode(void *md,
				     enum module_processing_mode mode)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	mod_adp->IadkModuleAdapter_SetProcessingMode(mode);
	return 0;
}

enum module_processing_mode iadk_wrapper_get_processing_mode(void *md)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_GetProcessingMode();
}

int iadk_wrapper_reset(void *md)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	mod_adp->IadkModuleAdapter_Reset();
	return 0;
}

int iadk_wrapper_free(void *md)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_Free();
}

int iadk_wrapper_set_configuration(void *md, uint32_t config_id,
				   enum module_cfg_fragment_position pos,
				   uint32_t data_offset_size,
				   const uint8_t *fragment, size_t fragment_size,
				   uint8_t *response, size_t response_size)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_SetConfiguration(config_id, pos,
							   data_offset_size,
							   fragment, fragment_size,
							   response, response_size);
}

int iadk_wrapper_get_configuration(void *md, uint32_t config_id,
				   enum module_cfg_fragment_position pos,
				   uint32_t data_offset_size,
				   uint8_t *fragment, size_t fragment_size)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_GetConfiguration(config_id, pos,
							   data_offset_size,
							   fragment,
							   fragment_size);
}


int iadk_wrapper_process(void *md, struct input_stream_buffer *input_buffers,
			 int num_input_buffers, struct output_stream_buffer *output_buffers,
			 int num_output_buffers)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_Process(input_buffers, num_input_buffers,
						  output_buffers, num_output_buffers);
}
} /* namespace dsp_fw */

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
void* operator new(size_t size, intel_adsp::InputStreamBuffer* placeholder) throw()
{
	return placeholder;
}

void* operator new(size_t size, intel_adsp::OutputStreamBuffer* placeholder) throw()
{
	return placeholder;
}
#endif

