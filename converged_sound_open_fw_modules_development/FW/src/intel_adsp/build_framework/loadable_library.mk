# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# default root directory of the output files
ifneq (,$(OUT_DIR))
  ROOT_OUT_DIR ?= $(OUT_DIR)../
else
  ROOT_OUT_DIR ?= out/
endif


# list of possible input variables for this file (and OPTIONS parameter in command line)
#<--
# path to the top makefile
#TOP_MAKEFILE :=
# custom name for the loadable library being generated
#LOADABLE_LIBRARY_NAME :=
# list of module names part of this loadable library (as defined by MODULE_FILENAME in each module makefile)
#MODULES_LIST :=
#-->


# _BUILD_FRMK_DIR indicates the path to the directory of this current file
override _BUILD_FRMK_DIR := $(subst \,/,$(dir $(lastword $(MAKEFILE_LIST))))

#include error functions
include $(_BUILD_FRMK_DIR)build_core/error_functions.mk


# check for variables requirements
#<--
$(call error_if_not_defined, \
  LOADABLE_LIBRARY_NAME \
  MODULES_LIST \
  TOP_MAKEFILE)


$(foreach module,$(MODULES_LIST),$(call error_if_not_defined, $(module)_PROPERTIES))
#-->


# define "local variable" for MAKECMDGOALS
override _MAKECMDGOALS := $(MAKECMDGOALS)
ifeq (,$(_MAKECMDGOALS))
  override _MAKECMDGOALS := default
endif


include $(_BUILD_FRMK_DIR)build_core/framework_core.mk
ifneq (1,$(_REENTRANCE_BARRIER))
  $(info --> Using "$(TOOLSCHAIN)" toolschain for custom platform generation based on "$(_PLATFORM)" platform)
  $(info --> ...)
endif

# evaluate modules properties
$(foreach module,$(MODULES_LIST),$(eval $(value $(module)_PROPERTIES)))

# check for variables requirements
#<--
# if (_MAKECMDGOALS value is stricly worth "build" or "default")
ifeq (,$(filter-out build default,$(_MAKECMDGOALS)))
  $(foreach module,$(MODULES_LIST),$(call error_if_not_defined, $(module)_FILE $(module)_INSTANCES_COUNT))

  $(call error_if_not_defined, \
    _MEMORY_CLASS_DEFAULT_VMA \
    _MEMORY_CLASS_DEFAULT_LMA \
  )
endif
#-->

# default value for output file suffix (might have been already set in CONFIG, PLATFORM or TOOLSCHAIN files)
FW_BINARY_PREFIX ?=
FW_BINARY_SUFFIX ?=
ifeq (xtensa, $(TOOLSCHAIN))
LDFLAGS += -mlsp="./"
else
LDFLAGS += -T"./ldscripts/elf32xtensa.x"
endif
LDFLAGS += -Wl,--no-undefined
LDFLAGS += -Wl,--unresolved-symbols=report-all
LDFLAGS += -Wl,--error-unresolved-symbols
LDFLAGS += -Wl,--gc-sections
ifeq (1,$(VERBOSE))
  LDFLAGS += -Wl,--print-gc-sections
  LDFLAGS += -Wl,--verbose
endif
LDFLAGS += $(foreach dir,$(sort $(dir $(STATIC_LIBRARIES))),-L$(dir))
LDLIBS += $(foreach libname,$(patsubst lib%.a,%,$(notdir $(STATIC_LIBRARIES))),-l$(libname))

# set default value for variables involved in fw_binary.mk
FW_BINARY_SHORTNAME := $(LOADABLE_LIBRARY_NAME)
ELF_FILES_LIST ?= $(LOADABLE_LIBRARY_ELFFILE)

# define the usage message
#<--
override define _USAGE_MESSAGE
  USAGE DESCRIPTION
  -----------------
  getting usage: make
    display this message
  standard usage: make [<TARGET_PLATFORM>] [<GOAL>] [VERBOSE=1] [PARALLELMFLAGS="-j N"]
    GOAL indicates the operation of the building process to apply for the loadable library. Optional.
    TARGET_PLATFORM indicates the target platform to consider for the building process. Optional.
    VERBOSE=1 add verbose building traces in console. Optional
    PARALLELMFLAGS="-j N", N indicates the number of parallel jobs. Optional workaround for Windows gnu make. by default builing process is executed in one single job.
	  On windows "-j N" option is not passed downward correctly to sub-make process, please use PARALLELMFLAGS as a workaround
    available TARGET_PLATFORM values:
      If not set the target platform is the building host.
      $(_AVAILABLE_PLATFORMS)
    available GOAL values:
      build -- generate the output target for the combination of TARGET_PLATFORM values. This is the default goal value.
      clean -- clean the output directory for the combination of TARGET_PLATFORM values
      usage|help -- display this message
    example:
      make -- build the release configuration targetted for the building host
	  make usage -- display this message
      make clean -- clean the output directory for the $(_DEFAULT_PLATFORM) (default) platform
      make broxton -- build the output target for the broxton platform
      make broxton clean -- clean the output directory for the broxton platform

  advanced usage: make [<TARGET_PLATFORM>] [<OPTIONS>] <GOAL> [VERBOSE=1]  [PARALLELMFLAGS="-j N"]
      OPTIONS is a list of variables assignments which control the building process.
      Those variables are usually automatically assigned over TARGET_PLATFORM, and GOAL values.
      OPTIONS allows to override some of the default variable assignments. Please look at "build_framework/loadable_library.mk" for the list of variables
    example:
      make TOOLSCHAIN=gcc build -- build the output target for the $(_DEFAULT_PLATFORM) (default) platform
                                    and forcing the using the GNU-C toolschain
endef
#-->


# define the output directory
CFG ?= release
OUT_DIR ?= $(ROOT_OUT_DIR)$(_PLATFORM)/$(CFG)/

# validate goals before executing rules
# <--
override _VALID_GOALS := \
 default \
 usage \
 build \
 report_settings \
 clean \
 help

override _INVALID_GOALS := $(filter-out $(_VALID_GOALS),$(_MAKECMDGOALS))

ifneq (,$(_INVALID_GOALS))
  $(error Invalid command line: following goals not supported "$(_INVALID_GOALS)" - for usage, please apply: make usage))
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
      _PLATFORM _HOST CFG TOOLSCHAINS _REENTRANCE_BARRIER

    export $(foreach param,$(_EXPORTED_SETTINGS),$(value param) )

    .PHONY: $(word 1,$(MAKECMDGOALS))
    default $(word 1,$(MAKECMDGOALS)): | $(OUT_DIR)
		$(MAKE) -C $(OUT_DIR) -f $(subst \,/,$(realpath $(TOP_MAKEFILE))) $(PARALLELMFLAGS) $(filter-out w,$(subst j 1,,$(MAKEFLAGS))) $(_MAKECMDGOALS) OUT_DIR=./ \
        PLATFORM_FILE=$(realpath $(PLATFORM_FILE)) TOOLSCHAIN_FILE=$(realpath $(TOOLSCHAIN_FILE))

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

    # generate output file
    #<--
    LOADABLE_LIBRARY_ELFFILE ?= $(FW_BINARY_PREFIX)$(LOADABLE_LIBRARY_NAME)$(FW_BINARY_SUFFIX)
    TARGET_TYPE ?= loadable_library

    include $(_BUILD_FRMK_DIR)build_core/intel_adsp_version.mk
    # Note that $(TARGET_TYPE)_rules.mk shall be included before common_rules.mk
    include $(_BUILD_FRMK_DIR)build_core/$(TARGET_TYPE)_rules.mk
    #-->

  else
    .PHONY: build clean default report_settings
    build clean default report_settings: ;

  endif

    include $(_BUILD_FRMK_DIR)build_core/common_rules.mk
endif
