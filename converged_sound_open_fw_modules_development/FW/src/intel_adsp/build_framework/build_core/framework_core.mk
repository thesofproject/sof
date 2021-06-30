# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# detect building host and provide host-dependent functions
#<--
include $(_BUILD_FRMK_DIR)build_settings/build_hosts/host.mk
#-->

# set default API version an provide API number getters
#<--
include $(_BUILD_FRMK_DIR)build_core/intel_adsp_version.mk
#-->



# if needed, retrieve the platform file getter and the list of supported platforms
#<--
ifndef PLATFORM_FILE
  # list of inputs variables for "platform.mk"
  EXTRA_PLATFORM_DIRS ?=
  VERBOSE ?=
  # "call" platform.mk
  include $(_BUILD_FRMK_DIR)build_settings/build_platforms/platform.mk
  # list of expected outputs from "platform.mk"
  $(call error_if_not_defined,get_platform_file)
  $(call error_if_not_defined,_AVAILABLE_PLATFORMS)
endif
#-->


# if needed, retrieve the toolschain file getter and the list of supported toolchains
#<--
ifndef TOOLSCHAIN_FILE
  # list of inputs variables for "toolschain.mk"
  EXTRA_TOOLSCHAIN_DIRS ?=
  VERBOSE ?=
  # "call" toolschain.mk
  include $(_BUILD_FRMK_DIR)build_settings/build_toolschains/toolschain.mk
  # list of expected outputs from "toolschain.mk"
  $(call error_if_not_defined,get_toolschain_file)
  $(call error_if_not_defined,_AVAILABLE_TOOLSCHAINS)
endif
#-->


# extract PLATFORM value from command line
#<--
ifndef _PLATFORM
  override _PARSED_PLATFORM := $(filter $(_AVAILABLE_PLATFORMS),$(_MAKECMDGOALS))
  $(call error_if_not_single_value,$(_PARSED_PLATFORM))
  ifneq (,$(_PARSED_PLATFORM))
    override _PLATFORM := $(_PARSED_PLATFORM)
  else
    $(info No valid platform value found in command line ... Default one will be used)
  endif
endif
override _MAKECMDGOALS := $(filter-out $(_PLATFORM),$(_MAKECMDGOALS))
#-->


# set building settings over _PLATFORM and TOOLSCHAIN values
#<--
# set default value for target platform
DEFAULT_PLATFORM ?= x86-cygwin
override _PLATFORM ?= $(DEFAULT_PLATFORM)
# set platform file value
PLATFORM_FILE ?= $(call get_platform_file,$(_PLATFORM))
# if exists, include platform file if exists
ifneq (,$(wildcard $(PLATFORM_FILE)))
  include $(PLATFORM_FILE)
endif
$(call error_if_not_defined,$(_PLATFORM)_platform)
$(eval $(value $(_PLATFORM)_platform))
# following the "eval" of $(_PLATFORM)_platform a bunch of building options may have been set

# set default value for building toolschain
TOOLSCHAIN ?= gcc
# set toolschain file value
TOOLSCHAIN_FILE ?= $(call get_toolschain_file,$(TOOLSCHAIN))
# if exists, include the toolschain file
$(call error_if_not_subset,$(TOOLSCHAIN),$(_AVAILABLE_TOOLSCHAINS))
ifneq (,$(wildcard $(TOOLSCHAIN_FILE)))
  include $(TOOLSCHAIN_FILE)
endif
$(call error_if_not_defined,$(TOOLSCHAIN)_toolschain)
$(eval $(value $(TOOLSCHAIN)_toolschain))
# following the "eval" of $(TOOLSCHAIN)_toolschain) a bunch of building options may have been set
