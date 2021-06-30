# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define xtensa-lx7hifi4-elf_toolschain
  CC := $(OVERLAY_TOOLS_DIR)xtensa-lx7hifi4-elf-gcc
  CXX := $(OVERLAY_TOOLS_DIR)xtensa-lx7hifi4-elf-g++
  NM := $(OVERLAY_TOOLS_DIR)xtensa-lx7hifi4-elf-nm
  RANLIB := $(OVERLAY_TOOLS_DIR)xtensa-lx7hifi4-elf-ranlib
  AR := $(OVERLAY_TOOLS_DIR)xtensa-lx7hifi4-elf-ar
  OBJCOPY := $(OVERLAY_TOOLS_DIR)xtensa-lx7hifi4-elf-objcopy
  OBJDUMP := $(OVERLAY_TOOLS_DIR)xtensa-lx7hifi4-elf-objdump
  CFLAGS += -DGCC_TOOLSCHAIN=1
  CXXFLAGS += -DGCC_TOOLSCHAIN=1
  LDFLAGS += -nostdlib -nodefaultlibs
  _TOOLSCHAIN_TEST = $(call linuxpath,$(shell $(CC) -v)
  ifneq (0,_TOOLSCHAIN_TEST)
  $(error, $(CC) not found.)
  endif
endef
