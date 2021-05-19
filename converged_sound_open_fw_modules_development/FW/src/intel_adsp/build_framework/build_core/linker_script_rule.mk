# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

#<--
# rule defines in this file is intended to generate the linker script file.
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
  OUTPUT_FILE \
  MODULES_LIST \
  PLATFORM \
  LOADABLE_LIBRARY_FILE \
)
#-->

include $(_BUILD_CORE_DIR)../build_settings/build_hosts/host.mk
include $(_BUILD_CORE_DIR)../build_settings/build_platforms/$(PLATFORM)_platform.mk

$(eval $(value $(PLATFORM)_platform))


#define recipe to append each $(MODULES_LIST) include value to $(OUTPUT_FILE)
define colocate_sections_per_module
  $(foreach module,$(MODULES_LIST),\
    $(call print,INCLUDE $(abspath $(module)_text_linker_script.txt))>> $(OUTPUT_FILE)$(_RETURN_)\
    $(call print,INCLUDE $(abspath $(module)_rodata_linker_script.txt))>> $(OUTPUT_FILE)$(_RETURN_)\
    $(call print,INCLUDE $(abspath $(module)_bss_linker_script.txt))>> $(OUTPUT_FILE)$(_RETURN_)\
  )
endef #colocate_sections_per_module


#define recipe to append each $(MODULES_LIST) include value to $(OUTPUT_FILE)
define colocate_sections_per_type
  $(foreach module,$(MODULES_LIST),\
    $(call print,INCLUDE $(abspath $(module)_text_linker_script.txt))>> $(OUTPUT_FILE)$(_RETURN_)\
    $(call print,INCLUDE $(abspath $(module)_rodata_linker_script.txt))>> $(OUTPUT_FILE)$(_RETURN_)\
  )
  $(foreach module,$(MODULES_LIST),$(call print,INCLUDE $(abspath $(module)_bss_linker_script.txt))>> $(OUTPUT_FILE)$(_RETURN_))
endef # colocate_sections_per_type

.PHONY: default
default:
	$(info --> generating linker script : $(OUTPUT_FILE)  ...)
	@$(call print,INCLUDE $(abspath $(LOADABLE_LIBRARY_FILE)_memory_header_linker_script.txt)) > $(OUTPUT_FILE)
ifeq (1, $(ELF_LOGGER_AVAILABLE))
	$(call print,INCLUDE $(abspath $(LOADABLE_LIBRARY_FILE)_logbuffers_linker_script.txt)) >> $(OUTPUT_FILE)
endif
	@$(call print,INCLUDE $(abspath $(LOADABLE_LIBRARY_FILE)_common_text_linker_script.txt)) >> $(OUTPUT_FILE)
	@$(call print,INCLUDE $(abspath $(LOADABLE_LIBRARY_FILE)_common_rodata_linker_script.txt)) >> $(OUTPUT_FILE)
ifeq (true,$(SIMULATION))
	@$(colocate_sections_per_type)
else
	@$($(FW_MEMORY_SECTIONS_LAYOUT))
endif
	@$(call print,INCLUDE $(abspath $(LOADABLE_LIBRARY_FILE)_xt_linker_script.txt)) >> $(OUTPUT_FILE)
	@$(call print,INCLUDE $(abspath $(LOADABLE_LIBRARY_FILE)_guard_linker_script.txt)) >> $(OUTPUT_FILE)
