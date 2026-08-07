/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_VAD_GATE_H__
#define __SOF_AUDIO_VAD_GATE_H__

#include <stdint.h>

/*
 * IPC4 SET_LARGE_CONFIG param ID to update the VAD gate tuning at runtime.
 * Payload: struct ipc4_vad_gate_config.
 */
#define IPC4_VAD_GATE_SET_CONFIG  1

/* Default tuning — suitable for hand-clap / speech in a quiet lab. */
#define VAD_DEFAULT_THRESHOLD     100000               /* ~-86 dBFS (int32 scale) */
#define VAD_DEFAULT_ONSET_FRAMES  3                    /* frames above threshold before SPEECH */
#define VAD_DEFAULT_HANGOVER      30                   /* frames below threshold before SILENCE */
#define VAD_DEFAULT_ENERGY_SHIFT  6                    /* IIR alpha = 1/2^6 */

/* Runtime config exchanged via SET_LARGE_CONFIG / GET_LARGE_CONFIG. */
struct ipc4_vad_gate_config {
	int32_t  threshold;       /* peak energy threshold in S32 amplitude units */
	uint16_t onset_frames;    /* consecutive frames above threshold for SPEECH */
	uint16_t hangover_frames; /* consecutive frames below threshold for SILENCE */
	uint8_t  energy_shift;    /* IIR smoothing shift (alpha = 1 / 2^shift) */
	uint8_t  _pad[3];
} __attribute__((packed));

#ifdef UNIT_TEST
void sys_comp_vad_gate_init(void);
#endif

#endif /* __SOF_AUDIO_VAD_GATE_H__ */
