# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


#<--
# Define rule to invoke tools for signing and building of the cAVS FW image
#-->


# _BUILD_CORE_DIR indicates the path to directory of this current file
override _BUILD_CORE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))


# define default rule
.PHONY: default
default: build_cavs

# rule for building of the cAVS fw binary
.PHONY: build_cavs
build_cavs: $(BIN_OUT_NAME)
	$(info --> building of the FW binary "$(BIN_OUT_NAME)" has succeeded <--)

# rule for signing (only) of the cAVS fw binary
.PHONY: sign
sign: $(BIN_OUT_NAME)


# rule for cleaning the signed artifacts
.PHONY: clean_signed
clean_signed:
	-$(call rm,$(BIN_OUT_NAME))
	-$(call rm,$(BIN_OUT_NAME_NOEXTMFT))

# rule for cleaning of the cAVS fw binary
.PHONY: clean_cavs
clean_cavs: clean_signed
	-$(call rm,$(BIN_OUT_NAME))
	-$(call rm,$(FW_BINARY_NAME).mk)
	-$(call rm,$(MANIFEST_EXTENDED_OUT_FILE))
	-$(call rm,$(MANIFEST_OUT_FILE))
	-$(call rm,$(BIN_PACKAGE_NAME))
	-$(call rm,$(BIN_OUT_NAME_UNSIGNED))
	-$(call rm,$(MANIFEST_SIGNED_OUT_FILE))
	-$(call rm,empty.h)

#rule for building of the signed binary
$(BIN_OUT_NAME): $(MANIFEST_EXTENDED_OUT_FILE) $(BIN_OUT_NAME_NOEXTMFT)
	$(info --> generating signed binary : $@ ...)
  ifeq (1,$(RELEASE_BINARY_WITHOUT_EXTENDED_MANIFEST))
    ifeq (1,$(HIDE_RECIPE))
		@$(call cp, $(BIN_OUT_NAME_NOEXTMFT), $(BIN_OUT_NAME))
    else
		$(call cp, $(BIN_OUT_NAME_NOEXTMFT), $(BIN_OUT_NAME))
    endif
  else
    ifeq (1,$(HIDE_RECIPE))
		@$(call merge_bin,$(MANIFEST_EXTENDED_OUT_FILE),$(BIN_OUT_NAME_NOEXTMFT),$(BIN_OUT_NAME))
    else
		$(call merge_bin,$(MANIFEST_EXTENDED_OUT_FILE),$(BIN_OUT_NAME_NOEXTMFT),$(BIN_OUT_NAME))
    endif
    ifeq (1,$(SLOT_1_SUPPORT))
		$(call merge_bin,$(MANIFEST_EXTENDED_OUT_FILE),slot_1_$(BIN_OUT_NAME_NOEXTMFT),slot_1_$(BIN_OUT_NAME))
    endif
  endif

ADDITIONAL_ARGS :=
ifeq (1,$(USE_MNVP))
   ADDITIONAL_ARGS += -mnpv $(MEU_PV_BIT)
else

endif

#rule for building of the signed binarie(s) w/o extended manifest
ifeq ($(SIGNING_METHOD),MEU)
$(BIN_OUT_NAME_NOEXTMFT): $(BIN_OUT_NAME_UNSIGNED)
	$(info --> generating signed binary w/o ext. mnft : $@ ...)

  ifeq (1,$(HIDE_RECIPE))
	@$(MEU_PATH) -w $(OUT_DIR) $(_MEU_SOURCE_PATH_OPTION) -key "$(SIGNING_KEYS)" -stp $(SIGNING_TOOL) -f $(MEU_CONF_FILE) -mnver $(BINARY_VERSION_NUMBER) -o $(BIN_OUT_NAME_NOEXTMFT) $(ADDITIONAL_ARGS)
  else
	$(MEU_PATH) -w $(OUT_DIR) $(_MEU_SOURCE_PATH_OPTION) -key "$(SIGNING_KEYS)" -stp $(SIGNING_TOOL) -f $(MEU_CONF_FILE) -mnver $(BINARY_VERSION_NUMBER) -o $(BIN_OUT_NAME_NOEXTMFT) $(ADDITIONAL_ARGS)
  endif
  ifeq (1,$(SLOT_1_SUPPORT))
	-$(call cp, $(MEU_SOURCE_PATH), $(SLOT_1_MEU_SOURCE_PATH))
	$(MEU_PATH) -w $(OUT_DIR) $(_SLOT_1_MEU_SOURCE_PATH_OPTION) -key "$(SIGNING_KEYS)" -stp $(SIGNING_TOOL) -f $(SLOT_1_MEU_CONF_FILE) -mnver $(BINARY_VERSION_NUMBER) -o slot_1_$(BIN_OUT_NAME_NOEXTMFT) $(ADDITIONAL_ARGS)
  endif
endif

ifeq ($(SIGNING_METHOD),BUILDER)
  define signed_binary_recipe
	$(FW_BUILDER_PATH) $(FW_BUILDER_FLAGS) sign debug "$(SIGNING_KEYS)" $(call syspath,$(OUT_DIR)$(MANIFEST_OUT_FILE)) $(call syspath,$(OUT_DIR)$(MANIFEST_SIGNED_OUT_FILE)) && \
	$(call merge_bin,$(MANIFEST_SIGNED_OUT_FILE),$(BIN_OUT_NAME_UNSIGNED),$(BIN_OUT_NAME_NOEXTMFT))
  endef

$(BIN_OUT_NAME_NOEXTMFT): $(BIN_OUT_NAME_UNSIGNED) $(MANIFEST_OUT_FILE)
	$(info --> generating signed binary w/o ext. mnft : $@ ...)
  ifeq (1,$(HIDE_RECIPE))
	@$(call wrap_recipe,$(signed_binary_recipe))
  else
	$(call wrap_recipe,$(signed_binary_recipe))
  endif
endif

ifeq ($(SIGNING_METHOD),NONE)
$(BIN_OUT_NAME_NOEXTMFT): $(BIN_OUT_NAME_UNSIGNED)
	$(info --> generating not signed binary w/o ext. mnft : $@ ...)
	$(call print,offset 0x480) > $(BIN_OUT_NAME_NOEXTMFT)_recipe.cfg
	$(call print,bin $(BIN_OUT_NAME_UNSIGNED)) >> $(BIN_OUT_NAME_NOEXTMFT)_recipe.cfg
#<<// TBD UPSTREAM: restore with new fwbuilder 
	$(call cp,$(BIN_OUT_NAME_UNSIGNED),$(BIN_OUT_NAME_NOEXTMFT))
#$(FW_BUILDER_PATH) build_img $(BIN_OUT_NAME_NOEXTMFT)_recipe.cfg $(OUT_DIR) $(BIN_OUT_NAME_NOEXTMFT)
endif

#rule for building of the unsigned binary parts
ifeq (sign,$(filter sign,$(MAKECMDGOALS)))
$(MANIFEST_EXTENDED_OUT_FILE) $(MANIFEST_OUT_FILE) $(BIN_PACKAGE_NAME):

else
$(MANIFEST_EXTENDED_OUT_FILE) $(MANIFEST_OUT_FILE) $(BIN_PACKAGE_NAME): binary_parts
endif

ifeq (sign,$(filter sign,$(MAKECMDGOALS)))
  $(BIN_OUT_NAME_UNSIGNED):

else
  $(BIN_OUT_NAME_UNSIGNED): binary_parts
  ifeq (1,$(HIDE_RECIPE))
	@$(call rm,$(BIN_OUT_NAME_UNSIGNED)) && \
	$(call mv,$(FW_BINARY_NAME).bin,$(BIN_OUT_NAME_UNSIGNED))
  else
	$(call rm,$(BIN_OUT_NAME_UNSIGNED)) && \
	$(call mv,$(FW_BINARY_NAME).bin,$(BIN_OUT_NAME_UNSIGNED))
    ifeq (1,$(SLOT_1_SUPPORT))
	$(call mv,slot_1_$(FW_BINARY_NAME).bin,slot_1_$(BIN_OUT_NAME_UNSIGNED))
    endif
  endif
endif

$(BIN_OUT_NAME_UNSIGNED): $(ELF_FILES_LIST)

_ELFLIST_DEP_FILE=$(abspath $(OUT_DIR))/$(patsubst %.lst,%.d,$(_ELFLIST_FILE))
ifeq ($(OS),Windows_NT)
-include $(_ELFLIST_DEP_FILE)
endif

define binary_parts_recipe
	cd $(BINMAP_DIR) && \
	$(FW_BUILDER_PATH) $(FW_BUILDER_FLAGS) build $(IMAGE_FLAGS) $(FEATURE_MASK) $(SLOT_1_IMAGE_TYPE) \
		$(_BINARY_VERSION_MAJOR) $(_BINARY_VERSION_MINOR) $(_BINARY_VERSION_HOTFIX) $(_BINARY_VERSION_BUILD) \
		$(CSS_HEADERS_SIZE) \
		$(call syspath,$(BINMAP_FILE_PATH)) \
		$(abspath $(MOD_DEF_FILE)) $(abspath $(KEYLIST_FILE)) \
		$(abspath $(OUT_DIR))/ \
		$(_ELFLIST_FILE) $(MANIFEST_OUT_FILE) $(FW_BINARY_NAME)
endef

define binary_parts_recipe_for_slot_1
	cd $(BINMAP_DIR) && \
	$(FW_BUILDER_PATH) $(FW_BUILDER_FLAGS) build $(IMAGE_FLAGS) $(FEATURE_MASK) $(IMAGE_TYPE) \
		$(_BINARY_VERSION_MAJOR) $(_BINARY_VERSION_MINOR) $(_BINARY_VERSION_HOTFIX) $(_BINARY_VERSION_BUILD) \
		$(CSS_HEADERS_SIZE) \
		$(call syspath,$(BINMAP_FILE_PATH)) \
		$(abspath $(MOD_DEF_FILE)) $(abspath $(KEYLIST_FILE)) \
		$(abspath $(OUT_DIR))/ \
		$(_ELFLIST_FILE) slot_1_$(MANIFEST_OUT_FILE) slot_1_$(FW_BINARY_NAME)
endef

.INTERMEDIATE: binary_parts

binary_parts: $(_ELFLIST_FILE) $(KEYLIST_FILE) $(BINMAP_FILE_PATH) $(MOD_DEF_FILE)
	$(info --> generating binary parts : $^ ...)
  ifeq (1,$(HIDE_RECIPE))
	@$(call wrap_recipe,$(binary_parts_recipe))
  else
	$(call wrap_recipe,$(binary_parts_recipe))
  endif
  ifeq (1,$(SLOT_1_SUPPORT))
	$(call wrap_recipe,$(binary_parts_recipe_for_slot_1))
  endif

#rule for building the elf list file
$(_ELFLIST_FILE): $(ELF_FILES_LIST) $(addsuffix .memmap,$(ELF_FILES_LIST)) $(addsuffix .nm,$(ELF_FILES_LIST))
	$(info --> generating ELF list file : $@ ...)
  ifeq (1,$(HIDE_RECIPE))
	@$(MAKE) -f $(_BUILD_CORE_DIR)elflist_file_rule.mk OUTPUT_FILE=$(_ELFLIST_FILE) ELF_FILES_LIST="$(ELF_FILES_LIST)" HOST=$(_HOST)
  else
	$(MAKE) -f $(_BUILD_CORE_DIR)elflist_file_rule.mk OUTPUT_FILE=$(_ELFLIST_FILE) ELF_FILES_LIST="$(ELF_FILES_LIST)" HOST=$(_HOST)
  endif

#rule for building of a dummy mod. def. file
empty.h:
  ifeq (1,$(HIDE_RECIPE))
	@$(call mkemptyfile,$@)
  else
	$(call mkemptyfile,$@)
  endif
 
#rule for building of a dummy key list file
empty.keys:
  ifeq (1,$(HIDE_RECIPE))
	@$(call mkemptyfile,$@)
  else
	$(call mkemptyfile,$@)
  endif
#-->
