# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# Get directory of this current file
override _BUILD_CORE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

include $(_BUILD_CORE_DIR)stddef.mk
include $(_BUILD_CORE_DIR)error_functions.mk


# validate goals (when fw_binary.mk is directly invoked on command line) before executing rules
# <--
override _VALID_GOALS += $(sort \
 help \
 usage \
 default \
 build_cavs \
 sign \
 clean_cavs \
 clean_signed \
)

override _INVALID_GOALS := $(filter-out $(_VALID_GOALS),$(MAKECMDGOALS))

ifneq (,$(_INVALID_GOALS))
  $(error Invalid command line: following goals not supported "$(MAKECMDGOALS)" - supported goals are :$(_RETURN_) \
    $(_VALID_GOALS))
endif
# -->

# define default value for make goal
override _MAKECMDGOALS ?= $(MAKECMDGOALS)
override _MAKECMDGOALS ?= default

# Check for mandatory variables over make goal
#<--
# if (_MAKECMDGOALS value is stricly worth "build_cavs", "default", "sign", "clean_signed" or "clean_cavs")
ifeq (,$(filter-out build_cavs sign clean_cavs clean_signed default,$(_MAKECMDGOALS)))

  $(call error_if_not_defined,OUT_DIR FW_BINARY_SHORTNAME)

  # if _MAKECMDGOALS value is strictly worth "build_cavs", "default" or "sign"
  ifeq (,$(filter-out default sign build_cavs,$(_MAKECMDGOALS)))
    $(call error_if_not_defined,SIGNING_KEYS SIGNING_METHOD)

    override _AVAILABLE_SIGNING_METHODS := MEU BUILDER NONE
    $(call error_if_not_subset,SIGNING_METHOD,_AVAILABLE_SIGNING_METHODS)

    ifeq (MEU,$(SIGNING_METHOD))
      $(call error_if_not_defined,MEU_PATH SIGNING_TOOL)
    endif
    ifeq (BUILDER,$(SIGNING_METHOD))
      $(call error_if_not_defined,FW_BUILDER_PATH)
    endif
  endif

  # if _MAKECMDGOALS value is strictly worth "build_cavs" or "default"
  ifeq (,$(filter-out build_cavs default,$(_MAKECMDGOALS)))
    $(call error_if_not_defined,BINMAP_DIR ELF_FILES_LIST FW_BUILDER_PATH)
  endif
endif
#-->


# include OS-dependent macros
include $(_BUILD_CORE_DIR)../build_settings/build_hosts/host.mk


# Define variables with default value
#<--
# when QUIET_RECIPE is set to 1, standard output from recipe execution is hidden
QUIET_RECIPE ?= 0
# when HIDE_RECIPE is set to 1, recipe is not display on command line
HIDE_RECIPE ?= 1
FW_BINARY_NAME := $(FW_BINARY_PREFIX)$(FW_BINARY_SHORTNAME)$(FW_BINARY_SUFFIX)
FW_BUILDER_FLAGS ?= -v
MOD_DEF_FILE ?= empty.h
KEYLIST_FILE ?= empty.keys
override _ELFLIST_FILE := $(FW_BINARY_NAME)_elf.lst

# binary version number shall have format : MAJOR_NUMBER.MINOR_NUMBER.HOTFIX_NUMBER.BUILD_NUMBER
BINARY_VERSION_NUMBER ?= 0.0.0.0

override _BINARY_VERSION_NUMBER := $(subst ., ,$(subst $(_SPACE_),,$(BINARY_VERSION_NUMBER)))
override _BINARY_VERSION_MAJOR := $(word 1,$(_BINARY_VERSION_NUMBER))
override _BINARY_VERSION_MAJOR := $(if $(_BINARY_VERSION_MAJOR),$(_BINARY_VERSION_MAJOR),0)
override _BINARY_VERSION_MINOR := $(word 2,$(_BINARY_VERSION_NUMBER))
override _BINARY_VERSION_MINOR := $(if $(_BINARY_VERSION_MINOR),$(_BINARY_VERSION_MINOR),0)
override _BINARY_VERSION_HOTFIX := $(word 3,$(_BINARY_VERSION_NUMBER))
override _BINARY_VERSION_HOTFIX := $(if $(_BINARY_VERSION_HOTFIX),$(_BINARY_VERSION_HOTFIX),0)
override _BINARY_VERSION_BUILD := $(word 4,$(_BINARY_VERSION_NUMBER))
override _BINARY_VERSION_BUILD := $(if $(_BINARY_VERSION_BUILD),$(_BINARY_VERSION_BUILD),0)

BIN_OUT_NAME ?= $(FW_BINARY_NAME).bin
BIN_OUT_NAME_UNSIGNED ?= $(FW_BINARY_NAME)_unsigned.bin
BIN_PACKAGE_NAME ?= $(FW_BINARY_NAME).pkg
BIN_OUT_NAME_NOEXTMFT ?= $(FW_BINARY_NAME)_noextmft.bin
MANIFEST_OUT_FILE := $(FW_BINARY_NAME).manifest
MANIFEST_SIGNED_OUT_FILE := $(MANIFEST_OUT_FILE).signed
MANIFEST_EXTENDED_OUT_FILE := $(MANIFEST_OUT_FILE).ext
MEU_CONF_FILE ?= $(_BUILD_CORE_DIR)$(MEU_CONFIG_PREFIX)generic_meu_conf.xml
MEU_SOURCE_PATH ?= $(FW_BINARY_NAME)
SLOT_1_MEU_SOURCE_PATH ?= slot_1_$(FW_BINARY_NAME)
MEU_PV_BIT ?= "false"
USE_MNVP ?= 0

BINMAP_FILE_PATH ?= $(BINMAP_DIR)$(FW_BINARY_SHORTNAME).binmap
IMAGE_FLAGS ?= 00000000
FEATURE_MASK ?= 1f
IMAGE_TYPE ?= 3
SLOT_1_IMAGE_TYPE ?= 4
ifdef MEU_SOURCE_PATH
  override _MEU_SOURCE_PATH_OPTION := -s $(MEU_SOURCE_PATH)
  override _SLOT_1_MEU_SOURCE_PATH_OPTION := -s $(SLOT_1_MEU_SOURCE_PATH)
else
  override _MEU_SOURCE_PATH_OPTION :=
  override _SLOT_1_MEU_SOURCE_PATH_OPTION :=
endif
#-->


# define wrapper for rule recipe
ifeq (1,$(QUIET_RECIPE))
  wrap_recipe = $(subst !!,$(_SPACE_),$(patsubst %$(_RETURN_),(%) > $(null_device)$(_RETURN_),$(subst $(_SPACE_),!!,$1)))
else
  wrap_recipe = $1
endif


# define the usage message
#<--
ifndef _USAGE_MESSAGE
override define _USAGE_MESSAGE
  USAGE DESCRIPTION
  -----------------
  getting usage: make usage
    display this message
  standard usage: make [<GOAL>] [<MANDATORY_OPTIONS>] [VERBOSE=1] [PARALLELMFLAGS="-j N"]
    GOAL indicates the operation of the building process to apply for the cavs fw binary. Optional.
    MANDATORY_OPTIONS is a list of mandatory variables assigment which depends on the GOAL value.
    VERBOSE=1 add verbose building traces in console. Optional
    PARALLELMFLAGS="-j N", N indicates the number of parallel jobs. Optional workaround for Windows gnu make. by default builing process is executed in one single job.
      On windows "-j N" option is not passed downward correctly to sub-make process, please use PARALLELMFLAGS as a workaround
    available GOAL values:
      build_cavs -- generate a cavs (signed) binary given an ELF binary. This is the default goal value.
      sign -- generate a cavs signed binary given an cavs unsigned binary.
      clean_cavs -- clean the output directory from all build artifacts.
      clean_signed -- clean the output directory from signed build artifacts.
      usage|help -- display this message
    OPTIONS set over GOAL value:
      when build_cavs, sign, clean_cavs or clean_signed goal is given, at least OUT_DIR and FW_BINARY_SHORTNAME shall be defined.
      when build_cavs or sign goal is given, SIGNING_KEYS and SIGNING_METHOD shall also be defined.
      when build_cavs or sign goal is given and SIGNING_METHOD is worth MEU, MEU_PATH and SIGNING_TOOL shall also be defined.
      when build_cavs or sign goal is given and SIGNING_METHOD is worth BUILDER, FW_BUILDER_PATH shall also be defined.
    example:
      make clean_signed OUT_DIR=./ FW_BINARY_SHORTNAME=my_lib
      make clean_cavs OUT_DIR=./ FW_BINARY_SHORTNAME=my_lib
      make sign OUT_DIR=./ FW_BINARY_SHORTNAME=my_lib SIGNING_KEYS=my_key.kp SIGNING_METHOD=MEU MEU_WORKING_DIR=./ MEU_PATH=meu_dir/meu.exe
      make sign OUT_DIR=./ FW_BINARY_SHORTNAME=my_lib SIGNING_KEYS=my_key.kp SIGNING_METHOD=BUILDER FW_BUILDER_PATH=fw_builder_dir/adsp_fw_builder.exe
      make build_cavs OUT_DIR=./ FW_BINARY_SHORTNAME=my_lib SIGNING_KEYS=my_key.kp SIGNING_METHOD=MEU MEU_PATH=meu_dir/meu.exe FW_BUILDER_PATH=fw_builder_dir/adsp_fw_builder.exe
      make usage -- display this message
endef
endif
#-->

# if (_MAKECMDGOALS value does not contain "usage" or "help" goal)
ifeq (,$(filter usage help,$(_MAKECMDGOALS)))
  include $(_BUILD_CORE_DIR)fw_binary_rules.mk

else
      .PHONY: build_cavs default sign clean_cavs clean_signed
     build_cavs default sign clean_cavs clean_signed: ;

endif

include $(_BUILD_CORE_DIR)common_rules.mk
