/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
 */

/**
 * \file ipc4/base_fw_vendor.h
 * \brief Platform specific IPC4 base firmware functionality.
 * \ingroup ipc4_base_fw_api
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
 * \ingroup ipc4_base_fw_api
 * \param[out] data_offset Data offset after tuples added.
 * \param[in,out] data Pointer to destination buffer where to add new entries.
 * \return 0 if successful, error code otherwise.
 */
int basefw_vendor_fw_config(uint32_t *data_offset, char *data);

/**
 * \brief Vendor specific routine to add data tuples to HW_CONFIG
 * structure sent to host via IPC.
 * \ingroup ipc4_base_fw_api
 * \param[out] data_offset Data offset after tuples added.
 * \param[in,out] data Pointer to destination buffer where to add new entries.
 * \return 0 if successful, error code otherwise.
 */
int basefw_vendor_hw_config(uint32_t *data_offset, char *data);

/**
 * \brief Vendor specific routine which returns the pointer to
 * the boot base manifest.
 * \ingroup ipc4_base_fw_api
 * \return Pointer to struct sof_man_fw_desc if successful, NULL otherwise.
 */
struct sof_man_fw_desc *basefw_vendor_get_manifest(void);

/**
 * \brief Vendor specific routine to get information about modules.
 * Function adds module information sent to host via IPC.
 * \ingroup ipc4_base_fw_api
 * \param[out] data_offset Data offset after structure added.
 * \param[in,out] data Pointer to destination buffer where to add new entries.
 * \return 0 if successful, error code otherwise.
 */
int basefw_vendor_modules_info_get(uint32_t *data_offset, char *data);

/**
 * \brief Implement vendor specific parameter retrieval for basefw module.
 * This function is called for parameters not handled by generic base_fw code.
 * \ingroup ipc4_base_fw_api
 * \param[in] dev Pointer to base firmware component device.
 * \param[in] param_id Parameter ID requested.
 * \param[in] first_block True if this is the first block of large config.
 * \param[in] last_block True if this is the last block of large config.
 * \param[in,out] data_offset Offset into large configuration buffer.
 * \param[out] data Buffer to receive parameter payload.
 * \return 0 on success, negative error code on failure.
 */
int basefw_vendor_get_large_config(struct comp_dev *dev,
				   uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t *data_offset,
				   char *data);

/**
 * \brief Implement vendor specific parameter configuration for basefw module.
 * This function is called for parameters not handled by generic base_fw code.
 * \ingroup ipc4_base_fw_api
 * \param[in] dev Pointer to base firmware component device.
 * \param[in] param_id Parameter ID configured.
 * \param[in] first_block True if this is the first block of large config.
 * \param[in] last_block True if this is the last block of large config.
 * \param[in] data_offset Offset into large configuration buffer.
 * \param[in] data Pointer to parameter payload.
 * \return 0 on success, negative error code on failure.
 */
int basefw_vendor_set_large_config(struct comp_dev *dev,
				   uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t data_offset,
				   const char *data);

/**
 * \brief Vendor specific routine to configure DMA gateway.
 * \ingroup ipc4_base_fw_api
 * \param[in] node_id The node ID of the DMA gateway to configure.
 * \param[in] config_data Pointer to the configuration data.
 * \param[in] data_size Size of the configuration data in bytes.
 * \return 0 if successful, error code otherwise.
 */
int basefw_vendor_dma_control(uint32_t node_id,
			      const char *config_data,
			      size_t data_size);

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

static inline int basefw_vendor_dma_control(uint32_t node_id,
					    const char *config_data,
					    size_t data_size)
{
	return IPC4_UNKNOWN_MESSAGE_TYPE;
}

#endif

#endif /* __SOF_IPC4_BASE_FW_VENDOR_H__ */
