// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#ifndef KERNEL_LOG_H
#define KERNEL_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <syntax_sugar.h>
#include <misc/cdecl.h>
#if defined(XTENSA_TOOLSCHAIN) || defined(__XCC__)
#include <xtensa/core-macros.h>
#elif defined(GCC_TOOLSCHAIN)
#include <xtensa_overlays.h>
#endif

EXTERN_C_BEGIN

/*!
  \addtogroup skernel Scalable Kernel
  @{
    \addtogroup logger Logger
    @{
      \addtogroup log_api API
      @{
  This section describes generic logging API. Using LOG macro (and as
  derivatives also FLOG, ALOG, SLOG etc) in code will:
  1. [COMPILATION] Create log_entry in .static_log section.
  2. [COMPILATION] Check message format (should be printf-like).
  3. [RUNTIME] Check whether message can be logged.
*/

#define LOGGER_THREADPTR_OFFSET 120

#ifdef MODULE_NAME
#define LOGGER_LIB_ID_VAR_NAME CONCAT(CONCAT(__, MODULE_NAME), _lib_id)
extern const volatile uint32_t LOGGER_LIB_ID_VAR_NAME;
#define LOGGER_LIB_ID_WRAP(input) (((uint32_t)input) | (LOGGER_LIB_ID_VAR_NAME << 3))
#else
#define LOGGER_LIB_ID_WRAP(input) input
#endif

//! The following function pointers are used to put logs in different ways
//! depending on selected logging backend
typedef void (*log_put_0_fn)(void*, uint32_t);
typedef void (*log_put_1_fn)(void*, uint32_t, uint32_t);
typedef void (*log_put_2_fn)(void*, uint32_t, uint32_t, uint32_t);
typedef void (*log_put_3_fn)(void*, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*log_put_4_fn)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*log_put_5_fn)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*log_put_6_fn)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*log_put_7_fn)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (*log_timer_callback_fn)(void* param);

//! Logger descriptor.
typedef struct logger
{
    struct log_transport* transport;
    uint32_t log_en_mask_;
    //! pointers to functions to put logs
    //! single function for different param count
    log_put_0_fn log_put_0;
    log_put_1_fn log_put_1;
    log_put_2_fn log_put_2;
    log_put_3_fn log_put_3;
    log_put_4_fn log_put_4;
    log_put_5_fn log_put_5;
    log_put_6_fn log_put_6;
    log_put_7_fn log_put_7;

    //! amount of dropped messages
    uint32_t dropped_counter_;
    //! reference host time
    uint64_t host_time_reference_;
    //! reference dsp time
    uint64_t dsp_time_reference_;

    //! Pointer to log buffer descriptor
    struct log_buffer* log_buffer;
    //! ACTUAL log buffer data capacity not log buffer size!
    uint32_t buffer_capacity;

    uint32_t cached_last_ro_f;
    uint32_t cached_wo;
} logger_s;

//! Log level priority enumeration.
typedef enum log_priority
{
    //! Critical message.
    L_CRITICAL = 0,
    //! Error message.
    L_ERROR = 0,
    //! High importance log level.
    L_HIGH = 1,
    //! Warning message.
    L_WARNING = 1,
    //! Medium importance log level.
    L_MEDIUM = 2,
    //! Low importance log level.
    L_LOW = 3,
    //! Information.
    L_INFO = 3,
    //! Verbose message.
    L_VERBOSE = 4,
    L_DEBUG   = 4,
    L_MAX     = 5,
} AdspLogPriority,
    log_priority_e;

//! Type of log source
typedef enum log_source
{
    L_INFRA      = 0,
    L_HAL        = 1,
    L_MODULE     = 2,
    L_AUDIO      = 3,
    L_SENSING    = 4,
    L_ULP_INFRA  = 5,
    L_ULP_MODULE = 6,
    L_VISION     = 7,
} log_source_e;

static inline logger_s* srv_get_logger()
{
    return (logger_s*)(RTHREADPTR() + LOGGER_THREADPTR_OFFSET);
}

#ifndef UT
static inline uint32_t srv_get_log_mask()
{
    // NOTE: this implementation might not provide the best performance as it
    // includes additional branch that can happen during FW initialization.
    if (0 == UNLIKELY(RTHREADPTR()))
    {
        return 0;
    }

    return srv_get_logger()->log_en_mask_;
}
#else // defined(UT)
static inline uint32_t srv_get_log_mask()
{
    return 0xffffffff;
}
#endif

#define FORMAT_CHECKER __attribute__((weak, format(printf, 1, 2)))
FORMAT_CHECKER void compile_time_format_verificator(const char* my_format, ...);

//! CHECK_FORMAT checks in compile time if format of LOG_MESSAGE is correct.
/*!
  compile_time_format_verificator is only declared but never defined. Linker
  will recognize that function is never actually used and remove all dependencies.
  For low optimization cases it will be replaced with call to NULL (since weak
  attribute is used)
*/
#define CHECK_FORMAT(format, ...)                               \
    if (false)                                                  \
    {                                                           \
        compile_time_format_verificator(format, ##__VA_ARGS__); \
    }

#define SET_LOG_SECT(level, log_source)                                     \
    __attribute__((section(                                                 \
        QUOTE(CONCAT(.static_log., __LINE__)) "." #level "." #log_source))) \
        __attribute__((aligned(128)))

#define _DECLARE_LE_STRUCT(format, args_count)                     \
    static const struct                                            \
    {                                                              \
        uint8_t padding[(args_count)];                             \
        uint8_t offset[8 - (args_count)];                          \
        uint32_t level;                                            \
        uint32_t log_source;                                       \
        uint32_t line_id;                                          \
        /* this string will be placed in .rodata.str1.4 section */ \
        const char* const file;                                    \
        uint32_t text_len;                                         \
        const char text[sizeof(format)];                           \
    }

//! Declares log_entry
/*!
  \attention: log_entry already contains information on how many arguments were
  passed
*/
#define _DECLARE_LOG_ENTRY(level, log_source, format, args_count)              \
    SECTIONATT(".function_strings")                                            \
    static const char log_entry_file[] = __FILE__;                             \
    SET_LOG_SECT(level, log_source)                                            \
    _DECLARE_LE_STRUCT(format, args_count)                                     \
    log_entry = {                                                              \
        /*padding    = */ { /* empty initializer - to get rid of warnings*/ }, \
        /*offset     = */ { /* empty initializer - to get rid of warnings*/ }, \
        /*level      = */ level,                                               \
        /*log_source = */ log_source,                                          \
        /*line_id    = */ __LINE__,                                            \
        /*file       = */ log_entry_file,                                      \
        /*text_len   = */ sizeof(format),                                      \
        /*text       = */ format                                               \
    };

#define _CAST_ARG0(t)
#define _CAST_ARG1(t, a) (t)(a)
#define _CAST_ARG2(t, a, b) _CAST_ARG1(t, a), (t)(b)
#define _CAST_ARG3(t, a, b, c) _CAST_ARG2(t, a, b), (t)(c)
#define _CAST_ARG4(t, a, b, c, d) _CAST_ARG3(t, a, b, c), (t)(d)
#define _CAST_ARG5(t, a, b, c, d, e) _CAST_ARG4(t, a, b, c, d), (t)(e)
#define _CAST_ARG6(t, a, b, c, d, e, f) _CAST_ARG5(t, a, b, c, d, e), (t)(f)
#define _CAST_ARG7(t, a, b, c, d, e, f, g) _CAST_ARG6(t, a, b, c, d, e, f), (t)(g)
#define _CAST_ARG8(t, a, b, c, d, e, f, g, h) _CAST_ARG7(t, a, b, c, d, e, f, g), (t)(h)

#define CAST_ARG(type, ...) CAST_ARG2(type, ##__VA_ARGS__)
#define CAST_ARG2(type, ...)                                              \
    /* Effectively will call for X args: _CAST_ARGX(type, __VA_ARGS__) */ \
    CONCAT(_CAST_ARG, __NARG__(__VA_ARGS__))                              \
    (type, ##__VA_ARGS__)


#define IS_LOG_LEVEL_ENABLED(log_level, log_source) \
    (IS_BIT_SET(srv_get_log_mask(), (log_level)) && \
        IS_BIT_SET(srv_get_log_mask(), (log_source) + L_MAX))

#if !defined(UT)
#define _LOG(log_level, log_source, format, ...)                                               \
    do                                                                                         \
    {                                                                                          \
        DEBUG_PRINT(format, ##__VA_ARGS__);                                                    \
        _DECLARE_LOG_ENTRY(log_level, log_source, format,                                      \
            (__NARG__(__VA_ARGS__)));                                                          \
        logger_s* logger_i_ = srv_get_logger();                                                \
        logger_i_->CONCAT(log_put_, __NARG__(__VA_ARGS__))                                     \
        (logger_i_, CAST_ARG(uint32_t, LOGGER_LIB_ID_WRAP(&log_entry.offset), ##__VA_ARGS__)); \
    } while (0)
#else
#define _LOG(log_level, log_source, format, ...) \
    do                                           \
    {                                            \
        DEBUG_PRINT(format, ##__VA_ARGS__);      \
    } while (0)
#endif

//! \endcond HIDDEN_SYMBOL

#if SUPPORTED(LOGGER)

//! Macro for creating single log entry.
/*!
  \note Macro requires printf-like formatting.
  \note Try to use %d, %X For formatting. Avoid %p

  \param log_level  Level of log. See log_priority_e
  \param log_source Log's source. See log_source_e
  \param format     C string that contains the text to be written to stdout. It
                    can optionally contain embedded format specifiers that are
                    replaced by the values specified in subsequent additional
                    arguments and formatted as requested.
  \param ...        Arguments.

  \code
  LOG(L_ERROR, L_INFRA, "Simple log without params");
  LOG(L_INFO, L_MODULE, "Simple log with 1 param = %d", arg);
  LOG(L_WARNING, L_AUDIO, "Simple log with 5 params = %d %d %d %d %d",
      arg1, arg2, arg3, arg4, arg5);

  // WILL NOT COMPILE - missing 1 argument.
  LOG(L_DEBUG, L_SENSING, "Simple log %d");
  // WILL NOT COMPILE - too many args.
  LOG(L_DEBUG, L_AUDIO, "Simple log %d", arg1, arg2);
  \endcode
*/
#define LOG(log_level, log_source, format, ...)                    \
    do                                                             \
    {                                                              \
        CHECK_FORMAT(format, ##__VA_ARGS__);                       \
        if (UNLIKELY(IS_LOG_LEVEL_ENABLED(log_level, log_source))) \
        {                                                          \
            _LOG(log_level, log_source, format, ##__VA_ARGS__);    \
        }                                                          \
    } while (0)

//! Macro for creating LP FW log entry
/*!
  \note Macro requires printf-like formatting.
  \note Macro will add "[%8.8X]: " and current prid as first param,

  \code
  FLOG(L_ERROR, "Simple log without params");
  FLOG(L_INFO, "Simple log with 1 param = %d", arg);
  FLOG(L_WARNING, "Simple log with 5 params = %d %d %d %d %d",
      arg1, arg2, arg3, arg4, arg5);

  // WILL NOT COMPILE - missing 1 argument.
  FLOG(L_DEBUG, "Simple log %d");
  // WILL NOT COMPILE - too many args.
  FLOG(L_DEBUG, "Simple log %d", arg1, arg2);
  \endcode
*/
#define FLOG(log_level, format, ...) \
    LOG(log_level, L_INFRA, "[%8.8X]: " format, get_prid(), ##__VA_ARGS__)

//! Macro for creating module log.
/*!
  \note Macro requires printf-like formatting.
  \note Macro will add "[%8.8X]: " and current resource_id as first param,

  \code
  MLOG(L_ERROR, "Simple log without params");
  MLOG(L_INFO, "Simple log with 1 param = %d", arg);
  MLOG(L_WARNING, "Simple log with 5 params = %d %d %d %d %d",
      arg1, arg2, arg3, arg4, arg5);

  // WILL NOT COMPILE - missing 1 argument.
  MLOG(L_DEBUG, "Simple log %d");
  // WILL NOT COMPILE - too many args.
  MLOG(L_DEBUG, "Simple log %d", arg1, arg2);
  \endcode
*/
#define MLOG(log_level, format, ...) \
    LOG(log_level, L_MODULE, "[%8.8X]: " format, GetResourceId(), ##__VA_ARGS__)

//! Macro for creating audio log (for audio modules).
#define ALOG(log_level, format, ...) \
    LOG(log_level, L_AUDIO, "[%8.8X]: " format, get_prid(), ##__VA_ARGS__)

//! Macro for creating gateway log.
#define GLOG(log_level, format, ...) \
    LOG(log_level, L_INFRA, "[%8.8X]: " format, GetBareNodeId(), ##__VA_ARGS__)

//! Macro for creating task log.
#define TLOG(log_level, format, ...) \
    LOG(log_level, L_INFRA, "[%8.8X]: " format, GetTaskId(), ##__VA_ARGS__)

//! Macro for creating pipeline log.
#define PLOG(log_level, format, ...) \
    LOG(log_level, L_INFRA, "[%8.8X]: " format, GetId(), ##__VA_ARGS__)

//! Macro for creating sensing log (for sensing modules).
#define SLOG(log_level, format, ...) \
    LOG(log_level, L_SENSING, "[%8.8X]: " format, get_prid(), ##__VA_ARGS__)

//! Macro for creating ULP log (for ULP infrastructure)
#define ULOG(log_level, format, ...) \
    LOG(log_level, L_ULP_INFRA, "[%8.8X]: " format, 0, ##__VA_ARGS__)

#define LOG_MESSAGE(log_level, format, resource_id, ...) \
    LOG(CONCAT(L_, log_level), L_INFRA, "[%8.8X]: " format, resource_id, ##__VA_ARGS__)

// DEPRECATED todo: remove
#define LIBRARY_MESSAGE(log_level, provider_id, log_entry, ...)                                                    \
    do                                                                                                             \
    {                                                                                                              \
        if (UNLIKELY(IS_LOG_LEVEL_ENABLED(log_level, L_MODULE)))                                                   \
        {                                                                                                          \
            logger_s* logger = srv_get_logger();                                                                   \
            logger->CONCAT(log_put_, __NARG__(__VA_ARGS__))(logger, CAST_ARG(uint32_t, log_entry, ##__VA_ARGS__)); \
        }                                                                                                          \
    } while (0)

#else
//! \cond HIDDEN_SYMBOL
#define LOG(...)
#define FLOG(...)
#define MLOG(...)
#define ALOG(...)
#define GLOG(...)
#define TLOG(...)
#define SLOG(...)
#define ULOG(...)
#define VLOG(...)
#define LOG_MESSAGE(...)
#define LIBRARY_MESSAGE(...)
//! \endcond HIDDEN_SYMBOL
#endif // #if SUPPORTED(LOGGER)

//! \cond HIDDEN_SYMBOL
#if defined(_DEBUG_PRINTF)
// USE PRINTF FROM STDIO LIBRARY FOR LOGGING TO CONSOLE
#define DEBUG_PRINT(format, ...) fw_printf(format "\n", ##__VA_ARGS__)
#elif defined(_DEBUG_SIM_PRINTF)
// USE PRINTF FROM SIM_PRINTF LIBRARY FOR LOGGING TO CONSOLE
#include <sim_utils/sim_printf.h>
#define DEBUG_PRINT(format, ...) fw_printf(format "\n", ##__VA_ARGS__)
#else
// DO NOT LOG INTO CONSOLE
#define DEBUG_PRINT(format, ...)
#endif
//! \endcond HIDDEN_SYMBOL

/*!
      @}
    @}
  @}
*/


// backward compatibility / legacy stuff
#define LOG_ENTRY_BASEFW 0

#if defined(__KLOCWORK__)
#include "misc/klocwork_overrides.h"
#endif //defined(__KLOCWORK__)

EXTERN_C_END
#endif // KERNEL_LOG_H
