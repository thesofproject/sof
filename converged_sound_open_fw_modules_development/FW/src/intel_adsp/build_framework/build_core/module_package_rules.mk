# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# Following variables are exported to avoid to pass them on command line when invoking "make" subprocess.
# Indeed limit of the command line length (8191 chars) on Windows host could be exceeded if passing such kind of variables in command line.
# Note that all those parameters are set in "module_package.mk" so all rules depending on those parameters shall at least depend on module_package.mk
export _SRC_FILES _SRC_DIRS _OBJ_FILES _QUALIFIED_OBJ_FILES _RELPATH_OBJ_FILES  LASTLIBS


.PHONY: default
default: build


.d: $(_SRC_FILES) $(MAKEFILE_LIST)
	$(info Generating compilation dependencies ".d" file ...)
	$(CXX) $(sort $(CXXFLAGS)) -MM $(_SRC_FILES) > $(call syspath,.d)

.file_id: $(_SRC_FILES) $(_BUILD_FRMK_DIR)build_core/files_id_rule.mk $(MAKEFILE_LIST)
	$(info Generating ".file_id" file ...)
	$(MAKE) -f $(_BUILD_FRMK_DIR)build_core/files_id_rule.mk \
      OUTPUT_FILE="$@" QUIET=true PLATFORM="$(_PLATFORM)" MODULE_SRC_DIRS="$(_SRC_FILES)" OUT_DIR="$(OUT_DIR)"

# if $(_MAKECMDGOALS) does contain default, build, .d or one of the files in $(_SRC_FILES)
ifneq (,$(filter default build .d _SRC_FILES,$(_MAKECMDGOALS)))
  # include makefile for definition of the objects dependencies
  -include .d
endif

ifeq (1,$(AUTO__FILE_ID__))
# if $(_MAKECMDGOALS) does contain default, build, .file_id or one of the files in $(_SRC_FILES)
ifneq (,$(filter default build .file_id _SRC_FILES,$(_MAKECMDGOALS)))
  # include makefile for definition of __FILE_ID__ parameter in source file
  -include .file_id
endif
endif

.PHONY: $(_CONFIG)
$(_CONFIG): build

.PHONY: build
build: build_settings.txt $(_OUTPUT_PACKAGE_FILE)
ifdef _SRC_FILES
  build: src_files.lst
endif

build: $(_MODULE_ENTRY_POINT_FILENAME) $(_MODULE_REQUIREMENTS_CHECK)

# Verify that the generated module package has an entry point
# (temporary solution until support for better scripting/shell language or dedicated tool)
.PHONY: has_entrypoint
has_entrypoint: $(_MODULE_ENTRY_POINT_FILENAME)
	$(info Verifying that module has a valid entrypoint ...)
	@$(call grep,PackageEntryPoint$$,$(_MODULE_ENTRY_POINT_FILENAME))

# In come cases when gc-sections is user linker throws exception
TO_REM = -Wl,--gc-sections
.INTERMEDIATE: $(_OUTPUT_PACKAGE_FILE).no_data
# Verify that the generated module package has no .data section
# (temporary solution until support for better scripting/shell language or dedicated tool)
$(_OUTPUT_PACKAGE_FILE).no_data: $(_MODULE_ENTRY_POINT_FILENAME) $(_RELPATH_OBJ_FILES)
	$(info Verifying that module .data section is empty ...)
	 $(filter-out $(TO_REM),$(LINK.o)) -Wl,-Ur $(_RELPATH_OBJ_FILES) $(LOADLIBES) $(LDLIBS) -o $@ @$(_MODULE_ENTRY_POINT_FILENAME)  $(LASTLIBS)

.INTERMEDIATE: $(_OUTPUT_PACKAGE_FILE).no_undefined
# Verify that the generated module package is self-contained
# (temporary solution until support for better scripting/shell language or dedicated tool)
$(_OUTPUT_PACKAGE_FILE).no_undefined: $(_OUTPUT_PACKAGE_FILE).no_data
	$(info Verifying that module is self-contained -no undefined symbol- ...)
	$(LINK.o) -Wl,--no-undefined $(_OUTPUT_PACKAGE_FILE).no_data -o $@


.PHONY: clean
clean: clean_tmp_archive | build_settings.txt
	-$(call rm,$(_OUTPUT_PACKAGE_FILE))
	-$(call rm,$(_OUTPUT_PACKAGE_FILE).i)
	-$(call rm,$(_RELPATH_OBJ_FILES))
	-$(call rm,$(_MODULE_ENTRY_POINT_FILENAME))
	-$(call rm,.d)
	-$(call rm,.file_id)
	-$(call rm,src_files.lst)
	-$(call rm,$(_OUTPUT_PACKAGE_FILE).no_data)
	-$(call rm,$(_OUTPUT_PACKAGE_FILE).no_undefined)

.PHONY: clean_tmp_archive
clean_tmp_archive: $(_BUILD_FRMK_DIR)build_core/archive_rule.mk
	$(MAKE) -f $(_BUILD_FRMK_DIR)build_core/archive_rule.mk \
      OUTPUT_FILE="-" AR="$(AR)" \
      STATIC_LIBRARIES="$(STATIC_LIBRARIES)" clean

build_settings.txt: $(MAKEFILE_LIST)
	@$(call print,OPTIONS : $(MAKEFLAGS)) > build_settings.txt
	@$(call print,GOAL : $(MAKECMDGOALS)) >> build_settings.txt
	@$(call print,detected CONFIG : $(_CONFIG)) >> build_settings.txt
	@$(call print,detected PLATFORM : $(_PLATFORM)) >> build_settings.txt
	@$(call print,detected HOST : $(_HOST)) >> build_settings.txt
	@$(call print,XTENSA_CORE : $(XTENSA_CORE)) >> build_settings.txt
	@$(call print,DISABLE_CSTUB_SUPPORT : $(DISABLE_CSTUB_SUPPORT)) >> build_settings.txt
	@$(call print,AS_INCLUDES=$(AS_INCLUDES)) >> build_settings.txt
	@$(call print,C_INCLUDES=$(C_INCLUDES)) >> build_settings.txt
	@$(call print,CXX_INCLUDES=$(sort $(CXX_INCLUDES))) >> build_settings.txt
	@$(call print,AS_DEFINES=$(AS_DEFINES)) >> build_settings.txt
	@$(call print,C_DEFINES=$(C_DEFINES)) >> build_settings.txt
	@$(call print,CXX_DEFINES=$(sort $(CXX_DEFINES))) >> build_settings.txt
	@$(call print,VERBOSE=$(VERBOSE)) >> build_settings.txt
	@$(call print,SRC_DIRS=$(SRC_DIRS)) >> build_settings.txt
	@$(call print,STATIC_LIBRARIES=$(STATIC_LIBRARIES)) >> build_settings.txt
	@$(call print,SRC_FILE_PATTERNS=$(SRC_FILE_PATTERNS)) >> build_settings.txt
	@$(call print,ROOT_OUT_DIR=$(ROOT_OUT_DIR)) >> build_settings.txt
	@$(call print,EXTRA_OBJ_FILES=$(EXTRA_OBJ_FILES)) >> build_settings.txt
	@$(call print,EXTRA_SRC_FILES=$(EXTRA_SRC_FILES)) >> build_settings.txt
	@$(call print,EXCLUDED_SRC_FILES=$(EXCLUDED_SRC_FILES)) >> build_settings.txt
	@$(call print,EXTRA_CONFIG_DIRS=$(EXTRA_CONFIG_DIRS)) >> build_settings.txt
	@$(call print,EXTRA_PLATFORM_DIRS=$(EXTRA_PLATFORM_DIRS)) >> build_settings.txt
	@$(call print,EXTRA_TOOLSCHAIN_DIRS=$(EXTRA_TOOLSCHAIN_DIRS)) >> build_settings.txt
	@$(call print,TOOLSCHAIN=$(TOOLSCHAIN)) >> build_settings.txt
	@$(call print,CC=$(CC)) >> build_settings.txt
	@$(call print,CXX=$(CXX)) >> build_settings.txt
	@$(call print,AS=$(AS)) >> build_settings.txt
	@$(call print,LD=$(LD)) >> build_settings.txt
	@$(call print,AR=$(AR)) >> build_settings.txt
	@$(call print,CFLAGS=$(sort $(CFLAGS))) >> build_settings.txt
	@$(call print,CXXFLAGS=$(sort $(CXXFLAGS))) >> build_settings.txt
	@$(call print,ASFLAGS=$(sort $(ASFLAGS))) >> build_settings.txt
	@$(call print,LDFLAGS=$(LDFLAGS)) >> build_settings.txt
	@$(call print,_TOOLSCHAIN_DEPEND=$(_TOOLSCHAIN_DEPEND)) >> build_settings.txt
	@$(call print,AUTO__FILE_ID__=$(AUTO__FILE_ID__)) >> build_settings.txt
	$(info build settings reported in file "$(CURDIR)/build_settings.txt")

$(_OUTPUT_PACKAGE_FILE): $(_OUTPUT_PACKAGE_FILE).i
ifeq ($(PREFIX_SYMBOLS),false)
	$(OBJCOPY) \
      $(_XTENSA_CORE_FLAGS) \
      --prefix-alloc-sections=.$(MODULE_FILENAME) \
      $(_OUTPUT_PACKAGE_FILE).i $@
else
	$(OBJCOPY) \
      --prefix-symbols $(MODULE_FILENAME) \
      $(_XTENSA_CORE_FLAGS) \
      --prefix-alloc-sections=.$(MODULE_FILENAME) \
      $(_OUTPUT_PACKAGE_FILE).i $@
endif

# generate intermediate module package file (without section renaming)
$(_OUTPUT_PACKAGE_FILE).i: $(_TOOLSCHAIN_DEPEND) $(_RELPATH_OBJ_FILES)
$(_OUTPUT_PACKAGE_FILE).i: $(STATIC_LIBRARIES) $(_BUILD_FRMK_DIR)build_core/archive_rule.mk
$(_OUTPUT_PACKAGE_FILE).i: $(EXTRA_OBJ_FILES)
	$(info --> creating archive : $@ - Static libraries $(STATIC_LIBRARIES), object files : $(_RELPATH_OBJ_FILES)...)
	$(MAKE) -f $(_BUILD_FRMK_DIR)build_core/archive_rule.mk \
      OUTPUT_FILE="$@" AR="$(AR)" \
      STATIC_LIBRARIES="$(STATIC_LIBRARIES)"\
      EXTRA_OBJ_FILES="$(EXTRA_OBJ_FILES)" \
      LASTLIBS="$(LASTLIBS)"

$(_MODULE_ENTRY_POINT_FILENAME): $(_RELPATH_OBJ_FILES) $(_BUILD_FRMK_DIR)build_core/entrypoint_option_rule.mk
	$(MAKE) -f $(_BUILD_FRMK_DIR)build_core/entrypoint_option_rule.mk \
      OUTPUT_FILE="$@" TOOLSCHAIN_FILE="$(TOOLSCHAIN_FILE)" QUIET=true NM="$(strip $(NM) $(_XTENSA_CORE_FLAGS))"

ifeq (1,$(AUTO__FILE_ID__))
  $(_RELPATH_OBJ_FILES): .file_id
endif

$(_RELPATH_OBJ_FILES): $(CONFIG_FILE) $(MAKEFILE_LIST) .d | $(dir $(call linuxpath,$(foreach obj,$(_RELPATH_OBJ_FILES),$(CURDIR)/$(obj))))
$(_RELPATH_OBJ_FILES): $(_BUILD_FRMK_DIR)build_core/module_package_rules.mk

src_files.lst: $(_SRC_FILES)  $(_BUILD_FRMK_DIR)build_core/src_files_rule.mk $(MAKEFILE_LIST)
	$(info Generating "src_files.lst" ...)
	$(MAKE) -f $(_BUILD_FRMK_DIR)build_core/src_files_rule.mk \
      OUTPUT_FILE="$@" QUIET=true PLATFORM="$(_PLATFORM)" MODULE_SRC_DIRS="$(_SRC_FILES)"
