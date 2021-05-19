# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.


# _CURRENT_MAKEFILE indicates the relative path to this current file
override _CURRENT_MAKEFILE := $(lastword $(MAKEFILE_LIST))
# _AMPLIFIER_DIR indicates the relative path to directory of the this current file
override _AMPLIFIER_DIR := $(dir $(_CURRENT_MAKEFILE))
# INTEL_ADSP_DIR indicates the path to the directory of the intel_adsp installation
INTEL_ADSP_DIR := $(_AMPLIFIER_DIR)../../../FW/src/intel_adsp/

# define MANDATORY VARIABLES for generation of a module package
#<--
# path to this makefile file
TOP_MAKEFILE ?= $(_CURRENT_MAKEFILE)
# name of the binary file to generate
MODULE_FILENAME := amplifier_module
# SRC_DIRS indicates the list of directories to look for source files
# Requirements on source directory path:
# * shall be absolute path or UNC path
# * can be relative path if all paths are below same drive letter (windows specific) (otherwise some possible name clash can happen)
# * shall not contain spaces
# * shall end with '/'
SRC_DIRS := $(_AMPLIFIER_DIR)../
#-->

# define OPTIONAL VARIABLES for generation of a module package
#<--
# CXX_INCLUDES indicates the list of include directories for c++  files
CXX_INCLUDES := $(_AMPLIFIER_DIR)../

# other possible variables are indicated at beginning of file module_package.mk
#
# ROOT_OUT_DIR define the output directory where intermediate and target files will be generated
# by default ROOT_OUT_DIR is set to generate files in some "out" directory right below the current execution directory.
# ROOT_OUT_DIR := my_out/
#-->

# following parameter shall be set to 1 for FDK module
IS_SELF_CONTAINED := 1


include $(INTEL_ADSP_DIR)build_framework/module_package.mk
