// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2024 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>
#include <sof/lib/dai-legacy.h>
#include <ipc/dai.h>
#include <sof/drivers/afe-drv.h>

/* The legacy driver stores register addresses as an offset from an
 * arbitrary base address (which is not actually a unified block of
 * AFE-related registers), where DTS naturally wants to provide full
 * addresses.  We store the base here, pending a Zephyrized driver.
 */
#if defined(CONFIG_SOC_MT8186)
#define MTK_AFE_BASE 0x11210000
#elif defined(CONFIG_SOC_SERIES_MT818X)
#define MTK_AFE_BASE 0x10b10000
#elif defined(CONFIG_SOC_MT8195)
#define MTK_AFE_BASE 0x10890000
#elif defined(CONFIG_SOC_MT8196)
#define MTK_AFE_BASE 0x1a110000
#else
#error Unrecognized device
#endif

/* Bitfield register: address, left shift amount, and number of bits */
struct afe_bitfld {
	uint32_t reg;
	uint8_t shift;
	uint8_t bits;
};

/* Pair of registers to store a 64 bit host/bus address */
struct afe_busreg {
	uint32_t hi;
	uint32_t lo;
};

/* Config struct for a DTS-defined AFE device */
struct afe_cfg {
	char afe_name[32];
	int dai_id;
	bool downlink;
	bool mono_invert;
	struct afe_busreg base;
	struct afe_busreg end;
	struct afe_busreg cur;
	struct afe_bitfld fs;
	struct afe_bitfld hd;
	struct afe_bitfld enable;
	struct afe_bitfld mono;
	struct afe_bitfld quad_ch;
	struct afe_bitfld int_odd;
	struct afe_bitfld msb;
	struct afe_bitfld msb2;
	struct afe_bitfld agent_disable;
	struct afe_bitfld ch_num;
};

/* Converts the DTS_derived afe_cfg struct to a runtime memif_data for
 * use by the legacy driver.  This is temporary, pending a
 * Zephyrization port that will get the driver using the config struct
 * directly.
 *
 * Note the preprocessor trickery to help mapping the regularized DTS
 * data to the "almost but not quite convention-conforming" original
 * naming.  Mostly just some naming quirks.  The only semantic
 * differences are that the register addresses in DTS become offsets
 * from MTK_AFE_BASE, that default/unset register addresses are stored
 * as -1 and not NULL.
 */
static void cfg_convert(const struct afe_cfg *src, struct mtk_base_memif_data *dst)
{
#define REGCVT(R) (((R) > 0) ? ((R) - MTK_AFE_BASE) : -1)

#define COPYBIT(S, Dr, Ds) do {		\
	dst->Dr = REGCVT(src->S.reg);	\
	dst->Ds = src->S.shift;		\
	} while (0)

#define COPYFLD(S, Dr, Ds, Dm) do {	\
	COPYBIT(S, Dr, Ds);		\
	dst->Dm = BIT(src->S.bits) - 1;	\
	} while (0)

#define COPY2(F) COPYBIT(F, F##_reg, F##_shift)
#define COPY3(F) COPYFLD(F, F##_reg, F##_shift, F##_mask)

	dst->name = src->afe_name; /* DTS values are string literals */
	dst->reg_ofs_base = REGCVT(src->base.lo);
	dst->reg_ofs_cur = REGCVT(src->cur.lo);
	dst->reg_ofs_end = REGCVT(src->end.lo);
	dst->reg_ofs_base_msb = REGCVT(src->base.hi);
	dst->reg_ofs_cur_msb = REGCVT(src->cur.hi);
	dst->reg_ofs_end_msb = REGCVT(src->end.hi);
	dst->mono_invert = src->mono_invert;

	COPYFLD(fs, fs_reg, fs_shift, fs_maskbit);
	COPY2(mono);
	COPY3(quad_ch);
	COPYBIT(int_odd, int_odd_flag_reg, int_odd_flag_shift);
	COPY2(enable);
	COPY2(hd);
	COPY2(msb);
	COPY2(msb2);
	COPY2(agent_disable);
	COPYFLD(ch_num, ch_num_reg, ch_num_shift, ch_num_maskbit);

#undef REGCVT
#undef COPYBIT
#undef COPYFLD
#undef COPY2
#undef COPY3
}

/* Some properties may be skipped/defaulted in DTS, leave them zero-filled */
#define COND_PROP(n, prop) \
	IF_ENABLED(DT_NODE_HAS_PROP(n, prop), (.prop = DT_PROP(n, prop),))

#define GENAFE(n) { \
	.afe_name = DT_PROP(n, afe_name), \
	.dai_id = DT_PROP(n, dai_id), \
	.downlink = DT_PROP(n, downlink), \
	.mono_invert = DT_PROP(n, mono_invert), \
	.base = DT_PROP(n, base), \
	.end = DT_PROP(n, end), \
	.cur = DT_PROP(n, cur), \
	.fs = DT_PROP(n, fs), \
	.hd = DT_PROP(n, hd), \
	.enable = DT_PROP(n, enable), \
	COND_PROP(n, mono) \
	COND_PROP(n, quad_ch) \
	COND_PROP(n, int_odd) \
	COND_PROP(n, msb) \
	COND_PROP(n, msb2) \
	COND_PROP(n, agent_disable) \
	COND_PROP(n, ch_num) \
	},

static const struct afe_cfg afes[] = {
	DT_FOREACH_STATUS_OKAY(mediatek_afe, GENAFE)
};

#define EMPTY_STRUCT(n) {},

static struct mtk_base_memif_data afe_memifs[] = {
	DT_FOREACH_STATUS_OKAY(mediatek_afe, EMPTY_STRUCT)
};

static struct dai mtk_dais[] = {
	DT_FOREACH_STATUS_OKAY(mediatek_afe, EMPTY_STRUCT)
};

extern const struct dma_ops memif_ops;
extern const struct dma_ops dummy_dma_ops;

// FIXME: remove this ID field?  Nothing seems to use it
enum dma_id {
	DMA_ID_AFE_MEMIF,
	DMA_ID_HOST,
};

static struct dma mtk_dma[] = {
	{
		.plat_data = {
			.id		= DMA_ID_HOST,
			.dir		= DMA_DIR_HMEM_TO_LMEM | DMA_DIR_LMEM_TO_HMEM,
			.devs		= DMA_DEV_HOST,
			.channels	= 16,
		},
		.ops	= &dummy_dma_ops,
	},
	{
		.plat_data = {
			.id		= DMA_ID_AFE_MEMIF,
			.dir		= DMA_DIR_MEM_TO_DEV | DMA_DIR_DEV_TO_MEM,
			.devs		= SOF_DMA_DEV_AFE_MEMIF,
			.base		= MTK_AFE_BASE,
			.channels	= ARRAY_SIZE(mtk_dais),
		},
		.ops = &memif_ops,
	},
};

static const struct dma_info mtk_dma_info = {
	.dma_array = mtk_dma,
	.num_dmas = ARRAY_SIZE(mtk_dma),
};

static const struct dai_type_info mtk_dai_types[] = {
	{
		.type = SOF_DAI_MEDIATEK_AFE,
		.dai_array = mtk_dais,
		.num_dais = ARRAY_SIZE(mtk_dais),
	},
};

static const struct dai_info mtk_dai_info = {
	.dai_type_array = mtk_dai_types,
	.num_dai_types = ARRAY_SIZE(mtk_dai_types),
};

/* Static table of fs register values.  TODO: binary search */
static unsigned int mtk_afe_fs_timing(unsigned int rate)
{
	static const struct { int hz, reg; } rate2reg[] = {
		{   8000,  0 },
		{  11025,  1 },
		{  12000,  2 },
		{  16000,  4 },
		{  22050,  5 },
		{  24000,  6 },
		{  32000,  8 },
		{  44100,  9 },
		{  48000, 10 },
		{  88200, 13 },
		{  96000, 14 },
		{ 176400, 17 },
		{ 192000, 18 },
		{ 352800, 21 },
		{ 384000, 22 },
	};

	for (int i = 0; i < ARRAY_SIZE(rate2reg); i++)
		if (rate2reg[i].hz == rate)
			return rate2reg[i].reg;
	return -EINVAL;
}

static unsigned int mtk_afe_fs(unsigned int rate, int aud_blk)
{
	return mtk_afe_fs_timing(rate);
}

/* Global symbol referenced by AFE driver */
struct mtk_base_afe_platform mtk_afe_platform = {
	.base_addr = MTK_AFE_BASE,
	.memif_datas = afe_memifs,
	.memif_size = ARRAY_SIZE(afe_memifs),
	.memif_32bit_supported = 0,
	.irq_datas = NULL,
	.irqs_size = 0,
	.dais_size = ARRAY_SIZE(mtk_dais),
	.afe_fs = mtk_afe_fs,
	.irq_fs = mtk_afe_fs_timing,
};

int mtk_dai_init(struct sof *sof)
{
	int i;

	/* Convert our DTS-defined AFE devices to legacy memif structs */
	for (i = 0; i < ARRAY_SIZE(afes); i++) {
		afe_memifs[i].id = i;
		cfg_convert(&afes[i], &afe_memifs[i]);

		/* Also initialize the dais array */
		extern const struct dai_driver afe_dai_driver;

		mtk_dais[i].index = i;
		mtk_dais[i].drv = &afe_dai_driver;

		/* Also construct the mtk_dais[] array, which is the
		 * mapping from the host-visible DAI index to a driver
		 * defined in afe_memifs[].  The mapping is ad-hoc,
		 * and stored, bitpacked, in the "handshake" variable
		 * in plat data.  The DAI index is the low byte, the
		 * AFE index is in the third byte.  There is an IRQ
		 * traditionally defined in the middle byte but unused
		 * here because the driver doesn't support
		 * interrupts.
		 */
		int di = afes[i].dai_id;
		int hs = (i << 16) | di;

		mtk_dais[di].plat_data.fifo[0].handshake = hs;
	}

	/* DTS stores the direction as a boolean property, but the
	 * legacy driver wants all the DL devices at the start of the
	 * array.  Compute memif_dl_num (and validate the order!).
	 */
	for (i = 0; i < ARRAY_SIZE(afes); i++) {
		if (!afes[i].downlink) {
			mtk_afe_platform.memif_dl_num = i;
			break;
		}
	}
	for (/**/; i < ARRAY_SIZE(afes); i++)
		__ASSERT_NO_MSG(!afes[i].downlink);

	sof->dai_info = &mtk_dai_info;
	sof->dma_info = &mtk_dma_info;
	return 0;
}
