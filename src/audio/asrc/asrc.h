/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_ASRC_H__
#define __SOF_ASRC_H__

#ifdef CONFIG_IPC_MAJOR_4

typedef struct ipc4_asrc_module_cfg ipc_asrc_cfg;

#else /*IPC3 version*/
typedef struct ipc_config_asrc ipc_asrc_cfg;

#endif
#endif
