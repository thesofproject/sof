# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# _CURRENT_MAKEFILE indicates the relative path to this current file
override _CURRENT_MAKEFILE := $(lastword $(MAKEFILE_LIST))
# _CUSTOM_FIRMWARE_DIR indicates the relative path to directory of this current file
override _CUSTOM_FIRMWARE_DIR := $(dir $(_CURRENT_MAKEFILE))
# INTEL_ADSP_DIR indicates the path to the directory of the intel_adsp installation
# value can be overriden on commmand line
INTEL_ADSP_DIR := $(_CUSTOM_FIRMWARE_DIR)../../


# Define MANDATORY VARIABLES for generation of a custom firmware
#<--
# BASE_FIRMWARE_ELFFILE is the path to the base FW ELFFILE
# given default value is unrelevant and shall be overriden on command line
BASE_FIRMWARE_ELFFILE := ___TEMPLATE_FIELD_BASE_FIRMWARE_ELFFILE_LOWER_SNAKE_CASE___
# path to the top makefile file
TOP_MAKEFILE ?= $(_CURRENT_MAKEFILE)
# name of the custom firmware to build
CUSTOM_FIRMWARE_NAME :=  ___TEMPLATE_FIELD_CUSTOM_FIRMWARE_NAME_LOWER_SNAKE_CASE___
# list of modules to include in the loadable library
MODULES_LIST := ___TEMPLATE_FIELD_MODULES_LIST___


___TEMPLATE_FIELD_MODULES_MAKEFILE_SECTION___
#-->


include $(INTEL_ADSP_DIR)build_framework/custom_firmware.mk
