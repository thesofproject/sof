/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __TESTBENCH_FILE_IPC4_H_
#define __TESTBENCH_FILE_IPC4_H_

#include <ipc/topology.h>
#include <ipc4/base-config.h>

struct ipc4_file_config {
	uint32_t rate;
	uint32_t channels;
	char *fn;
	uint32_t mode;
	uint32_t frame_fmt;
	uint32_t direction;	/**< SOF_IPC_STREAM_ */
};

struct ipc4_file_module_cfg {
	struct ipc4_base_module_cfg base_cfg;
	struct ipc4_file_config config;
} __packed __aligned(8);

#endif /*  __TESTBENCH_FILE_IPC4_H_ */
