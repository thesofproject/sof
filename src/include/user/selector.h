/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

#ifndef __USER_SELECTOR_H__
#define __USER_SELECTOR_H__

#include <stdint.h>

/** \brief Selector component configuration data. */
struct sof_sel_config {
	/* selector supports 1 input and 1 output */
	uint32_t in_channels_count;	/**< accepted values 0, 2 or 4 */
	uint32_t out_channels_count;	/**< accepted values 0, 1, 2 or 4 */
	/* note: 0 for in_channels_count or out_channels_count means that
	 * these are variable values and will be fetched from pcm params;
	 * if 2 or 4 output channels selected the component works in
	 * a passthrough mode
	 */
	uint32_t sel_channel;	/**< 0..3 */
};

#endif /* __USER_SELECTOR_H__ */
