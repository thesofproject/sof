# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# list possible input variables for this file
#<--
EXTRA_PLATFORM_DIRS ?=
VERBOSE ?=
#-->


# _BUILD_PLATFORM_DIR indicates the path to directory of this current file
override _BUILD_PLATFORM_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

#include error functions
include $(_BUILD_PLATFORM_DIR)../../build_core/error_functions.mk


# retrieve list of available platforms
#<--
$(info scanning directories for available platform files ...)
override _AVAILABLE_PLATFORM_FILES := $(wildcard $(_BUILD_PLATFORM_DIR)*_platform.mk) $(foreach platform_dir,$(EXTRA_PLATFORM_DIRS),$(platform_dir)*_platform.mk)
override _AVAILABLE_PLATFORMS := $(patsubst %_platform.mk,%,$(notdir $(_AVAILABLE_PLATFORM_FILES)))
$(call error_if_not_unique_value,_AVAILABLE_PLATFORMS)

ifeq (1,$(VERBOSE))
  $(info --> Found following platform files : $(_AVAILABLE_PLATFORM_FILES))
endif
$(info --> Detected following possible platforms : $(_AVAILABLE_PLATFORMS))
#-->

get_platform_file = $(call error_if_not_subset,$1,_AVAILABLE_PLATFORMS)$(filter-out $(patsubst %$1_platform.mk,,$(_AVAILABLE_PLATFORM_FILES)),$(_AVAILABLE_PLATFORM_FILES))


# list possible input variables/functions for this file
#<--
get_platform_file ?=
_AVAILABLE_PLATFORMS ?=
_AVAILABLE_PLATFORM_FILES ?=
#-->
