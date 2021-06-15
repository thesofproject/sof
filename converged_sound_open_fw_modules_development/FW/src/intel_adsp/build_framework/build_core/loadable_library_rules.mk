# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

#include error functions
include $(_BUILD_FRMK_DIR)build_core/error_functions.mk


# check for variables requirements
# (should be placed in loadable_library.mk but would imply to rework custom_firmware.mk)
#<--
ifndef LOADABLE_LIBRARY_BASE_ADDRESS
ifndef $(LOADABLE_LIBRARY_NAME)_MEMORY_CLASSES
  $(error either "$(LOADABLE_LIBRARY_NAME)_MEMORY_CLASSES" or "LOADABLE_LIBRARY_BASE_ADDRESS" shall be defined !)
endif
endif

ifdef LOADABLE_LIBRARY_BASE_ADDRESS
  $(call error_if_not_defined, \
    LOADABLE_LIBRARY_BASE_ADDRESS \
    LOADABLE_LIBRARY_BASE_ADDRESS_LMA \
    LOADABLE_LIBRARY_AVAILABLE_MEMORY_SIZE \
  )

  $(LOADABLE_LIBRARY_NAME)_MEMORY_CLASSES := $(_MEMORY_CLASS_DEFAULT_LMA) $(_MEMORY_CLASS_DEFAULT_VMA)
  $(LOADABLE_LIBRARY_NAME)_$(_MEMORY_CLASS_DEFAULT_VMA)_BA   := $(LOADABLE_LIBRARY_BASE_ADDRESS)
  $(LOADABLE_LIBRARY_NAME)_$(_MEMORY_CLASS_DEFAULT_VMA)_SIZE := $(LOADABLE_LIBRARY_AVAILABLE_MEMORY_SIZE)
  $(LOADABLE_LIBRARY_NAME)_$(_MEMORY_CLASS_DEFAULT_LMA)_BA   := $(LOADABLE_LIBRARY_BASE_ADDRESS_LMA)
  $(LOADABLE_LIBRARY_NAME)_$(_MEMORY_CLASS_DEFAULT_LMA)_SIZE := $(LOADABLE_LIBRARY_AVAILABLE_MEMORY_SIZE)

  # set logical and virtual memory class to default if not specified 
  $(foreach module,$(MODULES_LIST),\
    $(eval $(module)_LMA := $(_MEMORY_CLASS_DEFAULT_LMA))\
    $(eval $(module)_VMA := $(_MEMORY_CLASS_DEFAULT_VMA))\
  )

else
  $(call error_if_not_defined, \
    $(LOADABLE_LIBRARY_NAME)_MEMORY_CLASSES \
  )

  $(call error_if_not_subset,$($(LOADABLE_LIBRARY_NAME)_MEMORY_CLASSES),$(_MEMORY_CLASSES))

  $(foreach memory_class,$($(LOADABLE_LIBRARY_NAME)_MEMORY_CLASSES), \
  $(call error_if_not_defined, \
    $(LOADABLE_LIBRARY_NAME)_$(memory_class)_BA \
    $(LOADABLE_LIBRARY_NAME)_$(memory_class)_SIZE \
  ))

  $(foreach module,$(MODULES_LIST), \
  $(call error_if_not_defined, \
    $(module)_LMA \
    $(module)_VMA \
  ))
endif
#-->


# Use .ONESHELL directive to be sure of linker script file generation's atomicity.
# In this makefile some files are generated and require usage of this directive.
.ONESHELL:

.PHONY: default
default: build

.PHONY: build
build: build_cavs report_settings
	$(info --> building of the loadable library "$(LOADABLE_LIBRARY_NAME)" has succeeded <--)

build_cavs: $(LOADABLE_LIBRARY_ELFFILE) $(LOADABLE_LIBRARY_ELFFILE).memmap

.PHONY: clean
clean: clean_cavs report_settings
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE))
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE).memmap)
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE)_elf.lst)
	-$(call rm,$(dir $(LOADABLE_LIBRARY_ELFFILE))specs)
	-$(call rm,$(dir $(LOADABLE_LIBRARY_ELFFILE))ldscripts/elf32xtensa.x)
	-$(foreach module,$(MODULES_LIST),$(call rm,$(module)_text_linker_script.txt))
	-$(foreach module,$(MODULES_LIST),$(call rm,$(module)_rodata_linker_script.txt))
	-$(foreach module,$(MODULES_LIST),$(call rm,$(module)_bss_linker_script.txt))
ifeq (1, $(ELF_LOGGER_AVAILABLE))
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE)_logbuffers_linker_script.txt)
endif 
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE)_memory_header_linker_script.txt)
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE)_xt_linker_script.txt)
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE)_common_text_linker_script.txt)
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE)_common_rodata_linker_script.txt)
	-$(call rm,$(LOADABLE_LIBRARY_ELFFILE)_guard_linker_script.txt)
	-$(call rmdir,$(TMP_OUT_DIR))

.PHONY: report_settings
report_settings:
	@$(call print,OPTIONS : $(MAKEFLAGS)) > build_settings.txt
	@$(call print,GOAL : $(MAKECMDGOALS)) >> build_settings.txt
	@$(call print,detected PLATFORM : $(_PLATFORM)) >> build_settings.txt
	@$(call print,detected HOST : $(_HOST)) >> build_settings.txt
	@$(call print,detected CONFIG : $(CFG)) >> build_settings.txt
	@$(call print,XTENSA_CORE : $(XTENSA_CORE)) >> build_settings.txt
	@$(call print,VERBOSE=$(VERBOSE)) >> build_settings.txt
	@$(call print,ROOT_OUT_DIR=$(ROOT_OUT_DIR)) >> build_settings.txt
	@$(call print,EXTRA_PLATFORM_DIRS=$(EXTRA_PLATFORM_DIRS)) >> build_settings.txt
	@$(call print,EXTRA_TOOLSCHAIN_DIRS=$(EXTRA_TOOLSCHAIN_DIRS)) >> build_settings.txt
	@$(call print,TOOLSCHAIN=$(TOOLSCHAIN)) >> build_settings.txt
	@$(call print,LD=$(LD)) >> build_settings.txt
	@$(call print,LDFLAGS=$(LDFLAGS)) >> build_settings.txt
	@$(call print,_TOOLSCHAIN_DEPEND=$(_TOOLSCHAIN_DEPEND)) >> build_settings.txt
	$(info build settings reported in file "$(CURDIR)/build_settings.txt")

$(LOADABLE_LIBRARY_ELFFILE): $(LOADABLE_LIBRARY_ELFFILE)_memory_header_linker_script.txt
$(LOADABLE_LIBRARY_ELFFILE): $(LOADABLE_LIBRARY_ELFFILE)_common_text_linker_script.txt
$(LOADABLE_LIBRARY_ELFFILE): $(LOADABLE_LIBRARY_ELFFILE)_common_rodata_linker_script.txt
$(LOADABLE_LIBRARY_ELFFILE): $(LOADABLE_LIBRARY_ELFFILE)_xt_linker_script.txt
ifeq (1, $(ELF_LOGGER_AVAILABLE))
$(LOADABLE_LIBRARY_ELFFILE): $(LOADABLE_LIBRARY_ELFFILE)_logbuffers_linker_script.txt
endif
$(LOADABLE_LIBRARY_ELFFILE): $(LOADABLE_LIBRARY_ELFFILE)_guard_linker_script.txt
$(LOADABLE_LIBRARY_ELFFILE): $(foreach module,$(MODULES_LIST),$(module)_text_linker_script.txt)
$(LOADABLE_LIBRARY_ELFFILE): $(foreach module,$(MODULES_LIST),$(module)_rodata_linker_script.txt)
$(LOADABLE_LIBRARY_ELFFILE): $(foreach module,$(MODULES_LIST),$(module)_bss_linker_script.txt)
$(LOADABLE_LIBRARY_ELFFILE): $(dir $(LOADABLE_LIBRARY_ELFFILE))ldscripts/elf32xtensa.x $(dir $(LOADABLE_LIBRARY_ELFFILE))specs
$(LOADABLE_LIBRARY_ELFFILE): override _MODULE_FILES_LIST := $(foreach module,$(MODULES_LIST),$(value $(module)_FILE))
$(LOADABLE_LIBRARY_ELFFILE): $(foreach module,$(MODULES_LIST),$(value $(module)_FILE))
	$(LINK.o) -Wl,--start-group $(_MODULE_FILES_LIST) $(LOADLIBES) -Wl,--end-group -o $@  $(LDLIBS) -Wl,-Map,$@.map

%.memmap: %
	$(OBJDUMP) $(_XTENSA_CORE_FLAGS) -h $^ > $@

%.nm: %
	$(NM) $(_XTENSA_CORE_FLAGS) $^ > $@

$(LOADABLE_LIBRARY_ELFFILE)_memory_header_linker_script.txt: $(value $(LOADABLE_LIBRARY_ELFFILE)_FILE) $(TOP_MAKEFILE)
	$(info --> generating memory header linker script $@...)
	@$(call generate_memory_header_linker_script,$@,$($(LOADABLE_LIBRARY_NAME)_MEMORY_CLASSES))

$(LOADABLE_LIBRARY_ELFFILE)_common_text_linker_script.txt: $(value $(LOADABLE_LIBRARY_ELFFILE)_FILE) $(TOP_MAKEFILE)
	$(info --> generating common text section linker script $@...)
	@$(call generate_common_text_section_linker_script,$@,$(_MEMORY_CLASS_DEFAULT_VMA),$(_MEMORY_CLASS_DEFAULT_LMA))

$(LOADABLE_LIBRARY_ELFFILE)_common_rodata_linker_script.txt: $(value $(LOADABLE_LIBRARY_ELFFILE)_FILE) $(TOP_MAKEFILE)
	$(info --> generating common rodata section linker script $@...)
	@$(call generate_common_rodata_section_linker_script,$@,$(_MEMORY_CLASS_DEFAULT_VMA),$(_MEMORY_CLASS_DEFAULT_LMA))

$(LOADABLE_LIBRARY_ELFFILE)_xt_linker_script.txt: $(value $(LOADABLE_LIBRARY_ELFFILE)_FILE) $(TOP_MAKEFILE)
	$(info --> generating xt linker script $@...)
	@$(call generate_xt_sections_linker_script,$@)

ifeq (1, $(ELF_LOGGER_AVAILABLE))
$(LOADABLE_LIBRARY_ELFFILE)_logbuffers_linker_script.txt: $(value $(LOADABLE_LIBRARY_ELFFILE)_FILE) $(TOP_MAKEFILE)
	$(info --> generating logbuffers section linker script $@...)
	@$(call generate_logbuffers_linker_script,$@,$(_MEMORY_CLASS_DEFAULT_VMA),$(_MEMORY_CLASS_DEFAULT_LMA))
endif

$(LOADABLE_LIBRARY_ELFFILE)_guard_linker_script.txt: $(value $(LOADABLE_LIBRARY_ELFFILE)_FILE) $(TOP_MAKEFILE)
	$(info --> generating guard section linker script $@...)
	@$(call generate_guard_linker_script,$@,$(_MEMORY_CLASS_DEFAULT_VMA),$(_MEMORY_CLASS_DEFAULT_LMA))


$(dir $(LOADABLE_LIBRARY_ELFFILE))ldscripts/elf32xtensa.x: $(TOP_MAKEFILE) | $(dir $(LOADABLE_LIBRARY_ELFFILE))ldscripts/
	$(MAKE) -f $(_BUILD_FRMK_DIR)build_core/linker_script_rule.mk OUTPUT_FILE="$@" QUIET=true MODULES_LIST="$(MODULES_LIST)" LOADABLE_LIBRARY_FILE=$(LOADABLE_LIBRARY_ELFFILE) PLATFORM=$(_PLATFORM) SIMULATION=$(SIMULATION)

$(dir $(LOADABLE_LIBRARY_ELFFILE))ldscripts/:
	$(call mkdir,$@)

$(dir $(LOADABLE_LIBRARY_ELFFILE))specs:
	$(call mkemptyfile,$@)


include $(_BUILD_FRMK_DIR)build_core/linker_script_generator.mk

define module_text_linker_script_rule
$(T)_text_linker_script.txt: module := $(T)
$(T)_text_linker_script.txt: $(value $(T)_FILE) $(TOP_MAKEFILE)
	$(info --> generating text linker script for $(module) module ...)
	@$(call generate_module_text_linker_script,$@,$(module),$($(module)_VMA),$($(module)_LMA))
endef # module_text_linker_script_rule

define module_text_cmi_linker_script_rule
$(T)_text_linker_script.txt: module := $(T)
$(T)_text_linker_script.txt: $(value $(T)_FILE) $(TOP_MAKEFILE)
	$(info --> generating text and cmi linker script for $(module) module ...)
	@$(call generate_module_text_cmi_linker_script,$@,$(module),$($(module)_VMA),$($(module)_LMA),$(filter %PackageEntryPoint, $(shell $(NM) $(_XTENSA_CORE_FLAGS) $(value $(module)_FILE))))
endef # module_text_cmi_linker_script_rule

define module_rodata_linker_script_rule
$(T)_rodata_linker_script.txt: module := $(T)
$(T)_rodata_linker_script.txt: $(value $(T)_FILE) $(TOP_MAKEFILE)
	$(info --> generating rodata linker script for $(module) module ...)
	@$(call generate_module_rodata_linker_script,$@,$(module),$($(module)_VMA),$($(module)_LMA))
endef # module_rodata_linker_script_rule

define module_bss_linker_script_rule
$(T)_bss_linker_script.txt: module := $(T)
$(T)_bss_linker_script.txt: $(value $(T)_FILE) $(TOP_MAKEFILE)
	$(info --> generating bss linker script for $(module) module ...)
	@$(call generate_module_bss_linker_script,$@,$(module),$($(module)_VMA),$($(module)_LMA))
endef # module_bss_linker_script_rule


include $(_BUILD_FRMK_DIR)build_core/arithmetic.mk
filter_modules_zero_instance_count = $(foreach T,$(MODULES_LIST),$(if $(call compare_eq,_$($(T)_INSTANCES_COUNT),_0),$(T)))

$(foreach T,$(call filter_modules_zero_instance_count),$(eval $(value module_text_linker_script_rule)))
$(foreach T,$(filter-out $(call filter_modules_zero_instance_count),$(MODULES_LIST)),$(eval $(value module_text_cmi_linker_script_rule)))
$(foreach T,$(MODULES_LIST),$(eval $(value module_rodata_linker_script_rule)))
$(foreach T,$(MODULES_LIST),$(eval $(value module_bss_linker_script_rule)))

# adjust _MAKECMDGOALS value before passing it to fw_binary.mk
override _MAKECMDGOALS := $(subst build,build_cavs,$(_MAKECMDGOALS))
override _MAKECMDGOALS := $(subst clean,clean_cavs,$(_MAKECMDGOALS))
#include makefile for building of a cAVS FW binary
include $(_BUILD_FRMK_DIR)build_core/fw_binary.mk
