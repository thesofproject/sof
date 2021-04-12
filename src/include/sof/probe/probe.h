/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __SOF_PROBE_PROBE_H__
#define __SOF_PROBE_PROBE_H__

#if CONFIG_PROBE

#include <ipc/probe.h>

/*
 * \brief Initialize probes subsystem
 *
 * param[in,optional] extraction_probe_dma - DMA associated with extraction
 *		      In case extraction_probe_dma is NULL, extraction probes
 *		      are unavailable.
 */
int probe_init(struct probe_dma *extraction_probe_dma);

/*
 * \brief Deinitialize probes subsystem.
 *
 * Detach extraction DMA if was enabled. Return -EINVAL in case some probes
 * are still in use.
 */
int probe_deinit(void);

/*
 * \brief Setup injection DMAs for probes.
 *
 * param[in] count - number of DMAs configured during this call
 * param[in] probe_dma - Array of size 'count' with configuration data for DMAs
 */
int probe_dma_add(uint32_t count, struct probe_dma *probe_dma);

/*
 * \brief Get info about connected injection DMAs
 *
 * param[in,out] data - reply to write data to
 * param[in] max_size - maximum number of bytes available in data
 */
int probe_dma_info(struct sof_ipc_probe_info_params *data, uint32_t max_size);

/*
 * \brief Remove injection DMAs
 *
 * param[in] count - number of DMAs removed during this call
 * param[in] stream_tag - array for size 'count' with stream tags associated
 *			  with DMAs to be removed
 */
int probe_dma_remove(uint32_t count, uint32_t *stream_tag);

/*
 * \brief Set probe points
 *
 * param[in] count - number of probe points configured this call
 * param[in] probe - array of size 'count' with configuration of probe points
 */
int probe_point_add(uint32_t count, struct probe_point *probe);

/*
 * \brief Get info about connected probe points
 *
 * param[in,out] data - reply to write data to
 * param[in] max_size - maximum number of bytes available in data
 */
int probe_point_info(struct sof_ipc_probe_info_params *data, uint32_t max_size);

/*
 * \brief Remove probe points
 *
 * param[in] count - number of probe points removed this call
 * param[in] buffer_id - array of size 'count' with IDs of buffers to which
 *			 probes were attached
 */
int probe_point_remove(uint32_t count, uint32_t *buffer_id);

/**
 * \brief Retrieves probes structure.
 * \return Pointer to probes structure.
 */
static inline struct probe_pdata *probe_get(void)
{
	return sof_get()->probe;
}

#endif /* CONFIG_PROBE */

#endif /* __SOF_PROBE_PROBE_H__ */
