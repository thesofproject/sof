/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_IPC4_ALH_H__
#define __SOF_IPC4_ALH_H__

#define ALH_MAX_NUMBER_OF_GTW   16

struct AlhMultiGtwCfg {
	/* Number of single channels (valid items in mapping array). */
	uint32_t count;
	/* Single to multi aggregation mapping item. */
	struct {
		/* Vindex of a single ALH channel aggregated. */
		uint32_t alh_id;
		/* Channel mask */
		uint32_t channel_mask;
	} mapping[ALH_MAX_NUMBER_OF_GTW]; /* < Mapping items */
} __attribute__((packed, aligned(4)));

struct sof_alh_configuration_blob {
	GatewayAttributes gtw_attributes;
	struct AlhMultiGtwCfg alh_cfg;
} __attribute__((packed, aligned(4)));

#endif
