// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#ifndef _ADSP_IPC_TRANSPORT_H
#define _ADSP_IPC_TRANSPORT_H

#include "adsp_std_defs.h"
#include "intel_adsp/system_service_internal.h"

/**
 * @brief object representing single ipc message
 */
struct ipc_message
{
    uint32_t object_; ///< object id used by base fw
    uint32_t primary_; ///< primary dword of messge
    uint32_t extension_; ///< extension dword od message
    uint8_t* data_; ///< pointer to additional payload data
    uint32_t data_size_; ///< payload size
};

typedef bool (*pfn_ipc_transport_filter_t)(struct ipc_transport const* transport, struct ipc_message* msg);

/**
 * @brief IPC transport interface for IpcDriverDynamic
 *
 */
struct ipc_transport
{
    SystemServiceIface iface_;
    bool (*trigger_transport_)(struct ipc_transport const* transport, struct ipc_message* msg);
    void (*confirm_)(struct ipc_transport const* transport);
    pfn_ipc_transport_filter_t filter_;
};

/**
 * @brief IPC driver interface used by transport.
 *
 */
struct ipc_driver_dynamic
{
    SystemServiceIface iface_; ///< base fw interface pointer
    void* context_; ///< base fw interface private context

    /**
     * @brief Called when transport becomes idle after processing is done.
     *
     * @param [in] iface dynamic ipc interface pointer
     * @param [inout] pointer to structure to be filled with next pending message
     * @return true if another IPC was queued for TX and now been peaked for proccesing
     */
    bool (*get_next_message_)(
        struct ipc_driver_dynamic* iface,
        struct ipc_message* ipc);

    /**
     * @brief Signals dynamic ipc driver that a request waits for processing.
     *
     * @param [in] iface dynamic ipc interface pointer
     * @param [in] in_buffer buffer storing the request
     * @param [in] in_buffer_size size of request buffer.
     */
    void (*process_request_)(
        struct ipc_driver_dynamic* iface,
        uint8_t* in_buffer_,
        uint32_t in_buffer_size);

    /**
     * @brief Signals dynamic ipc driver that a message can be cleaned up.
     *
     * @param [in] iface dynamic ipc interface pointer
     * @param [in] ipc pointer to the message
     */
    void (*cleanup_message_)(
        struct ipc_driver_dynamic* iface,
        struct ipc_message* ipc);
};


#endif
