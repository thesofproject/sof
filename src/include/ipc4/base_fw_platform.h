/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
 */

/**
 * \file include/ipc4/base_fw_platform.h
 * \brief Platform specific IPC4 base firmware functionality.
 */

#ifndef __SOF_IPC4_BASE_FW_PLATFORM_H__
#define __SOF_IPC4_BASE_FW_PLATFORM_H__

/**
 * \brief Platform specific routine to add data tuples to HW_CONFIG
 * structure sent to host via IPC.
 * \param[out] data_offset data offset after tuples added
 * \parma[in] data pointer where to add new entries
 * \return 0 if successful, error code otherwise.
 */
int platform_basefw_hw_config(uint32_t *data_offset, char *data);

/**
 * \brief Platform specific routine which return the pointer to
 * the boot base manifest.
 * \return pointer to struct if successful, null otherwise.
 */
struct sof_man_fw_desc *platform_base_fw_get_manifest(void);

#endif /* __SOF_IPC4_BASE_FW_PLATFORM_H__ */
