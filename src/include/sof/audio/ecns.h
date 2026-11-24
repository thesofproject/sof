/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

/**
 * \file
 * \brief ECNS (Echo Cancellation & Noise Suppression) component definitions
 */

#ifndef __SOF_AUDIO_ECNS_H__
#define __SOF_AUDIO_ECNS_H__

#include <stdint.h>
#include <stdbool.h>

/** \brief Input pin 0: microphone stream from the DMIC DAI copier */
#define ECNS_PIN_MIC_IN		0
/** \brief Input pin 1: echo reference stream from the speaker */
#define ECNS_PIN_REF_IN		1
/** \brief Output pin 0: processed stream, mono e.g. to KPB or multichannel to host */
#define ECNS_PIN_OUT		0

/** \brief Supported sample rate in Hz, e.g. for WoV */
#define ECNS_RATE_16K		16000
/** \brief Supported sample rate in Hz, e.g. for VoIP capture */
#define ECNS_RATE_48K		48000

/** \brief Microphone channel enum value to average all channels */
#define ECNS_MIC_SEL_AVERAGE	0
/** \brief Largest microphone channel enum value, Mic4 */
#define ECNS_MIC_SEL_MAX	4

/** \brief Largest output channel count, one high-pass filter state per channel */
#define ECNS_OUT_CH_MAX		4

/** \brief IPC4 channel map for mono, one nibble per channel, 0xF marks unused */
#define ECNS_CH_MAP_MONO	0xFFFFFFF0

/** \brief Register the ECNS component driver */
void sys_comp_ecns_init(void);

#endif /* __SOF_AUDIO_ECNS_H__ */
