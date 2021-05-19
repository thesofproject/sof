# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# _BUILD_HOST_DIR indicates the path to directory of this current file
override _BUILD_HOST_DIR := $(dir $(lastword $(MAKEFILE_LIST)))


include $(_BUILD_HOST_DIR)linux_host.mk


define cygwin_host
  override DEFAULT_PLATFORM ?= x86-cygwin
  $(eval $(value linux_host))
  override syspath = $(foreach path,$1,$(shell cygpath -u $(path)))
endef
