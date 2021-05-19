# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define linux_host
  override linuxpath = $1
  override syspath = $1
  override mkdir_cmd := mkdir -p
  override print_cmd := echo
  override escape = \

  override print = $(print_cmd) "$1"
  override rm_cmd := rm -f
  override rmdir_cmd := rm -r
  override cp_cmd := cp
  override mv = mv $(call syspath,$1) $(call syspath,$2)
  override null_device = /dev/null
  override DEFAULT_PLATFORM ?= x86-linux
  override mkemptyfile = rm $1 2>$(null_device);touch $1
  override cat_cmd := cat
  override exec_default_extension =
  override merge_bin = cat $(call syspath,$1) $(call syspath,$2) > $(call syspath,$3)
  override _PATH_SEPARATOR_ = :
  override which_cmd := which
  override grep = grep "$1" $2

  override DEFAULT_TMP_DIR := /tmp/
endef
