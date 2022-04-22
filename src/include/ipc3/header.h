/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/**
 * \file include/ipc3/header.h
 * \brief IPC3 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC3_HEADER_H__
#define __SOF_IPC3_HEADER_H__

#include <stdint.h>

#define ipc_from_hdr(x) ((struct sof_ipc_cmd_hdr *)x)

/**
 * \brief Generic message header. IPC MAJOR 3 version.
 * All IPC3 messages use this header as abstraction
 * to platform specific calls.
 */
struct ipc_cmd_hdr {
	uint32_t dat[2];
};

#endif
