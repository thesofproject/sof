#include <rtos/userspace_helper.h>
#include <sof/audio/module_adapter/iadk/iadk_timestamping_service.h>
#include <sof/audio/module_adapter/iadk/timestamping_interface.h>

namespace intel_adsp
{

/**
 * \brief Timestamping compatibility provider with zeroed counters.
 */
class TimestampingServiceProvider final : public TimestampingInterface {
public:
	ErrorCode::Type GetCurrentPosition(const ModuleHandle &,
					  ModuleCounters &counters) const override
	{
		counters = {};
		return ErrorCode::NO_ERROR;
	}

	ErrorCode::Type AdjustPosition(const ModuleHandle &, ModuleCounters &,
				      int64_t ms_shift) const override
	{
		return ms_shift == 0 ? ErrorCode::NO_ERROR : ErrorCode::FATAL_FAILURE;
	}
};

static APP_TASK_DATA TimestampingServiceProvider timestamping_service;

AdspErrorCode GetTimestampingServiceInterface(uint32_t version, struct system_service_iface **iface)
{
	if (version != INTERFACE_VERSION_TIMESTAMPING_SERVICE_V1) {
		*iface = nullptr;
		return ADSP_SERVICE_VERSION_UNAVAILABLE;
	}

	*iface = reinterpret_cast<struct system_service_iface *>(&timestamping_service);
	return ADSP_NO_ERROR;
}

} /* namespace intel_adsp */
