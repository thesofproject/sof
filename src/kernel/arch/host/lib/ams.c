// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Krzysztof Frydryk <krzysztofx.frydryk@intel.com>

/**
 * \file
 * \brief Xtensa Asynchronous Messaging Service implementation file
 * \authors Krzysztof Frydryk <krzysztofx.frydryk@intel.com>
 */

#include <sof/lib/cpu.h>
#include <sof/lib/ams.h>
#include <xtos-structs.h>

static struct async_message_service *host_ams;

struct async_message_service **arch_ams_get(void)
{
	return &host_ams;
}
