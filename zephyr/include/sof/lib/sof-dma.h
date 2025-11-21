/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2025, Intel Corporation.
 */

/* need to use sof-dma.h to avoid syscalls/dma.h name conflict */

#ifndef SOF_DMA_H
#define SOF_DMA_H

/**
 * \brief API to request a platform DMAC.
 *
 * Users can request DMAC based on dev type, copy direction, capabilities
 * and access privilege.
 * For exclusive access, ret DMAC with no channels draining.
 * For shared access, ret DMAC with the least number of channels draining.
 */
__syscall struct sof_dma *sof_dma_get(uint32_t dir, uint32_t caps, uint32_t dev, uint32_t flags);

struct sof_dma *z_impl_sof_dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags);


/**
 * \brief API to release a platform DMAC.
 *
 * @param[in] dma DMAC to release.
 */
__syscall void sof_dma_put(struct sof_dma *dma);

void z_impl_sof_dma_put(struct sof_dma *dma);

__syscall int sof_dma_get_attribute(struct sof_dma *dma, uint32_t type, uint32_t *value);

__syscall int sof_dma_request_channel(struct sof_dma *dma, uint32_t stream_tag);

__syscall void sof_dma_release_channel(struct sof_dma *dma,
				       uint32_t channel);

__syscall int sof_dma_config(struct sof_dma *dma, uint32_t channel,
			     struct dma_config *config);

__syscall int sof_dma_start(struct sof_dma *dma, uint32_t channel);

__syscall int sof_dma_stop(struct sof_dma *dma, uint32_t channel);

__syscall int sof_dma_get_status(struct sof_dma *dma, uint32_t channel, struct dma_status *stat);

__syscall int sof_dma_reload(struct sof_dma *dma, uint32_t channel, size_t size);

/* include definitions from generated file */
#include <zephyr/syscalls/sof-dma.h>

#endif
