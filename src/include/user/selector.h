/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

#ifndef __INCLUDE_USER_SELECTOR_H__
#define __INCLUDE_USER_SELECTOR_H__

/** \brief Selector component configuration data. */
struct sof_sel_config {
	/* selector supports 1 input and 1 output */
	uint32_t in_channels_count;	/**< accepted values 2 or 4 */
	uint32_t out_channels_count;	/**< accepted values 1 or 2 or 4 */
	/* note: if 2 or 4 output channels selected the component works in
	 * a passthrough mode
	 */
	uint32_t sel_channel;	/**< 0..3 */
};

#endif
