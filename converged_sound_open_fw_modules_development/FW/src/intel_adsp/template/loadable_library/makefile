# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

################################################################################
# Rational:
#
# Makefile for building of a loadable library. a loadable library is
# normally composed by a set of N precompiled loadable modules.
#
# What you must provide in this file is:
#
# 1) The path to this makefile.
#
# 2) The name of loadable library.
#
# 3) For each module declaration you must provide:
#    3.1) The number of allowed instances of the module
#    3.2) The directory and the name of the compiled module.
#
#    See below for a concrete example.
#
# 4) You must add your module(s) to the variable MODULES_LIST
#
#
# To run the makefile you should position in its directory and execute
# make passing as parameter the target platform. Example:
#
# prompt@>make sunrisepoint
#
# More information about supported targets, configurations and platforms
# can be found by executing:
#
# prompt@>make usage
#
###################################################################################


# _CURRENT_MAKEFILE indicates the relative path to this current file
override _CURRENT_MAKEFILE := $(lastword $(MAKEFILE_LIST))
# _LOADABLE_LIBRARY_DIR indicates the relative path to directory of this current file
override _LOADABLE_LIBRARY_DIR := $(dir $(_CURRENT_MAKEFILE))
# INTEL_ADSP_DIR indicates the path to the directory of the intel_adsp installation
# value can be overriden on commmand line
INTEL_ADSP_DIR := $(_LOADABLE_LIBRARY_DIR)../../


# Define MANDATORY VARIABLES for generation of a loadable library
#<--
# LOADABLE_LIBRARY_BASE_ADDRESS indicates the start address in memory of the library to build
# given default value is unrelevant and shall be overriden on command line
LOADABLE_LIBRARY_BASE_ADDRESS ?= 0x0
# LOADABLE_LIBRARY_AVAILABLE_MEMORY_SIZE indicates the available size of the memory for the library to build
# given default value is unrelevant and shall be overriden on command line
LOADABLE_LIBRARY_AVAILABLE_MEMORY_SIZE ?= 0xFFFFFFFF
# path to the top makefile file
TOP_MAKEFILE ?= $(_CURRENT_MAKEFILE)
# name of the loadable library
LOADABLE_LIBRARY_NAME :=  ___TEMPLATE_FIELD_LIBRARY_NAME_LOWER_SNAKE_CASE___
# list of modules to include in the loadable library
MODULES_LIST := ___TEMPLATE_FIELD_MODULES_LIST___


___TEMPLATE_FIELD_MODULES_MAKEFILE_SECTION___
#-->


include $(INTEL_ADSP_DIR)build_framework/loadable_library.mk
