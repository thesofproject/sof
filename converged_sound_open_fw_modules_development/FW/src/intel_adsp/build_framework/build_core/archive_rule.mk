# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

#<--
# rule defines in this file is intended to produce an archive containing module object code.
#-->

# directive below is mandatory to avoid concurrent accesses in writing to file
.ONESHELL:

# _THIS_MAKEFILE indicates the path to directory of this current file
_THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))
_SELF_DIR := $(dir $(_THIS_MAKEFILE))


include $(_SELF_DIR)stddef.mk
include $(_SELF_DIR)error_functions.mk
include $(_SELF_DIR)../build_settings/build_hosts/host.mk


# check for variables requirements
# note that _RELPATH_OBJ_FILES is exported by module_package_rules.mk
#<--
$(call error_if_not_defined, \
  AR \
  OUTPUT_FILE \
  _RELPATH_OBJ_FILES \
)

STATIC_LIBRARIES ?=

override _ARCHIVE_TMP_DIR := $(DEFAULT_TMP_DIR)/tmp/
#-->

# Get content of archive - files list
archive_content = $(shell $(AR) t $(1))
archives_content = $(foreach arch,$(1),$(call archive_content,$(arch))$(_RETURN_))

# Unpack archive to current folder
archive_unpack = $(AR) x  $(call syspath,$(1))
# Unpack multipe archives to current folder
# LIMITATIONS: object names shall be unique over the overall static libraries
archives_unpack = $(foreach arch,$(1),cd $(_ARCHIVE_TMP_DIR) && $(call archive_unpack,$(abspath $(arch)))$(_RETURN_))


override _STATIC_LIBRARIES_OBJECTS := $(foreach obj,$(call archives_content,$(STATIC_LIBRARIES)),$(_ARCHIVE_TMP_DIR)$(obj))
# verify requirement on _STATIC_LIBRARIES_OBJECTS
$(call error_if_not_unique_value,$(_STATIC_LIBRARIES_OBJECTS))


.PHONY: default
default: build

# Note that if the module source defines an object file with same name as one of those in the static libraries,
# it will superseed the static library one.
# Warning : to observe the statement just above, the module objects shall be inserted in archive after static libraries ones.
.PHONY: build
build: $(_ARCHIVE_TMP_DIR) $(foreach obj,$(_STATIC_LIBRARIES_OBJECTS) $(_RELPATH_OBJ_FILES),$(OUTPUT_FILE)($(obj)))
	-$(call rm,$(_ARCHIVE_TMP_DIR)*)
	-$(call rmdir,$(_ARCHIVE_TMP_DIR))

# rule to update archive with a given file
$(OUTPUT_FILE)(%): %
	@$(AR) rscouU $@ $%

$(_STATIC_LIBRARIES_OBJECTS): unpack_archives

.PHONY: unpack_archives
unpack_archives: $(STATIC_LIBRARIES) $(_ARCHIVE_TMP_DIR)
	$(call archives_unpack,$(STATIC_LIBRARIES))

$(_ARCHIVE_TMP_DIR):
	-$(call mkdir,$(call syspath,$@))

.PHONY: clean
clean:
	-$(call rm,$(_ARCHIVE_TMP_DIR)*)
	-$(call rmdir,$(_ARCHIVE_TMP_DIR))
