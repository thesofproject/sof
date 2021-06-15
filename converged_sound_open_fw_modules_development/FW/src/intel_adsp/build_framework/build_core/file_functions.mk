# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# This makefile provides some functions for files path manipulation
# Functions defined below shall be OS-independent (we don't want to include host.mk here)
# _BUILD_CORE_DIR indicates the path to directory of this current file

ifndef _FILE_FUNCTIONS_MK_
override _FILE_FUNCTIONS_MK_ :=1


override _BUILD_CORE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))


include $(_BUILD_CORE_DIR)stddef.mk
include $(_BUILD_CORE_DIR)error_functions.mk
include $(_BUILD_CORE_DIR)arithmetic.mk


# some default definitions
CPP_FILE_EXTENSIONS ?= .cc .cpp
C_FILE_EXTENSIONS ?= .c
ASM_FILE_EXTENSIONS ?= .S



# Returns the language acronym aka C,CXX,A (for respectively C, C++, assembly language)
# regarding  the given qualified object file (or files list)
get_objfile_language = $(if $(filter $(call store,$(suffix $(patsubst %.o,%,$(notdir $1))),_suffix),$(C_FILE_EXTENSIONS)),C,$(if $(filter $(_suffix),$(CPP_FILE_EXTENSIONS)),CXX,$(if $(filter $(_suffix),$(ASM_FILE_EXTENSIONS)),A,)))


# Returns the language flag parameter name CFLAGS,CXXFLAGS,AFLAGS
# regarding  the given qualified object file (or files list)
# return nothing if object file is not qualified
get_objfile_flags_name = $(if $(call store,$(call get_objfile_language,$1),_result),$(_result)FLAGS,)


# remove the language file extension (.cc, .ccp, .c, etc) for the given qualified object file(s) in $1
unqualify_obj_file = $(addsuffix .o,$(basename $(patsubst %.o,%,$1)))


ifneq (,$(BUILD_FWK_UT))
.PHONY:
default::;


$(info Unit testing get_objfile_language)
$(info ---------------------------------)
$(call test_function,$$(call get_objfile_language,my_path/my_file.c.o),C)
$(call test_function,$$(call get_objfile_language,my_path/my_file.cc.o),CXX)
$(call test_function,$$(call get_objfile_language,my_path/my_file.S.o),A)
$(call test_function,$$(call get_objfile_language,my_path/my_file.o),)
$(call test_function,$$(call get_objfile_language,my_file.cpp.o),CXX)
$(info get_objfile_language UT done)

$(info Unit testing get_objfile_flags_name)
$(info -----------------------------------)
$(call test_function,$$(call get_objfile_flags_name,my_path/my_file.c.o),CFLAGS)
$(call test_function,$$(call get_objfile_flags_name,my_path/my_file.cc.o),CXXFLAGS)
$(call test_function,$$(call get_objfile_flags_name,my_path/my_file.S.o),AFLAGS)
$(call test_function,$$(call get_objfile_flags_name,my_path/my_file.o),)
$(call test_function,$$(call get_objfile_flags_name,my_file.cpp.o),CXXFLAGS)
$(info get_objfile_flags_name UT done)


endif

endif
