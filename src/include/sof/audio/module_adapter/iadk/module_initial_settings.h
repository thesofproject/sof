/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */
/*! \file module_initial_settings.h */

#ifndef _ADSP_MODULE_INITIAL_SETTINGS_H_
#define _ADSP_MODULE_INITIAL_SETTINGS_H_

#include "adsp_stddef.h"
#include "fixed_array.h"
#include <ipc4/base-config.h>

/* Mapping of IPC4 definitions into IADK naming counterpart */
typedef struct ipc4_base_module_cfg BaseModuleCfg;
typedef struct ipc4_base_module_cfg LegacyModuleInitialSettings;
typedef struct ipc4_input_pin_format InputPinFormat;
typedef struct ipc4_output_pin_format OutputPinFormat;
typedef struct ipc4_audio_format AudioFormat;

#define INPUT_PIN_COUNT		(1 << 3)
#define OUTPUT_PIN_COUNT	(1 << 3)

namespace intel_adsp
{
	/*! \brief Enumeration values of keys to access to the ModuleInitialSettings items */
	enum ModuleInitialSettingsKey {
		/*! \brief Key value to retrieve the LegacyModuleInitialSettings item from the
		 * ModuleInitialSettings.
		 *  \deprecated New module shall not work with this item as it will be removed
		 *  in next API release.
		 */
		LEGACY_STRUCT = 0,
		/*! \brief Key value to retrieve the array of InputPinFormat item from
		 * the ModuleInitialSettings.
		 */
		IN_PINS_FORMAT,
		/*! \brief Key value to retrieve the array of OutputPinFormat item from
		 * the ModuleInitialSettings.
		 */
		OUT_PINS_FORMAT
	};

	/*!    \brief Helps to identify type of a ModuleInitialSettings item referenced by its KEY
	 *      \tparam KEY          identifying the settings item
	 */
	template < ModuleInitialSettingsKey KEY > struct ModuleInitialSettingsItem
	{
		/*! \brief value type of the SETTINGS item for the given KEY value.
		 * \note ValueType shall have copy constructor
		 */
		typedef void ValueType;
	};

	/*! \brief Defines the interface to retrieve untyped items based
	 * on ModuleInitialSettingsKey values.
	 * \internal
	 */
	struct ModuleInitialSettingsInterface {
		/*! \internal */
		virtual void const *GetUntypedItem(ModuleInitialSettingsKey key,
						   size_t & length) = 0;
	};

	namespace system
	{ class SystemAgent; }

	/*! \brief The set of settings item given for initialization of a Module instance.
	 *
	 * The ModuleInitialSettings is a container of heterogeneous typed value items. Each item
	 * is a key-value pair where key is an enumeration value of ModuleInitialSettingsKey
	 */
	class ModuleInitialSettings
	{
		template < class DERIVED, class PROCESSING_MODULE >
				friend class ProcessingModuleFactory;
				friend class system::SystemAgent;

	public:
		/*! \brief A FixedArray whose construction is only granted to
		 * ModuleInitialSettings
		 */
		template < class VALUE >
		struct Array : public FixedArray < VALUE >
		{
			friend class ModuleInitialSettings;
			typedef VALUE ValueType;

		/*! \brief Initializes a new instance of Array.
		 */
		explicit Array(ValueType *array, size_t length) :
				FixedArray < ValueType > (array, length)
		{}

		private:
			/*! \brief copy constructor is invalidated to prevent client code from
			 * working with dangling reference. Consider the Copy() operation if
			 * a copy of the settings item array is required.
			 */
			Array(Array < ValueType > const &);

			/*! \brief copy-assignment operator is invalidated to prevent client code
			 * from working with dangling reference. Consider the Copy() operation if
			 * a copy of the settings item array is required.
			 */
			Array < ValueType > &operator = (Array < ValueType > const &);
		};

		/*! \brief the type of keys to access to the ModuleInitialSettings items */
		typedef ModuleInitialSettingsKey Key;

		/*! \brief Initializes a new instance of ModuleInitialSettings given some
		 * ModuleInitialSettingsInterface object
		 */
		explicit ModuleInitialSettings(ModuleInitialSettingsInterface & performer) :
			performer_(performer)
		{}

		/*! \brief Retrieves the item as an array of values for the given key.
		 *  \note Any item is represented as a value array even if it has a single value.
		 *  \remarks If no item is found for the given key, the returned array will
		 *  have null length.
		 *  \tparam key     value of the Key to retrieve the item.
		 */
		template < Key key >
		const Array < typename ModuleInitialSettingsItem < key >
					:: ValueType const > GetItem()
		{
			size_t length;

			return Array < typename ModuleInitialSettingsItem < key >
					:: ValueType const > (
			reinterpret_cast < typename ModuleInitialSettingsItem < key >
					:: ValueType const *>
					(performer_.GetUntypedItem(key, length)), length);
		}

	private:
		/*! \brief For sake of safety ModuleInitialSettings is not "publicly" copy-able.
		 * Indeed, ModuleInitialSettings instance holds references on some ADSP System
		 * resources which are only available for a temporary lifetime.
		 */
		ModuleInitialSettings(ModuleInitialSettings const &src) :
				performer_(src.performer_) { }
		/*! ModuleInitialSettings(ModuleInitialSettings const&) */
		ModuleInitialSettings operator = (ModuleInitialSettings const &src);

		ModuleInitialSettingsInterface & performer_;
	};

	/*! \brief Boilerplate to identify type of the ModuleInitialSettings item associated
	 * to the "LEGACY_STRUCT" key
	 *  \internal
	 */
	template < > struct ModuleInitialSettingsItem < LEGACY_STRUCT >
	{
		typedef LegacyModuleInitialSettings ValueType;
	};

	/*! \brief Boilerplate to identify type of the ModuleInitialSettings item associated
	 * to the "IN_PINS_FORMAT" key
	 *  \internal
	 */
	template < > struct ModuleInitialSettingsItem < IN_PINS_FORMAT >
	{
		typedef InputPinFormat ValueType;
	};

	/*! \brief Boilerplate to identify type of the ModuleInitialSettings item associated
	 * to the "OUT_PINS_FORMAT" key
	 *  \internal
	 */
	template < > struct ModuleInitialSettingsItem < OUT_PINS_FORMAT >
	{
		typedef OutputPinFormat ValueType;
	};

} /*namespace intel_adsp */

#endif /* #ifndef _ADSP_MODULE_INITIAL_SETTINGS_H_ */
