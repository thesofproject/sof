/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef _KPB_INTERFACE_H_
#define _KPB_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>
#include "system_error.h"

namespace intel_adsp
{
class ModuleHandle;

/**
 * \struct KpbToken
 * \brief Opaque state for a KPB client registration.
 */
struct KpbToken;

/**
 * \class KpbInterface
 * \brief Version 1 KPB service interface for IADK modules.
 */
class KpbInterface {
public:
	/**
	 * \brief Register an IADK module as a KPB client.
	 * \param module_handle Handle of the module registering the client.
	 * \param token Storage for the registered client's token.
	 * \param current_history_depth Initial history depth in milliseconds.
	 * \param max_hist_depth Maximum history depth in milliseconds.
	 * \param output_pin Zero-based buffered output pin index.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type RegisterKpbClient(const ModuleHandle& module_handle,
						 KpbToken& token,
						 size_t current_history_depth,
						 size_t max_hist_depth,
						 uint32_t output_pin) const = 0;

	/**
	 * \brief Report key-phrase detection for a registered client.
	 * \param token Token of the registered client.
	 * \param data Pointer to the detected phrase length in milliseconds.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type SignalDetection(KpbToken& token, void* data) const = 0;

	/**
	 * \brief Report voice detection for a registered client.
	 * \param token Token of the registered client.
	 * \param data Event-specific data (unused by the current native service).
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type SignalVoiceDetection(KpbToken& token, void* data) const = 0;

	/**
	 * \brief Report silence detection for a registered client.
	 * \param token Token of the registered client.
	 * \param data Event-specific data (unused by the current native service).
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type SignalSilenceDetection(KpbToken& token, void* data) const = 0;

	/**
	 * \brief Unregister a KPB client.
	 * \param token Token of the client to unregister.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type UnregisterKpbClient(KpbToken& token) const = 0;

	/**
	 * \brief Register an IADK module as a WOV producer.
	 * \param module_handle Handle of the module registering as a producer.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type RegisterWovProducer(const ModuleHandle& module_handle) const = 0;

	/**
	 * \brief Register an IADK module as a KPB producer.
	 * \param module_handle Handle of the module registering as a producer.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type RegisterKpbProducer(const ModuleHandle& module_handle) const = 0;

	/**
	 * \brief Request a WOV consumer for an IADK module.
	 * \param module_handle Handle of the module requesting a consumer.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type RequestWovConsumer(const ModuleHandle& module_handle) const = 0;
};

/**
 * \class KpbInterfaceV2
 * \brief Version 2 KPB service interface for IADK modules.
 */
class KpbInterfaceV2 {
public:
	/**
	 * \brief Register an IADK module as a KPB client.
	 * \param module_handle Handle of the module registering the client.
	 * \param token Storage for the registered client's token.
	 * \param current_history_depth Initial history depth in milliseconds.
	 * \param max_hist_depth Maximum history depth in milliseconds.
	 * \param output_pin Zero-based buffered output pin index.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type RegisterKpbClient(const ModuleHandle& module_handle,
						 KpbToken& token,
						 size_t current_history_depth,
						 size_t max_hist_depth,
						 uint32_t output_pin) const = 0;

	/**
	 * \brief Report key-phrase detection for a registered client.
	 * \param token Token of the registered client.
	 * \param data Pointer to the detected phrase length in milliseconds.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type SignalDetection(KpbToken& token, void* data) const = 0;

	/**
	 * \brief Unregister a KPB client.
	 * \param token Token of the client to unregister.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type UnregisterKpbClient(KpbToken& token) const = 0;

	/**
	 * \brief Register an IADK module as a KPB producer.
	 * \param module_handle Handle of the module registering as a producer.
	 * \return SOF error code, with ADSP_NO_ERROR on success.
	 */
	virtual ErrorCode::Type RegisterKpbProducer(const ModuleHandle& module_handle) const = 0;
};

} /* namespace intel_adsp */

#endif /* _KPB_INTERFACE_H_ */
