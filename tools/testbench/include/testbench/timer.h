/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef _INCLUDE_HOST_TIMER_H_
#define _INCLUDE_HOST_TIMER_H_

#include <sof/audio/component.h>
#include <ipc/stream.h>

/* get timestamp for host stream DMA position */
void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn);

/* get timestamp for DAI stream DMA position */
void platform_dai_timestamp(struct comp_dev *dai,
			    struct sof_ipc_stream_posn *posn);

#endif /* _INCLUDE_HOST_TIMER_H_ */
