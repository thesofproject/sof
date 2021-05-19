// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

//! This is general API for any kind of history buffer.
//! It is used by KPB and WHM as basic operations on storing data


#ifndef _KPB_SHARED_BUFFER_H
#define _KPB_SHARED_BUFFER_H
#include "adsp_error.h"
#include "adsp_std_defs.h"

EXTERN_C_BEGIN
ErrorCode memcpy_s(void * dst, size_t maxlen, const void * src, size_t len);
EXTERN_C_END

namespace dsp_fw
{
namespace kpb_hb
{
/*! Basic class for storing general data for history buffer.
    Separated, because of possibility of exchange between different modules
    (KPB and WHM)
*/
struct HistoryBufferDataBase
{
    HistoryBufferDataBase() : buffer_(NULL), size_(0), data_size_(0), wp_(NULL)
    {
    }

    uint8_t* buffer_;
    size_t size_;
    size_t data_size_;
    uint8_t* wp_;
};

/*! Basic class for operating on data.
    Provided base storing and read operations on any kind of
    history buffer.
    Usage: KPB, WHM, data buffer exchange
*/
struct HistoryBufferBase : protected HistoryBufferDataBase
{
    HistoryBufferBase()
    {
    }

    /*! Init history buffer
        \param buffer: pointer to memory that are provided for usage
        \param size: buffer size
     */
    void Init(uint8_t* buffer, size_t size)
    {
        buffer_ = buffer;
        size_ = size;
        data_size_ = 0;
        wp_ = buffer;
    }

    /*! Wait for memory that is used to read/write operations is ready for fully usage
        Example usage: Wait till TLB are fully programmed
    */
    virtual void WaitForMemory(uint8_t* dst, size_t size)
    {
    }

    /*! Push data to history buffer
        \param data: pointer to buffer that data are coped from
        \param size: nr of bytes to be copied
     */
    ErrorCode PushData(const uint8_t* data, size_t size)
    {
        RETURN_EC_ON_FAIL(size <= size_, ADSP_INVALID_SIZE);
        RETURN_EC_ON_FAIL(size != 0, ADSP_SUCCESS);

        uint8_t* be = buffer_ + size_;

        if ((wp_ + size) < be)
        {
            WaitForMemory(wp_, size);
            ErrorCode ec = memcpy_s(wp_, size, data, size);
            RETURN_ON_ERROR(ec);
            wp_ += size;
        }
        else
        {
            size_t size_till_end = be - wp_;
            size_t reminder = size - size_till_end;
            WaitForMemory(buffer_, size_);
            ErrorCode ec = memcpy_s(wp_, size_till_end, data, size_till_end);
            RETURN_ON_ERROR(ec);
            ec = memcpy_s(buffer_, reminder, data + size_till_end, reminder);
            RETURN_ON_ERROR(ec);
            wp_ = buffer_ + reminder;
        }

        size_t new_size = data_size_ + size;
        data_size_ = min(new_size, size_);

        return ADSP_SUCCESS;
    }

    /*! Read data from history buffer
        \param data: pointer to buffer where data are copied to
        \param rs: size of bytes to read
        \param hs: client history size
     */
    ErrorCode ReadData(uint8_t* data, size_t rs, size_t hs)
    {
        RETURN_EC_ON_FAIL(rs != 0, ADSP_SUCCESS);
        RETURN_EC_ON_FAIL(data_size_ >= rs, ADSP_INVALID_SIZE);
        RETURN_EC_ON_FAIL(data_size_ >= hs, ADSP_INVALID_SIZE);
        RETURN_EC_ON_FAIL(hs >= rs, ADSP_INVALID_SIZE);

        ErrorCode ec = ADSP_SUCCESS;
        const uint8_t* rp = GetRp(hs);
        const uint8_t* const be = buffer_ + size_;

        if ((rp + rs) < be)
        {
            ec = memcpy_s(data, rs, rp, rs);
            RETURN_ON_ERROR(ec);
        }
        else
        {
            size_t size_till_end = be - rp;
            size_t reminder = rs - size_till_end;
            ec = memcpy_s(data, size_till_end, rp, size_till_end);
            RETURN_ON_ERROR(ec);
            ec = memcpy_s(data + size_till_end, reminder, buffer_, reminder);
            RETURN_ON_ERROR(ec);
        }
        return ADSP_SUCCESS;
    }

    inline void Reset()
    {
        wp_ = buffer_;
        data_size_ = 0;
    }

    inline uint8_t* read_ptr(size_t hist_size)
    {
        return GetRp(hist_size);
    };

protected:
    //! Get Read Pointer base on client history size
    inline uint8_t* GetRp(size_t hs)
    {
        uint8_t* rp = wp_ - hs;

        if (rp < buffer_)
            rp += size_;

        return rp;
    }
};

} // namespace kpb_hb

} // namespace dsp_fw
#endif //_KPB_SHARED_BUFFER_H
