/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Jerome Laclavere <jerome.laclavere@nxp.com>
 * Author: Guido Roncarolo <guido.roncarolo@nxp.com>
 */

#ifndef __SOF_DRIVERS_SAI_H__
#define __SOF_DRIVERS_SAI_H__

#include <ipc/dai.h>
#include <ipc/dai-imx.h>
#include <sof/bit.h>
#include <sof/lib/dai.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#ifdef CONFIG_IMX8M
#define SAI_OFS		8
#else
#define SAI_OFS		0
#endif

#define REG_SAI_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S32_LE |\
		SNDRV_PCM_FMTBIT_DSD_U8 |\
		SNDRV_PCM_FMTBIT_DSD_U16_LE |\
		SNDRV_PCM_FMTBIT_DSD_U32_LE)

/* SAI Register Map Register */
#define REG_SAI_VERID	0x00 /* SAI Version ID Register */
#define REG_SAI_PARAM	0x04 /* SAI Parameter Register */
#define REG_SAI_TCSR	(0x00 + SAI_OFS) /* SAI Transmit Control */
#define REG_SAI_TCR1	(0x04 + SAI_OFS) /* SAI Transmit Configuration 1 */
#define REG_SAI_TCR2	(0x08 + SAI_OFS) /* SAI Transmit Configuration 2 */
#define REG_SAI_TCR3	(0x0c + SAI_OFS) /* SAI Transmit Configuration 3 */
#define REG_SAI_TCR4	(0x10 + SAI_OFS) /* SAI Transmit Configuration 4 */
#define REG_SAI_TCR5	(0x14 + SAI_OFS) /* SAI Transmit Configuration 5 */
#define REG_SAI_TDR0	0x20 /* SAI Transmit Data */
#define REG_SAI_TDR1	0x24 /* SAI Transmit Data */
#define REG_SAI_TDR2	0x28 /* SAI Transmit Data */
#define REG_SAI_TDR3	0x2C /* SAI Transmit Data */
#define REG_SAI_TDR4	0x30 /* SAI Transmit Data */
#define REG_SAI_TDR5	0x34 /* SAI Transmit Data */
#define REG_SAI_TDR6	0x38 /* SAI Transmit Data */
#define REG_SAI_TDR7	0x3C /* SAI Transmit Data */
#define REG_SAI_TFR0	0x40 /* SAI Transmit FIFO */
#define REG_SAI_TFR1	0x44 /* SAI Transmit FIFO */
#define REG_SAI_TFR2	0x48 /* SAI Transmit FIFO */
#define REG_SAI_TFR3	0x4C /* SAI Transmit FIFO */
#define REG_SAI_TFR4	0x50 /* SAI Transmit FIFO */
#define REG_SAI_TFR5	0x54 /* SAI Transmit FIFO */
#define REG_SAI_TFR6	0x58 /* SAI Transmit FIFO */
#define REG_SAI_TFR7	0x5C /* SAI Transmit FIFO */
#define REG_SAI_TMR	0x60 /* SAI Transmit Mask */
#define REG_SAI_TTCTL	0x70 /* SAI Transmit Timestamp Control Register */
#define REG_SAI_TTCTN	0x74 /* SAI Transmit Timestamp Counter Register */
#define REG_SAI_TBCTN	0x78 /* SAI Transmit Bit Counter Register */
#define REG_SAI_TTCAP	0x7C /* SAI Transmit Timestamp Capture */

#define REG_SAI_RCSR	(0x80 + SAI_OFS) /* SAI Receive Control */
#define REG_SAI_RCR1	(0x84 + SAI_OFS) /* SAI Receive Configuration 1 */
#define REG_SAI_RCR2	(0x88 + SAI_OFS) /* SAI Receive Configuration 2 */
#define REG_SAI_RCR3	(0x8c + SAI_OFS) /* SAI Receive Configuration 3 */
#define REG_SAI_RCR4	(0x90 + SAI_OFS) /* SAI Receive Configuration 4 */
#define REG_SAI_RCR5	(0x94 + SAI_OFS) /* SAI Receive Configuration 5 */
#define REG_SAI_RDR0	0xa0 /* SAI Receive Data */
#define REG_SAI_RDR1	0xa4 /* SAI Receive Data */
#define REG_SAI_RDR2	0xa8 /* SAI Receive Data */
#define REG_SAI_RDR3	0xac /* SAI Receive Data */
#define REG_SAI_RDR4	0xb0 /* SAI Receive Data */
#define REG_SAI_RDR5	0xb4 /* SAI Receive Data */
#define REG_SAI_RDR6	0xb8 /* SAI Receive Data */
#define REG_SAI_RDR7	0xbc /* SAI Receive Data */
#define REG_SAI_RFR0	0xc0 /* SAI Receive FIFO */
#define REG_SAI_RFR1	0xc4 /* SAI Receive FIFO */
#define REG_SAI_RFR2	0xc8 /* SAI Receive FIFO */
#define REG_SAI_RFR3	0xcc /* SAI Receive FIFO */
#define REG_SAI_RFR4	0xd0 /* SAI Receive FIFO */
#define REG_SAI_RFR5	0xd4 /* SAI Receive FIFO */
#define REG_SAI_RFR6	0xd8 /* SAI Receive FIFO */
#define REG_SAI_RFR7	0xdc /* SAI Receive FIFO */
#define REG_SAI_RMR	0xe0 /* SAI Receive Mask */
#define REG_SAI_RTCTL	0xf0 /* SAI Receive Timestamp Control Register */
#define REG_SAI_RTCTN	0xf4 /* SAI Receive Timestamp Counter Register */
#define REG_SAI_RBCTN	0xf8 /* SAI Receive Bit Counter Register */
#define REG_SAI_RTCAP	0xfc /* SAI Receive Timestamp Capture */

#define REG_SAI_MCTL	0x100 /* SAI MCLK Control Register */
#define REG_SAI_MDIV	0x104 /* SAI MCLK Divide Register */

#define REG_SAI_XCSR(rx)	(rx ? REG_SAI_RCSR : REG_SAI_TCSR)
#define REG_SAI_XCR1(rx)	(rx ? REG_SAI_RCR1 : REG_SAI_TCR1)
#define REG_SAI_XCR2(rx)	(rx ? REG_SAI_RCR2 : REG_SAI_TCR2)
#define REG_SAI_XCR3(rx)	(rx ? REG_SAI_RCR3 : REG_SAI_TCR3)
#define REG_SAI_XCR4(rx)	(rx ? REG_SAI_RCR4 : REG_SAI_TCR4)
#define REG_SAI_XCR5(rx)	(rx ? REG_SAI_RCR5 : REG_SAI_TCR5)
#define REG_SAI_XMR(rx)		(rx ? REG_SAI_RMR  : REG_SAI_TMR)

#define REG_SAI_XMR_MASK        0xffffffff

/* SAI Transmit/Receive Control Register */
#define REG_SAI_CSR_TERE	BIT(31)
#define REG_SAI_CSR_SE		BIT(30)
#define REG_SAI_CSR_FR		BIT(25)
#define REG_SAI_CSR_SR		BIT(24)
#define REG_SAI_CSR_XF_SHIFT	16
#define REG_SAI_CSR_XF_W_SHIFT	18
#define REG_SAI_CSR_XF_MASK	MASK(REG_SAI_CSR_XF_SHIFT + 4, \
		REG_SAI_CSR_XF_SHIFT)
#define REG_SAI_CSR_XF_W_MASK	MASK(REG_SAI_CSR_XF_W_SHIFT + 2,\
		REG_SAI_CSR_XF_W_SHIFT)
#define REG_SAI_CSR_WSF		BIT(20)
#define REG_SAI_CSR_SEF		BIT(19)
#define REG_SAI_CSR_FEF		BIT(18)
#define REG_SAI_CSR_FWF		BIT(17)
#define REG_SAI_CSR_FRF		BIT(16)
#define REG_SAI_CSR_XIE_SHIFT	8
#define REG_SAI_CSR_XIE_MASK	MASK(REG_SAI_CSR_XIE_SHIFT + 4,\
		REG_SAI_CSR_XIE_SHIFT)
#define REG_SAI_CSR_WSIE	BIT(12)
#define REG_SAI_CSR_SEIE	BIT(11)
#define REG_SAI_CSR_FEIE	BIT(10)
#define REG_SAI_CSR_FWIE	BIT(9)
#define REG_SAI_CSR_FRIE	BIT(8)
#define REG_SAI_CSR_FWDE	BIT(1)
#define REG_SAI_CSR_FRDE	BIT(0)

/* SAI Transmit and Receive Configuration 1 Register */
#ifdef CONFIG_IMX8M
#define REG_SAI_CR1_RFW_MASK	0x7f
#else
#define REG_SAI_CR1_RFW_MASK	0x3f
#endif

/* SAI Transmit and Receive Configuration 2 Register */
#define REG_SAI_CR2_SYNC	SET_BITS(31, 30, 1)
#define REG_SAI_CR2_SYNC_MASK	MASK(31, 30)
#define REG_SAI_CR2_MSEL_MASK	MASK(27, 26)
#define REG_SAI_CR2_MSEL_BUS	SET_BITS(27, 26, 0)
#define REG_SAI_CR2_MSEL_MCLK1	SET_BITS(27, 26, 1)
#define REG_SAI_CR2_MSEL_MCLK2	SET_BITS(27, 26, 2)
#define REG_SAI_CR2_MSEL_MCLK3	SET_BITS(27, 26, 3)
#define REG_SAI_CR2_MSEL(id)	((id) << 26)
#define REG_SAI_CR2_BCP		BIT(25)
#define REG_SAI_CR2_BCD_MSTR	BIT(24)
#define REG_SAI_CR2_BYP		BIT(23) /* BCLK bypass */
#define REG_SAI_CR2_DIV_MASK	0xff

/* SAI Transmit and Receive Configuration 3 Register */
#define REG_SAI_CR3_TRCE_MASK	MASK(23, 16)
#define REG_SAI_CR3_TRCE(x)	(x << 16)
#define REG_SAI_CR3_WDFL(x)	(x)
#define REG_SAI_CR3_WDFL_MASK	0x1f

/* SAI Transmit and Receive Configuration 4 Register */

#define REG_SAI_CR4_FCONT	BIT(28)
#define REG_SAI_CR4_FCOMB_SHIFT BIT(26)
#define REG_SAI_CR4_FCOMB_SOFT  BIT(27)
#define REG_SAI_CR4_FCOMB_MASK	MASK(27, 26)
#define REG_SAI_CR4_FPACK_8     (0x2 << 24)
#define REG_SAI_CR4_FPACK_16    (0x3 << 24)
#define REG_SAI_CR4_FRSZ(x)	(((x) - 1) << 16)
#define REG_SAI_CR4_FRSZ_MASK	MASK(20, 16)
#define REG_SAI_CR4_SYWD(x)	(((x) - 1) << 8)
#define REG_SAI_CR4_SYWD_MASK	MASK(12, 8)
#define REG_SAI_CR4_CHMOD	BIT(5)
#define REG_SAI_CR4_CHMOD_MASK	BIT(5)
#define REG_SAI_CR4_MF		BIT(4)
#define REG_SAI_CR4_FSE		BIT(3)
#define REG_SAI_CR4_FSP		BIT(1)
#define REG_SAI_CR4_FSD_MSTR	BIT(0)

/* SAI Transmit and Receive Configuration 5 Register */
#define REG_SAI_CR5_WNW(x)	(((x) - 1) << 24)
#define REG_SAI_CR5_WNW_MASK	MASK(28, 24)
#define REG_SAI_CR5_W0W(x)	(((x) - 1) << 16)
#define REG_SAI_CR5_W0W_MASK	MASK(20, 16)
#define REG_SAI_CR5_FBT(x)	(((x) - 1) << 8)
#define REG_SAI_CR5_FBT_MASK	MASK(12, 8)

/* SAI MCLK Control Register */
#define REG_SAI_MCTL_MCLK_EN	BIT(30)	/* MCLK Enable */
#define REG_SAI_MCTL_MSEL_MASK	MASK(25, 24)
#define REG_SAI_MCTL_MSEL(id)   ((id) << 24)
#define REG_SAI_MCTL_MSEL_BUS	SET_BITS(25, 24, 0)
#define REG_SAI_MCTL_MSEL_MCLK1	SET_BITS(25, 24, 1)
#define REG_SAI_MCTL_MSEL_MCLK2	SET_BITS(25, 24, 2)
#define REG_SAI_MCTL_MSEL_MCLK3	SET_BITS(25, 24, 3)
#define REG_SAI_MCTL_DIV_EN	BIT(23)
#define REG_SAI_MCTL_DIV_MASK	0xFF

/* SAI VERID Register */
#define REG_SAI_VER_ID_SHIFT	16
#define REG_SAI_VER_ID_MASK	MASK(REG_SAI_VER_ID_SHIFT + 15,\
		REG_SAI_VER_ID_SHIFT)
#define REG_SAI_VER_EFIFO_EN	BIT(0)
#define REG_SAI_VER_TSTMP_EN	BIT(1)

/* SAI PARAM Register */
#define REG_SAI_PAR_SPF_SHIFT	16
#define REG_SAI_PAR_SPF_MASK	MASK(REG_SAI_PAR_SPF_SHIFT + 3,\
		REG_SAI_PAR_SPF_SHIFT)
#define REG_SAI_PAR_WPF_SHIFT	8
#define REG_SAI_PAR_WPF_MASK	MASK(REG_SAI_PAR_WPF_SHIFT + 3,\
		REG_SAI_PAR_WPF_SHIFT)
#define REG_SAI_PAR_DLN_MASK	(0x0F)

/* SAI MCLK Divide Register */
#define REG_SAI_MDIV_MASK	0xFFFFF

/* SAI type */
#define REG_SAI_DMA		BIT(0)
#define REG_SAI_USE_AC97	BIT(1)
#define REG_SAI_NET		BIT(2)
#define REG_SAI_TRA_SYN		BIT(3)
#define REG_SAI_REC_SYN		BIT(4)
#define REG_SAI_USE_I2S_SLAVE	BIT(5)

#define REG_FMT_TRANSMITTER	0
#define REG_FMT_RECEIVER	1

/* SAI clock sources */
#define REG_SAI_CLK_BUS		0
#define REG_SAI_CLK_MAST1	1
#define REG_SAI_CLK_MAST2	2
#define REG_SAI_CLK_MAST3	3

#define REG_SAI_MCLK_MAX	4

/* SAI data transfer numbers per DMA request */
#define REG_SAI_MAXBURST_TX 6
#define REG_SAI_MAXBURST_RX 6

#define SAI_FLAG_PMQOS   BIT(0)

#ifdef CONFIG_IMX8M
#define SAI_FIFO_WORD_SIZE	128
#else
#define SAI_FIFO_WORD_SIZE	64
#endif

/* Divides down the audio master clock to generate the bit clock when
 * configured for an internal bit clock.
 * The division value is (DIV + 1) * 2.
 */
#define SAI_CLOCK_DIV		0x7
#define SAI_TDM_SLOTS		2

extern const struct dai_driver sai_driver;

/* SAI private data */
struct sai_pdata {
	struct sof_ipc_dai_config config;
	struct sof_ipc_dai_sai_params params;
};

#endif /*__SOF_DRIVERS_SAI_H__ */
