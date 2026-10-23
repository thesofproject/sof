#ifndef _IADK_TIMESTAMPING_SERVICE_H_
#define _IADK_TIMESTAMPING_SERVICE_H_

#include <stdint.h>
#include <system_service.h>

namespace intel_adsp
{

/**
 * \brief Get the IADK timestamping service interface.
 *
 * \param version Requested INTERFACE_VERSION_TIMESTAMPING_SERVICE_* version.
 * \param iface Non-null location for the returned TimestampingInterface instance.
 * \return ADSP_NO_ERROR, or ADSP_SERVICE_VERSION_UNAVAILABLE for unsupported versions.
 */
AdspErrorCode GetTimestampingServiceInterface(uint32_t version, struct system_service_iface **iface);

} /* namespace intel_adsp */

#endif /* _IADK_TIMESTAMPING_SERVICE_H_ */
