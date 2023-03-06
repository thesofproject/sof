/*
 * loadable_processing_module.h
 *
 *  Created on: 07-03-2023
 *      Author: paweldox
 */

#ifndef _LOADABLE_PROCESSING_MODULE_H_
#define _LOADABLE_PROCESSING_MODULE_H_

#include "adsp_stddef.h"

#ifdef __XTENSA__
/*!
 * \def SECTION_ATTRIBUTE(sect_name)
 */
  #define SECTION_ATTRIBUTE(sect_name) __attribute__((section(sect_name)))
#else
/*!
 * \def SECTION_ATTRIBUTE(sect_name)
 * Applies section attribute when compiling with XTENSA toolchain
 * and does nothing otherwise.
 */
  #define SECTION_ATTRIBUTE(sect_name)
#endif

#define MODULE_ENTRYPOINT_SECTION SECTION_ATTRIBUTE(".cmi.text")
#define FIRST_MODULE_SECTION SECTION_ATTRIBUTE(".first")
#define BUILD_INFO_SECTION SECTION_ATTRIBUTE(".buildinfo")

/*!
 * \def STR(s)
 * intermediate macro for Stringification
 * \note internal purpose only. Consider to use XSTR(s) for Stringification.
 */
#define STR(s) #s
/*!
 * \def XSTR(s)
 * Stringification macro
 */
#define XSTR(s) STR(s)

/*!
 * \brief Type definition of the package entry point.
 * \note internal purpose.
 */
typedef int (ModulePackageEntryPoint)(uint32_t costam);


/*! \def MODULE_PACKAGE_ENTRY_POINT_NAME(MODULE)
 * Defines function name for the entry point of a given custom module class.
 * \note internal purpose.
 */
#define MODULE_PACKAGE_ENTRY_POINT_NAME(MODULE) \
    MODULE ## PackageEntryPoint

/*! \def MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE)
 * Defines variable name for the build info of a given custom module class.
 * \note internal purpose.
 */
#define MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE) \
    MODULE ## BuildInfo

/*! \def MODULE_PLACEHOLDER_NAME(MODULE)
 * Defines variable name for the first instance of module placeholder.
 * \note internal purpose.
 */
#define MODULE_PLACEHOLDER_NAME(MODULE) \
    MODULE ## Placeholder

/*! \def MODULE_PLACEHOLDER_LENGTH_NAME(MODULE)
 * Defines variable name for module placeholder length
 * \note internal purpose.
 */
#define MODULE_PLACEHOLDER_LENGTH_NAME(MODULE) \
    MODULE ## PlaceholderLength

/*! \def DECLARE_MODULE_PACKAGE_ENTRY_POINT(MODULE)
 * package entry point declaration.
 * \note internal purpose.
 */
#define DECLARE_MODULE_PACKAGE_ENTRY_POINT(MODULE) \
    ModulePackageEntryPoint MODULE_PACKAGE_ENTRY_POINT_NAME(MODULE) MODULE_ENTRYPOINT_SECTION;

/*! \def DECLARE_MODULE_PLACEHOLDER_NAME(MODULE)
 * package entry point declaration.
 * \note internal purpose.
 */
#define DECLARE_MODULE_PLACEHOLDER_NAME(MODULE) \
    enum{ MODULE_PLACEHOLDER_LENGTH_NAME(MODULE) = ((sizeof(MODULE)+MODULE_INSTANCE_ALIGNMENT-1) & (~(MODULE_INSTANCE_ALIGNMENT-1))) }; \
	{ intptr_t MODULE_PLACEHOLDER_NAME(MODULE)[(MODULE_PLACEHOLDER_LENGTH_NAME(MODULE)+sizeof(intptr_t)-1)/sizeof(intptr_t)] FIRST_MODULE_SECTION; }

/*! \def DEFINE_MODULE_PACKAGE_ENTRY_POINT(MODULE, MODULE_FACTORY)
 * package entry point definition.
 * \note internal purpose.
 */
#define DEFINE_MODULE_PACKAGE_ENTRY_POINT(MODULE) \
    volatile AdspBuildInfo MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE) BUILD_INFO_SECTION = \
    { \
        ADSP_BUILD_INFO_FORMAT, \
        {(   ((0x3FF&MAJOR_IADSP_API_VERSION)<<20) | \
            ((0x3FF&MIDDLE_IADSP_API_VERSION)<<10) | \
            ((0x3FF&MINOR_IADSP_API_VERSION)<<0) )} \
    }; \
    int MODULE_PACKAGE_ENTRY_POINT_NAME(MODULE)(void *mod_cfg, void *parent_ppl, void **mod_ptr) \
    { \
        (void) MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE).FORMAT; \
        (void) MODULE_PACKAGE_ENTRY_BUILD_INFO_NAME(MODULE).API_VERSION_NUMBER.full; \
        return loadable_module_main(mod_cfg, parent_ppl, mod_ptr); \
    }

#define DECLARE_LOADABLE_MODULE(MODULE) \
    DEFINE_MODULE_PACKAGE_ENTRY_POINT(MODULE)

/*
 * #define DECLARE_LOADABLE_MODULE(MODULE, MODULE_FACTORY) \
    DECLARE_MODULE_PLACEHOLDER_NAME(MODULE) \
    DECLARE_MODULE_PACKAGE_ENTRY_POINT(MODULE) \
    DEFINE_MODULE_PACKAGE_ENTRY_POINT(MODULE, MODULE_FACTORY)
 */

#endif /* _LOADABLE_PROCESSING_MODULE_H_ */
