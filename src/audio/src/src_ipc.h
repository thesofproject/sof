/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017-2024 Intel Corporation
 */

#ifndef __SOF_AUDIO_SRC_SRC_IPC_H__
#define __SOF_AUDIO_SRC_SRC_IPC_H__

#include <sof/audio/ipc-config.h>
#include <ipc4/base-config.h>
#include <stdint.h>

/* src component private data */
struct ipc4_config_src {
	struct ipc4_base_module_cfg base;
	uint32_t sink_rate;
};

#endif /* __SOF_AUDIO_SRC_SRC_IPC_H__ */
