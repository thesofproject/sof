// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Bluetooth Transparent Mode (0x03) DSP Codec Algorithms for SOF:
//   1. BT_CODEC_LC3  - Zephyr liblc3 (7.5 ms @ 16 kHz WB / 8 kHz NB)
//   2. BT_CODEC_MSBC - Bluetooth SIG HFP v1.8 / A2DP v1.3 H2-framed mSBC
//   3. BT_CODEC_G722 - ITU-T Recommendation G.722 SB-ADPCM
//   4. BT_CODEC_SB4  - Experimental 4-band 24-tap tree-QMF subband ADPCM

#include "bt_codec_algos.h"
#include <string.h>

/* ========================================================================
 * Shared Cache-Line-Aligned CRC-8 (0x1D) and 24-Tap QMF Tables
 * ========================================================================
 */
const uint8_t bt_crc8_poly1d_table[256] __aligned(PLATFORM_DCACHE_ALIGN) = {
	0x00, 0x1d, 0x3a, 0x27, 0x74, 0x69, 0x4e, 0x53,
	0xe8, 0xf5, 0xd2, 0xcf, 0x9c, 0x81, 0xa6, 0xbb,
	0xcd, 0xd0, 0xf7, 0xea, 0xb9, 0xa4, 0x83, 0x9e,
	0x25, 0x38, 0x1f, 0x02, 0x51, 0x4c, 0x6b, 0x76,
	0x87, 0x9a, 0xbd, 0xa0, 0xf3, 0xee, 0xc9, 0xd4,
	0x6f, 0x72, 0x55, 0x48, 0x1b, 0x06, 0x21, 0x3c,
	0x4a, 0x57, 0x70, 0x6d, 0x3e, 0x23, 0x04, 0x19,
	0xa2, 0xbf, 0x98, 0x85, 0xd6, 0xcb, 0xec, 0xf1,
	0x13, 0x0e, 0x29, 0x34, 0x67, 0x7a, 0x5d, 0x40,
	0xfb, 0xe6, 0xc1, 0xdc, 0x8f, 0x92, 0xb5, 0xa8,
	0xde, 0xc3, 0xe4, 0xf9, 0xaa, 0xb7, 0x90, 0x8d,
	0x36, 0x2b, 0x0c, 0x11, 0x42, 0x5f, 0x78, 0x65,
	0x94, 0x89, 0xae, 0xb3, 0xe0, 0xfd, 0xda, 0xc7,
	0x7c, 0x61, 0x46, 0x5b, 0x08, 0x15, 0x32, 0x2f,
	0x59, 0x44, 0x63, 0x7e, 0x2d, 0x30, 0x17, 0x0a,
	0xb1, 0xac, 0x8b, 0x96, 0xc5, 0xd8, 0xff, 0xe2,
	0x26, 0x3b, 0x1c, 0x01, 0x52, 0x4f, 0x68, 0x75,
	0xce, 0xd3, 0xf4, 0xe9, 0xba, 0xa7, 0x80, 0x9d,
	0xeb, 0xf6, 0xd1, 0xcc, 0x9f, 0x82, 0xa5, 0xb8,
	0x03, 0x1e, 0x39, 0x24, 0x77, 0x6a, 0x4d, 0x50,
	0xa1, 0xbc, 0x9b, 0x86, 0xd5, 0xc8, 0xef, 0xf2,
	0x49, 0x54, 0x73, 0x6e, 0x3d, 0x20, 0x07, 0x1a,
	0x6c, 0x71, 0x56, 0x4b, 0x18, 0x05, 0x22, 0x3f,
	0x84, 0x99, 0xbe, 0xa3, 0xf0, 0xed, 0xca, 0xd7,
	0x35, 0x28, 0x0f, 0x12, 0x41, 0x5c, 0x7b, 0x66,
	0xdd, 0xc0, 0xe7, 0xfa, 0xa9, 0xb4, 0x93, 0x8e,
	0xf8, 0xe5, 0xc2, 0xdf, 0x8c, 0x91, 0xb6, 0xab,
	0x10, 0x0d, 0x2a, 0x37, 0x64, 0x79, 0x5e, 0x43,
	0xb2, 0xaf, 0x88, 0x95, 0xc6, 0xdb, 0xfc, 0xe1,
	0x5a, 0x47, 0x60, 0x7d, 0x2e, 0x33, 0x14, 0x09,
	0x7f, 0x62, 0x45, 0x58, 0x0b, 0x16, 0x31, 0x2c,
	0x97, 0x8a, 0xad, 0xb0, 0xe3, 0xfe, 0xd9, 0xc4,
};

/* ITU-T Rec. G.722 24-tap symmetric quadrature mirror filter (half-symmetric 12 taps) */
static const int16_t bt_qmf24_coeffs[12] __aligned(PLATFORM_DCACHE_ALIGN) = {
	3, -11, 12, 32, -210, 951, 3876, -805, 362, -156, 53, -11,
};

static inline void bt_qmf24_tx(int32_t *x, int32_t s0, int32_t s1,
			       int32_t *out_low, int32_t *out_high, int shift)
{
	int32_t sumeven = 0;
	int32_t sumodd = 0;
	int i;

	for (i = 0; i < 22; i++)
		x[i] = x[i + 2];
	x[22] = s0;
	x[23] = s1;

	for (i = 0; i < 12; i++) {
		sumodd += x[2 * i] * bt_qmf24_coeffs[i];
		sumeven += x[2 * i + 1] * bt_qmf24_coeffs[11 - i];
	}

	*out_low = (sumeven + sumodd) >> shift;
	*out_high = (sumeven - sumodd) >> shift;
}

static inline void bt_qmf24_rx(int32_t *x, int32_t rlow, int32_t rhigh,
			       int32_t *out0, int32_t *out1, int shift)
{
	int32_t xout1 = 0;
	int32_t xout2 = 0;
	int i;

	for (i = 0; i < 22; i++)
		x[i] = x[i + 2];
	x[22] = rlow + rhigh;
	x[23] = rlow - rhigh;

	for (i = 0; i < 12; i++) {
		xout2 += x[2 * i] * bt_qmf24_coeffs[i];
		xout1 += x[2 * i + 1] * bt_qmf24_coeffs[11 - i];
	}

	*out0 = xout1 >> shift;
	*out1 = xout2 >> shift;
}

/* ========================================================================
 * 1. SOF LC3 (Zephyr liblc3, 7.5 ms @ 16 kHz WB or 8 kHz NB)
 * ========================================================================
 */
void bt_lc3_encode_block(struct bt_codec_state *st, const int16_t *pcm_in, uint8_t *pkt_out)
{
	pkt_out[0] = BT_LC3_SYNC_0;
	pkt_out[1] = BT_LC3_SYNC_1;
	lc3_encode(st->u.lc3.enc, LC3_PCM_FORMAT_S16, pcm_in, 1,
		   BT_LC3_PAYLOAD_BYTES, pkt_out + 2);
	pkt_out[59] = bt_frame_crc8(pkt_out, 59);
}

void bt_lc3_decode_block(struct bt_codec_state *st, const uint8_t *pkt_in, int16_t *pcm_out)
{
	lc3_decode(st->u.lc3.dec, pkt_in ? (pkt_in + 2) : NULL,
		   pkt_in ? BT_LC3_PAYLOAD_BYTES : 0,
		   LC3_PCM_FORMAT_S16, pcm_out, 1);
}

/* ========================================================================
 * 2. SOF mSBC (Bluetooth SIG HFP v1.8 / A2DP v1.3 80-tap Filterbank mSBC)
 * ========================================================================
 */
#define BT_MSBC_BLOCKS    15
#define BT_MSBC_SUBBANDS  8
#define BT_MSBC_BITPOOL   26

static const uint8_t bt_msbc_h2_sn[4] __aligned(PLATFORM_DCACHE_ALIGN) = {
	0x08, 0x38, 0xC8, 0xF8,
};

/* A2DP v1.3 Section 12.6.3.1 LOUDNESS offset8 table for 16 kHz */
static const int8_t bt_msbc_offset8_16k[8] __aligned(PLATFORM_DCACHE_ALIGN) = {
	-2, 0, 0, 0, 0, 0, 0, 1,
};

/* 1 / (2^nbits - 1) in Q28 for division-free subband dequantization (nbits = 1..16) */
static const int32_t bt_msbc_recip_q28[16] __aligned(PLATFORM_DCACHE_ALIGN) = {
	0x0FFFFFFF, 0x05555556, 0x02492492, 0x01111111,
	0x00842108, 0x00410410, 0x00204081, 0x00101010,
	0x00080402, 0x00040100, 0x00020040, 0x00010010,
	0x00008004, 0x00004001, 0x00002000, 0x00001000,
};

/* A2DP v1.3 Table 12.30 80-tap prototype window Proto_8_80 in Q16 (synthesis D = -C in Q13) */
static const int16_t bt_msbc_proto80[80] __aligned(PLATFORM_DCACHE_ALIGN) = {
	     0,     10,     22,     36,     54,     75,     97,    117,
	   132,    138,    131,    106,     59,    -12,   -108,   -229,
	   371,    526,    685,    835,    960,   1042,   1063,   1004,
	   848,    580,    192,   -322,   -959,  -1711,  -2561,  -3486,
	  4456,   5438,   6395,   7287,   8078,   8734,   9224,   9528,
	  9631,   9528,   9224,   8734,   8078,   7287,   6395,   5438,
	 -4456,  -3486,  -2561,  -1711,   -959,   -322,    192,    580,
	   848,   1004,   1063,   1042,    960,    835,    685,    526,
	  -371,   -229,   -108,    -12,     59,    106,    131,    138,
	   132,    117,     97,     75,     54,     36,     22,     10,
};

/* Analysis matrix M[m][j] = round(cos((m + 0.5) * (j - 4) * pi / 8) * 8192) */
static const int16_t bt_msbc_ana_mat[8][16] __aligned(PLATFORM_DCACHE_ALIGN) = {
	{   5793,   6811,   7568,   8035,   8192,   8035,   7568,   6811,
	    5793,   4551,   3135,   1598,      0,  -1598,  -3135,  -4551 },
	{  -5793,  -1598,   3135,   6811,   8192,   6811,   3135,  -1598,
	   -5793,  -8035,  -7568,  -4551,      0,   4551,   7568,   8035 },
	{  -5793,  -8035,  -3135,   4551,   8192,   4551,  -3135,  -8035,
	   -5793,   1598,   7568,   6811,      0,  -6811,  -7568,  -1598 },
	{   5793,  -4551,  -7568,   1598,   8192,   1598,  -7568,  -4551,
	    5793,   6811,  -3135,  -8035,      0,   8035,   3135,  -6811 },
	{   5793,   4551,  -7568,  -1598,   8192,  -1598,  -7568,   4551,
	    5793,  -6811,  -3135,   8035,      0,  -8035,   3135,   6811 },
	{  -5793,   8035,  -3135,  -4551,   8192,  -4551,  -3135,   8035,
	   -5793,  -1598,   7568,  -6811,      0,   6811,  -7568,   1598 },
	{  -5793,   1598,   3135,  -6811,   8192,  -6811,   3135,   1598,
	   -5793,   8035,  -7568,   4551,      0,  -4551,   7568,  -8035 },
	{   5793,  -6811,   7568,  -8035,   8192,  -8035,   7568,  -6811,
	    5793,  -4551,   3135,  -1598,      0,   1598,  -3135,   4551 },
};

/* Synthesis matrix N[k][m] = round(cos((m + 0.5) * (k + 4) * pi / 8) * 8192) */
static const int16_t bt_msbc_syn_mat[16][8] __aligned(PLATFORM_DCACHE_ALIGN) = {
	{   5793,  -5793,  -5793,   5793,   5793,  -5793,  -5793,   5793 },
	{   4551,  -8035,   1598,   6811,  -6811,  -1598,   8035,  -4551 },
	{   3135,  -7568,   7568,  -3135,  -3135,   7568,  -7568,   3135 },
	{   1598,  -4551,   6811,  -8035,   8035,  -6811,   4551,  -1598 },
	{      0,      0,      0,      0,      0,      0,      0,      0 },
	{  -1598,   4551,  -6811,   8035,  -8035,   6811,  -4551,   1598 },
	{  -3135,   7568,  -7568,   3135,   3135,  -7568,   7568,  -3135 },
	{  -4551,   8035,  -1598,  -6811,   6811,   1598,  -8035,   4551 },
	{  -5793,   5793,   5793,  -5793,  -5793,   5793,   5793,  -5793 },
	{  -6811,   1598,   8035,   4551,  -4551,  -8035,  -1598,   6811 },
	{  -7568,  -3135,   3135,   7568,   7568,   3135,  -3135,  -7568 },
	{  -8035,  -6811,  -4551,  -1598,   1598,   4551,   6811,   8035 },
	{  -8192,  -8192,  -8192,  -8192,  -8192,  -8192,  -8192,  -8192 },
	{  -8035,  -6811,  -4551,  -1598,   1598,   4551,   6811,   8035 },
	{  -7568,  -3135,   3135,   7568,   7568,   3135,  -3135,  -7568 },
	{  -6811,   1598,   8035,   4551,  -4551,  -8035,  -1598,   6811 },
};

/* A2DP v1.3 Section 12.6.3.1 LOUDNESS bit allocation for mSBC (8 subbands, bitpool = 26) */
static void bt_msbc_compute_bits(const uint8_t scf[8], uint8_t nbits[8])
{
	int bitneeds[8];
	int max_bitneed = -5;
	int bitcount = 0;
	int bitslice;
	int bc = 0;
	int isb;

	for (isb = 0; isb < BT_MSBC_SUBBANDS; isb++) {
		int bn = scf[isb] ? ((int)scf[isb] - (int)bt_msbc_offset8_16k[isb]) : -5;

		if (bn > 0)
			bn >>= 1;
		if (bn > max_bitneed)
			max_bitneed = bn;
		bitneeds[isb] = bn;
	}

	bitslice = max_bitneed + 1;
	while (bc < BT_MSBC_BITPOOL) {
		int bs = bitslice--;

		bitcount = bc;
		if (bitcount == BT_MSBC_BITPOOL)
			break;
		for (isb = 0; isb < BT_MSBC_SUBBANDS; isb++) {
			int bn = bitneeds[isb];

			if (bn >= bs && bn < bs + 15)
				bc++;
			if (bn == bs)
				bc++;
		}
	}

	for (isb = 0; isb < BT_MSBC_SUBBANDS; isb++) {
		int nb = bitneeds[isb] - bitslice;

		nbits[isb] = (uint8_t)(nb < 2 ? 0 : (nb > 16 ? 16 : nb));
	}

	for (isb = 0; isb < BT_MSBC_SUBBANDS && bitcount < BT_MSBC_BITPOOL; isb++) {
		int add = 0;

		if (nbits[isb] > 0 && nbits[isb] < 16)
			add = 1;
		else if (bitneeds[isb] == bitslice + 1 && BT_MSBC_BITPOOL > bitcount + 1)
			add = 2;
		nbits[isb] = (uint8_t)(nbits[isb] + add);
		bitcount += add;
	}

	for (isb = 0; isb < BT_MSBC_SUBBANDS && bitcount < BT_MSBC_BITPOOL; isb++) {
		if (nbits[isb] < 16) {
			nbits[isb]++;
			bitcount++;
		}
	}
}

static void bt_msbc_analyze_8(int16_t x[80], const int16_t pcm[8], int16_t sb_out[8])
{
	int32_t y[16];
	int i, j, m;

	memmove(&x[8], &x[0], 72 * sizeof(int16_t));
	for (i = 0; i < 8; i++)
		x[7 - i] = pcm[i];

	for (j = 0; j < 16; j++) {
		int32_t acc = 0;

		for (i = 0; i < 5; i++)
			acc += (int32_t)x[j + 16 * i] * (int32_t)bt_msbc_proto80[j + 16 * i];
		y[j] = acc >> 16;
	}

	for (m = 0; m < 8; m++) {
		int32_t acc = 0;

		for (j = 0; j < 16; j++)
			acc += y[j] * (int32_t)bt_msbc_ana_mat[m][j];
		sb_out[m] = sat_int16((acc + 4096) >> 13);
	}
}

static void bt_msbc_synthesize_8(int32_t v[160], const int32_t sb_in[8], int16_t pcm_out[8])
{
	int32_t u[80];
	int i, j, k, m;

	memmove(&v[16], &v[0], 144 * sizeof(int32_t));
	for (k = 0; k < 16; k++) {
		int32_t acc = 0;

		for (m = 0; m < 8; m++)
			acc += sb_in[m] * (int32_t)bt_msbc_syn_mat[k][m];
		v[k] = acc >> 13;
	}

	for (i = 0; i < 5; i++) {
		for (j = 0; j < 8; j++) {
			u[16 * i + j] = v[32 * i + j];
			u[16 * i + 8 + j] = v[32 * i + 24 + j];
		}
	}

	for (j = 0; j < 8; j++) {
		int32_t acc = 0;

		for (i = 0; i < 10; i++)
			acc += u[j + 8 * i] * (-(int32_t)bt_msbc_proto80[j + 8 * i]);
		pcm_out[j] = sat_int16((acc + 4096) >> 13);
	}
}

void bt_msbc_encode_block(struct bt_codec_state *st, const int16_t *pcm_in, uint8_t *pkt_out)
{
	int16_t pcm16k_buf[BT_TRANSP_WB_SAMPLES];
	const int16_t *pcm16k = pcm_in;
	int16_t sb[BT_MSBC_BLOCKS][BT_MSBC_SUBBANDS];
	uint8_t scf[BT_MSBC_SUBBANDS];
	uint8_t nbits[BT_MSBC_SUBBANDS];
	uint32_t bit_pos = 0;
	int blk, band, k;

	if (st->pcm_rate == 8000) {
		for (k = 0; k < BT_TRANSP_WIRE_SAMPLES; k++) {
			int32_t s0, s1;

			bt_qmf24_rx(st->u.msbc.qmf_x, (int32_t)pcm_in[k] >> 1, 0, &s0, &s1, 11);
			pcm16k_buf[2 * k] = st->u.msbc.qmf_prev;
			pcm16k_buf[2 * k + 1] = sat_int16(s0);
			st->u.msbc.qmf_prev = sat_int16(s1);
		}
		pcm16k = pcm16k_buf;
	}

	for (blk = 0; blk < BT_MSBC_BLOCKS; blk++)
		bt_msbc_analyze_8(st->u.msbc.enc_x, &pcm16k[blk * 8], sb[blk]);

	for (band = 0; band < BT_MSBC_SUBBANDS; band++) {
		uint32_t mag = 0;
		uint8_t s = 0;

		for (blk = 0; blk < BT_MSBC_BLOCKS; blk++) {
			int32_t v = sb[blk][band];

			mag |= (uint32_t)(v < 0 ? ~v : v);
		}
		while (s < 15 && (mag >> (s + 1)) > 0)
			s++;
		scf[band] = s;
	}

	bt_msbc_compute_bits(scf, nbits);

	/* Official HFP v1.8 Transparent mSBC 60-byte packet:
	 *   [0..1]  H2 synchronization header (0x01, SN)
	 *   [2..58] 57-byte mSBC frame (0xAD, 0x00, 0x00, CRC8, scf[4], audio[49])
	 *   [59]    0x00 trailing HFP padding byte
	 */
	memset(pkt_out, 0, BT_TRANSP_PACKET_BYTES);
	pkt_out[0] = 0x01;
	pkt_out[1] = bt_msbc_h2_sn[st->frame_seq & 3u];
	st->frame_seq++;

	pkt_out[2] = 0xAD;
	pkt_out[3] = 0x00;
	pkt_out[4] = 0x00;
	for (band = 0; band < 4; band++)
		pkt_out[6 + band] = (uint8_t)((scf[2 * band] << 4) | (scf[2 * band + 1] & 0x0F));
	pkt_out[5] = bt_msbc_crc8(&pkt_out[3], &pkt_out[6]);

	for (blk = 0; blk < BT_MSBC_BLOCKS; blk++) {
		for (band = 0; band < BT_MSBC_SUBBANDS; band++) {
			uint8_t nb = nbits[band];
			int32_t levels, q;

			if (!nb)
				continue;
			levels = (1 << nb) - 1;
			q = ((((int32_t)sb[blk][band] * levels) >> (scf[band] + 1)) + levels) >> 1;
			if (q < 0)
				q = 0;
			if (q > levels)
				q = levels;
			bt_put_bits(pkt_out + 10, &bit_pos, (uint32_t)q, nb);
		}
	}
}

void bt_msbc_decode_block(struct bt_codec_state *st, const uint8_t *pkt_in, int16_t *pcm_out)
{
	int16_t pcm16k_buf[BT_TRANSP_WB_SAMPLES];
	int16_t *dst16k = (st->pcm_rate == 16000) ? pcm_out : pcm16k_buf;
	uint8_t scf[BT_MSBC_SUBBANDS];
	uint8_t nbits[BT_MSBC_SUBBANDS];
	uint32_t bit_pos = 0;
	int blk, band, k;

	for (band = 0; band < 4; band++) {
		scf[2 * band] = (pkt_in[6 + band] >> 4) & 0x0F;
		scf[2 * band + 1] = pkt_in[6 + band] & 0x0F;
	}

	bt_msbc_compute_bits(scf, nbits);

	for (blk = 0; blk < BT_MSBC_BLOCKS; blk++) {
		int32_t sb[BT_MSBC_SUBBANDS];

		for (band = 0; band < BT_MSBC_SUBBANDS; band++) {
			uint8_t nb = nbits[band];

			if (!nb) {
				sb[band] = 0;
			} else {
				int32_t q = (int32_t)bt_get_bits(pkt_in + 10, &bit_pos, nb);
				int64_t scaled = (int64_t)((q << 1) | 1) *
						 (int64_t)bt_msbc_recip_q28[nb - 1];
				int shr = 28 - ((int)scf[band] + 1);

				sb[band] = (int32_t)((scaled - (1LL << 28)) >> shr);
			}
		}
		bt_msbc_synthesize_8(st->u.msbc.dec_v, sb, &dst16k[blk * 8]);
	}

	if (st->pcm_rate == 8000) {
		for (k = 0; k < BT_TRANSP_WIRE_SAMPLES; k++) {
			int32_t xlow, xhigh;

			bt_qmf24_tx(st->u.msbc.qmf_x, pcm16k_buf[2 * k],
				    pcm16k_buf[2 * k + 1], &xlow, &xhigh, 14);
			pcm_out[k] = sat_int16(xlow << 1);
		}
	}
}

/* ========================================================================
 * 3. SOF G.722 (ITU-T Recommendation G.722 SB-ADPCM)
 *    - Raw Mode 1/2/3 (64/56/48 kbps) + Framed Transparent Mode 2 (56 kbps)
 * ========================================================================
 */
static const int16_t bt_g722_q6[32] __aligned(PLATFORM_DCACHE_ALIGN) = {
	   0,   35,   72,  110,  150,  190,  233,  276,
	 323,  370,  422,  473,  530,  587,  650,  714,
	 786,  858,  940, 1023, 1121, 1219, 1339, 1458,
	1612, 1765, 1980, 2195, 2557, 2919,    0,    0,
};

static const uint8_t bt_g722_iln[32] __aligned(PLATFORM_DCACHE_ALIGN) = {
	 0, 63, 62, 31, 30, 29, 28, 27,
	26, 25, 24, 23, 22, 21, 20, 19,
	18, 17, 16, 15, 14, 13, 12, 11,
	10,  9,  8,  7,  6,  5,  4,  0,
};

static const uint8_t bt_g722_ilp[32] __aligned(PLATFORM_DCACHE_ALIGN) = {
	 0, 61, 60, 59, 58, 57, 56, 55,
	54, 53, 52, 51, 50, 49, 48, 47,
	46, 45, 44, 43, 42, 41, 40, 39,
	38, 37, 36, 35, 34, 33, 32,  0,
};

static const int16_t bt_g722_wl[8] __aligned(PLATFORM_DCACHE_ALIGN) = {
	-60, -30, 58, 172, 334, 538, 1198, 3042,
};

static const uint8_t bt_g722_rl42[16] __aligned(PLATFORM_DCACHE_ALIGN) = {
	0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0,
};

static const int16_t bt_g722_ilb[32] __aligned(PLATFORM_DCACHE_ALIGN) = {
	2048, 2093, 2139, 2186, 2233, 2282, 2332, 2383,
	2435, 2489, 2543, 2599, 2656, 2714, 2774, 2834,
	2896, 2960, 3025, 3091, 3158, 3228, 3298, 3371,
	3444, 3520, 3597, 3676, 3756, 3838, 3922, 4008,
};

static const int16_t bt_g722_qm4[16] __aligned(PLATFORM_DCACHE_ALIGN) = {
	     0, -20456, -12896,  -8968,
	 -6288,  -4240,  -2584,  -1200,
	 20456,  12896,   8968,   6288,
	  4240,   2584,   1200,      0,
};

static const int16_t bt_g722_qm5[32] __aligned(PLATFORM_DCACHE_ALIGN) = {
	  -280,   -280, -23352, -17560,
	-14120, -11664,  -9752,  -8184,
	 -6864,  -5712,  -4696,  -3784,
	 -2960,  -2208,  -1520,   -880,
	 23352,  17560,  14120,  11664,
	  9752,   8184,   6864,   5712,
	  4696,   3784,   2960,   2208,
	  1520,    880,    280,   -280,
};

static const int16_t bt_g722_qm6[64] __aligned(PLATFORM_DCACHE_ALIGN) = {
	  -136,   -136,   -136,   -136,
	-24808, -21904, -19008, -16704,
	-14984, -13512, -12280, -11192,
	-10232,  -9360,  -8576,  -7856,
	 -7192,  -6576,  -6000,  -5456,
	 -4944,  -4464,  -4008,  -3576,
	 -3168,  -2776,  -2400,  -2032,
	 -1688,  -1360,  -1040,   -728,
	 24808,  21904,  19008,  16704,
	 14984,  13512,  12280,  11192,
	 10232,   9360,   8576,   7856,
	  7192,   6576,   6000,   5456,
	  4944,   4464,   4008,   3576,
	  3168,   2776,   2400,   2032,
	  1688,   1360,   1040,    728,
	   432,    136,   -432,   -136,
};

static const int16_t bt_g722_qm2[4] __aligned(PLATFORM_DCACHE_ALIGN) = {
	-7408, -1616, 7408, 1616,
};

static const uint8_t bt_g722_ihn[3] __aligned(PLATFORM_DCACHE_ALIGN) = { 0, 1, 0 };
static const uint8_t bt_g722_ihp[3] __aligned(PLATFORM_DCACHE_ALIGN) = { 0, 3, 2 };
static const int16_t bt_g722_wh[3] __aligned(PLATFORM_DCACHE_ALIGN)  = { 0, -214, 798 };
static const uint8_t bt_g722_rh2[4] __aligned(PLATFORM_DCACHE_ALIGN) = { 2, 1, 2, 1 };

/* ITU-T G.722 Block 4: 2-pole / 6-zero adaptive predictor update */
static void bt_g722_block4(struct bt_g722_band *band, int32_t d)
{
	int32_t wd1, wd2, wd3;
	int i;

	band->d[0] = d;
	band->r[0] = sat_int16(band->s + d);
	band->p[0] = sat_int16(band->sz + d);

	for (i = 0; i < 3; i++)
		band->sg[i] = band->p[i] >> 15;

	wd1 = sat_int16(band->a[1] << 2);
	wd2 = (band->sg[0] == band->sg[1]) ? -wd1 : wd1;
	if (wd2 > 32767)
		wd2 = 32767;

	wd3 = (wd2 >> 7) + ((band->sg[0] == band->sg[2]) ? 128 : -128);
	wd3 += (band->a[2] * 32512) >> 15;
	if (wd3 > 12288)
		wd3 = 12288;
	else if (wd3 < -12288)
		wd3 = -12288;
	band->ap[2] = wd3;

	band->sg[0] = band->p[0] >> 15;
	band->sg[1] = band->p[1] >> 15;
	wd1 = (band->sg[0] == band->sg[1]) ? 192 : -192;
	wd2 = (band->a[1] * 32640) >> 15;
	band->ap[1] = sat_int16(wd1 + wd2);

	wd3 = sat_int16(15360 - band->ap[2]);
	if (band->ap[1] > wd3)
		band->ap[1] = wd3;
	else if (band->ap[1] < -wd3)
		band->ap[1] = -wd3;

	wd1 = (d == 0) ? 0 : 128;
	band->sg[0] = d >> 15;
	for (i = 1; i < 7; i++) {
		band->sg[i] = band->d[i] >> 15;
		wd2 = (band->sg[i] == band->sg[0]) ? wd1 : -wd1;
		wd3 = (band->b[i] * 32640) >> 15;
		band->bp[i] = sat_int16(wd2 + wd3);
	}

	for (i = 6; i > 0; i--) {
		band->d[i] = band->d[i - 1];
		band->b[i] = band->bp[i];
	}

	for (i = 2; i > 0; i--) {
		band->r[i] = band->r[i - 1];
		band->p[i] = band->p[i - 1];
		band->a[i] = band->ap[i];
	}

	wd1 = sat_int16(band->r[1] + band->r[1]);
	wd1 = (band->a[1] * wd1) >> 15;
	wd2 = sat_int16(band->r[2] + band->r[2]);
	wd2 = (band->a[2] * wd2) >> 15;
	band->sp = sat_int16(wd1 + wd2);

	band->sz = 0;
	for (i = 6; i > 0; i--) {
		wd1 = sat_int16(band->d[i] + band->d[i]);
		band->sz += (band->b[i] * wd1) >> 15;
	}
	band->sz = sat_int16(band->sz);
	band->s = sat_int16(band->sp + band->sz);
}

static uint8_t bt_g722_encode_pair(struct bt_codec_state *st, int32_t xlow, int32_t xhigh,
				   bool eight_k, uint8_t bits_per_cw)
{
	struct bt_g722_band *b0 = &st->u.g722.band[0];
	struct bt_g722_band *b1 = &st->u.g722.band[1];
	int32_t el, wd, wd1, wd2, wd3, dlow, eh, dhigh;
	uint8_t ilow, ril, il4, mih, ihigh, ih2, code;
	int i;

	el = sat_int16(xlow - b0->s);
	wd = (el >= 0) ? el : -(el + 1);
	for (i = 1; i < 30; i++) {
		wd1 = ((int32_t)bt_g722_q6[i] * b0->det) >> 12;
		if (wd < wd1)
			break;
	}
	ilow = (el < 0) ? bt_g722_iln[i] : bt_g722_ilp[i];

	ril = ilow >> 2;
	wd2 = bt_g722_qm4[ril];
	dlow = (b0->det * wd2) >> 15;

	il4 = bt_g722_rl42[ril];
	wd = (b0->nb * 127) >> 7;
	b0->nb = wd + bt_g722_wl[il4];
	if (b0->nb < 0)
		b0->nb = 0;
	else if (b0->nb > 18432)
		b0->nb = 18432;

	wd1 = (b0->nb >> 6) & 31;
	wd2 = 8 - (b0->nb >> 11);
	wd3 = (wd2 < 0) ? ((int32_t)bt_g722_ilb[wd1] << -wd2) :
			  ((int32_t)bt_g722_ilb[wd1] >> wd2);
	b0->det = wd3 << 2;
	bt_g722_block4(b0, dlow);

	if (eight_k) {
		code = (uint8_t)(0xC0u | ilow);
	} else {
		eh = sat_int16(xhigh - b1->s);
		wd = (eh >= 0) ? eh : -(eh + 1);
		wd1 = (564 * b1->det) >> 12;
		mih = (wd >= wd1) ? 2 : 1;
		ihigh = (eh < 0) ? bt_g722_ihn[mih] : bt_g722_ihp[mih];

		wd2 = bt_g722_qm2[ihigh];
		dhigh = (b1->det * wd2) >> 15;

		ih2 = bt_g722_rh2[ihigh];
		wd = (b1->nb * 127) >> 7;
		b1->nb = wd + bt_g722_wh[ih2];
		if (b1->nb < 0)
			b1->nb = 0;
		else if (b1->nb > 22528)
			b1->nb = 22528;

		wd1 = (b1->nb >> 6) & 31;
		wd2 = 10 - (b1->nb >> 11);
		wd3 = (wd2 < 0) ? ((int32_t)bt_g722_ilb[wd1] << -wd2) :
				  ((int32_t)bt_g722_ilb[wd1] >> wd2);
		b1->det = wd3 << 2;
		bt_g722_block4(b1, dhigh);
		code = (uint8_t)((ihigh << 6) | ilow);
	}

	return code >> (8u - bits_per_cw);
}

static void bt_g722_decode_pair(struct bt_codec_state *st, uint8_t code,
				bool eight_k, uint8_t bits_per_cw,
				int32_t *out_rlow, int32_t *out_rhigh)
{
	struct bt_g722_band *b0 = &st->u.g722.band[0];
	struct bt_g722_band *b1 = &st->u.g722.band[1];
	int32_t wd1, wd2, wd3, rlow, dlowt, dhigh, rhigh = 0;
	uint8_t ihigh;

	switch (bits_per_cw) {
	case 6:
		wd1 = code & 0x0F;
		ihigh = (code >> 4) & 0x03;
		wd2 = bt_g722_qm4[wd1];
		break;
	case 7:
		wd1 = code & 0x1F;
		ihigh = (code >> 5) & 0x03;
		wd2 = bt_g722_qm5[wd1];
		wd1 >>= 1;
		break;
	default:
		wd1 = code & 0x3F;
		ihigh = (code >> 6) & 0x03;
		wd2 = bt_g722_qm6[wd1];
		wd1 >>= 2;
		break;
	}

	wd2 = (b0->det * wd2) >> 15;
	rlow = b0->s + wd2;
	if (rlow > 16383)
		rlow = 16383;
	else if (rlow < -16384)
		rlow = -16384;

	wd2 = bt_g722_qm4[wd1];
	dlowt = (b0->det * wd2) >> 15;

	wd2 = bt_g722_rl42[wd1];
	wd1 = ((b0->nb * 127) >> 7) + bt_g722_wl[wd2];
	if (wd1 < 0)
		wd1 = 0;
	else if (wd1 > 18432)
		wd1 = 18432;
	b0->nb = wd1;

	wd1 = (b0->nb >> 6) & 31;
	wd2 = 8 - (b0->nb >> 11);
	wd3 = (wd2 < 0) ? ((int32_t)bt_g722_ilb[wd1] << -wd2) :
			  ((int32_t)bt_g722_ilb[wd1] >> wd2);
	b0->det = wd3 << 2;
	bt_g722_block4(b0, dlowt);

	if (!eight_k) {
		wd2 = bt_g722_qm2[ihigh];
		dhigh = (b1->det * wd2) >> 15;
		rhigh = dhigh + b1->s;
		if (rhigh > 16383)
			rhigh = 16383;
		else if (rhigh < -16384)
			rhigh = -16384;

		wd2 = bt_g722_rh2[ihigh];
		wd1 = ((b1->nb * 127) >> 7) + bt_g722_wh[wd2];
		if (wd1 < 0)
			wd1 = 0;
		else if (wd1 > 22528)
			wd1 = 22528;
		b1->nb = wd1;

		wd1 = (b1->nb >> 6) & 31;
		wd2 = 10 - (b1->nb >> 11);
		wd3 = (wd2 < 0) ? ((int32_t)bt_g722_ilb[wd1] << -wd2) :
				  ((int32_t)bt_g722_ilb[wd1] >> wd2);
		b1->det = wd3 << 2;
		bt_g722_block4(b1, dhigh);
	}

	*out_rlow = rlow;
	*out_rhigh = rhigh;
}

void bt_g722_encode_raw(struct bt_codec_state *st, const int16_t *pcm_in,
			uint32_t num_samples, uint8_t *g722_out, uint8_t bits_per_cw)
{
	bool eight_k = (st->pcm_rate == 8000);
	uint32_t n = eight_k ? num_samples : (num_samples / 2u);
	uint32_t i;

	for (i = 0; i < n; i++) {
		int32_t xlow, xhigh = 0;

		if (eight_k)
			xlow = (int32_t)pcm_in[i] >> 1;
		else
			bt_qmf24_tx(st->u.g722.qmf_x, pcm_in[2 * i], pcm_in[2 * i + 1],
				    &xlow, &xhigh, 14);

		g722_out[i] = bt_g722_encode_pair(st, xlow, xhigh, eight_k, bits_per_cw);
	}
}

void bt_g722_decode_raw(struct bt_codec_state *st, const uint8_t *g722_in,
			uint32_t num_codewords, int16_t *pcm_out, uint8_t bits_per_cw)
{
	bool eight_k = (st->pcm_rate == 8000);
	uint32_t i;

	for (i = 0; i < num_codewords; i++) {
		int32_t rlow, rhigh;

		bt_g722_decode_pair(st, g722_in[i], eight_k, bits_per_cw, &rlow, &rhigh);
		if (eight_k) {
			pcm_out[i] = sat_int16(rlow << 1);
		} else {
			int32_t s0, s1;

			bt_qmf24_rx(st->u.g722.qmf_x, rlow, rhigh, &s0, &s1, 11);
			pcm_out[2 * i] = sat_int16(s0);
			pcm_out[2 * i + 1] = sat_int16(s1);
		}
	}
}

/* Framed Bluetooth Transparent G.722 60-byte packet:
 *   [0..1]   Sync word (0x72, 0x27)
 *   [2..54]  60 ITU-T G.722 Mode-2 (56 kbps, 7-bit) subband codewords (420 bits)
 *   [55..58] Zero padding
 *   [59]     CRC-8 (0x1D)
 */
void bt_g722_encode_block(struct bt_codec_state *st, const int16_t *pcm_in, uint8_t *pkt_out)
{
	uint8_t cw[BT_TRANSP_WIRE_SAMPLES];
	uint32_t bit_pos = 0;
	int i;

	memset(pkt_out, 0, BT_TRANSP_PACKET_BYTES);
	pkt_out[0] = BT_G722_SYNC_0;
	pkt_out[1] = BT_G722_SYNC_1;

	bt_g722_encode_raw(st, pcm_in, st->pcm_frame_samples, cw, 7);
	for (i = 0; i < BT_TRANSP_WIRE_SAMPLES; i++)
		bt_put_bits(pkt_out + 2, &bit_pos, cw[i], 7);

	pkt_out[59] = bt_frame_crc8(pkt_out, 59);
}

void bt_g722_decode_block(struct bt_codec_state *st, const uint8_t *pkt_in, int16_t *pcm_out)
{
	uint8_t cw[BT_TRANSP_WIRE_SAMPLES];
	uint32_t bit_pos = 0;
	int i;

	for (i = 0; i < BT_TRANSP_WIRE_SAMPLES; i++)
		cw[i] = (uint8_t)bt_get_bits(pkt_in + 2, &bit_pos, 7);

	bt_g722_decode_raw(st, cw, BT_TRANSP_WIRE_SAMPLES, pcm_out, 7);
}

/* ========================================================================
 * 4. SOF SB4 (Experimental BSD-3-Clause 4-Band 24-Tap Tree-QMF Subband ADPCM)
 *    - 2-stage 24-tap QMF splits 120 wideband samples into 30 x 4 subbands
 *      (LL: 6 bits, LH: 4 bits, HL: 3 bits, HH: 2 bits = 15 bits/group = 450 bits)
 * ========================================================================
 */
static const int16_t bt_sb4_step_table[89] __aligned(PLATFORM_DCACHE_ALIGN) = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

static const int8_t bt_sb4_idx6[32] __aligned(PLATFORM_DCACHE_ALIGN) = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,
};

static const int8_t bt_sb4_idx4[8] __aligned(PLATFORM_DCACHE_ALIGN) = {
	-1, -1, -1, -1, 2, 4, 6, 8,
};

static const int8_t bt_sb4_idx2[2] __aligned(PLATFORM_DCACHE_ALIGN) = {
	-1, 2,
};

static uint8_t bt_sb4_adpcm_enc(int32_t sample, int32_t *pred, int32_t *idx, uint8_t nbits)
{
	int32_t p = (*pred * 253) >> 8;
	int32_t diff = sample - p;
	int32_t step = bt_sb4_step_table[*idx];
	uint8_t mag_bits = nbits - 1u;
	uint8_t sign_mask = (uint8_t)(1u << mag_bits);
	uint8_t code = 0;
	int32_t vpdiff = step >> mag_bits;
	int b;

	if (diff < 0) {
		code = sign_mask;
		diff = -diff;
	}
	for (b = (int)mag_bits - 1; b >= 0; b--) {
		int32_t s = step >> (mag_bits - 1 - b);

		if (diff >= s) {
			code |= (uint8_t)(1u << b);
			diff -= s;
			vpdiff += s;
		}
	}

	*pred = sat_int16(p + ((code & sign_mask) ? -vpdiff : vpdiff));

	*idx = (*idx * 63) >> 6;
	if (nbits == 6)
		*idx += bt_sb4_idx6[code & 0x1F];
	else if (nbits == 4)
		*idx += bt_sb4_idx4[code & 0x07];
	else if (nbits == 3)
		*idx += (code & 0x03) >= 2 ? 2 : -1;
	else
		*idx += bt_sb4_idx2[code & 0x01];

	if (*idx < 0)
		*idx = 0;
	else if (*idx > 88)
		*idx = 88;

	return code;
}

static int32_t bt_sb4_adpcm_dec(uint8_t code, int32_t *pred, int32_t *idx, uint8_t nbits)
{
	int32_t p = (*pred * 253) >> 8;
	int32_t step = bt_sb4_step_table[*idx];
	uint8_t mag_bits = nbits - 1u;
	uint8_t sign_mask = (uint8_t)(1u << mag_bits);
	int32_t vpdiff = step >> mag_bits;
	int b;

	for (b = (int)mag_bits - 1; b >= 0; b--) {
		if (code & (1u << b))
			vpdiff += step >> (mag_bits - 1 - b);
	}

	*pred = sat_int16(p + ((code & sign_mask) ? -vpdiff : vpdiff));

	*idx = (*idx * 63) >> 6;
	if (nbits == 6)
		*idx += bt_sb4_idx6[code & 0x1F];
	else if (nbits == 4)
		*idx += bt_sb4_idx4[code & 0x07];
	else if (nbits == 3)
		*idx += (code & 0x03) >= 2 ? 2 : -1;
	else
		*idx += bt_sb4_idx2[code & 0x01];

	if (*idx < 0)
		*idx = 0;
	else if (*idx > 88)
		*idx = 88;

	return *pred;
}

void bt_sb4_encode_block(struct bt_codec_state *st, const int16_t *pcm_in, uint8_t *pkt_out)
{
	int16_t pcm16k_buf[BT_TRANSP_WB_SAMPLES];
	const int16_t *pcm16k = pcm_in;
	uint32_t bit_pos = 0;
	int grp, k;

	if (st->pcm_rate == 8000) {
		for (k = 0; k < BT_TRANSP_WIRE_SAMPLES; k++) {
			int32_t s0, s1;

			bt_qmf24_rx(st->u.sb4.qmf_8k_x, (int32_t)pcm_in[k] >> 1, 0,
				    &s0, &s1, 11);
			pcm16k_buf[2 * k] = sat_int16(s0);
			pcm16k_buf[2 * k + 1] = sat_int16(s1);
		}
		pcm16k = pcm16k_buf;
	}

	memset(pkt_out, 0, BT_TRANSP_PACKET_BYTES);
	pkt_out[0] = BT_SB4_SYNC_0;
	pkt_out[1] = BT_SB4_SYNC_1;

	for (grp = 0; grp < 30; grp++) {
		int32_t l0, h0, l1, h1;
		int32_t sb[4];
		uint8_t q0, q1, q2, q3;
		uint32_t cw15;

		bt_qmf24_tx(st->u.sb4.qmf_root_x,
			    pcm16k[4 * grp + 0], pcm16k[4 * grp + 1], &l0, &h0, 13);
		bt_qmf24_tx(st->u.sb4.qmf_root_x,
			    pcm16k[4 * grp + 2], pcm16k[4 * grp + 3], &l1, &h1, 13);

		bt_qmf24_tx(st->u.sb4.qmf_low_x, l0, l1, &sb[0], &sb[1], 13);
		bt_qmf24_tx(st->u.sb4.qmf_high_x, h0, h1, &sb[2], &sb[3], 13);

		q0 = bt_sb4_adpcm_enc(sb[0], &st->u.sb4.pred[0], &st->u.sb4.idx[0], 6);
		q1 = bt_sb4_adpcm_enc(sb[1], &st->u.sb4.pred[1], &st->u.sb4.idx[1], 4);
		q2 = bt_sb4_adpcm_enc(sb[2], &st->u.sb4.pred[2], &st->u.sb4.idx[2], 3);
		q3 = bt_sb4_adpcm_enc(sb[3], &st->u.sb4.pred[3], &st->u.sb4.idx[3], 2);

		cw15 = ((uint32_t)(q0 & 0x3Fu) << 9) |
		       ((uint32_t)(q1 & 0x0Fu) << 5) |
		       ((uint32_t)(q2 & 0x07u) << 2) |
		       (uint32_t)(q3 & 0x03u);
		bt_put_bits(pkt_out + 2, &bit_pos, cw15, 15);
	}

	pkt_out[59] = bt_frame_crc8(pkt_out, 59);
}

void bt_sb4_decode_block(struct bt_codec_state *st, const uint8_t *pkt_in, int16_t *pcm_out)
{
	int16_t pcm16k_buf[BT_TRANSP_WB_SAMPLES];
	int16_t *dst16k = (st->pcm_rate == 16000) ? pcm_out : pcm16k_buf;
	uint32_t bit_pos = 0;
	int grp, k;

	for (grp = 0; grp < 30; grp++) {
		uint32_t cw15 = bt_get_bits(pkt_in + 2, &bit_pos, 15);
		uint8_t q0 = (uint8_t)((cw15 >> 9) & 0x3Fu);
		uint8_t q1 = (uint8_t)((cw15 >> 5) & 0x0Fu);
		uint8_t q2 = (uint8_t)((cw15 >> 2) & 0x07u);
		uint8_t q3 = (uint8_t)(cw15 & 0x03u);
		int32_t sb0, sb1, sb2, sb3;
		int32_t l0, l1, h0, h1;
		int32_t s0, s1, s2, s3;

		sb0 = bt_sb4_adpcm_dec(q0, &st->u.sb4.pred[0], &st->u.sb4.idx[0], 6);
		sb1 = bt_sb4_adpcm_dec(q1, &st->u.sb4.pred[1], &st->u.sb4.idx[1], 4);
		sb2 = bt_sb4_adpcm_dec(q2, &st->u.sb4.pred[2], &st->u.sb4.idx[2], 3);
		sb3 = bt_sb4_adpcm_dec(q3, &st->u.sb4.pred[3], &st->u.sb4.idx[3], 2);

		bt_qmf24_rx(st->u.sb4.qmf_low_x, sb0, sb1, &l0, &l1, 12);
		bt_qmf24_rx(st->u.sb4.qmf_high_x, sb2, sb3, &h0, &h1, 12);

		bt_qmf24_rx(st->u.sb4.qmf_root_x, l0, h0, &s0, &s1, 12);
		bt_qmf24_rx(st->u.sb4.qmf_root_x, l1, h1, &s2, &s3, 12);

		dst16k[4 * grp + 0] = sat_int16(s0);
		dst16k[4 * grp + 1] = sat_int16(s1);
		dst16k[4 * grp + 2] = sat_int16(s2);
		dst16k[4 * grp + 3] = sat_int16(s3);
	}

	if (st->pcm_rate == 8000) {
		for (k = 0; k < BT_TRANSP_WIRE_SAMPLES; k++) {
			int32_t xlow, xhigh;

			bt_qmf24_tx(st->u.sb4.qmf_8k_x, pcm16k_buf[2 * k],
				    pcm16k_buf[2 * k + 1], &xlow, &xhigh, 14);
			pcm_out[k] = sat_int16(xlow << 1);
		}
	}
}

/* ========================================================================
 * Unified Initialization & Streaming Dispatch
 * ========================================================================
 */
void bt_codec_state_init(struct bt_codec_state *st, enum bt_codec_type type,
			 bool is_encoder, uint32_t pcm_rate)
{
	memset(st, 0, sizeof(*st));
	st->type = type;
	st->is_encoder = is_encoder;
	st->pcm_rate = (pcm_rate == 16000) ? 16000 : 8000;
	st->pcm_step = (st->pcm_rate == 16000) ? 2u : 1u;
	st->pcm_frame_samples = BT_TRANSP_WIRE_SAMPLES * st->pcm_step;

	if (type == BT_CODEC_LC3) {
		if (is_encoder)
			st->u.lc3.enc = lc3_setup_encoder(7500, (int)st->pcm_rate, 0,
							  &st->u.lc3.enc_mem);
		else
			st->u.lc3.dec = lc3_setup_decoder(7500, (int)st->pcm_rate, 0,
							  &st->u.lc3.dec_mem);
	} else if (type == BT_CODEC_G722) {
		st->u.g722.band[0].det = 32;
		st->u.g722.band[1].det = 8;
	}
}

bool bt_codec_valid_frame(enum bt_codec_type type, const uint8_t *pkt)
{
	switch (type) {
	case BT_CODEC_LC3:
		return pkt[0] == BT_LC3_SYNC_0 &&
		       pkt[1] == BT_LC3_SYNC_1 &&
		       pkt[59] == bt_frame_crc8(pkt, 59);
	case BT_CODEC_MSBC:
		return pkt[0] == 0x01 &&
		       (pkt[1] == 0x08 || pkt[1] == 0x38 ||
			pkt[1] == 0xC8 || pkt[1] == 0xF8) &&
		       pkt[2] == 0xAD &&
		       pkt[3] == 0x00 &&
		       pkt[4] == 0x00 &&
		       pkt[5] == bt_msbc_crc8(&pkt[3], &pkt[6]) &&
		       (pkt[58] & 0x03) == 0x00 &&
		       pkt[59] == 0x00;
	case BT_CODEC_G722:
		return pkt[0] == BT_G722_SYNC_0 &&
		       pkt[1] == BT_G722_SYNC_1 &&
		       pkt[59] == bt_frame_crc8(pkt, 59);
	case BT_CODEC_SB4:
		return pkt[0] == BT_SB4_SYNC_0 &&
		       pkt[1] == BT_SB4_SYNC_1 &&
		       pkt[59] == bt_frame_crc8(pkt, 59);
	}

	return false;
}

static void bt_codec_encode_block_dispatch(struct bt_codec_state *st,
					   const int16_t *pcm_in, uint8_t *pkt_out)
{
	switch (st->type) {
	case BT_CODEC_LC3:
		bt_lc3_encode_block(st, pcm_in, pkt_out);
		break;
	case BT_CODEC_MSBC:
		bt_msbc_encode_block(st, pcm_in, pkt_out);
		break;
	case BT_CODEC_G722:
		bt_g722_encode_block(st, pcm_in, pkt_out);
		break;
	case BT_CODEC_SB4:
		bt_sb4_encode_block(st, pcm_in, pkt_out);
		break;
	}
}

static void bt_codec_decode_block_dispatch(struct bt_codec_state *st,
					   const uint8_t *pkt_in, int16_t *pcm_out)
{
	switch (st->type) {
	case BT_CODEC_LC3:
		bt_lc3_decode_block(st, pkt_in, pcm_out);
		break;
	case BT_CODEC_MSBC:
		bt_msbc_decode_block(st, pkt_in, pcm_out);
		break;
	case BT_CODEC_G722:
		bt_g722_decode_block(st, pkt_in, pcm_out);
		break;
	case BT_CODEC_SB4:
		bt_sb4_decode_block(st, pkt_in, pcm_out);
		break;
	}
}

/* Process 1 wire step in PLAYBACK (Encoder) direction:
 * Consumes st->pcm_step PCM samples (1 @ 8 kHz, 2 @ 16 kHz) and produces
 * 1 SSP2-BT Transparent 8 kHz word (encoded byte << 8).
 */
int16_t bt_codec_process_enc_step(struct bt_codec_state *st, const int16_t *pcm_in)
{
	uint8_t out_byte = 0;
	uint32_t s;

	for (s = 0; s < st->pcm_step; s++)
		st->enc_in_buf[st->enc_in_count++] = pcm_in[s];

	if (st->enc_in_count >= st->pcm_frame_samples) {
		uint8_t pkt[BT_TRANSP_PACKET_BYTES];
		int i;

		bt_codec_encode_block_dispatch(st, st->enc_in_buf, pkt);
		for (i = 0; i < BT_TRANSP_PACKET_BYTES; i++) {
			st->enc_out_fifo[st->enc_fifo_w & BT_TRANSP_ENC_FIFO_MASK] = pkt[i];
			st->enc_fifo_w++;
		}
		st->enc_in_count = 0;
	}

	if (st->enc_fifo_r != st->enc_fifo_w) {
		out_byte = st->enc_out_fifo[st->enc_fifo_r & BT_TRANSP_ENC_FIFO_MASK];
		st->enc_fifo_r++;
	}

	return (int16_t)((uint16_t)out_byte << 8);
}

/* Process 1 wire step in CAPTURE (Decoder) direction:
 * Consumes 1 SSP2-BT Transparent 8 kHz word (rx_byte = word >> 8) and produces
 * st->pcm_step decoded PCM samples (1 @ 8 kHz, 2 @ 16 kHz).
 */
void bt_codec_process_dec_step(struct bt_codec_state *st, int16_t ssp_word, int16_t *pcm_out)
{
	uint8_t rx_byte = (uint8_t)((uint16_t)ssp_word >> 8);
	uint32_t plc_len = 4u * st->pcm_frame_samples;
	uint32_t s;

	st->dec_bytes_since_valid++;
	st->dec_pkt_buf[st->dec_pkt_pos++] = rx_byte;
	if (st->dec_pkt_pos == BT_TRANSP_PACKET_BYTES) {
		if (bt_codec_valid_frame(st->type, st->dec_pkt_buf)) {
			int16_t decoded[BT_TRANSP_WB_SAMPLES];
			uint32_t i;

			if (st->dec_bytes_since_valid == BT_TRANSP_PACKET_BYTES)
				st->dec_consec_valid++;
			else
				st->dec_consec_valid = 1;
			st->dec_bytes_since_valid = 0;

			bt_codec_decode_block_dispatch(st, st->dec_pkt_buf, decoded);
			for (i = 0; i < st->pcm_frame_samples; i++) {
				st->dec_out_fifo[st->dec_fifo_w &
						 BT_TRANSP_DEC_FIFO_MASK] = decoded[i];
				st->dec_fifo_w++;
				st->plc_hist[st->plc_pos % plc_len] = decoded[i];
				st->plc_pos++;
			}
			st->dec_valid_frames++;
			st->dec_pkt_pos = 0;
		} else {
			if (st->dec_valid_frames > 0 &&
			    (st->dec_bytes_since_valid % BT_TRANSP_PACKET_BYTES) == 0) {
				st->dec_consec_valid = 0;
				if (st->type == BT_CODEC_LC3) {
					int16_t decoded[BT_TRANSP_WB_SAMPLES];
					uint32_t i;

					bt_lc3_decode_block(st, NULL, decoded);
					for (i = 0; i < st->pcm_frame_samples; i++) {
						st->dec_out_fifo[st->dec_fifo_w &
								 BT_TRANSP_DEC_FIFO_MASK] =
							decoded[i];
						st->dec_fifo_w++;
						st->plc_hist[st->plc_pos % plc_len] = decoded[i];
						st->plc_pos++;
					}
				}
			}
			memmove(st->dec_pkt_buf, st->dec_pkt_buf + 1,
				BT_TRANSP_PACKET_BYTES - 1);
			st->dec_pkt_pos = BT_TRANSP_PACKET_BYTES - 1;
		}
	}

	for (s = 0; s < st->pcm_step; s++) {
		if (st->dec_fifo_r != st->dec_fifo_w) {
			pcm_out[s] = st->dec_out_fifo[st->dec_fifo_r &
						      BT_TRANSP_DEC_FIFO_MASK];
			st->dec_fifo_r++;
		} else if (st->dec_valid_frames >= 2) {
			pcm_out[s] = st->plc_hist[st->plc_pos % plc_len];
			st->plc_pos++;
		} else {
			pcm_out[s] = 0;
		}
	}
}
