/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef DSP_FW_FW_ARRAY_H_
#define DSP_FW_FW_ARRAY_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! Wrapper for buffer descriptor. */
typedef struct byte_array {
	/*! Pointer to buffer begin. */
	uint8_t    *data;
	/*! Size of buffer (in number of elements, typically bytes). */
	size_t      size;
} byte_array_t;

static inline uint8_t *array_get_data(const byte_array_t *ba)
{
	return (uint8_t *)ba->data;
}

static inline uint8_t *array_get_data_end(const byte_array_t *ba)
{
	return array_get_data(ba) + ba->size;
}


static inline size_t array_get_size(const byte_array_t *ba)
{
	return ba->size;
}

static inline uint8_t *array_alloc_from(byte_array_t *ba,
					size_t required_size)
{
	/* TODO: add alignment */
	/* TODO: validate inputs */
	/* TODO: move this to more appropriate file */

	uint8_t *cached_data = array_get_data(ba);

	ba->data += required_size;
	ba->size -= required_size;

	return cached_data;
}
#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
/* clang-format off */
#define assert(cond)

static inline uint8_t *array_get_data(const byte_array_t &ba)
{
	return (uint8_t *)ba.data;
}

static inline size_t array_get_size(const byte_array_t &ba)
{
	return ba.size;
}

namespace dsp_fw
{
/*!
 *  Provides a safe wrapper around array allocated in continuous memory.
 *  Size of the array specified by the client is checked on any attempt
 *  to access the array data.
 *
 * \note Wrapper does not take over ownership of the array. The array
 * must be deallocated elsewhere by its proper owner.
 *
 * \note All copy operations are shallow bit copies of the array.
 * The array is expected to keep built-in types or pointers, so
 * there is no assignment operator called for each copied entry.
 *
 * \note Array (especially Array<uint8_t>/ByteArray) can be casted to
 * byte_array_t as there are binary compatible
 */
template < class T >
class Array /*: public byte_array*/
{
public:

/*!
 * Default ctor to provide two-stage initialization completed by Init() call.
 */
Array()
{
	data_ = NULL;
	size_ = 0;
}

/*!
 * Constructs the object and initializes pointer and size to the provided
 * values.
 *
 * \param ptr - Pointer to the array.
 * \param size - Size of the array pointed by the ptr.
 */
Array(T * ptr, size_t size)
{
	data_ = (T *)ptr;
	size_ = size;
}

/*!
 *  Completes two-stage object initialization when initialized by the default
 *  ctor.
 *
 * \param ptr - Pointer to the array.
 * \param size - Size of the array pointed by the ptr.
 */
void Init(T *ptr, size_t size)
{
	data_ = (T *)ptr;
	size_ = size;
}

/*!
 * Completes two-stage object initialization when initialized by the default
 * ctor.
 *
 * \param ptr - Pointer to the array.
 * \param end - Pointer to the end of the array.
 */
void InitWithRange(T *ptr, T *ptr_end)
{
	assert(ptr_end >= ptr);
	data_ = (T *)ptr;
	size_ = ptr_end - ptr;
}

/*!
 * Detaches the wrapper from the array object. Provided for the sake of
 * completeness since array lifetime is not bound to the wrapper lifetime.
 */
void Detach(void) { data_ = NULL; size_ = 0; }
/*!
 * Retrieves the size of the array.
 *
 * \return Size of the array. It may be zero if the wrapper is not fully
 *         initialized.
 */
size_t size(void) const { return size_; }
/*!
 * Retrieves allocated size of buffer in bytes.
 */
size_t alloc_size(void) const { return size()*sizeof(T); }
/*!
 * Resizes the array.
 */
void Resize(size_t new_size) { size_ = new_size; }
/*!
 * Retrieves address of the array (const version).
 * \return Address of the array. It may be null if the wrapper is not fully
 * initialized.
 */
const T *data(void) const { return (const T *)data_; }
/*!
 * Retrieves address of the array (modifiable version).
 *
 * \return Address of the array. It may be null if the wrapper is not fully
 * initialized.
 */
T *data(void) { return (T *)data_; }
/*!
 * Retrieves address of end of the array (const version).
 * \return Address of the array.
 */
const T *data_end(void) const { return data() + size_; }
/*!
 * Retrieves address of end of the array (modifiable version).
 * \return Address of end of the array.
 */
T *data_end(void) { return data() + size_; }
/*!
 * Safe (in debug) operator to access element of the array (const version).
 *
 * \param idx Index of the element to be accessed.
 * \return Reference to the element.
 */
const T & operator[](size_t idx) const {
	/*assert(idx < size()); //TODO: no exceptions, release err handling req */
	return data()[idx];
}

/*!
 * Safe (in debug) operator to access element of the array (modifiable version).
 *
 * \param idx Index of the element to be accessed.
 * \return Reference to the element.
 */
T & operator[](size_t idx) {
	/*assert(idx < size()); //TODO: no exceptions, release err handling req */
	return data()[idx];
}

/*!
 * Safe copy-from operation that verifies size of source and this.
 *
 * \param src Address of the source array to be copied into this.
 * \param srcSize Size of the source array.
 * \param dst_offset Offset in the destination array to start copying data at
 * (expressed in number of items).
 */
void copyFrom(const T *src, size_t srcSize, size_t dst_offset = 0)
{
	assert(data() != NULL);
	assert(size() > dst_offset);
	assert((size() - dst_offset) >= srcSize);
	memcpy_s(data() + dst_offset, alloc_size() - dst_offset * sizeof(T),
		src, srcSize * sizeof(T));
}

/*!
 * Safe copy-from operation that verifies size of source and this.
 *
 * \param src The source array to be copied into this.
 * \param dst_offset Offset in the destination array to start copying data at
 * (expressed in number of items).
 */
void copyFrom(const Array < T > &src, size_t dst_offset = 0)
{
	assert(data() != 0);
	assert(size() > dst_offset);
	assert((size() - dst_offset) >= src.size());
	memcpy_s(data() + dst_offset, alloc_size() - dst_offset * sizeof(T),
		src.data(), src.alloc_size());
}

/*!
 * Safe copy-to operation that verifies size of the destination and this.
 * \param dst Address of the destination array to be overwritten by this.
 * \param dstSize Size of the destination array.
 */
void copyTo(T *dst, size_t dstSize) const {
	assert(data() != 0);
	assert(size() <= dstSize);
	memcpy_s(dst, dstSize * sizeof(T), data(), alloc_size());
}

/*!
 * Safe copy-to operation that verifies size of the destination and this.
 * \param dst The destination array to be overwritten by this.
 */
void copyTo(Array < T > &dst) const {
	assert(data() != 0);
	assert(size() <= dst.size());
	memcpy_s(dst.data(), dst.alloc_size(), data(), alloc_size());
}

/*!
 * Copies as much as specified size starting from specified offset.
 * \param dst Destination buffer.
 * \param copy_size Number of items to be copied.
 * \param src_offset Offset in the source buffer (in number of items).
 */
void copyFragmentTo(Array < T > &dst, size_t copy_size, size_t src_offset = 0) const {
	assert(data() != 0);
	assert((src_offset + copy_size) <= size());
	assert(copy_size <= dst.size());
	memcpy_s(dst.data(), dst.alloc_size(), data() + src_offset, copy_size * sizeof(T));
}

/*!
 * Safe cast of the content of the array to the specified type (modifiable
 * version).
 * \return Pointer to the requested type if array is large enough to be
 *        casted, NULL otherwise.
 */
template < class TC >
TC *dataAs(void)
{
	if (alloc_size() < sizeof(TC)) {
		assert(false);
		return NULL;
	}
	return reinterpret_cast < TC * > (data());
}

template < class TC >
TC *dataAsArray(size_t size)
{
	if (alloc_size() < sizeof(TC)*size) {
		assert(false);
		return NULL;
	}
	return reinterpret_cast < TC * > (data());
}

/*!
 * Safe cast of the content of the array to the specified type (const version).
 * \return Pointer to the requested type if array is large enough to be
 * casted, NULL otherwise.
 */
template < class TC >
const TC *dataAs(void) const {
	if (alloc_size() < sizeof(TC)) {
		assert(false);
		return NULL;
	}
	return reinterpret_cast < const TC * > (data());
}

/*!
 * Inserts object of specified type into the buffer.
 * Use this explicit call instead of operator=(TC) or stream-like <<.
 */
template < class TC >
void setDataAs(const TC & d)
{
	TC *t = dataAs < TC > ();

	if (t == NULL) {
		assert(false);
		return;
	}
	*t = d; /* using operator=, not shallow copy */
	Resize(sizeof(TC));
}

/*!
 * Safe zero-memory on the underlying buffer.
 */
void clear(void)
{
	memset(data(), 0x00, alloc_size());
}

void Swap(Array < T > *array)
{
	Array < T > temp(*array);
	array->data_ = data_;
	array->size_ = size_;
	this->data_ = temp.data_;
	this->size_ = temp.size_;
}

void Swap(Array < T > &array)
{
	Array < T > temp(array);
	array.data_ = data_;
	array.size_ = size_;
	this->data_ = temp.data_;
	this->size_ = temp.size_;
}
private:
	T *data_;
	size_t size_;
};

/*!
 * Predefined type of array of bytes.
 */
typedef Array < uint8_t > ByteArray;

/*!
 * Predefined type of array of dwords.
 */
typedef Array < uint32_t > DwordArray;

} /* end of namespace dsp_fw */

#endif /* __cplusplus */
/* clang-format on */
#endif /* #ifndef DSP_FW_FW_ARRAY_H_ */
