/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

/**
 * \file include/sof/probe/probe.h
 * \brief Audio probe injection and extraction tap points and DMA manager.
 * \ingroup audio_probe_api
 */

#ifndef __SOF_PROBE_PROBE_H__
#define __SOF_PROBE_PROBE_H__

#include <ipc/probe.h>
#include <sys/types.h>

/**
 * \brief Callback hook invoked when a chunk of probe logging data is ready.
 * \ingroup audio_probe_api
 * @param buffer Pointer to raw probe buffer payload.
 * @param length Byte length of data available.
 * @return Number of bytes processed, or negative error code.
 */
typedef ssize_t(*probe_logging_hook_t)(uint8_t *buffer, size_t length);

#if CONFIG_LOG_BACKEND_SOF_PROBE
const struct log_backend *log_backend_probe_get(void);
bool probe_is_backend_configured(void);
#endif

/**
 * \brief Initialize the probe logging backend.
 * \ingroup audio_probe_api
 * @param hook Callback invoked when new logging data is written out.
 */
void probe_logging_init(probe_logging_hook_t hook);

/**
 * \brief Initialize the audio probes subsystem.
 * \ingroup audio_probe_api
 * @param[in] extraction_probe_dma Host DMA channel configured for data extraction,
 *                                 or NULL if extraction probes are not used.
 * @return 0 on success, or negative error code on failure.
 */
int probe_init(const struct probe_dma *extraction_probe_dma);

/**
 * \brief Deinitialize the probes subsystem and detach extraction DMA.
 * \ingroup audio_probe_api
 * @return 0 on success, or -EINVAL if active probe points remain connected.
 */
int probe_deinit(void);

/**
 * \brief Configure injection DMA channels for inserting audio streams into pipelines.
 * \ingroup audio_probe_api
 * @param[in] count Number of DMA descriptors provided in array.
 * @param[in] probe_dma Array of probe DMA descriptors specifying stream tags and buffer sizes.
 * @return 0 on success, or negative error code on failure.
 */
int probe_dma_add(uint32_t count, const struct probe_dma *probe_dma);

/**
 * \brief Query configuration information for active injection DMAs.
 * \ingroup audio_probe_api
 * @param[out] data Reply payload structure populated with connected DMA details.
 * @param[in] max_size Maximum available capacity in data buffer in bytes.
 * @return 0 on success, or negative error code on failure.
 */
int probe_dma_info(struct sof_ipc_probe_info_params *data, uint32_t max_size);

/**
 * \brief Disconnect and remove active injection DMA channels.
 * \ingroup audio_probe_api
 * @param[in] count Number of stream tags in array.
 * @param[in] stream_tag Array of host DMA stream tags to remove.
 * @return 0 on success, or negative error code on failure.
 */
int probe_dma_remove(uint32_t count, const uint32_t *stream_tag);

/**
 * \brief Attach probe points to audio pipeline buffers for stream tap/injection.
 * \ingroup audio_probe_api
 * @param[in] count Number of probe points in array.
 * @param[in] probe Array of probe point descriptors specifying buffer IDs and purpose.
 * @return 0 on success, or negative error code on failure.
 */
int probe_point_add(uint32_t count, const struct probe_point *probe);

/**
 * \brief Query configuration of currently active probe points.
 * \ingroup audio_probe_api
 * @param[out] data Reply payload structure populated with connected probe points.
 * @param[in] max_size Maximum available capacity in data buffer in bytes.
 * @return 0 on success, or negative error code on failure.
 */
int probe_point_info(struct sof_ipc_probe_info_params *data, uint32_t max_size);

/**
 * \brief Detach and remove probe points from pipeline buffers.
 * \ingroup audio_probe_api
 * @param[in] count Number of buffer IDs in array.
 * @param[in] buffer_id Array of buffer IDs from which probe points should be detached.
 * @return 0 on success, or negative error code on failure.
 */
int probe_point_remove(uint32_t count, const uint32_t *buffer_id);

/**
 * \brief Retrieve global probes private data context from SOF runtime.
 * \ingroup audio_probe_api
 * \return Pointer to struct probe_pdata.
 */
static inline struct probe_pdata *probe_get(void)
{
	return sof_get()->probe;
}

#endif /* __SOF_PROBE_PROBE_H__ */
