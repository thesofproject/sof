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

#ifndef __SOF_IPC4_BASE_FW_VENDOR_H__
#define __SOF_IPC4_BASE_FW_VENDOR_H__

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

struct comp_dev;

/*
 * TODO: Add a more generic BASE_FW_VENDOR Kconfig if/when
 *       more vendor specific implementations are added.
 */
#ifdef CONFIG_IPC4_BASE_FW_INTEL

/**
 * \brief Vendor specific routine to add data tuples to FW_CONFIG
 * structure sent to host via IPC.
 * \param[out] data_offset data offset after tuples added
 * \param[in] data pointer where to add new entries
 * \return 0 if successful, error code otherwise.
 */
int basefw_vendor_fw_config(uint32_t *data_offset, char *data);

/**
 * \brief Vendor specific routine to add data tuples to HW_CONFIG
 * structure sent to host via IPC.
 * \param[out] data_offset data offset after tuples added
 * \param[in] data pointer where to add new entries
 * \return 0 if successful, error code otherwise.
 */
int basefw_vendor_hw_config(uint32_t *data_offset, char *data);

/**
 * \brief Vendor specific routine which return the pointer to
 * the boot base manifest.
 * \return pointer to struct if successful, null otherwise.
 */
struct sof_man_fw_desc *basefw_vendor_get_manifest(void);

/**
 * \brief Vendor specific routine to get information about modules.
 * Function add information and sent to host via IPC.
 * \param[out] data_offset data offset after structure added
 * \param[in] data pointer where to add new entries
 * \return 0 if successful, error code otherwise.
 */
int basefw_vendor_modules_info_get(uint32_t *data_offset, char *data);

/**
 * \brief Implement vendor specific parameter for basefw module.
 * This function is called for parameters not handled by
 * generic base_fw code.
 */
int basefw_vendor_get_large_config(struct comp_dev *dev,
				   uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t *data_offset,
				   char *data);

/**
 * \brief Implement vendor specific parameter for basefw module.
 * This function is called for parameters not handled by
 * generic base_fw code.
 */
int basefw_vendor_set_large_config(struct comp_dev *dev,
				   uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t data_offset,
				   const char *data);

#else /* !CONFIG_IPC4_BASE_FW_INTEL */

static inline int basefw_vendor_fw_config(uint32_t *data_offset, char *data)
{
	*data_offset = 0;

	return 0;
}

static inline int basefw_vendor_hw_config(uint32_t *data_offset, char *data)
{
	*data_offset = 0;

	return 0;
}

static inline struct sof_man_fw_desc *basefw_vendor_get_manifest(void)
{
	struct sof_man_fw_desc *desc = NULL;

	return desc;
}

static inline int basefw_vendor_modules_info_get(uint32_t *data_offset, char *data)
{
	*data_offset = 0;

	return 0;
}

static inline int basefw_vendor_get_large_config(struct comp_dev *dev,
						 uint32_t param_id,
						 bool first_block,
						 bool last_block,
						 uint32_t *data_offset,
						 char *data)
{
	return -EINVAL;
}

static inline int basefw_vendor_set_large_config(struct comp_dev *dev,
						 uint32_t param_id,
						 bool first_block,
						 bool last_block,
						 uint32_t data_offset,
						 const char *data)
{
	return IPC4_UNKNOWN_MESSAGE_TYPE;
}

#endif

#endif /* __SOF_IPC4_BASE_FW_VENDOR_H__ */
