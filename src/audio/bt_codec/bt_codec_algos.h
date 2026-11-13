/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Bluetooth Transparent Mode (0x03) DSP Codec Algorithms for SOF:
 *   1. BT_CODEC_LC3  - Zephyr liblc3 (7.5 ms @ 16 kHz WB / 8 kHz NB, 57B frame + 2B sync + CRC8)
 *   2. BT_CODEC_MSBC - Bluetooth HFP v1.8 / A2DP v1.3 H2-framed mSBC (80-tap filterbank, 60B pkt)
 *   3. BT_CODEC_G722 - ITU-T Rec. G.722 SB-ADPCM (24-tap QMF + 2-pole/6-zero predictor)
 *   4. BT_CODEC_SB4  - Experimental 4-band 24-tap tree-QMF subband ADPCM (BSD-3-Clause)
 */

#ifndef __SOF_AUDIO_BT_CODEC_ALGOS_H__
#define __SOF_AUDIO_BT_CODEC_ALGOS_H__

#include <sof/audio/format.h>
#include <rtos/cache.h>
#include <zephyr/toolchain.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <lc3.h>

#define BT_TRANSP_WIRE_SAMPLES     60
#define BT_TRANSP_WB_SAMPLES       120
#define BT_TRANSP_PACKET_BYTES     60
#define BT_TRANSP_ENC_FIFO_SIZE    256
#define BT_TRANSP_ENC_FIFO_MASK    (BT_TRANSP_ENC_FIFO_SIZE - 1)
#define BT_TRANSP_DEC_FIFO_SIZE    512
#define BT_TRANSP_DEC_FIFO_MASK    (BT_TRANSP_DEC_FIFO_SIZE - 1)
#define BT_TRANSP_PLC_MAX_SAMPLES  480

#define BT_LC3_SYNC_0        0x5A
#define BT_LC3_SYNC_1        0xA5
#define BT_LC3_PAYLOAD_BYTES 57

#define BT_G722_SYNC_0       0x72
#define BT_G722_SYNC_1       0x27

#define BT_SB4_SYNC_0        0x53
#define BT_SB4_SYNC_1        0x42

enum bt_codec_type {
	BT_CODEC_LC3 = 0,
	BT_CODEC_MSBC = 1,
	BT_CODEC_G722 = 2,
	BT_CODEC_SB4 = 3,
};

struct bt_g722_band {
	int32_t s;
	int32_t sp;
	int32_t sz;
	int32_t r[3];
	int32_t a[3];
	int32_t ap[3];
	int32_t p[3];
	int32_t d[7];
	int32_t b[7];
	int32_t bp[7];
	int32_t sg[7];
	int32_t nb;
	int32_t det;
};

struct bt_codec_state {
	enum bt_codec_type type;
	bool is_encoder;
	uint32_t pcm_rate;
	uint32_t pcm_step;
	uint32_t pcm_frame_samples;

	/* Streaming PCM block staging (60 samples @ 8 kHz or 120 samples @ 16 kHz) */
	int16_t enc_in_buf[BT_TRANSP_WB_SAMPLES];
	uint32_t enc_in_count;
	uint8_t enc_out_fifo[BT_TRANSP_ENC_FIFO_SIZE];
	uint32_t enc_fifo_r;
	uint32_t enc_fifo_w;

	uint8_t dec_pkt_buf[BT_TRANSP_PACKET_BYTES];
	uint32_t dec_pkt_pos;
	int16_t dec_out_fifo[BT_TRANSP_DEC_FIFO_SIZE];
	uint32_t dec_fifo_r;
	uint32_t dec_fifo_w;
	int16_t plc_hist[BT_TRANSP_PLC_MAX_SAMPLES];
	uint32_t plc_pos;
	uint32_t dec_valid_frames;
	uint32_t dec_consec_valid;
	uint32_t dec_bytes_since_valid;

	uint32_t frame_seq;

	/* Per-codec state (one active codec type per component instance) */
	union {
		struct {
			lc3_encoder_t enc;
			lc3_decoder_t dec;
			union {
				LC3_ENCODER_MEM_T(7500, 16000) enc_mem;
				LC3_DECODER_MEM_T(7500, 16000) dec_mem;
			};
		} lc3;

		struct {
			int16_t enc_x[80];
			int32_t dec_v[160];
			int32_t qmf_x[24];
			int16_t qmf_prev;
		} msbc;

		struct {
			int32_t qmf_x[24];
			struct bt_g722_band band[2];
		} g722;

		struct {
			int32_t qmf_root_x[24];
			int32_t qmf_low_x[24];
			int32_t qmf_high_x[24];
			int32_t qmf_8k_x[24];
			int32_t pred[4];
			int32_t idx[4];
		} sb4;
	} u;
};

/* Cache-line-aligned CRC-8 table for polynomial x^8 + x^4 + x^3 + x^2 + 1 (0x1D) */
extern const uint8_t bt_crc8_poly1d_table[256];

static inline uint8_t bt_crc8_1d(uint8_t crc, const uint8_t *data, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		crc = bt_crc8_poly1d_table[crc ^ data[i]];

	return crc;
}

static inline uint8_t bt_frame_crc8(const uint8_t *data, size_t len)
{
	return bt_crc8_1d(0xA5, data, len);
}

static inline uint8_t bt_msbc_crc8(const uint8_t *hdr2, const uint8_t *sf4)
{
	uint8_t crc = bt_crc8_1d(0x0F, hdr2, 2);

	return bt_crc8_1d(crc, sf4, 4);
}

static inline void bt_put_bits(uint8_t *buf, uint32_t *bit_pos, uint32_t val, uint8_t nbits)
{
	int b;

	for (b = (int)nbits - 1; b >= 0; b--) {
		uint32_t byte_idx = (*bit_pos) >> 3;
		uint32_t bit_idx = 7u - ((*bit_pos) & 7u);

		if ((val >> b) & 1u)
			buf[byte_idx] |= (uint8_t)(1u << bit_idx);
		(*bit_pos)++;
	}
}

static inline uint32_t bt_get_bits(const uint8_t *buf, uint32_t *bit_pos, uint8_t nbits)
{
	uint32_t val = 0;
	uint8_t b;

	for (b = 0; b < nbits; b++) {
		uint32_t byte_idx = (*bit_pos) >> 3;
		uint32_t bit_idx = 7u - ((*bit_pos) & 7u);

		val = (val << 1) | ((buf[byte_idx] >> bit_idx) & 1u);
		(*bit_pos)++;
	}
	return val;
}

void bt_codec_state_init(struct bt_codec_state *st, enum bt_codec_type type,
			 bool is_encoder, uint32_t pcm_rate);

bool bt_codec_valid_frame(enum bt_codec_type type, const uint8_t *pkt);

void bt_lc3_encode_block(struct bt_codec_state *st, const int16_t *pcm_in, uint8_t *pkt_out);
void bt_lc3_decode_block(struct bt_codec_state *st, const uint8_t *pkt_in, int16_t *pcm_out);

void bt_msbc_encode_block(struct bt_codec_state *st, const int16_t *pcm_in, uint8_t *pkt_out);
void bt_msbc_decode_block(struct bt_codec_state *st, const uint8_t *pkt_in, int16_t *pcm_out);

void bt_g722_encode_raw(struct bt_codec_state *st, const int16_t *pcm_in,
			uint32_t num_samples, uint8_t *g722_out, uint8_t bits_per_cw);
void bt_g722_decode_raw(struct bt_codec_state *st, const uint8_t *g722_in,
			uint32_t num_codewords, int16_t *pcm_out, uint8_t bits_per_cw);
void bt_g722_encode_block(struct bt_codec_state *st, const int16_t *pcm_in, uint8_t *pkt_out);
void bt_g722_decode_block(struct bt_codec_state *st, const uint8_t *pkt_in, int16_t *pcm_out);

void bt_sb4_encode_block(struct bt_codec_state *st, const int16_t *pcm_in, uint8_t *pkt_out);
void bt_sb4_decode_block(struct bt_codec_state *st, const uint8_t *pkt_in, int16_t *pcm_out);

int16_t bt_codec_process_enc_step(struct bt_codec_state *st, const int16_t *pcm_in);
void bt_codec_process_dec_step(struct bt_codec_state *st, int16_t ssp_word, int16_t *pcm_out);

#endif /* __SOF_AUDIO_BT_CODEC_ALGOS_H__ */
