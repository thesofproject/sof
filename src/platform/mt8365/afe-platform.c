// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#include <sof/common.h>
#include <errno.h>
#include <sof/drivers/afe-drv.h>
#include <mt8365-afe-regs.h>
#include <mt8365-afe-common.h>

/*
 * AFE: Audio Front-End
 *
 * frontend (memif):
 *   memory interface
 *   AWB, VULx, TDM_IN (uplink for capture)
 *   DLx, TDM_OUT (downlink for playback)
 * backend:
 *   TDM In
 *   TMD out
 *   DMIC
 *   GASRC
 *   etc.
 * interconn:
 *   inter-connection,
 *   connect frontends and backends as DSP path
 */

static const struct mtk_base_memif_data memif_data[MT8365_MEMIF_NUM] = {
	[MT8365_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT8365_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 0,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = 21,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 1,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 16,
		.agent_disable_reg = -1,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = -1,
		.msb_shift = 0,
		.msb2_reg = -1,
		.msb2_shift = 0,
	},
	[MT8365_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT8365_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 4,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = 22,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 2,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 18,
		.agent_disable_reg = -1,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = -1,
		.msb_shift = 0,
		.msb2_reg = -1,
		.msb2_shift = 0,
	},
	[MT8365_MEMIF_TDM_OUT] = {
		.name = "TDM_OUT",
		.id = MT8365_MEMIF_DL2,
		.reg_ofs_base = AFE_HDMI_OUT_BASE,
		.reg_ofs_cur = AFE_HDMI_OUT_CUR,
		.reg_ofs_end = AFE_HDMI_OUT_END,
		.fs_reg = -1,
		.fs_shift = 0,
		.fs_maskbit = 0,
		.mono_reg = -1,
		.mono_shift = 0,
		.enable_reg = AFE_HDMI_OUT_CON0,
		.enable_shift = 0,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 28,
		.agent_disable_reg = -1,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = -1,
		.msb_shift = 0,
		.msb2_reg = -1,
		.msb2_shift = 0,
	},
	[MT8365_MEMIF_AWB] = {
		.name = "AWB",
		.id = MT8365_MEMIF_AWB,
		.reg_ofs_base = AFE_AWB_BASE,
		.reg_ofs_cur = AFE_AWB_CUR,
		.reg_ofs_end = AFE_AWB_END,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 12,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = 24,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 6,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 20,
		.agent_disable_reg = -1,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 17,
		.msb2_reg = -1,
		.msb2_shift = 0,
	},
	[MT8365_MEMIF_VUL] = {
		.name = "VUL",
		.id = MT8365_MEMIF_VUL,
		.reg_ofs_base = AFE_VUL_BASE,
		.reg_ofs_cur = AFE_VUL_CUR,
		.reg_ofs_end = AFE_VUL_END,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 16,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = 27,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 3,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 22,
		.agent_disable_reg = -1,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 20,
		.msb2_reg = -1,
		.msb2_shift = 0,
	},
	[MT8365_MEMIF_VUL2] = {
		.name = "VUL2",
		.id = MT8365_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL_D2_BASE,
		.reg_ofs_cur = AFE_VUL_D2_CUR,
		.reg_ofs_end = AFE_VUL_D2_END,
		.fs_reg = AFE_DAC_CON0,
		.fs_shift = 20,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON0,
		.mono_shift = 10,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 9,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 14,
		.agent_disable_reg = -1,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 21,
		.msb2_reg = -1,
		.msb2_shift = 0,
	},
	[MT8365_MEMIF_VUL3] = {
		.name = "VUL3",
		.id = MT8365_MEMIF_VUL3,
		.reg_ofs_base = AFE_VUL3_BASE,
		.reg_ofs_cur = AFE_VUL3_CUR,
		.reg_ofs_end = AFE_VUL3_END,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 8,
		.fs_maskbit = 0xf,
		.mono_reg = AFE_DAC_CON0,
		.mono_shift = 13,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 12,
		.hd_reg = AFE_MEMIF_PBUF2_SIZE,
		.hd_shift = 10,
		.agent_disable_reg = -1,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 27,
		.msb2_reg = -1,
		.msb2_shift = 0,
	},
	[MT8365_MEMIF_TDM_IN] = {
		.name = "TDM_IN",
		.id = MT8365_MEMIF_TDM_IN,
		.reg_ofs_base = AFE_HDMI_IN_2CH_BASE,
		.reg_ofs_cur = AFE_HDMI_IN_2CH_CUR,
		.reg_ofs_end = AFE_HDMI_IN_2CH_END,
		.fs_reg = -1,
		.fs_shift = 0,
		.fs_maskbit = 0,
		.mono_reg = AFE_HDMI_IN_2CH_CON0,
		.mono_shift = 1,
		.enable_reg = AFE_HDMI_IN_2CH_CON0,
		.enable_shift = 0,
		.hd_reg = AFE_MEMIF_PBUF2_SIZE,
		.hd_shift = 8,
		.agent_disable_reg = -1,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_MEMIF_MSB,
		.msb_shift = 28,
		.msb2_reg = -1,
		.msb2_shift = 0,
	},
};

struct mt8365_afe_rate {
	unsigned int rate;
	unsigned int reg_value;
};

static const struct mt8365_afe_rate mt8365_afe_rates[] = {
	{
		.rate = 8000,
		.reg_value = 0,
	},
	{
		.rate = 11025,
		.reg_value = 1,
	},
	{
		.rate = 12000,
		.reg_value = 2,
	},
	{
		.rate = 16000,
		.reg_value = 4,
	},
	{
		.rate = 22050,
		.reg_value = 5,
	},
	{
		.rate = 24000,
		.reg_value = 6,
	},
	{
		.rate = 32000,
		.reg_value = 8,
	},
	{
		.rate = 44100,
		.reg_value = 9,
	},
	{
		.rate = 48000,
		.reg_value = 10,
	},
	{
		.rate = 88200,
		.reg_value = 11,
	},
	{
		.rate = 96000,
		.reg_value = 12,
	},
	{
		.rate = 176400,
		.reg_value = 13,
	},
	{
		.rate = 192000,
		.reg_value = 14,
	},
};

static unsigned int mt8365_afe_fs_timing(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8365_afe_rates); i++)
		if (mt8365_afe_rates[i].rate == rate)
			return mt8365_afe_rates[i].reg_value;

	return -EINVAL;
}

static unsigned int mt8365_afe_fs(unsigned int rate, int aud_blk)
{
	return mt8365_afe_fs_timing(rate);
}

static unsigned int mt8365_afe2adsp_addr(unsigned int addr)
{
	/*TODO : Need apply the address remap */
	return addr;
}

static unsigned int mt8365_adsp2afe_addr(unsigned int addr)
{
	/* TODO : Need apply the address remap */
	return addr;
}

struct mtk_base_afe_platform mtk_afe_platform = {
	.base_addr = AFE_REG_BASE,
	.memif_datas = memif_data,
	.memif_size = MT8365_MEMIF_NUM,
	.memif_dl_num = MT8365_MEMIF_DL_NUM,
	.memif_32bit_supported = 0,
	.irq_datas = NULL,
	.irqs_size = 0,
	.dais_size = MT8365_DAI_NUM,
	.afe2adsp_addr = mt8365_afe2adsp_addr,
	.adsp2afe_addr = mt8365_adsp2afe_addr,
	.afe_fs = mt8365_afe_fs,
	.irq_fs = mt8365_afe_fs_timing,
};
