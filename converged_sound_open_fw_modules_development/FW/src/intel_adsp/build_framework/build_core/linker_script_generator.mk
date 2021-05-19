# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# list of mandatory input variables for this file. To be set here.
#<--
# section page size used to align module's sections on a multiple of 1024 bytes
override _SECTIONS_PAGE_SIZE := 4096

# Alignment specific to module's text section
override _TEXT_SECTION_ALIGNMENT := 8
#-->

# template used to generate linker script memory header
define generate_memory_header_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* memory header of lds */) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print,MEMORY{) >> $(1)
$(foreach memory_class,$(2), \
	$(if $($(LOADABLE_LIBRARY_NAME)_$(memory_class)_CODE_BA), \
		$(call print,  $(memory_class)_code_seg   :        \
          org = $($(LOADABLE_LIBRARY_NAME)_$(memory_class)_CODE_BA)$(_COMMA_)        \
         len = $($(LOADABLE_LIBRARY_NAME)_$(memory_class)_CODE_SIZE)) >> $(1)
		$(call print,  $(memory_class)_bss_seg   :        \
          org = $($(LOADABLE_LIBRARY_NAME)_$(memory_class)_BSS_BA)$(_COMMA_)        \
         len = $($(LOADABLE_LIBRARY_NAME)_$(memory_class)_BSS_SIZE)) >> $(1) \
         , \
         $(call print,  $(memory_class)_seg   :        \
         org = $($(LOADABLE_LIBRARY_NAME)_$(memory_class)_BA)$(_COMMA_)        \
         len = $($(LOADABLE_LIBRARY_NAME)_$(memory_class)_SIZE)) >> $(1) \
     )
)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print,PHDRS{) >> $(1)
$(foreach memory_class,$(2),
	$(if $($(LOADABLE_LIBRARY_NAME)_$(memory_class)_CODE_BA), \
	  $(call print,  $(memory_class)_code_phdr    PT_LOAD;) >> $(1)
	  $(call print,  $(memory_class)_bss_phdr    PT_LOAD;) >> $(1)
	,\
  $(call print,  $(memory_class)_phdr    PT_LOAD;) >> $(1) \
  )
)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
endef #generate_memory_header_linker_script


# template used to generate linker script common text section
define generate_common_text_section_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* common code text section */) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print,PHDRS{) >> $(1)
$(call print, text_phdr PT_LOAD;) >> $(1)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print, SECTIONS{) >> $(1)
$(call print,     .common.text : ALIGN($(_SECTIONS_PAGE_SIZE)){) >> $(1)
$(call print,         _common_text_start = ABSOLUTE(.);) >> $(1)
$(call print,         *(.entry.text)) >> $(1)
$(call print,         *(.init.literal)) >> $(1)
$(call print,         KEEP(*(.init))) >> $(1)
$(call print,         *(.literal .text)) >> $(1)
$(call print,         *(.literal.* .text.*)) >> $(1)
$(call print,         *(.stub)) >> $(1)
$(call print,         *(.gnu.warning)) >> $(1)
$(call print,         *(.gnu.linkonce.literal*)) >> $(1)
$(call print,         *(.gnu.linkonce.t.*.literal*)) >> $(1)
$(call print,         *(.gnu.linkonce.t*)) >> $(1)
$(call print,         *(.fini.literal)) >> $(1)
$(call print,         KEEP(*(.fini))) >> $(1)
$(call print,         *(.gnu.version)) >> $(1)
$(call print,         _common_text_end = ABSOLUTE(.);) >> $(1)
$(if $($(LOADABLE_LIBRARY_NAME)_$(2)_CODE_BA), \
$(call print,     } $(escape)>$(2)_code_seg AT$(escape)>$(3)_code_seg :text_phdr) >> $(1)
, \
$(call print,     } $(escape)>$(2)_seg AT$(escape)>$(3)_seg :text_phdr) >> $(1)
)
$(call print, }) >> $(1)
endef #generate_common_text_section_linker_script


# template used to generate linker script common rodata section
#TODO: clean .rodata section rules
define generate_common_rodata_section_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* common code rodata section */) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print,PHDRS{) >> $(1)
$(call print, rodata_phdr PT_LOAD;) >> $(1)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print, SECTIONS{) >> $(1)
$(call print,     .common.rodata : ALIGN($(_SECTIONS_PAGE_SIZE)){) >> $(1)
$(call print,         _rodata_start = ABSOLUTE(.);) >> $(1)
$(call print,         *(.rodata)) >> $(1)
$(call print,         *(.rodata.*)) >> $(1)
$(call print,         *(.rodata1)) >> $(1)
$(call print,         __XT_EXCEPTION_TABLE__ = ABSOLUTE(.);) >> $(1)
$(call print,         KEEP (*(.xt_except_table))) >> $(1)
$(call print,         KEEP (*(.gcc_except_table))) >> $(1)
$(call print,         *(.gnu.linkonce.e.*)) >> $(1)
$(call print,         *(.gnu.version_r)) >> $(1)
$(call print,         KEEP (*(.eh_frame))) >> $(1)
$(call print,         /*  C++ constructor and destructor tables$(COMMA) properly ordered:  */) >> $(1)
$(call print,         KEEP (*crtbegin.o(.ctors))) >> $(1)
$(call print,         KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))) >> $(1)
$(call print,         KEEP (*(SORT(.ctors.*)))) >> $(1)
$(call print,         KEEP (*(.ctors))) >> $(1)
$(call print,         KEEP (*crtbegin.o(.dtors))) >> $(1)
$(call print,         KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))) >> $(1)
$(call print,         KEEP (*(SORT(.dtors.*)))) >> $(1)
$(call print,         KEEP (*(.dtors))) >> $(1)
$(call print,         /*  C++ exception handlers table:  */) >> $(1)
$(call print,         __XT_EXCEPTION_DESCS__ = ABSOLUTE(.);) >> $(1)
$(call print,         *(.xt_except_desc)) >> $(1)
$(call print,         *(.gnu.linkonce.h.*)) >> $(1)
$(call print,         __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);) >> $(1)
$(call print,         *(.xt_except_desc_end)) >> $(1)
$(call print,         *(.dynamic)) >> $(1)
$(call print,         *(.gnu.version_d)) >> $(1)
$(call print,         . = ALIGN(4);       /* this table MUST be 4-byte aligned */) >> $(1)
$(call print,         _rodata_end = ABSOLUTE(.);) >> $(1)
$(if $($(LOADABLE_LIBRARY_NAME)_$(2)_CODE_BA), \
$(call print,     } $(escape)>$(2)_code_seg AT$(escape)>$(3)_code_seg :rodata_phdr) >> $(1)
, \
$(call print,     } $(escape)>$(2)_seg AT$(escape)>$(3)_seg :rodata_phdr) >> $(1)
)
$(call print, }) >> $(1)
endef #generate_common_rodata_section_linker_script


# template used to generate linker script xt sections - required by debugger
define generate_xt_sections_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* xt sections */) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print, SECTIONS{) >> $(1)
$(call print,         .debug  0 :           { *(.debug) }) >> $(1)
$(call print,         .line  0 :            { *(.line) }) >> $(1)
$(call print,         .xtensa.info  0 :     { *(.xtensa.info) }) >> $(1)
$(call print,         .comment  0 :         { *(.comment) }) >> $(1)
$(call print,         .debug_ranges  0 :    { *(.debug_ranges) }) >> $(1)
$(call print,         .debug_srcinfo  0 :   { *(.debug_srcinfo) }) >> $(1)
$(call print,         .debug_sfnames  0 :   { *(.debug_sfnames) }) >> $(1)
$(call print,         .debug_aranges  0 :   { *(.debug_aranges) }) >> $(1)
$(call print,         .debug_pubnames  0 :  { *(.debug_pubnames) }) >> $(1)
$(call print,         .debug_info  0 :      { *(.debug_info) }) >> $(1)
$(call print,         .debug_abbrev  0 :    { *(.debug_abbrev) }) >> $(1)
$(call print,         .debug_line  0 :      { *(.debug_line) }) >> $(1)
$(call print,         .debug_frame  0 :     { *(.debug_frame) }) >> $(1)
$(call print,         .debug_str  0 :       { *(.debug_str) }) >> $(1)
$(call print,         .debug_loc  0 :       { *(.debug_loc) }) >> $(1)
$(call print,         .debug_macinfo  0 :   { *(.debug_macinfo) }) >> $(1)
$(call print,         .debug_weaknames  0 : { *(.debug_weaknames) }) >> $(1)
$(call print,         .debug_funcnames  0 : { *(.debug_funcnames) }) >> $(1)
$(call print,         .debug_typenames  0 : { *(.debug_typenames) }) >> $(1)
$(call print,         .debug_varnames  0 :  { *(.debug_varnames) }) >> $(1)
$(call print,         .xt.insn 0 :) >> $(1)
$(call print,         {) >> $(1)
$(call print,           KEEP (*(.xt.insn))) >> $(1)
$(call print,           KEEP (*(.gnu.linkonce.x.*))) >> $(1)
$(call print,         }) >> $(1)
$(call print,         .xt.prop 0 :) >> $(1)
$(call print,         {) >> $(1)
$(call print,           KEEP (*(.xt.prop))) >> $(1)
$(call print,           KEEP (*(.xt.prop.*))) >> $(1)
$(call print,           KEEP (*(.gnu.linkonce.prop.*))) >> $(1)
$(call print,         }) >> $(1)
$(call print,         .xt.lit 0 :) >> $(1)
$(call print,         {) >> $(1)
$(call print,           KEEP (*(.xt.lit))) >> $(1)
$(call print,           KEEP (*(.xt.lit.*))) >> $(1)
$(call print,           KEEP (*(.gnu.linkonce.p.*))) >> $(1)
$(call print,         }) >> $(1)
$(call print,         .xt.profile_range 0 :) >> $(1)
$(call print,         {) >> $(1)
$(call print,           KEEP (*(.xt.profile_range))) >> $(1)
$(call print,           KEEP (*(.gnu.linkonce.profile_range.*))) >> $(1)
$(call print,         }) >> $(1)
$(call print,         .xt.profile_ranges 0 :) >> $(1)
$(call print,         {) >> $(1)
$(call print,           KEEP (*(.xt.profile_ranges))) >> $(1)
$(call print,           KEEP (*(.gnu.linkonce.xt.profile_ranges.*))) >> $(1)
$(call print,         }) >> $(1)
$(call print,         .xt.profile_files 0 :) >> $(1)
$(call print,         {) >> $(1)
$(call print,           KEEP (*(.xt.profile_files))) >> $(1)
$(call print,           KEEP (*(.gnu.linkonce.xt.profile_files.*))) >> $(1)
$(call print,         }) >> $(1)
$(call print, }) >> $(1)
endef #generate_xt_sections_linker_script

# template used to generate linker script log buffers section
define generate_logbuffers_linker_script
$(call cp, "$(_BUILD_FRMK_DIR)linker_scripts/logger.ld", $(1))
endef #generate_guard_linker_script

# template used to generate linker script .guard section
define generate_guard_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* .guard section */) >> $(1)
$(call print,PHDRS{) >> $(1)
$(call print, guard_phdr PT_LOAD;) >> $(1)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print, SECTIONS{) >> $(1)
$(call print,     .$(LOADABLE_LIBRARY_NAME).guard : ALIGN($(_SECTIONS_PAGE_SIZE)){) >> $(1)
$(call print,         _guard_section_start = ABSOLUTE(.);) >> $(1)
$(call print,         *(.*)) >> $(1)
$(call print,         _guard_section_end = ABSOLUTE(.);) >> $(1)
$(if $($(LOADABLE_LIBRARY_NAME)_$(2)_CODE_BA), \
$(call print,     } $(escape)>$(2)_code_seg AT$(escape)>$(3)_code_seg :guard_phdr) >> $(1)
, \
$(call print,     } $(escape)>$(2)_seg AT$(escape)>$(3)_seg :guard_phdr) >> $(1)
)
$(call print, }) >> $(1)
$(call print,) >> $(1)
endef #generate_guard_linker_script


# template used to generate the module text sections linker script
define generate_module_text_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* text sections of $(1) $(3) $(2) $(4) */) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print,PHDRS{) >> $(1)
$(call print, $(2)_text_phdr PT_LOAD;) >> $(1)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print, SECTIONS{) >> $(1)
$(call print,     .$(2).text : ALIGN($(_SECTIONS_PAGE_SIZE)){) >> $(1)
$(call print,         _$(2)_text_start = ABSOLUTE(.);) >> $(1)
$(call print,         *(.$(2).buildinfo)) >> $(1)
$(call print,         *(.$(2).gnu.linkonce.literal.*)) >> $(1)
$(call print,         *(.$(2).gnu.linkonce.lit4)) >> $(1)
$(call print,         *(.$(2).literal)) >> $(1)
$(call print,         *(.$(2).literal.*)) >> $(1)
$(call print,         *(.$(2).gnu.linkonce.t*)) >> $(1)
$(call print,         *(.$(2).text)) >> $(1)
$(call print,         *(.$(2).text.*)) >> $(1)
$(call print,         *(.$(2).cmi.literal)) >> $(1)
$(call print,         _$(2)_text_end = ABSOLUTE(.);) >> $(1)
$(if $($(LOADABLE_LIBRARY_NAME)_$(3)_CODE_BA), \
$(call print,     } $(escape)>$(3)_code_seg AT$(escape)>$(4)_code_seg :$(2)_text_phdr) >> $(1)
, \
$(call print,     } $(escape)>$(3)_seg AT$(escape)>$(4)_seg :$(2)_text_phdr) >> $(1)
)
$(call print, }) >> $(1)
endef #generate_module_text_linker_script


# template used to generate the module cmi and text sections linker script
define generate_module_text_cmi_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* text sections of $(1) */) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print, EXTERN($(5))) >> $(1)
$(call print,PHDRS{) >> $(1)
$(call print, $(2)_text_phdr PT_LOAD;) >> $(1)
$(call print, $(2)_cmi_text_phdr PT_LOAD;) >> $(1)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print, SECTIONS{) >> $(1)
$(call print,     .$(2).text : ALIGN($(_SECTIONS_PAGE_SIZE)){) >> $(1)
$(call print,         _$(2)_text_start = ABSOLUTE(.);) >> $(1)
$(call print,         *(.$(2).buildinfo)) >> $(1)
$(call print,         *(.$(2).gnu.linkonce.literal.*)) >> $(1)
$(call print,         *(.$(2).gnu.linkonce.lit4)) >> $(1)
$(call print,         *(.$(2).literal)) >> $(1)
$(call print,         *(.$(2).literal.*)) >> $(1)
$(call print,         *(.$(2).gnu.linkonce.t*)) >> $(1)
$(call print,         *(.$(2).text)) >> $(1)
$(call print,         *(.$(2).text.*)) >> $(1)
$(call print,         *(.$(2).cmi.literal)) >> $(1)
$(call print,         _$(2)_text_end = ABSOLUTE(.);) >> $(1)
$(if $($(LOADABLE_LIBRARY_NAME)_$(3)_CODE_BA), \
$(call print,     } $(escape)>$(3)_code_seg AT$(escape)>$(4)_code_seg :$(2)_text_phdr) >> $(1)
, \
$(call print,     } $(escape)>$(3)_seg AT$(escape)>$(4)_seg :$(2)_text_phdr) >> $(1)
)
$(call print,) >> $(1)
$(call print,     .$(2).cmi.text : ALIGN($(_TEXT_SECTION_ALIGNMENT)){) >> $(1)
$(call print,         _$(2)_cmi_text_start = ABSOLUTE(.);) >> $(1)
$(call print,         *(.$(2).cmi.text)) >> $(1)
$(call print,         _$(2)_cmi_text_end = ABSOLUTE(.);) >> $(1)
$(if $($(LOADABLE_LIBRARY_NAME)_$(3)_CODE_BA), \
$(call print,     } $(escape)>$(3)_code_seg AT$(escape)>$(4)_code_seg :$(2)_cmi_text_phdr) >> $(1)
, \
$(call print,     } $(escape)>$(3)_seg AT$(escape)>$(4)_seg :$(2)_cmi_text_phdr) >> $(1)
)
$(call print, }) >> $(1)
endef #generate_module_text_cmi_linker_script


# template used to generate the module rodata sections linker script
define generate_module_rodata_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* rodata sections of $(1) */) >> $(1)
$(call print,PHDRS{) >> $(1)
$(call print, $(2)_rodata_phdr PT_LOAD;) >> $(1)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print, EXTERN($(3))) >> $(1)
$(call print,) >> $(1)
$(call print, SECTIONS{) >> $(1)
$(call print,     .$(2).rodata : ALIGN($(_SECTIONS_PAGE_SIZE)){) >> $(1)
$(call print,         _$(2)_rodata_start = ABSOLUTE(.);) >> $(1)
$(call print,         *(.$(2).gnu.linkonce.r.*)) >> $(1)
$(call print,         *(.$(2).rodata)) >> $(1)
$(call print,         *(.$(2).rodata.*)) >> $(1)
$(call print,         KEEP (*(.$(2).eh_frame))) >> $(1)
$(call print,         _$(2)_rodata_end = ABSOLUTE(.);) >> $(1)
$(if $($(LOADABLE_LIBRARY_NAME)_$(3)_CODE_BA), \
$(call print,     } $(escape)>$(3)_code_seg AT$(escape)>$(4)_code_seg :$(2)_rodata_phdr) >> $(1)
, \
$(call print,     } $(escape)>$(3)_seg AT$(escape)>$(4)_seg :$(2)_rodata_phdr) >> $(1)
)
$(call print,) >> $(1)
$(call print, }) >> $(1)
endef #generate_module_rodata_linker_script


# template used to generate the module bss linker script
define generate_module_bss_linker_script
$(call print,/* Automatically generated lsp file$(_COMMA_) suitable to build a */) > $(1)
$(call print,/* bss sectionss of $(1) */) >> $(1)
$(call print,PHDRS{) >> $(1)
$(call print, $(2)_bss_phdr PT_LOAD;) >> $(1)
$(call print,}) >> $(1)
$(call print,) >> $(1)
$(call print,) >> $(1)
$(call print, SECTIONS{) >> $(1)
$(call print, $(2)_max_instances = $(value $(2)_INSTANCES_COUNT);) >> $(1)
$(call print, $(2)_max_instances-1 = $(2)_max_instances - 1;) >> $(1)
$(call print,) >> $(1)
$(call print,     .$(2).noload (NOLOAD) : ALIGN($(_SECTIONS_PAGE_SIZE)){) >> $(1)
$(call print,         _$(2)_first_start = ABSOLUTE(.);) >> $(1)
$(call print,         *(.$(2).first)) >> $(1)
$(call print,         _$(2)_first_end = ABSOLUTE(.);) >> $(1)
$(call print,         _$(2)_next_start = ABSOLUTE(.);) >> $(1)
$(call print,         . += (_$(2)_first_end - _$(2)_first_start) * $(2)_max_instances-1;) >> $(1)
$(call print,         _$(2)_next_end = ABSOLUTE(.);) >> $(1)
$(call print,         *(.$(2).bss)) >> $(1)
$(call print,         *(.$(2).bss.*)) >> $(1)
$(if $($(LOADABLE_LIBRARY_NAME)_$(3)_CODE_BA), \
$(call print,     } $(escape)>$(3)_bss_seg AT$(escape)>$(4)_bss_seg :$(2)_bss_phdr) >> $(1)
, \
$(call print,     } $(escape)>$(3)_seg AT$(escape)>$(4)_seg :$(2)_bss_phdr) >> $(1)
)
$(call print,) >> $(1)
$(call print, }) >> $(1)
endef #generate_module_bss_linker_script
