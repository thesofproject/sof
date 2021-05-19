# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


ifndef _HOST_MK_
_HOST_MK_ := 1


# possible input parameter for this makefile
# when QUIET is true, no display on console is performed
QUIET ?=

# _BUILD_HOST_DIR indicates the path to directory of this current file
override _BUILD_HOST_DIR := $(dir $(lastword $(MAKEFILE_LIST)))


include $(_BUILD_HOST_DIR)../../build_core/stddef.mk
include $(_BUILD_HOST_DIR)../../build_core/arithmetic.mk


override _MAKE_VERSION := $(subst ., ,$(MAKE_VERSION))
#corner case: 3.82+dbg0.9
override _MAKE_VERSION := $(subst +, ,$(_MAKE_VERSION))
override _MAKE_MAJOR_VERSION := $(word 1,$(_MAKE_VERSION))
override _MAKE_MIDDLE_VERSION := $(word 2,$(_MAKE_VERSION))
override _MAKE_MINOR_VERSION := $(word 3,$(_MAKE_VERSION))
override _WRONG_VERSION_NUMBER :=

ifndef _HOST
  ifneq (true,$(call is_true,$(QUIET)))
    $(info scanning building host ...)
  endif
  ifeq ($(OS),Windows_NT)
    ifeq ($(shell echo $$OSTYPE),cygwin)
      ifeq ($(_1),$(call compare_lt,_$(_MAKE_MAJOR_VERSION),_4))
          override _WRONG_VERSION_NUMBER := 1
      else
        ifeq ($(_1),$(call compare_eq,_$(_MAKE_MAJOR_VERSION),_4))
          ifeq ($(_1),$(call compare_lt,_$(_MAKE_MIDDLE_VERSION),_1))
            override _WRONG_VERSION_NUMBER := 1
          endif
        endif
      endif
      ifneq (,$(_WRONG_VERSION_NUMBER))
        $(error gnu make shall have version 4.1 or higher (current version is $(MAKE_VERSION)))
      endif
      override _HOST = cygwin
    else
      ifeq ($(_1),$(call compare_lt,_$(_MAKE_MAJOR_VERSION),_3))
          override _WRONG_VERSION_NUMBER := 1
      else
        ifeq ($(_1),$(call compare_eq,_$(_MAKE_MAJOR_VERSION),_3))
          ifeq ($(_1),$(call compare_lt,_$(_MAKE_MIDDLE_VERSION),_81))
            override _WRONG_VERSION_NUMBER := 1
          else
            ifeq ($(_1),$(call compare_eq,_$(_MAKE_MAJOR_VERSION),_81))
              ifeq ($(_1),$(call compare_lt,_$(_MAKE_MINOR_VERSION),_2))
                override _WRONG_VERSION_NUMBER := 1
              endif
            endif
          endif
        endif
      endif
      ifneq (,$(_WRONG_VERSION_NUMBER))
        $(error gnu make shall have version 3.81.2 or higher (current version is $(MAKE_VERSION)))
      endif
      override _HOST = mswindows
    endif
  else
    ifeq ($(filter Linux%,$(shell uname)),)
      $(warning Unknow OS, Linux shell assumed.);
    endif
      ifeq ($(_1),$(call compare_lt,_$(_MAKE_MAJOR_VERSION),_3))
          override _WRONG_VERSION_NUMBER := 1
      else
        ifeq ($(_1),$(call compare_eq,_$(_MAKE_MAJOR_VERSION),_3))
          ifeq ($(_1),$(call compare_lt,_$(_MAKE_MIDDLE_VERSION),_81))
            override _WRONG_VERSION_NUMBER := 1
          endif
        endif
      endif
      ifneq (,$(_WRONG_VERSION_NUMBER))
        $(error gnu make shall have version 3.81 or higher (current version is $(MAKE_VERSION)) $(_MAKE_MAJOR_VERSION) $(_MAKE_MIDDLE_VERSION))
      endif
      override _HOST = linux
  endif
  ifneq (true,$(call is_true,$(QUIET)))
    $(info --> detected "$(_HOST)" building host)
  endif
endif

include $(_BUILD_HOST_DIR)$(_HOST)_host.mk
$(eval $(value $(_HOST)_host))


override mkdir = $(if $(wildcard $1),,$(mkdir_cmd) $(call syspath,$1))
override rm = $(rm_cmd) $(call syspath,$1) 2> $(null_device)
override rmdir = $(rmdir_cmd) $(call syspath,$1) 2> $(null_device)
override cp_single_input = $(cp_cmd) $(call syspath,$1) $(call syspath,$2) $(_RETURN_)
override cp_files_list = $(foreach file,$1,$(call cp_single_input,$(file),$2))
override cp = $(if $(filter 1,$(words $1)), $(call cp_single_input,$1,$2),$(call cp_files_list,$1,$2))
override which = $(which_cmd) $1 2> $(null_device)


# returns fist element of list $1 if equal to last element
override first_equal_last = $(filter $(firstword $1),$(lastword $1))
# returns fist element of list $1 if NOT equal to last element
override first_notequal_last = $(filter-out $(firstword $1),$(lastword $1))
# if $1 is empty returns !
override not_empty = $(if $1,$1,!)
# returns beginning of $1 up to first character !
override begin = $(patsubst !%,,$(subst !, !,$(subst ! , ,$1)))


# returns the depth of the linux-style (not backslash) path in $1
# $1 can contain a list of space-separated paths. In that case function will returns a list for depth values.
# note that:
# * windows drive letter is taken into account
# * linux '/' root char is not taken into account if alone (returns 0)
# * if input is empty, result returned is empty
# * max depth that can be computed is limited by _MAX_INT value (defined in arithmetic.mk)
override pathdepth = $(foreach path,$(1),$(words $(value _$(words $(subst /, ,$(path))))))


# given $1 a linux-style (not backslash) path and $2 the path depth to keep,
# returns the root of the path for the given path depth. No leading '/' in path returned (FIXME).
# $1 can contain a list of space-separated paths. In that case function will returns the list of root paths for the path depth given.
# note that if $2 is worth 0 and path starts with the root '/' char, this '/' root path is returned
override pathroot = $(foreach path,$1,$(if $(value _$(strip $2)),$(subst $(_SPACE_)/,/,$(wordlist 1,$2,$(subst /, /,$(path)))),$(filter /,$(patsubst /%,/,$1))))


# return the common root path of paths $1 and $2
# FIXME: would expect to get the leading '/' in returned path
override commonrootpath = \
$(eval _min := $$(call min,_$(call pathdepth,$1),_$(call pathdepth,$2)))$(eval _shortpath1 := $$(call pathroot,$1,$(_min)))$(eval _shortpath2 := $$(call pathroot,$2,$(_min)))$(call begin,$(subst $(_SPACE_),,$(foreach i,$(join $(addsuffix !,$(subst /, /,$(_shortpath1))),$(subst /, /,$(_shortpath2))),$(call not_empty,$(call first_equal_last,$(subst !, ,$(i)))))))

# $1 is an absolute canonical path (no ../ in path), $2 the depth to remove, returns leaf of the path
# on linux, operation will always remove the "/" root character.
override pathleaf = $(patsubst /%,%,$(foreach path,$1,$(subst $(_SPACE_)/,/,$(wordlist $(call add,_$2,_1),$(_MAX_INT),$(subst /, /,$(path))))))

# given $1 the reference absolute path and $2 a list of paths to adapt, returns for each element of $2 the path relative to the common root of $1 and $2
# $3 is an output parameter on which the common root path values will be appended
override relativepath = \
$(foreach path,$2,$(call pathleaf,$(path),$(call pathdepth,$(eval override $3 += $(call store,$(call commonrootpath,$1,$(path)),_rootpath))$(_rootpath))))


ifneq (,$(BUILD_FWK_UT))
.PHONY:
default::;


$(info Unit testing pathroot)
$(info ---------------------)
$(call test_function,$$(call pathroot,/a/b/c/d/ef,0),/)
$(call test_function,$$(call pathroot,a/b/c/d/ef,0),)
$(call test_function,$$(call pathroot,/a/b/c/d/ef,1),/a)
$(call test_function,$$(call pathroot,a/b/c/d/ef,4),a/b/c/d)
$(call test_function,$$(call pathroot,a/b/c a/b/c/d/ef,4),a/b/c a/b/c/d)
$(call test_function,$$(call pathroot,/a/b/c/d/ef,5),/a/b/c/d/ef)
$(call test_function,$$(call pathroot,,0),)
$(call test_function,$$(call pathroot,,1),)
$(call test_function,$$(call pathroot,c$(_COLON_)/b/c/d/ef,0),)
$(call test_function,$$(call pathroot,c$(_COLON_)/b/c/d/ef,1),c$(_COLON_))
$(info pathroot UT done)

$(info Unit testing commonrootpath)
$(info ---------------------------)
$(call test_function,$$(call commonrootpath,/a/b/c/d/ef,/a),/a)
$(call test_function,$$(call commonrootpath,/a/b/c/d/ef,/),/)
$(call test_function,$$(call commonrootpath,/a/b/c/d/ef,a),)
$(call test_function,$$(call commonrootpath,/a/b/c/d/ef,/a/b/c/d/ef),/a/b/c/d/ef)
$(call test_function,$$(call commonrootpath,/a/b/c/d/,/a/b/c/d/ef),/a/b/c/d)
$(call test_function,$$(call commonrootpath,a/b/c/d/ef,),)
$(call test_function,$$(call commonrootpath,c$(_COLON_)/b/c/d/ef,c$(_COLON_)/),c$(_COLON_))
$(info commonrootpath UT done)

$(info Unit testing pathdepth)
$(info ----------------------)
$(call test_function,$$(call pathdepth,/a/b/c/d/ef),5)
$(call test_function,$$(call pathdepth,a/b/c/d/ef),5)
$(call test_function,$$(call pathdepth,/a/b/),2)
$(call test_function,$$(call pathdepth,/a/b),2)
$(call test_function,$$(call pathdepth,/a),1)
$(call test_function,$$(call pathdepth,/),0)
$(call test_function,$$(call pathdepth,),)
$(call test_function,$$(call pathdepth,  ),)
$(call test_function,$$(call pathdepth,c$(_COLON_)/b/c/d/ef),5)
$(call test_function,$$(call pathdepth,c$(_COLON_)/b/c/d/ef /a/b/c/d/ef  a),5 5 1)
$(info pathdepth UT done)

$(info Unit testing pathleaf)
$(info ---------------------)
$(call test_function,$$(call pathleaf,/a/b/c/d/ef,0),a/b/c/d/ef)
$(call test_function,$$(call pathleaf,a/b/c/d/ef,0),a/b/c/d/ef)
$(call test_function,$$(call pathleaf,/a/b/c/d/ef,1),b/c/d/ef)
$(call test_function,$$(call pathleaf,/a/b/c/d/ef,4),ef)
$(call test_function,$$(call pathleaf,/a/b/c/d/ef,5),)
$(call test_function,$$(call pathleaf,/a/b/c/d/ef,6),)
$(call test_function,$$(call pathleaf,,5),)
$(call test_function,$$(call pathleaf,c$(_COLON_)/b/c/d/ef,0),c$(_COLON_)/b/c/d/ef)
$(call test_function,$$(call pathleaf,c$(_COLON_)\b\c\d\ef,0),c$(_COLON_)\b\c\d\ef)
$(call test_function,$$(call pathleaf,c$(_COLON_)/b/c/d/ef,1),b/c/d/ef)
$(info pathleaf UT done)

$(info Unit testing relativepath)
$(info -------------------------)
$(call test_function,$$(call relativepath,c$(_COLON_)/toto/le/heros/,c$(_COLON_)/toto/le/petit/bonhomme c$(_COLON_)/toto/le/heros/a,_result),petit/bonhomme a)
$(call test_function,$$(_result),c$(_COLON_)/toto/le c$(_COLON_)/toto/le/heros)
override _result :=
$(call test_function,$$(call relativepath,/toto/le/heros/,/toto/le/petit /toto/le/heros/a /toto/le/petit/a,_result),petit a petit/a)
$(call test_function,$$(_result), /toto/le /toto/le/heros /toto/le)
$(info relativepath UT done)


endif

endif
