# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


#<--
# rule defines in this file is intended to generate the elf list file.
#-->

# directive below is mandatory to avoid concurrent accesses in writing to file
.ONESHELL:

# _BUILD_CORE_DIR indicates the path to directory of this current file
override _BUILD_CORE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

include $(_BUILD_CORE_DIR)stddef.mk
include $(_BUILD_CORE_DIR)error_functions.mk


# check for variables requirements
#<--
$(call error_if_not_defined, \
  HOST \
  OUTPUT_FILE \
  ELF_FILES_LIST \
)
#-->

include $(_BUILD_CORE_DIR)../build_settings/build_hosts/$(HOST)_host.mk
$(eval $(value $(HOST)_host))


.PHONY: default
default: $(OUTPUT_FILE)

#rule for building the elf list file
$(OUTPUT_FILE): $(ELF_FILES_LIST)
	$(call mkemptyfile,$@)
	$(foreach elf_file,$(ELF_FILES_LIST),\
		@$(call print,$(abspath $(elf_file))) >> $@$(_RETURN_)\
		@$(call print,$(abspath $(elf_file).memmap)) >> $@$(_RETURN_)\
		@$(call print,$(abspath $(elf_file).nm)) >> $@$(_RETURN_)\
	)
