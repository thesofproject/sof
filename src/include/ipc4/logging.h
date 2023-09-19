/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_IPC4_LOGGING_H__
#define __SOF_IPC4_LOGGING_H__

#define SOF_IPC4_LOGGING_MTRACE_PAGE_SIZE	0x1000

#if CONFIG_LIBRARY
static inline int ipc4_logging_enable_logs(bool first_block,
					   bool last_block,
					   uint32_t data_offset_or_size,
					   const char *data)
{
	return 0;
}
#else
int ipc4_logging_enable_logs(bool first_block,
			     bool last_block,
			     uint32_t data_offset_or_size,
			     const char *data);
#endif

int ipc4_logging_shutdown(void);

#endif
