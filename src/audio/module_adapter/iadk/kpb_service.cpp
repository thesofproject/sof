// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2026 Intel Corporation. All rights reserved.

#include <rtos/userspace_helper.h>
#include <sof/audio/module_adapter/iadk/iadk_kpb_service.h>
#include <sof/audio/module_adapter/iadk/kpb_interface.h>
#include <sof/audio/module_adapter/library/kpb_service.h>

namespace intel_adsp
{

struct KpbToken {
	const ModuleHandle *owner;
	void *kpb_client;
};

/**
 * \class KpbServiceV1
 * \brief SOF implementation of the KPB service v1 interface.
 */
class KpbServiceV1 final : public KpbInterface {
public:
	ErrorCode::Type RegisterKpbClient(const ModuleHandle& module_handle, KpbToken& token,
					 size_t current_history_depth, size_t max_history_depth,
					 uint32_t output_pin) const override
	{
		struct kpb_service_client *client;
		AdspErrorCode error = kpb_service_register_client(
								  current_history_depth,
								  max_history_depth,
								  output_pin, &client);

		if (error == ADSP_NO_ERROR) {
			token.owner = &module_handle;
			token.kpb_client = client;
		}

		return error;
	}

	ErrorCode::Type SignalDetection(KpbToken& token, void* data) const override
	{
		if (!token.owner || !token.kpb_client || !data)
			return ADSP_INVALID_PARAMETERS;

		return kpb_service_signal_detection(
			reinterpret_cast<struct kpb_service_client *>(token.kpb_client),
			*static_cast<uint32_t *>(data));
	}

	ErrorCode::Type SignalVoiceDetection(KpbToken&, void*) const override
	{
		return ADSP_NO_ERROR;
	}

	ErrorCode::Type SignalSilenceDetection(KpbToken&, void*) const override
	{
		return ADSP_NO_ERROR;
	}

	ErrorCode::Type UnregisterKpbClient(KpbToken& token) const override
	{
		if (!token.kpb_client) {
			token.owner = NULL;
			return ADSP_NO_ERROR;
		}

		AdspErrorCode error = kpb_service_unregister_client(
			reinterpret_cast<struct kpb_service_client *>(token.kpb_client));

		if (error == ADSP_NO_ERROR) {
			token.owner = NULL;
			token.kpb_client = NULL;
		}

		return error;
	}

	ErrorCode::Type RegisterWovProducer(const ModuleHandle&) const override
	{
		return ADSP_NO_ERROR;
	}

	ErrorCode::Type RegisterKpbProducer(const ModuleHandle&) const override
	{
		return ADSP_NO_ERROR;
	}

	ErrorCode::Type RequestWovConsumer(const ModuleHandle&) const override
	{
		return ADSP_NO_ERROR;
	}
};

/**
 * \class KpbServiceV2
 * \brief KPB service v2 implemented as the compatible subset of v1.
 */
class KpbServiceV2 final : public KpbInterfaceV2 {
public:
	ErrorCode::Type RegisterKpbClient(const ModuleHandle& module_handle, KpbToken& token,
					 size_t current_history_depth, size_t max_history_depth,
					 uint32_t output_pin) const override
	{
		return v1_.RegisterKpbClient(module_handle, token, current_history_depth,
					    max_history_depth, output_pin);
	}

	ErrorCode::Type SignalDetection(KpbToken& token, void* data) const override
	{
		return v1_.SignalDetection(token, data);
	}

	ErrorCode::Type UnregisterKpbClient(KpbToken& token) const override
	{
		return v1_.UnregisterKpbClient(token);
	}

	ErrorCode::Type RegisterKpbProducer(const ModuleHandle& module_handle) const override
	{
		return v1_.RegisterKpbProducer(module_handle);
	}

private:
	KpbServiceV1 v1_;
};

static APP_TASK_DATA KpbServiceV1 kpb_service_v1;
static APP_TASK_DATA KpbServiceV2 kpb_service_v2;

AdspErrorCode GetKpbServiceInterface(uint32_t version, struct system_service_iface **iface)
{
	switch (version) {
	case INTERFACE_VERSION_KPB_SERVICE_V1:
		*iface = reinterpret_cast<struct system_service_iface *>(&kpb_service_v1);
		return ADSP_NO_ERROR;
	case INTERFACE_VERSION_KPB_SERVICE_V2:
		*iface = reinterpret_cast<struct system_service_iface *>(&kpb_service_v2);
		return ADSP_NO_ERROR;
	default:
		*iface = NULL;
		return ADSP_SERVICE_VERSION_UNAVAILABLE;
	}
}

} /* namespace intel_adsp */
