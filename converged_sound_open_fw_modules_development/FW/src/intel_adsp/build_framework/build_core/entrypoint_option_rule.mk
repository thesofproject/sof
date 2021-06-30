# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# _BUILD_CORE_DIR indicates the path to directory of this current file
override _BUILD_CORE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))


include $(_BUILD_CORE_DIR)error_functions.mk

# list of mandatory input
#<--
# list of object files within to seek for the entrypoint name
#RELPATH_OBJ_FILES :=
# nm tool
#NM :=
# output file containing the result
# OUTPUT_FILE :=
#-->


# check for variables requirements
#<--
$(call error_if_not_defined, \
  NM \
  _RELPATH_OBJ_FILES \
  OUTPUT_FILE )

$(call error_if_file_not_found, $(OBJ_FILES))
#-->

include $(_BUILD_CORE_DIR)../build_settings/build_hosts/host.mk

override _ENTRYPOINT_NAME := $(filter %PackageEntryPoint,$(shell $(NM) $(_RELPATH_OBJ_FILES) -g))

$(call error_if_not_unique_value, \
  $(_ENTRYPOINT_NAME))

.PHONY: default
default:
	$(call print,-e $(_ENTRYPOINT_NAME))> $(OUTPUT_FILE)

.PHONY: dummy
dummy:
	$(call print,-e dummy_entry_point)> $(OUTPUT_FILE)
