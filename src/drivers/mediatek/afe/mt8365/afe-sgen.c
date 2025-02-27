// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#include <stdint.h>

#include <sof/drivers/afe-sgen.h>
#include <sof/lib/io.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>

#include <mt8365-afe-regs.h>
#include <mt8365-afe-common.h>

SOF_DEFINE_REG_UUID(sgen_mt8365);

DECLARE_TR_CTX(sgen_tr, SOF_UUID(sgen_mt8365_uuid), LOG_LEVEL_INFO);

/*
 * Note: TEST_SGEN for test only
 * Define this TEST_SGEN to enable sine tone generator
 * then output data to audio memory interface(memif),
 * you can set TEST_SGEN_ID to choose output to which memif.
 * e.g. set TEST_SGEN as '1' and TEST_SGEN_ID as "MT8365_MEMIF_DL1",
 * the data source of DL2 will from sine generator.
 */
#define TEST_SGEN_ID MT8365_MEMIF_DL1
#define AUDIO_TML_PD_MASK 1
#define AUDIO_TML_PD_SHIFT 27

#define AFE_SGEN_FREQ_DIV_CH1_MASK 0x1f
#define AFE_SGEN_FREQ_DIV_CH1_SHIFT 0
#define AFE_SGEN_FREQ_DIV_CH2_MASK 0x1f
#define AFE_SGEN_FREQ_DIV_CH2_SHIFT 12
#define AFE_SGEN_AMP_DIV_CH1_MASK 0x7
#define AFE_SGEN_AMP_DIV_CH1_SHIFT 5
#define AFE_SGEN_AMP_DIV_CH2_MASK 0x7
#define AFE_SGEN_AMP_DIV_CH2_SHIFT 17
#define AFE_SGEN_MUTE_CH1_MASK 0x1
#define AFE_SGEN_MUTE_CH1_SHIFT 24
#define AFE_SGEN_MUTE_CH2_MASK 0x1
#define AFE_SGEN_MUTE_CH2_SHIFT 25
#define AFE_SGEN_ENABLE_MASK 0x1
#define AFE_SGEN_ENABLE_SHIFT 26

#define AFE_SGEN_TIMING_CH1_MASK 0x0f
#define AFE_SGEN_TIMING_CH1_SHIFT 8
#define AFE_SGEN_TIMING_CH2_MASK 0x0f
#define AFE_SGEN_TIMING_CH2_SHIFT 20

#define AFE_SINEGEN_LB_MODE_MSK 0x1f
#define AFE_SINEGEN_LB_MODE_SHIFT 27

enum {
	MT8365_SGEN_AWB = (0x3 << 1) | 1,		// o5 + o6 + SGEN to out
	MT8365_SGEN_VUL = (0x5 << 1) | 1,		// o9 + o10 + SGEN to out
	MT8365_SGEN_DL1 = (0x3 << 1) | 0,		// i5 + i6 + SGEN to in
	MT8365_SGEN_DL2 = (0x4 << 1) | 0		// i7 + i8 + SGEN to in
};

/*sgen freq div*/
enum {
	SGEN_FREQ_64D1 = 1,
	SGEN_FREQ_64D2 = 2,
	SGEN_FREQ_64D3 = 3,
	SGEN_FREQ_64D4 = 4,
	SGEN_FREQ_64D5 = 5,
	SGEN_FREQ_64D6 = 6,
	SGEN_FREQ_64D7 = 7,
	SGEN_FREQ_64D8 = 8,
};

/*sgen amp div*/
enum {
	SGEN_AMP_D1 = 0,
	SGEN_AMP_D2 = 1,
	SGEN_AMP_D4 = 2,
	SGEN_AMP_D8 = 3,
	SGEN_AMP_D16 = 4,
	SGEN_AMP_D32 = 5,
	SGEN_AMP_D64 = 6,
	SGEN_AMP_D128 = 7,
};

enum {
	SGEN_CH_TIMING_8K = 0,
	SGEN_CH_TIMING_11P025K = 1,
	SGEN_CH_TIMING_12K = 2,
	SGEN_CH_TIMING_16K = 4,
	SGEN_CH_TIMING_22P05K = 5,
	SGEN_CH_TIMING_24K = 6,
	SGEN_CH_TIMING_32K = 8,
	SGEN_CH_TIMING_44P1K = 9,
	SGEN_CH_TIMING_48K = 10,
	SGEN_CH_TIMING_88P2K = 11,
	SGEN_CH_TIMING_96K = 12,
	SGEN_CH_TIMING_176P4K = 13,
	SGEN_CH_TIMING_192K = 14,
};

static uint32_t mt8365_sinegen_timing(uint32_t rate)
{
	uint32_t sinegen_timing;

	switch (rate) {
	case 8000:
		sinegen_timing = SGEN_CH_TIMING_8K;
		break;
	case 11025:
		sinegen_timing = SGEN_CH_TIMING_11P025K;
		break;
	case 12000:
		sinegen_timing = SGEN_CH_TIMING_12K;
		break;
	case 16000:
		sinegen_timing = SGEN_CH_TIMING_16K;
		break;
	case 22050:
		sinegen_timing = SGEN_CH_TIMING_22P05K;
		break;
	case 24000:
		sinegen_timing = SGEN_CH_TIMING_24K;
		break;
	case 32000:
		sinegen_timing = SGEN_CH_TIMING_32K;
		break;
	case 44100:
		sinegen_timing = SGEN_CH_TIMING_44P1K;
		break;
	case 48000:
		sinegen_timing = SGEN_CH_TIMING_48K;
		break;
	case 88200:
		sinegen_timing = SGEN_CH_TIMING_88P2K;
		break;
	case 96000:
		sinegen_timing = SGEN_CH_TIMING_96K;
		break;
	case 176400:
		sinegen_timing = SGEN_CH_TIMING_176P4K;
		break;
	case 192000:
		sinegen_timing = SGEN_CH_TIMING_192K;
		break;
	default:
		sinegen_timing = SGEN_CH_TIMING_48K;
		tr_err(&sgen_tr, "invalid rate %d, set default 48k ", rate);
	}
	tr_dbg(&sgen_tr, "rate %d, sinegen_timing %d ", rate, sinegen_timing);
	return sinegen_timing;
}

static void mtk_afe_reg_update_bits(uint32_t addr_offset, uint32_t mask, uint32_t val, int shift)
{
	io_reg_update_bits(AFE_REG_BASE + addr_offset, mask << shift, val << shift);
}

static uint32_t mtk_afe_reg_read(uint32_t addr_offset)
{
	return io_reg_read(AFE_REG_BASE + addr_offset);
}

static void mt8365_afe_sinegen_enable(uint32_t sgen_id, uint32_t rate, int enable)
{
	uint32_t loopback_mode, reg_1, sinegen_timing;

	tr_dbg(&sgen_tr, "sgen_id %d, enable %d", sgen_id, enable);

	sinegen_timing = mt8365_sinegen_timing(rate);

	if (enable == 1) {
		/* set loopback mode */
		switch (sgen_id) {
		case MT8365_MEMIF_AWB:
			loopback_mode = MT8365_SGEN_AWB;
			break;
		case MT8365_MEMIF_VUL:
			loopback_mode = MT8365_SGEN_VUL;
			break;
		case MT8365_MEMIF_DL1:
			loopback_mode = MT8365_SGEN_DL1;
			break;
		case MT8365_MEMIF_DL2:
			loopback_mode = MT8365_SGEN_DL2;
			break;
		default:
			tr_err(&sgen_tr, "invalid sgen_id %d", sgen_id);
			return;
		}
		/* enable sinegen clock*/
		mtk_afe_reg_update_bits(AUDIO_TOP_CON0, AUDIO_TML_PD_MASK, 0, AUDIO_TML_PD_SHIFT);

		/*loopback source*/
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SINEGEN_LB_MODE_MSK, loopback_mode,
					AFE_SINEGEN_LB_MODE_SHIFT);

		/* sine gen timing*/
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SGEN_TIMING_CH1_MASK,
					sinegen_timing, AFE_SGEN_TIMING_CH1_SHIFT);
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SGEN_TIMING_CH2_MASK,
					sinegen_timing, AFE_SGEN_TIMING_CH2_SHIFT);

		/*freq div*/
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SGEN_FREQ_DIV_CH1_MASK,
					SGEN_FREQ_64D1, AFE_SGEN_FREQ_DIV_CH1_SHIFT);
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SGEN_FREQ_DIV_CH2_MASK,
					SGEN_FREQ_64D2, AFE_SGEN_FREQ_DIV_CH2_SHIFT);

		/*amp div*/
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SGEN_AMP_DIV_CH1_MASK, SGEN_AMP_D2,
					AFE_SGEN_AMP_DIV_CH1_SHIFT);
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SGEN_AMP_DIV_CH2_MASK, SGEN_AMP_D2,
					AFE_SGEN_AMP_DIV_CH2_SHIFT);
		/* enable sgen*/
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SGEN_ENABLE_MASK, 1,
					AFE_SGEN_ENABLE_SHIFT);
	} else {
		/* disable sgen*/
		mtk_afe_reg_update_bits(AFE_SGEN_CON0, AFE_SGEN_ENABLE_MASK, 0,
					AFE_SGEN_ENABLE_SHIFT);

		/* disable sgen clock */
		mtk_afe_reg_update_bits(AUDIO_TOP_CON0, AUDIO_TML_PD_MASK, 1, AUDIO_TML_PD_SHIFT);
	}

	reg_1 = mtk_afe_reg_read(AFE_SGEN_CON0);
	tr_dbg(&sgen_tr, "AFE_SGEN_CON0 0x%x", reg_1);
}

void afe_sinegen_enable(void)
{
	mt8365_afe_sinegen_enable(TEST_SGEN_ID, 48000, 1);
}

void afe_sinegen_disable(void)
{
	mt8365_afe_sinegen_enable(TEST_SGEN_ID, 48000, 0);
}
