// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#include "core/kernel/logger/log.h"

#if defined(GCC_TOOLSCHAIN)
#define ATTRIBUTE __attribute__((section(".rodata#"))) = 0xF;
#else
#define ATTRIBUTE __attribute__((section(".rodata"))) = 0xF;
#endif

const volatile uint32_t LOGGER_LIB_ID_VAR_NAME ATTRIBUTE
