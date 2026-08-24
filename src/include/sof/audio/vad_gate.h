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
#define IPC4_VAD_GATE_GET_STATUS  2

/* Default tuning.
 * VAD_DEFAULT_THRESHOLD=0 is a bypass (pass-through): all audio reaches the KPB
 * regardless of energy.  This is ideal for tone-based testing because:
 *   - A pure sine at lab volume has energy well above ambient noise
 *   - The detect_test frequency matcher handles the keyphrase decision
 *   - The VAD gate adds no risk of suppressing the tone before detection
 * To activate energy gating (e.g. for power-saving), set a non-zero threshold
 * via SET_LARGE_CONFIG at runtime or change VAD_DEFAULT_THRESHOLD here.
 */
#define VAD_DEFAULT_THRESHOLD     CONFIG_VAD_GATE_DEFAULT_THRESHOLD
#define VAD_DEFAULT_ONSET_FRAMES  CONFIG_VAD_GATE_DEFAULT_ONSET_FRAMES
#define VAD_DEFAULT_HANGOVER      CONFIG_VAD_GATE_DEFAULT_HANGOVER_FRAMES
#define VAD_DEFAULT_ENERGY_SHIFT  6                    /* IIR alpha = 1/2^6 */

/* Runtime config exchanged via SET_LARGE_CONFIG / GET_LARGE_CONFIG. */
struct ipc4_vad_gate_config {
	int32_t  threshold;       /* peak energy threshold; scale matches sample depth (INT16_MAX for S16, INT32_MAX for S32) */
	uint16_t onset_frames;    /* consecutive frames above threshold for SPEECH */
	uint16_t hangover_frames; /* consecutive frames below threshold for SILENCE */
	uint8_t  energy_shift;    /* IIR smoothing shift (alpha = 1 / 2^shift) */
	uint8_t  _pad[3];
} __attribute__((packed));

/* Payload for IPC4_VAD_GATE_GET_STATUS (8 bytes). */
struct ipc4_vad_gate_status {
	uint32_t energy;     /* current IIR energy; scale matches pipeline sample depth */
	uint8_t  vad_active; /* 1 = speech, 0 = silence */
	uint8_t  _pad[3];
} __attribute__((packed));

#ifdef UNIT_TEST
void sys_comp_vad_gate_init(void);
#endif

#endif /* __SOF_AUDIO_VAD_GATE_H__ */
