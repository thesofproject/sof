# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


define mswindows_host
  override linuxpath = $(subst \,/,$1)
  override syspath = $(subst /,\,$1)
  override mkdir_cmd := mkdir
  override print_cmd := echo
  override escape := ^
  override print = $(print_cmd).$1
  override rm_cmd := del /Q
  override rmdir_cmd := rmdir /s /q
  override cp_cmd := copy /Y
  override mv = move "$(call syspath,$1)" "$(call syspath,$2)"
  override null_device := nul
  override driveletter = $(filter %:,$(subst :,: ,$1))
  override removedriveletter = $(filter-out %:,$(subst :,: ,$1))
  override mkemptyfile = type NUL > $1
  override cat_cmd := type
  override exec_default_extension := .exe
  override merge_bin = copy /b $(call syspath,$1) + $(call syspath,$2) $(call syspath,$3)
  override which_cmd := where
  override _PATH_SEPARATOR_ := ;
  override grep = findstr /R /C:"$1" $2

  override DEFAULT_TMP_DIR := ./tmp/
endef
