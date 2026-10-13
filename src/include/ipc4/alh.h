/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/**
 * \file include/ipc4/alh.h
 * \brief IPC4 Audio Hub Link (ALH) and SoundWire multi-gateway definitions.
 * \ingroup ipc4_gateway_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_ALH_H__
#define __SOF_IPC4_ALH_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <ipc4/gateway.h>

/** \brief Maximum number of single ALH channels supported in multi-gateway aggregation. */
#define IPC4_ALH_MAX_NUMBER_OF_GTW 16
#ifdef CONFIG_DMA_INTEL_ADSP_GPDMA
/** \brief DAI index offset for GPDMA platforms. */
#define IPC4_ALH_DAI_INDEX_OFFSET 7
#else
/** \brief DAI index offset for non-GPDMA platforms. */
#define IPC4_ALH_DAI_INDEX_OFFSET 0
#endif

#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_CAVS) || \
	defined(CONFIG_SOC_ACE15_MTPM)
/** \brief Number of bidirectional ALH links on CAVS/ACE1.5. */
#define IPC4_DAI_NUM_ALH_BI_DIR_LINKS	16
/** \brief Number of bidirectional ALH link groups. */
#define IPC4_DAI_NUM_ALH_BI_DIR_LINKS_GROUP	4
#else
/** \brief Number of bidirectional ALH links on non-CAVS platforms. */
#define IPC4_DAI_NUM_ALH_BI_DIR_LINKS	0
/** \brief Number of bidirectional ALH link groups on non-CAVS platforms. */
#define IPC4_DAI_NUM_ALH_BI_DIR_LINKS_GROUP	0
#endif

/**
 * \brief Compute ALH DAI index from group and codec identifiers.
 *
 * copier id = (group id << 4) + codec id + IPC4_ALH_DAI_INDEX_OFFSET
 * dai_index = (group id << 8) + codec id;
 */
#define IPC4_ALH_DAI_INDEX(x) ((((x) & 0xF0) << IPC4_DAI_NUM_ALH_BI_DIR_LINKS_GROUP) + \
				(((x) & 0xF) - IPC4_ALH_DAI_INDEX_OFFSET))

/** \brief Base virtual index marking the beginning of aggregated multi-gateway addressing. */
#define IPC4_ALH_MULTI_GTW_BASE 0x50

/**
 * \brief Check if an IPC4 connector node ID corresponds to an aggregated multi-gateway.
 * \ingroup ipc4_gateway_api
 * \param node_id Gateway connector node ID.
 * \return True if the node represents an aggregated multi-gateway, false otherwise.
 */
static inline bool is_multi_gateway(const union ipc4_connector_node_id node_id)
{
	return node_id.f.v_index >= IPC4_ALH_MULTI_GTW_BASE;
}

/**
 * \brief Multi-gateway channel aggregation configuration.
 * \ingroup ipc4_gateway_api
 */
struct ipc4_alh_multi_gtw_cfg {
	uint32_t count; /**< Number of single channels in mapping array. */
	/** Single-to-multi aggregation mapping table. */
	struct {
		uint32_t alh_id;       /**< Virtual index (v_index) of single ALH channel. */
		uint32_t channel_mask; /**< Bitmask of audio channels mapped to this ALH endpoint. */
	} mapping[IPC4_ALH_MAX_NUMBER_OF_GTW]; /**< Array of mapping items. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Full configuration blob for Audio Hub Link (ALH) gateways.
 * \ingroup ipc4_gateway_api
 */
struct sof_alh_configuration_blob {
	union ipc4_gateway_attributes gtw_attributes; /**< Gateway memory placement attributes. */
	struct ipc4_alh_multi_gtw_cfg alh_cfg;        /**< Multi-gateway channel aggregation mappings. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Calculate the serialized size of an ALH configuration blob.
 * \ingroup ipc4_gateway_api
 * \param alh_blob Pointer to the ALH configuration structure.
 * \return Size in bytes of the configuration blob.
 */
static inline size_t
get_alh_config_size(const struct sof_alh_configuration_blob *alh_blob)
{
	return sizeof(alh_blob->gtw_attributes) +
	       sizeof(alh_blob->alh_cfg.count) +
	       sizeof(alh_blob->alh_cfg.mapping[0]) * alh_blob->alh_cfg.count;
}

#endif
