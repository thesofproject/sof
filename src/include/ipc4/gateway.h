/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 */

/**
 * \file include/ipc4/header.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_GATEWAY_H__
#define __SOF_IPC4_GATEWAY_H__

#include <stdint.h>

//! Type of the gateway.
enum ipc4_connector_node_id_type
{
    //! HD/A host output (-> DSP).
    kHdaHostOutputClass = 0,
    //! HD/A host input (<- DSP).
    kHdaHostInputClass = 1,
    //! HD/A host input/output (rsvd for future use).
    kHdaHostInoutClass = 2,

    //! HD/A link output (DSP ->).
    kHdaLinkOutputClass = 8,
    //! HD/A link input (DSP <-).
    kHdaLinkInputClass = 9,
    //! HD/A link input/output (rsvd for future use).
    kHdaLinkInoutClass = 10,

    //! DMIC link input (DSP <-).
    kDmicLinkInputClass = 11,

    //! I2S link output (DSP ->).
    kI2sLinkOutputClass = 12,
    //! I2S link input (DSP <-).
    kI2sLinkInputClass = 13,

    //! ALH link output, legacy for SNDW (DSP ->).
    kALHLinkOutputClass = 16,
    //! ALH link input, legacy for SNDW (DSP <-).
    kALHLinkInputClass = 17,

    //! SNDW link output (DSP ->).
    kAlhSndWireStreamLinkOutputClass = 16,
    //! SNDW link input (DSP <-).
    kAlhSndWireStreamLinkInputClass = 17,

    //! UAOL link output (DSP ->).
    kAlhUAOLStreamLinkOutputClass = 18,
    //! UAOL link input (DSP <-).
    kAlhUAOLStreamLinkInputClass = 19,

    //! IPC output (DSP ->).
    kIPCOutputClass = 20,
    //! IPC input (DSP <-).
    kIPCInputClass = 21,

    //! I2S Multi gtw output (DSP ->).
    kI2sMultiLinkOutputClass = 22,
    //! I2S Multi gtw input (DSP <-).
    kI2sMultiLinkInputClass = 23,
    //! GPIO
    kGpioClass = 24,
    //! SPI
    kSpiOutputClass = 25,
    kSpiInputClass = 26,
    kMaxConnectorNodeIdType
};

//! Invalid raw node id (to indicate uninitialized node id).
#define kInvalidNodeId      0xffffffff

//! Base top-level structure of an address of a gateway.
/*!
  The virtual index value, presented on the top level as raw 8 bits,
  is expected to be encoded in a gateway specific way depending on
  the actual type of gateway.
*/
union ipc4_connector_node_id
{
    //! Raw 32-bit value of node id.
    uint32_t dw;
    //! Bit fields
    struct
    {
        //! Index of the virtual DMA at the gateway.
        uint32_t v_index    : 8;

        //! Type of the gateway, one of ConnectorNodeId::Type values.
        uint32_t dma_type   : 5;

        //! Rsvd field.
        uint32_t _rsvd      : 19;
    } f; //!< Bits
} __attribute__((packed, aligned(4)));

// HD/A Part begins here -> public IO driver

// TODO: The following HD-A DMA Nodes have IDs defined by spec
#define kHwHostOutputNodeIdBase     0x00
#define kHwCodeLoaderNodeId         0x0F
#define kHwLinkInputNodeIdBase      0x10

/*!
  Attributes are usually provided along with the gateway configuration
  BLOB when the FW is requested to instantiate that gateway.

  There are flags which requests FW to allocate gateway related data
  (buffers and other items used while transferring data, like linked list)
  to be allocated from a special memory area, e.g low power memory.
*/
union ipc4_gateway_attributes
{
    //! Raw value
    uint32_t dw;
    //! Access to the fields
    struct
    {
        //! Gateway data requested in low power memory.
        uint32_t lp_buffer_alloc        : 1;

        //! Gateway data requested in register file memory.
        uint32_t alloc_from_reg_file    : 1;

        //! Reserved field
        uint32_t _rsvd                  : 30;
    } bits; //!< Bits
} __attribute__((packed, aligned(4)));

//! Configuration for the IPC Gateway
struct IpcGatewayConfigBlob
{
    //! Size of the gateway buffer, specified in bytes
    uint32_t buffer_size;
    //! Flags
    union Flags
    {
        struct Bits
        {
            //! Activates high threshold notification
            /*!
              Indicates whether notification should be sent to the host
              when the size of data in the buffer reaches the high threshold
              specified by threshold_high parameter.
             */
            uint32_t notif_high : 1;

            //! Activates low threshold notification
            /*!
              Indicates whether notification should be sent to the host
              when the size of data in the buffer reaches the low threshold
              specified by threshold_low parameter.
             */
            uint32_t notif_low : 1;

            //! Reserved field
            uint32_t rsvd : 30;
        } f; //!< Bits
        //! Raw value of flags
        uint32_t flags_raw;
    } u; //!< Flags

    //! High threshold
    /*!
      Specifies the high threshold (in bytes) for notifying the host
      about the buffered data level.
     */
    uint32_t threshold_high;

    //! Low threshold
    /*!
      Specifies the low threshold (in bytes) for notifying the host
      about the buffered data level.
     */
    uint32_t threshold_low;
} __attribute__((packed, aligned(4)));

#endif

