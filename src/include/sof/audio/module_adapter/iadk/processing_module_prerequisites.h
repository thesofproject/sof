/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */
/*! \file processing_module_prerequisites.h */

#ifndef _PROCESSING_MODULE_PREREQUISITES_H_
#define _PROCESSING_MODULE_PREREQUISITES_H_

#include <stdint.h>
#include <stddef.h>

namespace intel_adsp
{
	/*!
	 * \brief "Scoped enumeration" of values which specify data buffer alignment of input or
	 * output stream buffer.
	 */
	struct StreamBufferAlignment {
		/*! \brief the enumeration type of StreamBufferAlignment "scoped enumeration" */
		enum WordSize {
			/*! \brief enumeration tag for 4-bytes aligned buffer */
			_4_BYTES = 4,
			/*! \brief enumeration tag for 8-bytes aligned buffer */
			_8_BYTES = 8
		};

		/*! \brief Type of the inner integral value */
		typedef uint8_t Type;

		/*! \brief Initializes a new instance of StreamBufferAlignment with value set
		 * to _4_BYTES
		 */
		StreamBufferAlignment(void)
			:   value(_4_BYTES)
		{}

		/*! \brief Initializes a new instance of StreamBufferAlignment given
		 * a StreamBufferAlignment::WordSize value
		 */
		StreamBufferAlignment(WordSize val)
			:   value(val)
		{}

		/*! \brief Initializes a new instance of StreamBufferAlignment given
		 * a StreamBufferAlignment::Type value
		 */
		explicit StreamBufferAlignment(Type val)
			:   value(val)
		{}

		/*! \brief Copy constructor */
		StreamBufferAlignment(const StreamBufferAlignment &ref)
			:   value(ref.value)
		{}

		/*! \brief Implicit cast operator to IoChunkAligment::WordSize */
		operator WordSize(void)
		{
			return (WordSize) value;
		}

		/*! \brief Implicit cast operator to IoChunkAligment::Type */
		operator Type(void)
		{
			return value;
		}

		/*! \brief default assignment operator */
		StreamBufferAlignment &operator = (const StreamBufferAlignment &src)
		{
			value = src.value;
			return *this;
		}

		/*! \brief assignment operator given a StreamBufferAlignment::WordSize value */
		StreamBufferAlignment &operator = (const StreamBufferAlignment::WordSize &src)
		{
			value = src;
			return *this;
		}

		/*! \brief implement comparison operator */
		bool operator>(const StreamBufferAlignment &r) const
		{ return value > r.value; }

		/*! \brief implement comparison operator */
		bool operator<(const StreamBufferAlignment &r) const
		{ return value < r.value; }

		/*! \brief implement comparison operator */
		bool operator == (const StreamBufferAlignment &r) const
		{ return value == r.value; }

		/*! \brief implement comparison operator */
		bool operator != (const StreamBufferAlignment &r) const
		{ return value != r.value; }

	private:
		/*! \brief inner integral value for the StreamBufferAlignment */
		Type value;
	};

	/*!
	 * \brief Descriptor on prerequisites for ProcessingModuleInterface instance creation.
	 */
	struct ProcessingModulePrerequisites {
	public:
		/*! \brief Initializes a new instance of ProcessingModulePrerequisites
		 * with default values
		 */
		ProcessingModulePrerequisites()
			:   stream_buffer_alignment(StreamBufferAlignment::_4_BYTES),
			    input_pins_count(0), output_pins_count(0), event_count(0)
		{}

		/*! \brief holds the buffer alignment constraint in size of bytes for input
		 *  or output chunk buffer
		 *
		 * Default constructor set this field value to StreamBufferAlignment::_4_BYTES.
		 */
		StreamBufferAlignment stream_buffer_alignment;

		/*! \brief Indicates the count of input pins for the module type about
		 *  to be created.
		 */
		size_t input_pins_count;
		/*! \brief Indicates the count of output pins for the module type about
		 * to be created.
		 */
		size_t output_pins_count;
		/*! \brief Indicates the count of events for the module type about
		 *  to be created.
		 */
		size_t event_count;
	};
}

#endif /* _PROCESSING_MODULE_PREREQUISITES_H_ */
