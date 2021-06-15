# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# this makefile is intended to generate a list of makefile rules which help to set a unique id
# for each source file involved in compilation of a module package
# Limitation : Only source files in the module makefile directory or below are considered.

# _BUILD_CORE_DIR indicates the path to directory of this current file
override _BUILD_CORE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

include $(_BUILD_CORE_DIR)stddef.mk
include $(_BUILD_CORE_DIR)error_functions.mk
include $(_BUILD_CORE_DIR)arithmetic.mk
include $(_BUILD_CORE_DIR)file_functions.mk


# check for variables requirements
#<--
$(call error_if_not_defined, \
  OUT_DIR \
  MODULE_SRC_DIRS \
  _QUALIFIED_OBJ_FILES \
  OUTPUT_FILE \
  PLATFORM )
#-->

include $(_BUILD_CORE_DIR)../build_settings/build_hosts/host.mk
include $(_BUILD_CORE_DIR)../build_settings/build_platforms/$(PLATFORM)_platform.mk

$(eval $(value $(PLATFORM)_platform))

override _MODULE_QUALIFIED_OBJ_FILES := $(foreach module_src_dir, $(MODULE_SRC_DIRS), $(filter $(module_src_dir)%,$(_QUALIFIED_OBJ_FILES)))
override _RELPATH_MODULE_QUALIFIED_OBJ_FILES := $(addprefix ./,$(call relativepath,$(abspath $(OUT_DIR)),$(call linuxpath,$(abspath $(_MODULE_QUALIFIED_OBJ_FILES))),_DUMMY))

# directive below is mandatory to avoid concurrent accesses in writing to file
.ONESHELL:

.PHONY: default
.default: create_files_id_file


override _i := $(value _0)
define file_id_recipe
  $(foreach obj_file,$(_RELPATH_MODULE_QUALIFIED_OBJ_FILES),\
    $(if $(call store,$(filter-out AFLAGS,$(call get_objfile_flags_name,$(obj_file))),_langflags),\
      $(eval override _i := $(call _add,_i,_1))$(call print,$(call unqualify_obj_file,$(obj_file)): $(_langflags) += -D__FILE_ID__=$(words $(value _i))) >> $(OUTPUT_FILE)$(_RETURN_)\
      $(call print,) >> $(OUTPUT_FILE)$(_RETURN_),)\
  )
endef #file_id_recipe


.PHONY: create_files_id_file
create_files_id_file:
	@$(call print,## Automatically generated file for enablement of LOG_MESSAGE in source code ##) > $(OUTPUT_FILE)
	@$(call print,) >> $(OUTPUT_FILE)
	@$(call print,) >> $(OUTPUT_FILE)
	@$(file_id_recipe)
