# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


$(call error_if_file_not_found,$(BASE_FIRMWARE_ELFFILE))

override _BASE_FIRMWARE_SYMBOLS_FILE := $(CUSTOM_FIRMWARE_NAME)_symbols.txt

#generate symbols list file for the base FW
$(shell $(NM) $(_XTENSA_CORE_FLAGS) $(BASE_FIRMWARE_ELFFILE) > $(_BASE_FIRMWARE_SYMBOLS_FILE))
$(call error_if_file_not_found,$(_BASE_FIRMWARE_SYMBOLS_FILE))

#grep symbol "_extra_loadable_modules_start"
override _HEX_LOADABLE_MODULES_BASE_ADDRESS := $(word 1,$(shell $(call grep,_extra_loadable_modules_start,$(_BASE_FIRMWARE_SYMBOLS_FILE))))
$(call error_if_not_defined,_HEX_LOADABLE_MODULES_BASE_ADDRESS)

#set base address of the loadable lib
LOADABLE_LIBRARY_BASE_ADDRESS := 0x$(_HEX_LOADABLE_MODULES_BASE_ADDRESS)
LOADABLE_LIBRARY_BASE_ADDRESS_LMA := 0x$(_HEX_LOADABLE_MODULES_BASE_ADDRESS)

include $(_BUILD_FRMK_DIR)build_core/loadable_library_rules.mk

$(LOADABLE_LIBRARY_ELFFILE)_memory_header_linker_script.txt: $(_BASE_FIRMWARE_SYMBOLS_FILE)

.PHONY: clean_symbols_file
clean_symbols_file:
	-$(call rm,$(_BASE_FIRMWARE_SYMBOLS_FILE))

clean: clean_symbols_file
