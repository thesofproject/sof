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
 */

/**
 * \file include/ipc4/alh.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_ALH_H__
#define __SOF_IPC4_ALH_H__

#include <stdint.h>
#include <ipc4/gateway.h>

#define ALH_MAX_NUMBER_OF_GTW   16

struct ipc4_alh_multi_gtw_cfg {
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
	union ipc4_gateway_attributes gtw_attributes;
	struct ipc4_alh_multi_gtw_cfg alh_cfg;
} __attribute__((packed, aligned(4)));

#endif
