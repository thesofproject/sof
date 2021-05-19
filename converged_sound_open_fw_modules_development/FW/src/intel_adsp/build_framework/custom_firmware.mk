# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# list of possible input variables for this file (and OPTIONS parameter in command line)
#<--
# path to the top makefile
#TOP_MAKEFILE :=
# custom name for the firmware being generated
#CUSTOM_FIRMWARE_NAME :=
# path to the ELF file of the base firmware
# BASE_FIRMWARE_ELFFILE :=
# list of module names part of this custom platform image (as defined by MODULE_FILENAME in each module makefile)
#MODULES_LIST :=
#-->


# _BUILD_FRMK_DIR indicates the path to the directory of this current file
override _BUILD_FRMK_DIR := $(subst \,/,$(dir $(lastword $(MAKEFILE_LIST))))

#include error functions
include $(_BUILD_FRMK_DIR)build_core/error_functions.mk


# check for variables requirements
#<--
$(call error_if_not_defined, \
  CUSTOM_FIRMWARE_NAME \
  BASE_FIRMWARE_ELFFILE \
  MODULES_LIST \
  TOP_MAKEFILE)


$(foreach module,$(MODULES_LIST),$(call error_if_not_defined,$(module)_PROPERTIES))
#-->

# set variables required for building the loadable library
LOADABLE_LIBRARY_NAME := $(CUSTOM_FIRMWARE_NAME)
LOADABLE_LIBRARY_AVAILABLE_MEMORY_SIZE := 0xFFFFFFFF
ELF_FILES_LIST = $(BASE_FIRMWARE_ELFFILE) $(LOADABLE_LIBRARY_ELFFILE)
# LOADABLE_LIBRARY_BASE_ADDRESS will actually be set later in custom_firmware_rules.mk (scheme to be improved)
LOADABLE_LIBRARY_BASE_ADDRESS :=

TARGET_TYPE := custom_firmware
include $(_BUILD_FRMK_DIR)loadable_library.mk
