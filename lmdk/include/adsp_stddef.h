// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.


#ifndef _ADSP_STDDEF_H_
#define _ADSP_STDDEF_H_


#include <stddef.h>
#include <stdint.h>

#if  defined(XTENSA_TOOLSCHAIN) || defined(__XCC__)
  #include <xtensa/config/tie.h>
#endif

#ifndef XCHAL_CP1_SA_ALIGN
  #define XCHAL_CP1_SA_ALIGN sizeof(intptr_t)
#endif


#ifdef __GNUC__
  #define ALIGNED(x) __attribute__((aligned((x))))
#else
  #define ALIGNED(x)
#endif

#define DCACHE_ALIGNED ALIGNED(64)

#ifdef __XTENSA__
  #define RESTRICT __restrict
#else
  #define RESTRICT
#endif

#define MODULE_INSTANCE_ALIGNMENT 4096

#ifndef MIN
#define MIN(a,b) ((a<b) ? a : b)
#endif
#ifndef MAX
#define MAX(a,b) ((a<b) ? b : a)
#endif

#ifdef __cplusplus
namespace intel_adsp
{
struct ModulePlaceholder {};
}
inline void* operator new(size_t size, intel_adsp::ModulePlaceholder* placeholder) throw()
{
    (void)size;
    return placeholder;
}
#endif //#ifdef __cplusplus


#define ADSP_BUILD_INFO_FORMAT 0

/*! \cond INTERNAL */
typedef union AdspApiVersion
{
    uint32_t full;
    struct
    {
        uint32_t minor    : 10;
        uint32_t middle   : 10;
        uint32_t major    : 10;
        uint32_t reserved : 2;
    } fields;
} AdspApiVersion;
/*! \endcond INTERNAL */

/*! \cond INTERNAL */
typedef const struct
{
    uint32_t FORMAT;
    AdspApiVersion API_VERSION_NUMBER;
} AdspBuildInfo;
/*! \endcond INTERNAL */

/*! Log level priority enumeration. */
typedef enum log_priority {
	/*! Critical message. */
	L_CRITICAL,
	/*! Error message. */
	L_ERROR,
	/*! High importance log level. */
	L_HIGH,
	/*! Warning message. */
	L_WARNING,
	/*! Medium importance log level. */
	L_MEDIUM,
	/*! Low importance log level. */
	L_LOW,
	/*! Information. */
	L_INFO,
	/*! Verbose message. */
	L_VERBOSE,
	L_DEBUG,
	L_MAX,
} AdspLogPriority,
	log_priority_e;
struct AdspLogHandle;
typedef struct AdspLogHandle AdspLogHandle;


#endif //_ADSP_STDDEF_H_
