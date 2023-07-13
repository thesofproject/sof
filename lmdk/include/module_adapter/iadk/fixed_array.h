/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */
/*! \file fixed_array.h */

#ifndef _ADSP_FIXED_ARRAY_H_
#define _ADSP_FIXED_ARRAY_H_

#include <stddef.h>

namespace intel_adsp
{
	/* \brief Fixed-size container of continuous elements whose size is specified at runtime.
	 *
	 * This container is very similar to the C-array except that its fixed-size is specified at
	 * runtime.
	 * It acts as a facade to expose a memory buffer as a array of elements with same type.
	 *
	 *  \tparam VALUE   type of the array elements.
	 */
	template < class VALUE >
	struct FixedArray {
		typedef VALUE ValueType;

		/*! \brief Initializes a new instance of FixedArray.
		 */
		explicit FixedArray(ValueType *array, size_t length) :
				    array_(array), length_(length)
		{}

		/*! \brief Gets value at the given index of the array.
		 *  \param index    index of the value to retrieve. By default index is set to 0.
		 *  \note If index is out of range (i.e. is equal or greater than value returned by
		 *        GetLength()) the returned value is mal-formed.
		 *  \remarks Return is by value rather than reference to prevent client code from
		 *           working with dangling reference.
		 *  Indeed a FixedArray object does not own the underlying associated buffer.
		 */
		ValueType GetValue(int index = 0) const
		{
			return array_[index];
		}

		/*! \brief Subscript operator.
		 *
		 * This method actually returns  the result of GetValue()
		 */
		ValueType operator[](int index) const
		{
			return GetValue(index);
		}

		/*! \brief Gets the number of elements
		 */
		size_t GetLength(void) const
		{
			return length_;
		}

		/*! \brief Copies the values wrapped by the FixedArray into the given C-array
		 */
		void Copy(ValueType *array, size_t length) const
		{
			length = MIN(length, length_);
			for (int i = 0 ; i < length ; i++)
				array[i] = array_[i];
		}

	private:
		ValueType * const array_;
		size_t const length_;
	};

} /* namespace intel_adsp */

#endif /* #ifndef _ADSP_FIXED_ARRAY_H_ */
