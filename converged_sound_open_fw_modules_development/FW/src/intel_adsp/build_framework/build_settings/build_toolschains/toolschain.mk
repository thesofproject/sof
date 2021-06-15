# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# list possible input variables for this file
#<--
EXTRA_TOOLSCHAIN_DIRS ?=
VERBOSE ?=
#-->


# _BUILD_TOOLSCHAIN_DIR indicates the path to directory of this current file
override _BUILD_TOOLSCHAIN_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

#include error functions
include $(_BUILD_TOOLSCHAIN_DIR)../../build_core/error_functions.mk


# retrieve list of available toolschains
#<--
$(info scanning directories for available toolschain files ...)
override _AVAILABLE_TOOLSCHAIN_FILES := $(wildcard $(_BUILD_TOOLSCHAIN_DIR)*_toolschain.mk) $(foreach toolschain_dir,$(EXTRA_TOOLSCHAIN_DIRS),$(toolschain_dir)*_toolschain.mk)
override _AVAILABLE_TOOLSCHAINS := $(patsubst %_toolschain.mk,%,$(notdir $(_AVAILABLE_TOOLSCHAIN_FILES)))
$(call error_if_not_unique_value,_AVAILABLE_TOOLSCHAINS)

ifeq (1,$(VERBOSE))
  $(info --> Found following toolschain files : $(_AVAILABLE_TOOLSCHAIN_FILES))
endif
$(info --> Detected following possible toolschains : $(_AVAILABLE_TOOLSCHAINS))
#-->

get_toolschain_file = $(call error_if_not_subset,$1,_AVAILABLE_TOOLSCHAINS)$(filter-out $(patsubst %$1_toolschain.mk,,$(_AVAILABLE_TOOLSCHAIN_FILES)),$(_AVAILABLE_TOOLSCHAIN_FILES))

# list possible output variables/functions for this file
#<--
get_toolschain_file ?=
_AVAILABLE_TOOLSCHAINS ?=
_AVAILABLE_TOOLSCHAIN_FILES ?=
#-->
