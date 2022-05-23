/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *
 */

#ifndef _IADK_MODULE_ADAPTER_H
#define _IADK_MODULE_ADAPTER_H

#ifdef __cplusplus

#include <processing_module_interface.h>
#include <module_initial_settings.h>
#include <adsp_stddef.h>
#include <system_error.h>

#include <sof/audio/module_adapter/module/module_interface.h>

extern "C" {
namespace dsp_fw
{
	/*!
	 * \brief This ModuleAdapter class can adapt a ProcessingModuleInterface instance into
	 *        a ModuleInstance instance.
	 *
	 * The overall base FW can only handle ModuleInstance object. Purpose of this adapter is
	 * to turn an intel_adsp::ProcessingModuleInterface object into a ModuleInstance object.
	 */
	class IadkModuleAdapter
	{
	public:
		IadkModuleAdapter(intel_adsp::ProcessingModuleInterface &processing_module,
		                  void *comp_dev_instance,
		                  uint32_t module_id,
		                  uint32_t instance_id,
		                  uint32_t core_id,
		                  size_t module_size);

		/**
		 * Module specific initialization procedure, called as part of
		 *
		 */
		int IadkModuleAdapter_Init(void);

		/**
		 * Module specific prepare procedure, called as part of codec_adapter
		 * component preparation in .prepare()
		 */
		int IadkModuleAdapter_Prepare(void);

		/**
		 * Module specific processing procedure, called as part of codec_adapter
		 * component copy in .copy(). This procedure is responsible to consume
		 * samples provided by the codec_adapter and produce/output the processed
		 * ones back to codec_adapter.
		 */
		uint32_t IadkModuleAdapter_Process(struct input_stream_buffer *input_buffers,
						   int num_input_buffers,
						   struct output_stream_buffer *output_buffers,
						   int num_output_buffers);

		/**
		 * Module specific apply config procedure, called by codec_adapter every time
		 * a new RUNTIME configuration has been sent if the adapter has been
		 * prepared. This will not be called for SETUP cfg.
		 */
		AdspErrorCode
		IadkModuleAdapter_SetConfiguration(uint32_t config_id,
		                               enum module_cfg_fragment_position fragment_position,
		                               uint32_t data_offset_size,
		                               const uint8_t *fragment_buffer,
		                               size_t fragment_size,
		                               uint8_t *response,
		                               size_t &response_size) /*override*/;

		/**
		 * Retrieves the configuration message for the given configuration ID.
		 */
		AdspErrorCode
		IadkModuleAdapter_GetConfiguration(uint32_t config_id,
		                               enum module_cfg_fragment_position fragment_position,
		                               uint32_t data_offset_size,
		                               uint8_t *fragment_buffer,
		                               size_t fragment_size);
		/**
		 * Module specific reset procedure, called as part of codec_adapter component
		 * reset in .reset(). This should reset all parameters to their initial stage
		 * but leave allocated memory intact.
		 */
		void IadkModuleAdapter_Reset(void);
		/**
		 * Module specific free procedure, called as part of codec_adapter component
		 * free in .free(). This should free all memory allocated by module.
		 */
		int IadkModuleAdapter_Free(void);
		void IadkModuleAdapter_SetProcessingMode(enum module_processing_mode sof_mode);

		enum module_processing_mode IadkModuleAdapter_GetProcessingMode(void);

	private:

		intel_adsp::ProcessingModuleInterface &processing_module_;
	};

} /* namespace dsp_fw */

} /* extern "C" */

void *operator new(size_t size, intel_adsp::InputStreamBuffer *placeholder) throw();
void *operator new(size_t size, intel_adsp::OutputStreamBuffer *placeholder) throw();

#else /* __cplusplus */

/* C wrappers for C++  ProcessingModuleInterface() methods. */
int iadk_wrapper_init(void *md);

int iadk_wrapper_prepare(void *md);

int iadk_wrapper_set_processing_mode(void *md, enum module_processing_mode mode);

enum module_processing_mode iadk_wrapper_get_processing_mode(void *md);

int iadk_wrapper_reset(void *md);

int iadk_wrapper_free(void *md);

int iadk_wrapper_set_configuration(void *md, uint32_t config_id,
				   enum module_cfg_fragment_position pos,
				   uint32_t data_offset_size,
				   const uint8_t *fragment, size_t fragment_size,
				   uint8_t *response, size_t response_size);

int iadk_wrapper_get_configuration(void *md, uint32_t config_id,
				   enum module_cfg_fragment_position pos,
				   uint32_t data_offset_size,
				   uint8_t *fragment, size_t fragment_size);

int iadk_wrapper_process(void *md, struct input_stream_buffer *input_buffers,
			 int num_input_buffers, struct output_stream_buffer *output_buffers,
			 int num_output_buffers);

#endif /* __cplusplus */

#endif /*_IADK_MODULE_ADAPTER_H */
