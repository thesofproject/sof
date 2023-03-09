/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */
/*! \file logger.h */

#ifndef _ADSP_FDK_LOGGER_H_
#define _ADSP_FDK_LOGGER_H_

#include <stdint.h>
#include "system_service.h"
#ifdef __cplusplus
namespace intel_adsp
{
/*! \brief Helper class which handles the values list passed at call to the LOG_MESSAGE macro.
 * \internal
 * This class should not be used directly. log sending can be performed with help of the macro
 *  \ref LOG_MESSAGE.
 */
class Logger
{
public:
	/*! \cond INTERNAL */
	Logger(adsp_system_service const &system_service, AdspLogHandle const &log_handle)
		:
			system_service_(system_service),
			log_handle_(log_handle)
	{}

	template<AdspLogPriority LOG_LEVEL>
	void SendMessage(uint32_t log_entry, uint32_t _ignored, uint32_t param1 = 0,
			 uint32_t param2 = 0, uint32_t param3 = 0, uint32_t param4 = 0)
	{
		(void)_ignored;
		system_service_.LogMessage(LOG_LEVEL, log_entry, &log_handle_,
					   param1, param2, param3, param4);
	}

	/*! \endcond INTERNAL */
private:
	adsp_system_service const &system_service_;
	AdspLogHandle const &log_handle_;
};

} /* namespace intel_adsp */

#endif /* #ifdef __cplusplus */

#endif /* _ADSP_FDK_LOGGER_H_ */
