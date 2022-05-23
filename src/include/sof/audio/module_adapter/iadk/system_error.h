/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */
/*! \file system_error.h */

#ifndef _ADSP_SYSTEM_ERROR_H_
#define _ADSP_SYSTEM_ERROR_H_

#include "adsp_error_code.h"

namespace intel_adsp
{
	/*!
	 * \brief Scoped enumeration of common error code values which can be reported
	 *  to ADSP System
	 */
	struct ErrorCode {
		/*! \brief type of the error code value */
		typedef int Type;

		/*! \brief list of named error codes */
		enum Enum {
			NO_ERROR = ADSP_NO_ERROR,
			INVALID_PARAMETERS = ADSP_INVALID_PARAMETERS,
			BUSY = ADSP_BUSY_RESOURCE,
			FATAL_FAILURE = ADSP_FATAL_FAILURE,

		};
		/*! \brief Indicates the minimal value in the enumeration list */
		static Enum const MinValue = NO_ERROR;
		/*! \brief Indicates the maximal value in the enumeration list */
		static Enum const MaxValue = FATAL_FAILURE;

		/*! \brief Initializes a new ErrorCode instance given a value of error code */
		explicit ErrorCode(Type value)
			:   value_(value) { }

		/*! \brief Returns the current value of the ErrorCode */
		Type operator()() const
		{
			return value_;
		}

		/*! \brief Converts the ErrorCode instance into its code value */
		operator Type(void) const
		{
			return value_;
		}

		/*! \brief Evaluates the ErrorCode value against a given value */
		bool operator == (Type a)
		{
			return value_ == a;
		}

		/*!
		 * \brief Gets a const reference on the error code value
		 */
		const Type & Value() const
		{
			return value_;
		}

	protected:
		/*!
		 * \brief Gets a reference on the error code value
		 */
		Type & Value()
		{
			return value_;
		}

	private:
		Type value_;
	};
}

#endif /*_ADSP_SYSTEM_ERROR_H_ */
