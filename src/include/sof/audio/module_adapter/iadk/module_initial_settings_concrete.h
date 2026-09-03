/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef _MODULE_INITIAL_SETTINGS_CONCRETE_H
#define _MODULE_INITIAL_SETTINGS_CONCRETE_H

#include "module_initial_settings.h"
#include <utilities/array.h>
#include <sof/compiler_attributes.h>

#include <stddef.h>

#pragma pack(push, 4)
struct BaseModuleCfgExt {
	/*!
	 * \brief Specifies number of items in input_pins array. Maximum size is 8.
	 */
	uint16_t nb_input_pins;
	/*!
	 * \brief Specifies number of items in output_pins array. Maximum size is 8.
	 */
	uint16_t nb_output_pins;
	/*!
	 * \brief Not used, set to 0.
	 */
	uint8_t  reserved[12];
	/*!
	 * \brief Specifies format of input pins.
	 * \remarks Pin format arrays may be non-continuous i.e. may contain pin #0
	 * format followed by pin #2 format
	 * in case pin #1 will not be in use. FW assigned format of the pin based
	 * on pin_index, not on a position of the item in the array.
	 * Applies to both input and output pins.
	 */
	InputPinFormat input_pins[1];
	/*!
	 * \brief Specifies format of output pins.
	 */
	OutputPinFormat output_pins[1];
};
#pragma pack(pop)

namespace dsp_fw
{
/*! \brief concrete implementation of the intel_adsp::ModuleInitialSettingsInterface
 *
 * Allow to retrieve the settings items in the INIT_INSTANCE IPC message
 */
struct ModuleInitialSettingsConcrete : public intel_adsp::ModuleInitialSettingsInterface
{
	/*! \brief Initializes a new ModuleInitialSettingsConcrete instance given
	 *  an INIT_INSTANCE IPC message blob
	 */
	explicit ModuleInitialSettingsConcrete(DwordArray const &cfg_ipc_msg);

	/*! \brief Extrapolates some hard-coded BaseModuleCfgExt based on the legacy
	 * BaseModuleCfg and the given input count and output count.
	 *  \remarks If BaseModuleCfgExt was actually already part of
	 *  the INIT_INSTANCE IPC message, nothing is performed.
	 */
	void DeduceBaseModuleCfgExt(size_t in_pins_count, size_t out_pins_count);

	/*! \brief Gets the untyped value array of the settings item for the given key
	 *  \note In this methods it is assumed that the given INIT_INSTANCE IPC message
	 *   has a valid content
	 *  \warning IsParsable() result shall have been checked before invoking this method.
	 */
	virtual void const *GetUntypedItem(intel_adsp::ModuleInitialSettingsKey key,
					   size_t &length);

	/*! \brief Indicates if the given INIT_INSTANCE IPC message is parse-able */
	bool IsParsable(void) const
	{
		return ((cfg_ != NULL) || (cfg_ext_ != NULL));
	}

	/*! \brief Gets pointer on the BaseModuleCfg data retrieved from the IPC message */
	BaseModuleCfg const *GetBaseModuleCfg(void) const
	{
		return cfg_;
	}

	/*! \brief Gets pointer on the BaseModuleCfgExt data retrieved from the IPC message */
	BaseModuleCfgExt const *GetBaseModuleCfgExt(void) const
	{
		return cfg_ext_;
	}

private:
	BaseModuleCfg const *cfg_;
	BaseModuleCfgExt const *cfg_ext_;
	/* temporary extended module config for case where it is not part of
	 * the INIT_INSTANCE IPC message
	 */
	union {
		BaseModuleCfgExt tlv;
		/* struct below reserved the placeholder for the biggest possible BaseModuleCfgExt
		 * block.
		 */
		struct {
			uint16_t           do_not_use1;
			uint16_t           do_not_use2;
			uint8_t            do_not_use3[8];
			uint32_t           do_not_use4;
			InputPinFormat     do_not_use5[INPUT_PIN_COUNT];
			OutputPinFormat    do_not_use6[OUTPUT_PIN_COUNT];
		} placeholder;
	} tmp_cfg_ext_;
};
}

#endif /*_MODULE_INITIAL_SETTINGS_CONCRETE_H */
