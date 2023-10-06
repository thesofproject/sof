/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2021 - 2023 Intel Corporation. All rights reserved.
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
 * \file include/ipc4/base-config.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_BASE_CONFIG_H__
#define __SOF_IPC4_BASE_CONFIG_H__

#include <sof/compiler_attributes.h>
#include <module/ipc4/base-config.h>

struct sof_ipc_stream_params;
void ipc4_base_module_cfg_to_stream_params(const struct ipc4_base_module_cfg *base_cfg,
					   struct sof_ipc_stream_params *params);
struct comp_buffer;
void ipc4_update_buffer_format(struct comp_buffer *buf_c,
			       const struct ipc4_audio_format *fmt);
struct sof_source;
void ipc4_update_source_format(struct sof_source *source,
			       const struct ipc4_audio_format *fmt);
struct sof_sink;
void ipc4_update_sink_format(struct sof_sink *sink,
			     const struct ipc4_audio_format *fmt);

#endif
