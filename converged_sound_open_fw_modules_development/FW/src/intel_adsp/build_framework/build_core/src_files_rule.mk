# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# this makefile is intended to generate the list of source files involved in compilation
# in the format suitable for the "make_cache" tool


# _BUILD_CORE_DIR indicates the path to directory of this current file
override _BUILD_CORE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

include $(_BUILD_CORE_DIR)stddef.mk
include $(_BUILD_CORE_DIR)error_functions.mk
include $(_BUILD_CORE_DIR)arithmetic.mk



# check for variables requirements
#<--
$(call error_if_not_defined, \
  OUTPUT_FILE \
  _QUALIFIED_OBJ_FILES \
  MODULE_SRC_DIRS \
  PLATFORM )
#-->

include $(_BUILD_CORE_DIR)../build_settings/build_hosts/host.mk
include $(_BUILD_CORE_DIR)../build_settings/build_platforms/$(PLATFORM)_platform.mk

$(eval $(value $(PLATFORM)_platform))

# directive below is mandatory to avoid concurrent accesses in writing to file
.ONESHELL:

override _MODULE_SRC_FILES := $(foreach module_src_dir, $(MODULE_SRC_DIRS), $(patsubst %.o,%,$(filter $(module_src_dir)%,$(_QUALIFIED_OBJ_FILES))))

define src_file_recipe
  $(foreach src_file,$(_MODULE_SRC_FILES), $(call print,$(src_file)) >> $(OUTPUT_FILE)$(_RETURN_))
endef #src_file_recipe

.PHONY: default
default:
	@$(call print,$(words $(_MODULE_SRC_FILES))) > $(OUTPUT_FILE)
	@$(src_file_recipe)
