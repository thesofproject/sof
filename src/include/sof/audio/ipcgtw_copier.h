/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Baofeng Tian <baofeng.tian@intel.com>
 */

/**
 * \file audio/ipcgtw_copier.h
 * \brief ipcgtw copier shared header file
 * \authors Baofeng Tian <baofeng.tian@intel.com>
 */

#ifndef __SOF_IPCGTW_COPIER_H__
#define __SOF_IPCGTW_COPIER_H__

#include <sof/audio/component_ext.h>
#include <ipc4/gateway.h>
#include <sof/list.h>
#include <ipc/stream.h>

/* Host communicates with IPC gateways via global IPC messages. To address a particular
 * IPC gateway, its node_id is sent in message payload. Hence we need to keep a list of existing
 * IPC gateways and their node_ids to search for a gateway host wants to address.
 */
struct ipcgtw_data {
	union ipc4_connector_node_id node_id;
	struct comp_dev *dev;
	struct list_item item;

	/* IPC gateway buffer size comes in blob at creation time, we keep size here
	 * to resize buffer later at ipcgtw_params().
	 */
	uint32_t buf_size;
};

int ipcgtw_zephyr_params(struct ipcgtw_data *ipcgtw_data, struct comp_dev *dev,
			 struct sof_ipc_stream_params *params);

void ipcgtw_zephyr_reset(struct comp_dev *dev);

int copier_ipcgtw_create(struct comp_dev *parent_dev, struct copier_data *cd,
			 struct comp_ipc_config *config,
			 const struct ipc4_copier_module_cfg *copier);

void copier_ipcgtw_free(struct copier_data *cd);
#endif /* __SOF_IPCGTW_COPIER_H__ */
