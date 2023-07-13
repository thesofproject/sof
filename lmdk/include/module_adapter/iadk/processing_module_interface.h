/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */
/*! \file processing_module_interface.h */

#ifndef _ADSP_PROCESSING_MODULE_INTERFACE_H_
#define _ADSP_PROCESSING_MODULE_INTERFACE_H_

#include "system_error.h"

#include <stdint.h>
#include <stddef.h>

namespace intel_adsp
{
	/*! \brief Scoped enumeration which defines processing mode values
	 * \see ProcessingModuleInterface::SetProcessingMode()
	 */
	struct ProcessingMode {
		/*! \brief enumeration values of processing mode */
		enum Enum {
			NORMAL = 0,/*!< Indicates that module is expected to apply its custom
				    * processing on signal.
				    */
			BYPASS	/*!< Indicates that module is expected to not apply its custom
				 * processing on signal. The module is expected to forward as far
				 * as possible the input signals unmodified
				 * with respect of the signal continuity at the mode transition.
				 */
		};

		/*! \brief Underlying type for storing of ProcessingMode value */
		typedef int Type;

		/*! \brief Initializes a new instance of ProcessingMode with the NORMAL
		 * as default value
		 */
		ProcessingMode(void)
			:   value_(NORMAL)
		{}

		/*! \brief Implicitly initializes a new instance of ProcessingMode given
		 * an Enum value
		 */
		ProcessingMode(Enum mode_value)
			:   value_(mode_value)
		{}

		/*! \brief Implicitly converts a ProcessingMode object into an Enum value */
		operator Enum(void)
		{
			return (Enum) value_;
		}

	private:
		Type value_;
	};

	/*! \brief defines the bitfield structure of flags associated to an InputStreamBuffer
	 */
	struct InputStreamFlags {
		/*!< indicates that End Of Stream condition has occurred on the input stream */
		bool end_of_stream : 1;
	};

	/*! \brief Descriptor of the data stream buffer extracted from a input module pin
	 * \see ProcessingModuleInterface::Process()
	 */
	struct InputStreamBuffer {
		InputStreamBuffer() : data(), size(), flags() {}
		InputStreamBuffer(uint8_t *_data, size_t _size,
		InputStreamFlags _flags) : data(_data), size(_size), flags(_flags) {}
		uint8_t *const data; /*!< data stream buffer */
		/*!
		 * \brief size indicator about the data in the stream buffer
		 *
		 * - When read, it indicates the size of available data in the data stream buffer
		 * - When written, it reports the size of data which has actually be considered
		 *   during the buffer processing
		 *   (can be less than the given available data size)
		 */
		size_t size;
		const InputStreamFlags flags; /*!< readonly status flags about the input stream */
	};

	/*! \brief Descriptor of the data stream buffer to inject into an output module pin
	 * \see ProcessingModuleInterface::Process()
	 */
	struct OutputStreamBuffer {
		OutputStreamBuffer() : data(), size() {}
		OutputStreamBuffer(uint8_t *_data, size_t _size) : data(_data), size(_size) {}
		uint8_t *const data; /*!< data stream buffer */
		/*!
		 * \brief size indicator about the data in the stream buffer
		 *
		 * - When read, it indicates the size of available room in the stream buffer
		 * - When written, it reports the size of data which has actually be produced
		 *   into the buffer during the buffer processing
		 *   (can be less than the given available room size)
		 */
		size_t size;
	};

	/*! \brief Scoped enumeration which defines location of a configuration message fragment
	 *  in the whole message
	 *
	 * \see
	 * - ProcessingModuleInterface::SetConfiguration()
	 * - ProcessingModuleInterface::GetConfiguration()
	 */
	struct ConfigurationFragmentPosition {
		/*! \brief enumeration values of fragment position tag */
		enum Enum {
			/*!< Indicates that the associacted fragment is in the middle of message
			 *  transmission (nor first neither last one)
			 */
			MIDDLE = 0,
			/*!< Indicates that the associacted fragment is the first one of
			 * a multi-fragment message transmission
			 */
			FIRST = 1,
			/*!< Indicates that the associacted fragment is the last one of
			 * a multi-fragment message transmission
			 */
			LAST = 2,
			/*!< Indicates that the associacted fragment is the single one of
			 * the message transmission
			 */
			SINGLE = 3
		};

		/*! \brief Underlying type for storing of ConfigurationFragmentPosition value */
		typedef int Type;

		/*! \brief Implicitly initializes a new instance of ConfigurationFragmentPosition
		 * given an Enum value
		 */
		ConfigurationFragmentPosition(Enum mode_value)
			:   value_(mode_value)
		{}

		/*! \brief Implicitly converts a ConfigurationFragmentPosition object
		 * into an Enum value
		 */
		operator Enum(void)
		{
			return (Enum) value_;
		}

	private:
		Type value_;
	};

	/*!
	 * \brief The ProcessingModuleInterface class defines the interface that user-defined module
	 * shall comply with to be manageable by the ADSP System.
	 *
	 * It is also configurable through the couple of method
	 * SetConfiguration() / GetConfiguration().
	 * A ProcessingModuleInterface object consumes data stream from its input pins and produces
	 * data stream into its output pins.
	 */
	class ProcessingModuleInterface
	{
	public:
		/*!
		 * \brief Scoped enumeration of error code value which can be reported by
		 * a ProcessingModuleInterface object
		 */
		struct ErrorCode : intel_adsp::ErrorCode {
			/*! \brief list of named error codes specific to the
			 * ProcessingModuleInterface
			 */
			enum Enum {
				/*!< Reports that the message content given for configuration
				 * is invalid
				 */
				INVALID_CONFIGURATION = intel_adsp::ErrorCode::MaxValue + 1,
				/*!< Reports that the module does not support retrieval of its
				 * current configuration information
				 */
				NO_CONFIGURATION
			};
			/*! \brief Indicates the minimal value of the enumeration */
			static Enum const MinValue = INVALID_CONFIGURATION;
			/*! \brief Indicates the maximal value of the enumeration */
			static Enum const MaxValue = NO_CONFIGURATION;

			/*!
			 * \brief Initializes a new instance of ErrorCode given a value
			 */
			explicit ErrorCode(Type value)
				:   intel_adsp::ErrorCode(value)
			{}
		};
		/*!
		 * \brief Additional method called after module initialization
		 */
		virtual ErrorCode::Type Init(void) = 0;
		/*!
		 * \brief Destructor that may contain logic executed on module destruction
		 */
		virtual ErrorCode::Type Delete(void) = 0;

		/*!
		 * \brief Processes the stream buffers extracted from the input pins and produces
		 * the resulting signal in stream buffer of the output pins.
		 *
		 * The user-defined implementation of Process() is generally expected to consume
		 * all the samples available in the input stream buffers and should produce the
		 * samples for all free room available in the output stream buffers. Note that
		 * in normal condition all connected input pins will receive "ibs"
		 * (i.e. "Input Buffer Size") data bytes in their input stream buffers and output
		 * pins should produce "obs" (i.e. "Output Buffer Size") data bytes in their
		 * output stream buffers. ("ibs" and "obs" values are given to module at
		 * construction time within the \ref ModuleInitialSettings parameter).
		 * However in "end of stream" condition input stream buffers may be filled with
		 * less data count than "ibs".
		 * Therefore less data count than "obs" can be put in the output buffers.
		 *
		 * \remarks Length of input_stream_buffers and output_stream_buffers C-arrays
		 * don't need to be part of the Process() prototype as those lengths are
		 * well-known by the user-defined implementation of the ProcessingModuleInterface.
		 * \return Custom implementation can return a user-defined error code value.
		 * This user-defined error code will be transmitted to
		 * host driver if the value is different from 0 (0 is considered as
		 * a "no-error value")
		 */
		virtual uint32_t Process(
			InputStreamBuffer * input_stream_buffers,
			/*!< [in,out] C-array of input buffers to process. "data" field value can
			 * be NULL if the associated pin is not connected
			 */
			OutputStreamBuffer * output_stream_buffers
			/*!< [in,out] C-array of output buffers to produce. "data" field value
			 * can be NULL if the associated pin is not connected
			 * \note "size" field value is set with the total room available in
			 * the output buffers at Process() method call.
			 * It shall be updated within the method to report to the ADSP System
			 * the actual data size put in the output buffers.
			 */
		) = 0;
		/*!
		 * \brief Upon call to this method the ADSP system requires the module to reset its
		 * internal state into a well-known initial value.
		 *
		 * Parameters which may have been set through SetConfiguration() are supposed to be
		 * left unchanged.
		 * \remarks E.g. a configurable FIR filter module will reset its internal samples
		 * history buffer but not the taps values
		 * (which may have been configured through SetConfiguration())
		 */
		virtual void Reset(void) = 0;

		/*!
		 * \brief Sets the processing mode for the module.
		 *
		 * Upon the transition from one processing mode to another, the module is required
		 * to handle enabling/disabling of its custom processing
		 * as smoothly as possible (no glitch, no signal discontinuity).
		 *
		 * \note This method is actually only relevant for modules which only manipulate
		 * PCM signal streams.
		 * Thus, the ADSP System will only fire the SetProcessingMode() method for
		 * those kind of modules. (e.g. not for signal decoders, encoders etc.)
		 * Moreover, disabling the processing of modules which convert the trait of
		 * the signal samples (bit depth, sampling rate, etc.)
		 * would make the resulting stream(s) unsuitable for the downstream modules.
		 * Therefore ADSP System will not fire this method for such modules too.
		 */
		virtual void SetProcessingMode(ProcessingMode mode) = 0;
		/*!
		 * \brief Gets the processing mode for the module.
		 */
		virtual ProcessingMode GetProcessingMode(void) = 0;
		/*!
		 * \brief Applies the upcoming configuration message for the given
		 * configuration ID.
		 *
		 * If the complete configuration message is greater than 4096 bytes,
		 * the transmission will be split into several fragments (lesser or equal
		 * to 4096 bytes). In this case the ADSP System will perform multiple calls
		 * to SetConfiguration() until completion of the configuration message sending.
		 * \note config_id indicates ID of the configuration message only on the first
		 * fragment sending otherwise it is set to 0.
		 */
		virtual ErrorCode::Type SetConfiguration(
				uint32_t config_id,
				/*!< [in] indicates ID of the configuration message that
				 * is provided
				 */
				ConfigurationFragmentPosition fragment_position,
				/*!< [in] indicates position of the fragment in the
				 * whole message transmission
				 */
				uint32_t data_offset_size,
				/*!< [in] Meaning of parameter depends on the fragment_position
				 * value:
				 * - if fragment_position is worth
				 *   ConfigurationFragmentPosition::FIRST or
				 *   ConfigurationFragmentPosition::SINGLE
				 *   data_offset_size indicates the data size of the full message
				 * - if fragment_position is worth
				 *   ConfigurationFragmentPosition::MIDDLE or
				 *   ConfigurationFragmentPosition::LAST
				 *   data_offset_size indicates the position offset of the received
				 *   fragment in the full message.
				 */
				const uint8_t *fragment_buffer,
				/*!< [in] the configuration fragment buffer */
				size_t fragment_size,
				 /*!< [in] the fragment buffer size.
				  * As per ADSP System design the fragment_size value will not
				  * exceed 4096 bytes.
				  */
				uint8_t *response,
				/*!< [out] the response message buffer to optionally fill */
				size_t & response_size
				/** [in,out] the response message size.
				 * As per ADSP System design the response_size value shall not
				 * exceed 2048 bytes.
				 * Implementation of SetConfiguration shall set response_size
				 * value to the actual size (in bytes) of the response message
				 */

			) = 0;
		/*!
		 * \brief Retrieves the configuration message for the given configuration ID.
		 *
		 * If the complete configuration message is greater than 4096 bytes,
		 * the transmission will be split into several fragments (lesser or equal
		 * to 4096 bytes). In this case the ADSP System will perform multiple call to
		 * GetConfiguration() until completion of the configuration message retrieval.
		 * \note config_id indicates ID of the configuration message only on first
		 * fragment retrieval otherwise it is set to 0.
		 */
		virtual ErrorCode::Type GetConfiguration(
				uint32_t config_id,
				/*!< [in] indicates ID of the configuration message that is
				 * requested to be returned
				 */
				ConfigurationFragmentPosition fragment_position,
				/*!< [in] indicates position of the fragment in the whole
				 * message transmission
				 */
				uint32_t & data_offset_size,
				/*!< [in,out] Meaning of parameter depends on the
				 * fragment_position value.
				 * - if fragment_position is worth
				 * ConfigurationFragmentPosition::FIRST or
				 * ConfigurationFragmentPosition::SINGLE
				 *   data_offset_size shall report the data size of the
				 *   full message
				 * - if fragment_position is worth
				 * ConfigurationFragmentPosition::MIDDLE or
				 * ConfigurationFragmentPosition::LAST
				 *   data_offset_size indicates the position offset of the
				 *   received fragment in the full message
				 */
				uint8_t *fragment_buffer,
				/*!< [out] the fragment buffer to fill */
				size_t & fragment_size
				/*!< [in,out] the fragment buffer size.
				 * The actual size of data written into the fragment buffer
				 * shall be reported to the ADSP System.
				 */
			) = 0;
	};
	/*! \class ProcessingModuleInterface
	 * See also
	 * --------
	 * - the ProcessingModuleFactoryInterface interface which defines the factory for custom
	 *   processing module (in processing_module_factory_interface.h)
	 * - the template class ProcessingModule which provides a partial default implementation
	 *   for ProcessingModuleInterface suitable for most of custom processing modules
	 *   (in processing_module.h).
	 */

	class DetectorModuleInterface : public ProcessingModuleInterface
	{
	public:

		/*! Object processing state */
		typedef enum {
			/*! Data are processed */
			PROCESSING = 0,
			/*! No data are processed */
			IDLE = 1,
		} State;

		/*! Get processing state of module. */
		virtual State GetState(void) = 0;

		/*! Get idle period that the module processing are not required */
		virtual uint64_t GetIdlePeriod(void) = 0;

		/*! Method for handle stream processing */
		virtual void OnStreamState(uint64_t counter,
					uint32_t stream_index,
					State state) = 0;
	};
} /*namespace intel_adsp */

#endif /* #ifndef _ADSP_PROCESSING_MODULE_INTERFACE_H_ */
