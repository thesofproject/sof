/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef SOF_AUDIO_MODULE_ADAPTER_LIBRARY_KPB_SERVICE_H
#define SOF_AUDIO_MODULE_ADAPTER_LIBRARY_KPB_SERVICE_H

#include <module/iadk/adsp_error_code.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \struct kpb_service_client
 * \brief Opaque handle for a client registered with the SOF KPB component.
 */
struct kpb_service_client;

/**
 * \brief Register an IADK client with the SOF KPB component.
 *
 * \param current_history_depth Initial history depth in milliseconds.
 * \param max_history_depth Maximum history depth in milliseconds.
 * \param output_pin Zero-based buffered output pin index.
 * \param client Location for the registered client handle.
 * \return ADSP error code.
 */
AdspErrorCode kpb_service_register_client(size_t current_history_depth,
					  size_t max_history_depth, uint32_t output_pin,
					  struct kpb_service_client **client);

/**
 * \brief Notify KPB about key phrase detection.
 *
 * \param client Registered client handle.
 * \param phrase_length Detected phrase length in milliseconds.
 * \return ADSP error code.
 */
AdspErrorCode kpb_service_signal_detection(struct kpb_service_client *client,
					   uint32_t phrase_length);

/**
 * \brief Unregister an IADK client from the SOF KPB component.
 *
 * \param client Registered client handle.
 * \return ADSP error code.
 */
AdspErrorCode kpb_service_unregister_client(struct kpb_service_client *client);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOF_AUDIO_MODULE_ADAPTER_LIBRARY_KPB_SERVICE_H */
