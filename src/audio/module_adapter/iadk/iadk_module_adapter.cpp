// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@linux.intel.com>

#include <iadk_module_adapter.h>
#include <system_error.h>
#include <errno.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>

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

int IadkModuleAdapter::IadkModuleAdapter_Process(struct sof_source **sources,
						 int num_of_sources,
						 struct sof_sink **sinks,
						 int num_of_sinks)
{
	int ret = 0;

	if ((num_of_sources > 0) && (num_of_sinks > 0)) {
		intel_adsp::InputStreamBuffer input_stream_buffers[INPUT_PIN_COUNT];
		intel_adsp::OutputStreamBuffer output_stream_buffers[OUTPUT_PIN_COUNT];
		for (int i = 0; i < (int)num_of_sources; i++) {
			uint8_t *input, *input_start;
			size_t input_end, i_size;

			intel_adsp::InputStreamFlags flags = {};
			i_size = source_get_data_available(sources[i]);
			ret = source_get_data(sources[i], i_size, (const void **)&input,
					      (const void **)&input_start, &input_end);
			if (ret != 0)
				return ret;

			const intel_adsp::InputStreamBuffer isb_data(
				(uint8_t *)input, i_size, flags);
			new (&input_stream_buffers[i]) intel_adsp::InputStreamBuffer(isb_data);
		}

		for (int i = 0; i < num_of_sinks; i++) {
			uint8_t *output, *output_start;
			size_t output_end, o_size;

			o_size = sink_get_free_size(sinks[i]);
			ret = sink_get_buffer(sinks[i], o_size, (void **)&output,
					(void **)&output_start, &output_end);
			if (ret != 0)
				return ret;

			const intel_adsp::OutputStreamBuffer osb_data(
					(uint8_t *)output, o_size);
			new (&output_stream_buffers[i]) intel_adsp::OutputStreamBuffer(osb_data);
		}

		uint32_t iadk_ret =
			processing_module_.Process(input_stream_buffers, output_stream_buffers);

		/* IADK modules returns uint32_t return code. Convert to failure if Process
		 * not successful.
		 */
		if (iadk_ret != 0)
			ret = -ENODATA;

		for (int i = 0; i < num_of_sources; i++) {
			source_release_data(sources[i], input_stream_buffers[i].size);
		}

		for (int i = 0; i < num_of_sinks; i++) {
			sink_commit_buffer(sinks[i], output_stream_buffers[i].size);
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
						      uint32_t &data_offset_size,
						      uint8_t *fragment_buffer,
						      size_t &fragment_size)
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
				   uint32_t *data_offset_size,
				   uint8_t *fragment, size_t *fragment_size)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_GetConfiguration(config_id, pos,
							   *data_offset_size,
							   fragment,
							   *fragment_size);
}

int iadk_wrapper_process(void *md,
			 struct sof_source **sources, int num_of_sources,
			 struct sof_sink **sinks, int num_of_sinks)
{
	struct IadkModuleAdapter *mod_adp = (struct IadkModuleAdapter *) md;
	return mod_adp->IadkModuleAdapter_Process(sources, num_of_sources,
						  sinks, num_of_sinks);
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

