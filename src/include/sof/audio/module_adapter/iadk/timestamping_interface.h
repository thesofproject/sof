#ifndef SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_INTERFACE_H
#define SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_INTERFACE_H

#include <stdint.h>

#include "system_error.h"

namespace intel_adsp
{

class ModuleHandle;

/**
 * \brief Snapshot of timestamping counters associated with a module.
 */
struct ModuleCounters {
	uint64_t timer_value;
	uint64_t linear_link_position;
	uint64_t gateway_total_processed_data;
	uint64_t module_total_processed_data;
};

/**
 * \brief IADK timestamping service v1 ABI.
 */
class TimestampingInterface {
public:
	/** \brief Retrieve the current module counters. */
	virtual ErrorCode::Type GetCurrentPosition(const ModuleHandle &module_handle,
						  ModuleCounters &counters) const = 0;
	/** \brief Adjust module counters by the requested shift in milliseconds. */
	virtual ErrorCode::Type AdjustPosition(const ModuleHandle &module_handle,
					      ModuleCounters &counters, int64_t ms_shift) const = 0;
};

} /* namespace intel_adsp */

#endif /* SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_INTERFACE_H */
