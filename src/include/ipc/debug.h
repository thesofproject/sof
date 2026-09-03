/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#ifndef __IPC_DEBUG_H__
#define __IPC_DEBUG_H__

#include <ipc/header.h>
#include <stdint.h>

/** ABI3.18 */
enum sof_ipc_dbg_mem_zone {
	SOF_IPC_MEM_ZONE_SYS		= 0,	/**< System zone */
	SOF_IPC_MEM_ZONE_SYS_RUNTIME	= 1,	/**< System-runtime zone */
	SOF_IPC_MEM_ZONE_RUNTIME	= 2,	/**< Runtime zone */
	SOF_IPC_MEM_ZONE_BUFFER		= 3,	/**< Buffer zone */
	SOF_IPC_MEM_ZONE_RUNTIME_SHARED	= 4,	/**< System runtime zone */
	SOF_IPC_MEM_ZONE_SYS_SHARED	= 5,	/**< System shared zone */
};

/** ABI3.18 */
struct sof_ipc_dbg_mem_usage_elem {
	uint32_t zone;		/**< see sof_ipc_dbg_mem_zone */
	uint32_t id;		/**< heap index within zone */
	uint32_t used;		/**< number of bytes used in zone */
	uint32_t free;		/**< number of bytes free to use within zone */
	uint32_t reserved;	/**< reserved for future use */
} __attribute__((packed, aligned(4)));

/** ABI3.18 */
struct sof_ipc_dbg_mem_usage {
	struct sof_ipc_reply rhdr;			/**< generic IPC reply header */
	uint32_t reserved[4];				/**< reserved for future use */
	uint32_t num_elems;				/**< elems[] counter */
	struct sof_ipc_dbg_mem_usage_elem elems[];	/**< memory usage information */
} __attribute__((packed, aligned(4)));

#endif /* __IPC_DEBUG_H__ */
