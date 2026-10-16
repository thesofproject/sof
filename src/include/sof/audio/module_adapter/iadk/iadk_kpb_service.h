/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef _IADK_KPB_SERVICE_H_
#define _IADK_KPB_SERVICE_H_

#include <stdint.h>
#include <system_service.h>

namespace intel_adsp
{

/**
 * \brief Get the IADK KPB service interface.
 *
 * \param version Requested INTERFACE_VERSION_KPB_SERVICE_* version.
 * \param iface Location for the returned KpbInterface or KpbInterfaceV2 instance.
 * \return ADSP_NO_ERROR, or ADSP_SERVICE_VERSION_UNAVAILABLE for unsupported versions.
 */
AdspErrorCode GetKpbServiceInterface(uint32_t version, struct system_service_iface **iface);

} /* namespace intel_adsp */

#endif /* _IADK_KPB_SERVICE_H_ */
