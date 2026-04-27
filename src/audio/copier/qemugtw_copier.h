/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_COPIER_QEMUGTW_COPIER_H__
#define __SOF_AUDIO_COPIER_QEMUGTW_COPIER_H__

#include <stdint.h>
#include <sof/audio/module_adapter/module/generic.h>
#include "copier.h"

struct ipc4_qemu_gateway_config_blob {
	/* 0: sine, 1: square, 2: saw, 3: linear increment */
	uint32_t signal_type;
	uint32_t amplitude;
	uint32_t frequency;
} __attribute__((packed, aligned(4)));

struct qemugtw_data {
	struct comp_dev *dev;
	union ipc4_connector_node_id node_id;
	struct list_item item;       /* list in qemugtw_list_head */
	uint32_t buf_size;

	/* Signal generation/validation state */
	struct ipc4_qemu_gateway_config_blob config;
	uint32_t phase;              /* Current phase for synthesis/validation */
	uint32_t counter;            /* Sample counter for linear increment */
	uint32_t error_count;
	uint32_t validated_bytes;
};

/* Creates the qemu gateway in the copier module */
int copier_qemugtw_create(struct processing_module *mod,
			  const struct ipc4_copier_module_cfg *copier,
			  struct pipeline *pipeline);

/* Handles copier parameter updates for qemu gateway */
int copier_qemugtw_params(struct qemugtw_data *qemugtw_data, struct comp_dev *dev,
			  struct sof_ipc_stream_params *params);

/* Copier processing function for qemu gateway */
int copier_qemugtw_process(struct comp_dev *dev);

/* Resets the qemu gateway buffers */
void copier_qemugtw_reset(struct comp_dev *dev);

/* Frees the qemu gateway */
void copier_qemugtw_free(struct processing_module *mod);

#endif /* __SOF_AUDIO_COPIER_QEMUGTW_COPIER_H__ */
