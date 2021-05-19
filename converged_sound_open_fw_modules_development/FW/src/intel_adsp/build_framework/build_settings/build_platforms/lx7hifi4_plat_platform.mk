# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define lx7hifi4_plat_platform
  override XTENSA_CORE := ace10_LX7HiFi4_RI_2020_5
  XTENSA_TOOLSCHAIN_REVISION := RI-2020.5
  ALLOWED_TOOLCHAINS := gcc xtensa xtensa-lx7hifi4-elf

  override FW_MEMORY_SECTIONS_LAYOUT := colocate_sections_per_type
  SIGNING_METHOD := MEU
  MEU_CONFIG_PREFIX := ace_
  # keep decimal representation as std::atoi won't work for hex inside adsp_fw_builder
  CSS_HEADERS_SIZE := 1280

  ELF_LOGGER_AVAILABLE := 1

  # supported memory classes definitions
  override _MEMORY_CLASSES            := IMR HPSRAM LPSRAM
  override _MEMORY_CLASS_DEFAULT_LMA  := IMR
  override _MEMORY_CLASS_DEFAULT_VMA  := HPSRAM

  # set default toolschain
  TOOLSCHAIN ?= xtensa
    ifneq ($(TOOLSCHAIN),$(filter $(TOOLSCHAIN), $(ALLOWED_TOOLCHAINS)))
      override _DEFERRED_ERROR := "lx7hifi4_plat" target can not be build with $(TOOLSCHAIN)
    endif
    ifeq (cygwin,$(_HOST))
      override _DEFERRED_ERROR := "lx7hifi4_plat" target cannot be built on "cygwin" host
    endif

    # get MODULE_PASS_BUFFER_SIZE given the API version
    MODULE_PASS_BUFFER_SIZE_4.5.x := 320
    MODULE_PASS_BUFFER_SIZE_4.3.x := 256
    MODULE_PASS_BUFFER_SIZE_4.2.x := 248
    MODULE_PASS_BUFFER_SIZE_4.1.x := 236
    MODULE_PASS_BUFFER_SIZE_4.0.x := 256
    MODULE_PASS_BUFFER_SIZE_3.0.x := 208
    MODULE_PASS_BUFFER_SIZE_2.0.x := 304
    MODULE_PASS_BUFFER_SIZE_1.0.x := 304
    MODULE_PASS_BUFFER_SIZE_0.6.x := 304
    MODULE_PASS_BUFFER_SIZE_0.5.x := 236
    MODULE_PASS_BUFFER_SIZE_0.4.x := 204
    MODULE_PASS_BUFFER_SIZE := $(value MODULE_PASS_BUFFER_SIZE_$(_MAJOR_API_VERSION_NUMBER).$(_MIDDLE_API_VERSION_NUMBER).x)
endef
