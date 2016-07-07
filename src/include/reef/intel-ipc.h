/*
 * BSD 3 Clause - See LICENCE file for details.
 *
 */

#ifndef __INCLUDE_INTEL_IPC_H__
#define __INCLUDE_INTEL_IPC_H__

#include <stdint.h>

/* private data for IPC */
struct intel_ipc_data {
	/* DMA */
	struct dma *dmac0;
	uint8_t *page_table;
	completion_t complete;

	/* PM */
	int pm_prepare_D3;	/* do we need to prepare for D3 */
};

uint32_t ipc_cmd(void);

#endif
