// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@linux.intel.com>

#include <adsp_stddef.h>
#include <module_initial_settings_concrete.h>
#include <logger.h>

extern "C" {
int memcpy_s(void * dst, size_t maxlen, const void * src, size_t len);
} /* extern "C" */

using namespace intel_adsp;

namespace dsp_fw
{

#pragma pack(4) /* this directive is useless when compiling for xtensa but it highlights */
		/* the packing requirement. */
struct CompoundCfg
{
	BaseModuleCfg cfg;
	BaseModuleCfgExt cfg_ext;
};
#pragma pack()


/*! \brief Initializes a new ModuleInitialSettingsConcrete instance given an */
/*   INIT_INSTANCE IPC message blob */
ModuleInitialSettingsConcrete::ModuleInitialSettingsConcrete(DwordArray const &cfg_ipc_msg) :
							     cfg_(NULL), cfg_ext_(NULL)
{
	const size_t ipc_msg_size =
		reinterpret_cast<DwordArray const&>(cfg_ipc_msg).size() * sizeof(uint32_t);

	if (ipc_msg_size < sizeof(BaseModuleCfg)) {
		/* unexpected INIT_INSTANCE message size. Simply return as message
		 * is unparsable.
		 */
		return;
	}

	if (ipc_msg_size > sizeof(CompoundCfg) -
		sizeof(InputPinFormat) - sizeof(OutputPinFormat)) {

		/* INIT_INSTANCE message seems to be compound message        */
		/* It shall contain BaseModuleCfg + BaseModuleCfgExt +       */
		/* optionally some InputPinFormat[] + OutputPinFormat[] data */
		CompoundCfg const * unvalidated_compound_cfg = cfg_ipc_msg.dataAs<CompoundCfg>();
		if (!unvalidated_compound_cfg)
			return;

		const size_t computed_msg_size =
			sizeof(CompoundCfg) -
			/* CompoundCfg already contains one InputPinFormat and
			 * one InputPinFormat
			 */
			(sizeof(InputPinFormat) + sizeof(OutputPinFormat)) +
			unvalidated_compound_cfg->cfg_ext.nb_input_pins*sizeof(InputPinFormat) +
			unvalidated_compound_cfg->cfg_ext.nb_output_pins*sizeof(InputPinFormat);

		/* check size consistency */
		if (ipc_msg_size != computed_msg_size) {
			/* unexpected INIT_INSTANCE message size. Simply return as message
			 * is unparsable.
			 */
			return;
		}

		/* looks like a valid compound config message has been found */
		cfg_ = &unvalidated_compound_cfg->cfg;
		cfg_ext_ = &unvalidated_compound_cfg->cfg_ext;
	}
	else if (ipc_msg_size == sizeof(BaseModuleCfg)) {
		/* INIT_INSTANCE message seems to be the legacy one */
		cfg_ = cfg_ipc_msg.dataAs<BaseModuleCfg>();
	}
}

void ModuleInitialSettingsConcrete::DeduceBaseModuleCfgExt(size_t in_pins_count,
							   size_t out_pins_count)
{
	if (!cfg_ext_) {
		/* BaseModuleCfgExt data was not part of the INIT_INSTANCE IPC message */
		/* We need to create it on-the-fly */
		tmp_cfg_ext_.tlv.nb_input_pins = in_pins_count;
		tmp_cfg_ext_.tlv.nb_output_pins = out_pins_count;

		InputPinFormat* input_pins = tmp_cfg_ext_.tlv.input_pins;
		/* InputPinFormat data are all identically initialized based on audio format
		 * available in the BaseModuleCfg data.
		 */
		for (size_t i = 0 ; i < in_pins_count ; i++) {
			/*  Initialize all input pins with same audio format */
			input_pins[i].ibs = cfg_->ibs;
			input_pins[i].pin_index = i;
			memcpy_s(&input_pins[i].audio_fmt,
				 sizeof(input_pins[i].audio_fmt),
				 &cfg_->audio_fmt,
				 sizeof(AudioFormat));
		}

		OutputPinFormat* output_pins =
				reinterpret_cast<OutputPinFormat*>(&input_pins[in_pins_count]);
		for (size_t i = 0 ; i < out_pins_count ; i++) {
			/*  Initialize all pins obs with same obs value */
			output_pins[i].obs = cfg_->obs;
			output_pins[i].pin_index = i;
			memcpy_s(&output_pins[i].audio_fmt,
				 sizeof(output_pins[i].audio_fmt),
				 &cfg_->audio_fmt,
				 sizeof(AudioFormat));
		}

		/* a valid BaseModuleCfgExt has been created on-the-fly */
		cfg_ext_ = reinterpret_cast<BaseModuleCfgExt const*>(&tmp_cfg_ext_);
	}
}

void const* ModuleInitialSettingsConcrete::GetUntypedItem(ModuleInitialSettingsKey key,
							  size_t &length)
{
	void const* array = NULL;
	length = 0;

	switch(key)
	{
	case intel_adsp::LEGACY_STRUCT:
		length = 1;
		array = cfg_;
		break;

	case intel_adsp::IN_PINS_FORMAT:
		length = cfg_ext_->nb_input_pins;
		array = &cfg_ext_->input_pins[0];
		break;

	case intel_adsp::OUT_PINS_FORMAT:
		length = cfg_ext_->nb_output_pins;
		/* output_pins array follows the input_pins array in BaseModuleCfgExt
		 * struct layout
		 */
		array = reinterpret_cast<OutputPinFormat const*>
					(&cfg_ext_->input_pins[cfg_ext_->nb_input_pins]);
		break;

	default:
		break;
	}
	return array;
}

} /* namespace dsp_fw */
