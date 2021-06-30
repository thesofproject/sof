// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
/*! \file logger.h */


#ifndef _ADSP_FDK_LOGGER_H_
#define _ADSP_FDK_LOGGER_H_

#include "core/kernel/logger/log.h"
#include "system_service.h"

#include <stdint.h>

#undef LOG_ENTRY
#define LOG_ENTRY 0
#undef LOG_MESSAGE

#define LOG_ENTRY_MODULE GetResourceId()

// added resource_id in declare_log_entry call to reserve space for resource_id
// - added later in system_service_ as first arg
#define LOG_MESSAGE(level, format, resource_id, ...)                           \
    do {                                                                       \
        CHECK_FORMAT("[%8.8X]: " format, resource_id, ##__VA_ARGS__);          \
        _DECLARE_LOG_ENTRY(                                                    \
                CONCAT(L_,level),                                              \
                L_MODULE,                                                      \
                "[%8.8X]: " format,                                            \
                __NARG__(resource_id, __VA_ARGS__)                             \
        );                                                                     \
        using namespace intel_adsp;                                            \
        Logger logger(GetSystemService(), GetLogHandle());                     \
        logger.SendMessage<CONCAT(L_,level)>(                                  \
                ((uint32_t)&log_entry.offset),                                 \
                resource_id,                                                   \
                ##__VA_ARGS__                                                  \
        );                                                                     \
    } while(0)

#define LOG_MESSAGE_STATIC(level, format, mod_inst_ptr, ...)                   \
    do                                                                         \
    {                                                                          \
        CHECK_FORMAT(format, ##__VA_ARGS__);\
        _DECLARE_LOG_ENTRY(                                                    \
                CONCAT(L_,level),                                              \
                L_MODULE,                                                      \
                "[L_" #level "][L_MODULE][%8.8X]: " format,                    \
                __NARG__(0x0, __VA_ARGS__));                                   \
        using namespace intel_adsp;                                            \
        Logger logger(mod_inst_ptr->GetSystemService(), mod_inst_ptr->GetLogHandle()); \
        logger.SendMessage<CONCAT(L_,level)>(                                  \
                ((uint32_t)&log_entry.offset),                                 \
                0x0,                                                           \
                ##__VA_ARGS__                                                  \
        );                                                                     \
    } while(0)

#ifdef __cplusplus
namespace intel_adsp
{

/*! \brief Helper class which handles the values list passed at call to the LOG_MESSAGE macro.
 * \internal
 * This class should not be used directly. log sending can be performed with help of the macro \ref LOG_MESSAGE.
 */
class Logger
{
public:
    /*! \cond INTERNAL */
    Logger(AdspSystemService const& system_service, AdspLogHandle const& log_handle)
        :
            system_service_(system_service),
            log_handle_(log_handle)
    {}

    template<AdspLogPriority LOG_LEVEL>
    void SendMessage(uint32_t log_entry, uint32_t _ignored, uint32_t param1 = 0, uint32_t param2 = 0, uint32_t param3 = 0, uint32_t param4 = 0)
    { (void)_ignored; system_service_.LogMessage(LOG_LEVEL, log_entry, &log_handle_, param1, param2, param3, param4); }

    /*! \endcond INTERNAL */
private:
    AdspSystemService const& system_service_;
    AdspLogHandle const& log_handle_;
};

} // namespace intel_adsp

#endif // #ifdef __cplusplus

#endif // _ADSP_FDK_LOGGER_H_
