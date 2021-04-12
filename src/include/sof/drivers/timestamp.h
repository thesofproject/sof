/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_DRIVERS_TIMESTAMP_H__
#define __SOF_DRIVERS_TIMESTAMP_H__

#include <platform/drivers/timestamp.h>

struct dai;
struct timestamp_cfg;
struct timestamp_data;

/* HDA */
int timestamp_hda_config(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_hda_start(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_hda_stop(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_hda_get(struct dai *dai, struct timestamp_cfg *cfg,
		      struct timestamp_data *tsd);

/* DMIC */
int timestamp_dmic_config(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_dmic_start(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_dmic_stop(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_dmic_get(struct dai *dai, struct timestamp_cfg *cfg,
		       struct timestamp_data *tsd);

/* SSP */
int timestamp_ssp_config(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_ssp_start(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_ssp_stop(struct dai *dai, struct timestamp_cfg *cfg);
int timestamp_ssp_get(struct dai *dai, struct timestamp_cfg *cfg,
		      struct timestamp_data *tsd);

#endif /* __SOF_DRIVERS_TIMESTAMP_H__ */
