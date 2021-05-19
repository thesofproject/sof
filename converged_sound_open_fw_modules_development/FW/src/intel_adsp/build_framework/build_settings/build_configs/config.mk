# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# list possible input variables for this file
#<--
EXTRA_CONFIG_DIRS ?=
VERBOSE ?=
#-->


# _BUILD_CONFIG_DIR indicates the path to directory of this current file
override _BUILD_CONFIG_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

#include error functions
include $(_BUILD_CONFIG_DIR)../../build_core/error_functions.mk
include $(_BUILD_CONFIG_DIR)../../build_settings/build_hosts/host.mk


# retrieve list of available building configurations
#<--
$(info scanning directories for available configuration files ...)
override _EXTRA_CONFIG_DIRS := $(call linuxpath,$(EXTRA_CONFIG_DIRS))
override _AVAILABLE_CONFIG_FILES := $(wildcard $(foreach config_dir,$(_EXTRA_CONFIG_DIRS),$(config_dir)*_config.mk) $(_BUILD_CONFIG_DIR)*_config.mk)
override _AVAILABLE_CONFIGS := $(patsubst %_config.mk,%,$(notdir $(_AVAILABLE_CONFIG_FILES)))

ifeq (1,$(VERBOSE))
  $(info --> Found following extra configuration path : $(_EXTRA_CONFIG_DIRS))
  $(info --> Found following configuration files : $(_AVAILABLE_CONFIG_FILES))
endif
$(info --> Detected following possible configurations : $(_AVAILABLE_CONFIGS))
#-->

get_config_file = $(call error_if_not_subset,$1,_AVAILABLE_CONFIGS)$(firstword $(filter-out $(patsubst %$1_config.mk,,$(_AVAILABLE_CONFIG_FILES)),$(_AVAILABLE_CONFIG_FILES)))


# list possible input variables/functions for this file
#<--
get_config_file ?=
_AVAILABLE_CONFIGS ?=
_AVAILABLE_CONFIG_FILES ?=
#-->
