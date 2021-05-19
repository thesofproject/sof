# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


override _DEFAULT_XTENSA_TOOLSCHAIN_DIR := $(firstword $(dir $(shell $(call which,xt-xcc))))
ifdef _DEFAULT_XTENSA_TOOLSCHAIN_DIR
  XTENSA_TOOLS_DIR ?= $(_DEFAULT_XTENSA_TOOLSCHAIN_DIR)../../../
endif

ifndef XTENSA_TOOLS_DIR
  $(error $(_RETURN_)$(_RETURN_)\
No installation of an Xtensa compilation toolchain has been found.$(_RETURN_)\
  - If you are building with the FDK toolsuite, please verify that the Xtensa toolchain path is correctly configured in the tool or$(_RETURN_)\
  - if you are building with the command line, please create the "XTENSA_TOOLS_DIR" environment variable and set it to the root directory of the Xtensa toolchains installation path$(_RETURN_)\
    (Note that for a default installation of the Xtensa toolchain the expected value for XTENSA_TOOLS_DIR is C:\usr\xtensa\XtDevTools\install\tools\)$(_RETURN_))
# Note for developer : Another extra option is to add one of the "bin" directory of an Xtensa toolchain installation in the PATH environment variable.
endif

ifeq (linux,$(_HOST))
  override _XTENSA_REVISION_EXTENSION := linux
else
  override _XTENSA_REVISION_EXTENSION := win32
endif

override _XTENSA_TOOLSCHAIN_DIR := $(call linuxpath,$(XTENSA_TOOLS_DIR)/$(XTENSA_TOOLSCHAIN_REVISION)-$(_XTENSA_REVISION_EXTENSION)/XtensaTools/bin)
# expand ../
_XTENSA_TOOLSCHAIN_DIR_ABS := $(abspath $(_XTENSA_TOOLSCHAIN_DIR))
# and get realpath
override _XTENSA_TOOLSCHAIN_DIR := $(realpath $(_XTENSA_TOOLSCHAIN_DIR_ABS))

ifeq ($(_XTENSA_TOOLSCHAIN_DIR),)
  $(error $(_RETURN_)$(_RETURN_)\
Toolchain not found at $(_XTENSA_TOOLSCHAIN_DIR_ABS).$(_RETURN_))
endif

override _XTENSA_TOOLSCHAIN_DIR := $(_XTENSA_TOOLSCHAIN_DIR)/

$(info _XTENSA_TOOLSCHAIN_DIR is [${_XTENSA_TOOLSCHAIN_DIR}])

define xtensa_toolschain
  #shall add $(_XTENSA_TOOLSCHAIN_DIR) in PATH variable because of libgcc_s_dw2-1.dll dependency with "RF-2014.1-win32" revision
  PATH := $(PATH)$(call not_in_string,$(_PATH_SEPARATOR_)$(call syspath,$(_XTENSA_TOOLSCHAIN_DIR)),$(PATH))
  override _TOOLSCHAIN_DEPEND := $(_BUILD_FRMK_DIR)linker_scripts/ldscripts/elf32xtensa.xu
  override _XTENSA_CONFIG := $(shell $(_XTENSA_TOOLSCHAIN_DIR)xt-xcc --show-config=config --xtensa-core=$(XTENSA_CORE))
  OBJCOPY := $(_XTENSA_TOOLSCHAIN_DIR)xt-objcopy
  CC := $(CCACHE) $(_XTENSA_TOOLSCHAIN_DIR)xt-xcc
  CXX := $(CCACHE) $(_XTENSA_TOOLSCHAIN_DIR)xt-xc++
  NM := $(_XTENSA_TOOLSCHAIN_DIR)xt-nm
  OBJDUMP := $(_XTENSA_TOOLSCHAIN_DIR)xt-objdump
  RANLIB := $(_XTENSA_TOOLSCHAIN_DIR)xt-ranlib
  AR := $(_XTENSA_TOOLSCHAIN_DIR)xt-ar
  override _XTENSA_FLAGS := \
      -OPT:memmove_count=0 \
      -mlongcalls \
      --xtensa-system="$(_XTENSA_CONFIG)/config" \
      --xtensa-core=$(XTENSA_CORE) \
      -I"$(_XTENSA_CONFIG)/xtensa-elf/include" \
      -I"$(_XTENSA_CONFIG)/xtensa-elf/arch/include"
  ASFLAGS += $(_XTENSA_FLAGS)
  CFLAGS += $(_XTENSA_FLAGS) -DXTENSA_TOOLSCHAIN
  CXXFLAGS += $(_XTENSA_FLAGS) -DXTENSA_TOOLSCHAIN
  LDFLAGS += --xtensa-core=$(XTENSA_CORE)
  override _XTENSA_CORE_FLAGS := --xtensa-core=$(XTENSA_CORE)

  #target-dependent definition
  ifneq (,$(filter module_package.mk,$(notdir $(MAKEFILE_LIST))))
    LDFLAGS += -mlsp="$(_BUILD_FRMK_DIR)linker_scripts"
  endif
endef
