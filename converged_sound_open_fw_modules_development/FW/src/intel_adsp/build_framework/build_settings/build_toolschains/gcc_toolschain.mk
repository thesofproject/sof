# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define gcc_toolschain
  CC := gcc
  CXX := g++
  NM := nm
  RANLIB := ranlib
  AR := ar
  OBJCOPY := objcopy
  OBJDUMP: objdump
  ifneq (1,$(DISABLE_XTENSA_CSTUB))
    # try to add support for xtensa intrinsic
    override _XTENSA_CONFIG := $(call linuxpath,$(shell xt-xcc --show-config=config --xtensa-core=$(XTENSA_CORE)))
    ifeq (,$(_XTENSA_CONFIG))
      $(warning cannot locate xtensa toolchains binary : No possible support of CSTUB for xtensa intrinsic !)
      CFLAGS += -DGCC_TOOLSCHAIN=1
      CXXFLAGS += -DGCC_TOOLSCHAIN=1
      LDFLAGS += -nostdlib
    else
        VPATH += $(_XTENSA_CONFIG)/src/
        TARGET_ARCH += \
            -m32
        override _XTENSA_STUB_FLAGS := \
            -nostdlib \
            -I"$(_XTENSA_CONFIG)/src/cstub" \
            -I"$(_XTENSA_CONFIG)/xtensa-elf/include" \
            -I"$(_XTENSA_CONFIG)/xtensa-elf/arch/include" \
            -DXTENSA_CORE=$(XTENSA_CORE) \
            -D__STDC_LIMIT_MACROS
        ASFLAGS += $(_XTENSA_STUB_FLAGS)
        CFLAGS += $(_XTENSA_STUB_FLAGS)
        CXXFLAGS += $(_XTENSA_STUB_FLAGS)
        override _TOOLSCHAIN_OBJ_FILES := cstub/$(patsubst %.cpp,%.o,$(notdir $(wildcard $(_XTENSA_CONFIG)/src/cstub/*.cpp)))
    endif
    LDFLAGS += -nodefaultlibs -nostdlib
  endif
endef

define gcc_toolschain_rules
  cstub/%.o: CFLAGS += -Wint-to-pointer-cast
endef
