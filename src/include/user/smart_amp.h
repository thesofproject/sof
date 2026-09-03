/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Maxim Integrated. All rights reserved.
 *
 * Author: Ryan Lee <ryans.lee@maximintegrated.com>
 */

#ifndef __USER_SMART_AMP_H__
#define __USER_SMART_AMP_H__

#include <stdint.h>

/* smart amp component configuration data. */
struct sof_smart_amp_config {
	/* total config size in bytes */
	uint32_t size;
	/* Number of smart amp feedback channel */
	uint32_t feedback_channels;
	/* channel map for audio source */
	int8_t source_ch_map[PLATFORM_MAX_CHANNELS];
	/* channel map for audio feedback */
	int8_t feedback_ch_map[PLATFORM_MAX_CHANNELS];
};

#endif /* __USER_SMART_AMP_H__ */
