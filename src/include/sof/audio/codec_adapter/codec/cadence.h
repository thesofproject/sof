/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifndef __SOF_AUDIO_CADENCE_CODEC__
#define __SOF_AUDIO_CADENCE_CODEC__

#include <sof/audio/cadence/xa_type_def.h>
#include <sof/audio/cadence/xa_apicmd_standards.h>
#include <sof/audio/cadence/xa_error_standards.h>
#include <sof/audio/cadence/xa_memory_standards.h>

/*****************************************************************************/
/* Codec API interface							     */
/*****************************************************************************/
extern xa_codec_func_t xa_dap_vlldp;
#define API xa_dap_vlldp
#define API_CALL(cd, cmd, sub_cmd, value, ret) \
		do { \
			ret = API((cd)->self, \
				  (cmd), \
				  (sub_cmd), \
				  (value)); \
		} while (0);
/*****************************************************************************/
/* Codec private data types						     */
/*****************************************************************************/
#define LIB_NAME_MAX_LEN 30
#define LIB_NO_ERROR XA_NO_ERROR

struct cadence_codec_param {
	uint32_t id;
	uint32_t size;
	int32_t data[];
};

struct cadence_codec_data {
	char name[LIB_NAME_MAX_LEN];
	void *self;
	void *mem_tabs;
};

int cadence_codec_init(struct comp_dev *dev);
int cadence_codec_prepare(struct comp_dev *dev);
int cadence_codec_process(struct comp_dev *dev);
int cadence_codec_apply_config(struct comp_dev *dev);

#endif /* __SOF_AUDIO_CADENCE_CODEC__ */
