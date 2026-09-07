/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_ECNS_H__
#define __SOF_AUDIO_ECNS_H__

#include <stdint.h>
#include <stdbool.h>

#define ECNS_IN_CHANNELS	4
#define ECNS_PIN_KPB		0	/* Output Pin 0: Channel 0 mono to KPB */
#define ECNS_PIN_HOST		1	/* Output Pin 1: Channels 0,1 stereo to Host Copier */

#define ECNS_PERIOD_MS		20
#define ECNS_FRAME_SAMPLES	320	/* 20ms @ 16 kHz */

#ifdef UNIT_TEST
void sys_comp_ecns_init(void);
#endif

#endif /* __SOF_AUDIO_ECNS_H__ */
