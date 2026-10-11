/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/**
 * @file ipc4/logging.h
 * @brief IPC4 host logging control and configuration interface.
 * @ingroup ipc4_api
 */

#ifndef __SOF_IPC4_LOGGING_H__
#define __SOF_IPC4_LOGGING_H__

#define SOF_IPC4_LOGGING_MTRACE_PAGE_SIZE	0x1000

/**
 * @brief Configure and enable firmware logging requested by the host via IPC4.
 * @param first_block True if this buffer is the first chunk of logging configuration.
 * @param last_block True if this buffer is the final chunk of logging configuration.
 * @param data_offset_or_size Byte offset or payload size of the configuration block.
 * @param data Pointer to configuration payload buffer.
 * @return 0 on success, negative error code on failure.
 */
int ipc4_logging_enable_logs(bool first_block,
			     bool last_block,
			     uint32_t data_offset_or_size,
			     const char *data);

/**
 * @brief Shutdown active IPC4 firmware logging and flush remaining trace buffers.
 * @return 0 on success, negative error code on failure.
 */
int ipc4_logging_shutdown(void);

#endif
