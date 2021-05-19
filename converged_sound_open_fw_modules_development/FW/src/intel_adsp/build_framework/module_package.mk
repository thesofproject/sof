# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# default root directory of the output files
ifneq (,$(OUT_DIR))
  ROOT_OUT_DIR ?= $(OUT_DIR)../../
else
  ROOT_OUT_DIR ?= out/
endif


# list of possible input variables for this file (and OPTIONS parameter in command line)
#<--
# path to the top makefile
#TOP_MAKEFILE :=
# name of the processing module
#MODULE_FILENAME :=
# list of source files directories
#SRC_DIRS :=
# list of the file paths for static libraries required for module building
#STATIC_LIBRARIES :=
# list of extra directories to look for configuration files
#EXTRA_CONFIG_DIRS :=
# list of extra directories to look for platform files
#EXTRA_PLATFORM_DIRS :=
# list of extra directories to look for toolschain files
#EXTRA_TOOLSCHAIN_DIRS :=
# indicates the toolschain to consider for building
#TOOLSCHAIN =
# indicates to print verbose trace message of the building process
#VERBOSE :=
# indicates path to custom assembly include files (-I automatically appended in final ASFLAGS)
#AS_INCLUDES :=
# indicates path to custom C include files (-I automatically appended in final CFLAGS)
#C_INCLUDES :=
# indicates path to custom C++ include files (-I automatically appended in final CXXFLAGS)
#CXX_INCLUDES :=
# indicates assembly defines  (-D automatically appended in final ASFLAGS)
#AS_DEFINES :=
# indicates C defines  (-D automatically appended in final CFLAGS)
#C_DEFINES :=
# indicates C++ defines (-D automatically appended in final CXXFLAGS)
#CXX_DEFINES :=
# indicates some extra flags to append at end of CFLAGS
#EXTRA_CFLAGS :=
# indicates some extra flags to append at end of CXXFLAGS
#EXTRA_CXXFLAGS :=
# indicates some extra flags to append at end of ASFLAGS
#EXTRA_ASFLAGS :=
# indicates some extra flags to append at end of LDFLAGS
#EXTRA_LDFLAGS :=
# list of extra source files
#EXTRA_SRC_FILES :=
# prevent support of xtensa cstub for the given XTENSA_CORE value
#DISABLE_XTENSA_CSTUB ?= 1
# enforce usage of the given configuration file
#CONFIG_FILE =
#enforce usage of the given platform file
#PLATFORM_FILE =
#enforce usage of the given toolschain file
#TOOLSCHAIN_FILE =
#automate the definition of a unique __FILE_ID__ for source file compilation
#AUTO__FILE_ID__ ?= 1
#indicate if the module package is a "common module" (not instantiable)
#IS_COMMON_MODULE ?= 0
#indicate if the module is self-contained
#IS_SELF_CONTAINED ?= 0
#disable check of empty .data section
#ALLOW_DATA_SECTION ?= 0
#-->


# _BUILD_FRMK_DIR indicates the path to the directory of this current file
override _BUILD_FRMK_DIR := $(subst \,/,$(dir $(lastword $(MAKEFILE_LIST))))

#include error functions
include $(_BUILD_FRMK_DIR)build_core/error_functions.mk
#include file manipulation functions
include $(_BUILD_FRMK_DIR)build_core/file_functions.mk
# note that file_functions.mk will set default values for CPP_FILE_EXTENSIONS, C_FILE_EXTENSIONS and ASM_FILE_EXTENSIONS variables


# check for variables requirements
#<--
$(call error_if_not_defined, \
  MODULE_FILENAME \
  SRC_DIRS \
  TOP_MAKEFILE \
  CPP_FILE_EXTENSIONS \
  C_FILE_EXTENSIONS \
  ASM_FILE_EXTENSIONS)
#-->

# if needed, retrieve configuration file getter and the list of available configurations
#<--
ifndef CONFIG_FILE
  # list of inputs variables for "config.mk"
  EXTRA_CONFIG_DIRS ?=
  VERBOSE ?=
  # "call" config.mk
  include $(_BUILD_FRMK_DIR)build_settings/build_configs/config.mk
  # list of expected outputs from "config.mk"
  $(call error_if_not_defined,get_config_file)
  $(call error_if_not_defined,_AVAILABLE_CONFIGS)
endif
#-->

# define "local variable" for MAKECMDGOALS
override _MAKECMDGOALS := $(MAKECMDGOALS)

# extract CONFIGuration value from command line
#<--
ifndef _CONFIG
  override _PARSED_CONFIG := $(strip $(filter $(_AVAILABLE_CONFIGS),$(_MAKECMDGOALS)))
  $(call error_if_not_single_value,$(_PARSED_CONFIG))
  ifneq (,$(_PARSED_CONFIG))
    override _CONFIG := $(_PARSED_CONFIG)
  else
    $(info No valid configuration value found in command line ... Default one will be used)
  endif
endif
override _MAKECMDGOALS := $(filter-out $(_CONFIG),$(MAKECMDGOALS))
ifeq (,$(_MAKECMDGOALS))
  override _MAKECMDGOALS := default
endif
#-->

# set building settings over _CONFIG values
#<--
# set default value for building configuration
DEFAULT_CONFIG ?= release
override _CONFIG ?= $(DEFAULT_CONFIG)
# set config file value
CONFIG_FILE ?= $(call get_config_file,$(_CONFIG))
# if exists, include the config file
ifneq (,$(wildcard $(CONFIG_FILE)))
  include $(CONFIG_FILE)
endif
$(call error_if_not_defined,$(_CONFIG)_config)
$(eval $(value $(_CONFIG)_config))
# following the "eval" of $(_CONFIG)_config a bunch of building options may have been set
#-->


include $(_BUILD_FRMK_DIR)build_core/framework_core.mk
ifneq (1,$(_REENTRANCE_BARRIER))
  $(info --> Using "$(TOOLSCHAIN)" toolschain for module generation in "$(_CONFIG)" config and targetted to "$(_PLATFORM)" platform)
  $(info --> ...)
endif

# check for variables requirements
#<--
# if (_MAKECMDGOALS value is strictly worth "build" or "default")
ifeq (,$(filter-out build default,$(_MAKECMDGOALS)))
  $(call error_if_not_defined, \
    MODULE_PASS_BUFFER_SIZE \
    _MAJOR_API_VERSION_NUMBER \
    _MIDDLE_API_VERSION_NUMBER \
    _MINOR_API_VERSION_NUMBER \
  )
endif
#-->

# Set default values for remaining variables
#<--
# default value for output file suffix (might have been already set in CONFIG, PLATFORM or TOOLSCHAIN files)
# ".dlm" stands for Dynamically Loadable aDSP processing Module
override _OUTPUT_FILE_SUFFIX ?= .dlm.a
# set default value for search mask on source files
SRC_FILE_PATTERNS ?= $(addprefix *,$(CPP_FILE_EXTENSIONS) $(C_FILE_EXTENSIONS) $(ASM_FILE_EXTENSIONS))
# set the list of some precompiled object files to include in the build
EXTRA_OBJ_FILES ?=
# set the list of source files to exclude in the compilation
EXCLUDED_SRC_FILES ?=
# set the list of file paths for static libraries required for module building
STATIC_LIBRARIES ?=

CFLAGS += $(sort \
  -I$(_BUILD_FRMK_DIR)../include \
  -I$(SRCDIR)/src/common \
  -I$(SRCDIR)/src/common/include \
  $(foreach include,$(C_INCLUDES),-I$(include)) \
  $(foreach define,$(C_DEFINES),-D$(define)) \
  $(EXTRA_CFLAGS))
ASFLAGS += $(sort \
  $(foreach include,$(AS_INCLUDES),-I$(include)) \
  $(foreach define,$(AS_DEFINES),-D$(define)) \
  $(EXTRA_ASFLAGS))
CXXFLAGS += $(sort \
  -DMODULE_PASS_BUFFER_SIZE=$(MODULE_PASS_BUFFER_SIZE) \
  -DMAJOR_IADSP_API_VERSION=$(_MAJOR_API_VERSION_NUMBER) \
  -DMIDDLE_IADSP_API_VERSION=$(_MIDDLE_API_VERSION_NUMBER) \
  -DMINOR_IADSP_API_VERSION=$(_MINOR_API_VERSION_NUMBER) \
  -fno-rtti \
  -fdata-sections \
  -fconserve-space \
  -fcommon \
  -Wformat \
  -Wno-invalid-offsetof \
  -Werror \
  -I$(_BUILD_FRMK_DIR)../include \
  -I$(SRCDIR)/src/common/fdk/include \
  -I$(SRCDIR)/src/common \
  -I$(SRCDIR)/src/common/internal/include \
  -I$(SRCDIR)/src/common/include \
  -I$(SRCDIR)/portable/architecture/xtensa_lx/include \
  $(foreach include,$(sort $(CXX_INCLUDES)),-I$(include)) \
  $(foreach define,$(sort $(CXX_DEFINES)),-D$(define)) \
  $(EXTRA_CXXFLAGS))

ifneq (xtensa, $(TOOLSCHAIN))
LDFLAGS += -T"$(_BUILD_FRMK_DIR)linker_scripts/ldscripts/elf32xtensa.x"
endif
LDFLAGS += $(EXTRA_LDFLAGS)
LDFLAGS += $(foreach dir,$(sort $(dir $(STATIC_LIBRARIES))),-L$(dir))
LDLIBS += $(foreach libname,$(notdir $(STATIC_LIBRARIES)),-l:$(libname))
AUTO__FILE_ID__ ?= 1
IS_COMMON_MODULE ?= 0
IS_SELF_CONTAINED ?= 0
ALLOW_DATA_SECTION ?= 0
_MODULE_ENTRY_POINT_FILENAME := $(MODULE_FILENAME)_entrypoint_option.txt
#-->


# define the usage message
#<--
override define _USAGE_MESSAGE
  USAGE DESCRIPTION
  -----------------
  getting usage: make usage
    display this message
  standard usage: make [<TARGET_PLATFORM>] [<CONFIG>] [<GOAL>] [VERBOSE=1] [PARALLELMFLAGS="-j N"]
    GOAL indicates the operation of the building process to apply for the module package. Optional.
    CONFIG indicates the building configuration to apply. Optional.
    TARGET_PLATFORM indicates the target platform to consider for the building process. Optional.
    VERBOSE=1 add verbose building traces in console. Optional
    PARALLELMFLAGS="-j N", N indicates the number of parallel jobs. Optional workaround for Windows gnu make. by default builing process is executed in one single job.
      On windows "-j N" option is not passed downward correctly to sub-make process, please use PARALLELMFLAGS as a workaround
    available TARGET_PLATFORM values:
      If not set the target platform is the building host.
      $(_AVAILABLE_PLATFORMS)
    available CONFIG values:
      If not set the configuration considered is "$(DEFAULT_CONFIG)"
      $(_AVAILABLE_CONFIGS)
    available GOAL values:
      build -- generate the module package for the combination of TARGET_PLATFORM and CONFIG values. This is the default goal value.
      clean -- clean the output directory for the combination of TARGET_PLATFORM and CONFIG values
      usage|help -- display this message
    example:
      make -- build the module package in "$(DEFAULT_CONFIG)" configuration and targetted for the building host
      make usage -- display this message
      make clean -- clean the output directory for the "$(DEFAULT_CONFIG)" (default) configuration of the "$(DEFAULT_PLATFORM)" (default) platform
      make debug -- build the module package in "debug" configuration for the "$(DEFAULT_PLATFORM)" (default) platform
      make broxton -- build the module package in  "$(DEFAULT_CONFIG)" (default) configuration for the "broxton" platform
      make broxton debug -- build the module package in "debug" configuration for the broxton platform
      make broxton release clean -- clean the output directory for the debug configuration of the broxton platform

  advanced usage: make [<OPTIONS>] [<TARGET_PLATFORM>] [<CONFIG>] [<GOAL>] [VERBOSE=1]  [PARALLELMFLAGS="-j N"]
      OPTIONS is a list of variables assignments which control the building process.
      Those variables are usually automatically assigned over TARGET_PLATFORM, CONFIG and GOAL values.
      OPTIONS allows to override some of the default variable assignments.
      Please look at "build_framework/module_package.mk" for the list of variables which can be set.
    example:
      make TOOLSCHAIN=gcc build -- build the module package in "$(DEFAULT_CONFIG)" (default) configuration for the "$(DEFAULT_PLATFORM)" (default) platform
                                    and forcing the using of the GNU-C toolschain
endef
#-->


# define the output directory & file name
OUT_DIR ?= $(ROOT_OUT_DIR)$(_PLATFORM)/$(_CONFIG)/
override _OUTPUT_PACKAGE_FILE := $(MODULE_FILENAME)$(_OUTPUT_FILE_SUFFIX)

# validate goals before executing rules
# <--
override _VALID_GOALS := \
 default \
 usage \
 build \
 clean \
 help \
 .d \
 .file_id \
 src_files.lst

override _INVALID_GOALS := $(filter-out $(_VALID_GOALS),$(_MAKECMDGOALS))

ifneq (,$(_INVALID_GOALS))
  $(error Invalid command line: following goals not supported "$(MAKECMDGOALS)" - for usage, please apply: make usage)
endif

ifdef _DEFERRED_ERROR
  # if (_MAKECMDGOALS value does not contain "usage" or "help" goal)
  ifeq (,$(filter usage help,$(_MAKECMDGOALS)))
    $(error $(_DEFERRED_ERROR))
  endif
endif

ifneq (1,$(_REENTRANCE_BARRIER))
  $(info --> Applying $(if $(_MAKECMDGOALS),"$(_MAKECMDGOALS)","build") goal.)
  $(info --> ...)
endif
#-->


# define the building rules
#<--
## recall itself if not currently in the target directory
ifneq ($(abspath $(OUT_DIR)),$(CURDIR))

  ifneq (1,$(_REENTRANCE_BARRIER))
    _REENTRANCE_BARRIER := 1
    $(info start building process inside the output directory $(OUT_DIR))
    override _EXPORTED_SETTINGS := \
      _CONFIG _PLATFORM _HOST TOOLSCHAIN _REENTRANCE_BARRIER

    export $(foreach param,$(_EXPORTED_SETTINGS),$(value param) )

    .PHONY: $(word 1,$(MAKECMDGOALS))
    default $(word 1,$(MAKECMDGOALS)): | $(OUT_DIR)
		$(MAKE) -C $(OUT_DIR) -f $(subst \,/,$(realpath $(TOP_MAKEFILE))) $(PARALLELMFLAGS) \
          $(filter-out w,$(subst j 1,,$(MAKEFLAGS))) $(_MAKECMDGOALS) OUT_DIR=./ \
          CONFIG_FILE=$(realpath $(CONFIG_FILE)) PLATFORM_FILE=$(realpath $(PLATFORM_FILE)) TOOLSCHAIN_FILE=$(realpath $(TOOLSCHAIN_FILE))

    .PHONY: $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
    $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS)):

    .PHONY:
    $(OUT_DIR):
		$(call mkdir,$@)
  else
    $(error building process stopped because of reentrance issue !)
  endif

else

  # if (_MAKECMDGOALS value does not contain "usage" or "help" goal)
  ifeq (,$(filter usage help,$(_MAKECMDGOALS)))
    # scan source files directories
    #<--
    $(info scanning source files directories for source files ...)
    # set list of source files
    override _SRC_DIRS += $(_BUILD_FRMK_DIR)../source/ $(SRC_DIRS)
    # _SRC_DIRS contains absolute paths for directories of the SRC_DIRS list.
    override _SRC_DIRS := $(call linuxpath,$(_SRC_DIRS))
    override _SRC_FILES += $(foreach dir,$(_SRC_DIRS),$(foreach file_pattern,$(SRC_FILE_PATTERNS),$(wildcard $(dir)$(file_pattern))))
    override _SRC_FILES := $(sort $(filter-out $(EXCLUDED_SRC_FILES),$(_SRC_FILES)) $(EXTRA_SRC_FILES))
    ifndef _SRC_FILES
      $(warning --> No source files found !)
    else
      $(info --> Found following source files in source directories : $(_SRC_FILES))
    endif
    #-->

    # define output files
    #<--
    # get_obj_files_list macro
    # return the list of object files
    # $1, boolean value ([true,false]) which indicates if object filename is qualified with the source file extension
    override get_obj_files_list = $(strip $(foreach file_extension,$(_SRC_FILE_EXTENSIONS),$(patsubst %$(file_extension),%$(if $(filter true,$1),$(file_extension),).o,$(filter %$(file_extension),$(_SRC_FILES)))) $(EXTRA_OBJ_FILES))

    override _SRC_FILE_EXTENSIONS := $(suffix $(SRC_FILE_PATTERNS))
    override _OBJ_FILES := $(call get_obj_files_list,false) $(_TOOLSCHAIN_OBJ_FILES)
    override _QUALIFIED_OBJ_FILES := $(call get_obj_files_list,true)
    # paths list of object files relative to the OUT_DIR directory
    override _RELPATH_OBJ_FILES := $(addprefix ./,$(call relativepath,$(abspath $(OUT_DIR)),$(call linuxpath,$(abspath $(_OBJ_FILES))),VPATH))
    #-->

    # select checks to perform
    #<--
    ifneq (1,$(IS_COMMON_MODULE))
      override _MODULE_REQUIREMENTS_CHECK += has_entrypoint

      ifneq (1,$(ALLOW_DATA_SECTION))
        override _MODULE_REQUIREMENTS_CHECK += $(_OUTPUT_PACKAGE_FILE).no_data
      endif
      ifeq (1,$(IS_SELF_CONTAINED))
        override _MODULE_REQUIREMENTS_CHECK += $(_OUTPUT_PACKAGE_FILE).no_undefined
      endif
    endif
    #-->

    # remove duplicates in VPATH
    VPATH := $(sort $(VPATH))

    # deleting systematically the build settings file
    $(shell $(call rm,build_settings.txt))

    # Note that module_package_rules.mk shall be included before common_rules.mk
    include $(_BUILD_FRMK_DIR)build_core/module_package_rules.mk

  else
    .PHONY: build clean $(_CONFIG) default
    build clean $(_CONFIG) default: ;
    
  endif

  include $(_BUILD_FRMK_DIR)build_core/common_rules.mk
endif
#-->
