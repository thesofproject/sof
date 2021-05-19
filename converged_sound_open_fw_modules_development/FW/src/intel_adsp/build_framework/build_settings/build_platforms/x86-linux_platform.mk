# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define x86-linux_platform
  # defining XTENSA_CORE for CSTUB support (by default sunrisepoint dsp core)
  XTENSA_CORE ?= LX4H3I32w4l64D64w4l64e
  ifneq (linux,$(_HOST))
    override _DEFERRED_ERROR := "x86-linux" target can only be built on linux host
  endif
  override _PLATFORM_ACRONYM := x86-linux

  # get MODULE_PASS_BUFFER_SIZE given the API version
  MODULE_PASS_BUFFER_SIZE_0.4.x := 192
  MODULE_PASS_BUFFER_SIZE_0.5.x := 224
  MODULE_PASS_BUFFER_SIZE := $(value MODULE_PASS_BUFFER_SIZE_$(_MAJOR_API_VERSION_NUMBER).$(_MIDDLE_API_VERSION_NUMBER).x)
endef
