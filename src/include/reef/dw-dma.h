/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __REEF_DW_DMA_H__
#define __REEF_DW_DMA_H__

#include <stdint.h>

#define DW_DMA_MAX_NR_CHANNELS	8

/*
 * Redefine this macro to handle differences between 32- and 64-bit
 * addressing, big vs. little endian, etc.
 */
#define DW_REG(name)		uint32_t name; uint32_t __pad_##name

/* Hardware register definitions. */
struct dw_dma_chan_regs {
	DW_REG(SAR);		/* Source Address Register */
	DW_REG(DAR);		/* Destination Address Register */
	DW_REG(LLP);		/* Linked List Pointer */
	uint32_t	CTL_LO;		/* Control Register Low */
	uint32_t	CTL_HI;		/* Control Register High */
	DW_REG(SSTAT);
	DW_REG(DSTAT);
	DW_REG(SSTATAR);
	DW_REG(DSTATAR);
	uint32_t	CFG_LO;		/* Configuration Register Low */
	uint32_t	CFG_HI;		/* Configuration Register High */
	DW_REG(SGR);
	DW_REG(DSR);
};

struct dw_dma_irq_regs {
	DW_REG(XFER);
	DW_REG(BLOCK);
	DW_REG(SRC_TRAN);
	DW_REG(DST_TRAN);
	DW_REG(ERROR);
};

struct dw_dma_regs {
	/* per-channel registers */
	struct dw_dma_chan_regs	CHAN[DW_DMA_MAX_NR_CHANNELS];

	/* irq handling */
	struct dw_dma_irq_regs	RAW;		/* r */
	struct dw_dma_irq_regs	STATUS;		/* r (raw & mask) */
	struct dw_dma_irq_regs	MASK;		/* rw (set = irq enabled) */
	struct dw_dma_irq_regs	CLEAR;		/* w (ack, affects "raw") */

	DW_REG(STATUS_INT);			/* r */

	/* software handshaking */
	DW_REG(REQ_SRC);
	DW_REG(REQ_DST);
	DW_REG(SGL_REQ_SRC);
	DW_REG(SGL_REQ_DST);
	DW_REG(LAST_SRC);
	DW_REG(LAST_DST);

	/* miscellaneous */
	DW_REG(CFG);
	DW_REG(CH_EN);
	DW_REG(ID);
	DW_REG(TEST);

	/* reserved */
	DW_REG(__reserved0);
	DW_REG(__reserved1);

	/* optional encoded params, 0x3c8..0x3f7 */
	uint32_t	__reserved;

	/* per-channel configuration registers */
	uint32_t	DWC_PARAMS[DW_DMA_MAX_NR_CHANNELS];
	uint32_t	MULTI_BLK_TYPE;
	uint32_t	MAX_BLK_SIZE;

	/* top-level parameters */
	uint32_t	DW_PARAMS;
};

/* Bitfields in DW_PARAMS */
#define DW_PARAMS_NR_CHAN	8		/* number of channels */
#define DW_PARAMS_NR_MASTER	11		/* number of AHB masters */
#define DW_PARAMS_DATA_WIDTH(n)	(15 + 2 * (n))
#define DW_PARAMS_DATA_WIDTH1	15		/* master 1 data width */
#define DW_PARAMS_DATA_WIDTH2	17		/* master 2 data width */
#define DW_PARAMS_DATA_WIDTH3	19		/* master 3 data width */
#define DW_PARAMS_DATA_WIDTH4	21		/* master 4 data width */
#define DW_PARAMS_EN		28		/* encoded parameters */

/* Bitfields in DWC_PARAMS */
#define DWC_PARAMS_MBLK_EN	11		/* multi block transfer */

/* Bitfields in CTL_LO */
#define DWC_CTLL_INT_EN		(1 << 0)	/* irqs enabled? */
#define DWC_CTLL_DST_WIDTH(n)	((n)<<1)	/* bytes per element */
#define DWC_CTLL_SRC_WIDTH(n)	((n)<<4)
#define DWC_CTLL_DST_INC	(0<<7)		/* DAR update/not */
#define DWC_CTLL_DST_DEC	(1<<7)
#define DWC_CTLL_DST_FIX	(2<<7)
#define DWC_CTLL_SRC_INC	(0<<9)		/* SAR update/not */
#define DWC_CTLL_SRC_DEC	(1<<9)
#define DWC_CTLL_SRC_FIX	(2<<9)
#define DWC_CTLL_DST_MSIZE(n)	((n)<<11)	/* burst, #elements */
#define DWC_CTLL_SRC_MSIZE(n)	((n)<<14)
#define DWC_CTLL_S_GATH_EN	(1 << 17)	/* src gather, !FIX */
#define DWC_CTLL_D_SCAT_EN	(1 << 18)	/* dst scatter, !FIX */
#define DWC_CTLL_FC(n)		((n) << 20)
#define DWC_CTLL_FC_M2M		(0 << 20)	/* mem-to-mem */
#define DWC_CTLL_FC_M2P		(1 << 20)	/* mem-to-periph */
#define DWC_CTLL_FC_P2M		(2 << 20)	/* periph-to-mem */
#define DWC_CTLL_FC_P2P		(3 << 20)	/* periph-to-periph */
/* plus 4 transfer types for peripheral-as-flow-controller */
#define DWC_CTLL_DMS(n)		((n)<<23)	/* dst master select */
#define DWC_CTLL_SMS(n)		((n)<<25)	/* src master select */
#define DWC_CTLL_LLP_D_EN	(1 << 27)	/* dest block chain */
#define DWC_CTLL_LLP_S_EN	(1 << 28)	/* src block chain */

/* Bitfields in CTL_HI */
#define DWC_CTLH_DONE		0x00001000
#define DWC_CTLH_BLOCK_TS_MASK	0x00000fff

/* Bitfields in CFG_LO */
#define DWC_CFGL_CH_PRIOR_MASK	(0x7 << 5)	/* priority mask */
#define DWC_CFGL_CH_PRIOR(x)	((x) << 5)	/* priority */
#define DWC_CFGL_CH_SUSP	(1 << 8)	/* pause xfer */
#define DWC_CFGL_FIFO_EMPTY	(1 << 9)	/* pause xfer */
#define DWC_CFGL_HS_DST		(1 << 10)	/* handshake w/dst */
#define DWC_CFGL_HS_SRC		(1 << 11)	/* handshake w/src */
#define DWC_CFGL_LOCK_CH_XFER	(0 << 12)	/* scope of LOCK_CH */
#define DWC_CFGL_LOCK_CH_BLOCK	(1 << 12)
#define DWC_CFGL_LOCK_CH_XACT	(2 << 12)
#define DWC_CFGL_LOCK_BUS_XFER	(0 << 14)	/* scope of LOCK_BUS */
#define DWC_CFGL_LOCK_BUS_BLOCK	(1 << 14)
#define DWC_CFGL_LOCK_BUS_XACT	(2 << 14)
#define DWC_CFGL_LOCK_CH	(1 << 15)	/* channel lockout */
#define DWC_CFGL_LOCK_BUS	(1 << 16)	/* busmaster lockout */
#define DWC_CFGL_HS_DST_POL	(1 << 18)	/* dst handshake active low */
#define DWC_CFGL_HS_SRC_POL	(1 << 19)	/* src handshake active low */
#define DWC_CFGL_MAX_BURST(x)	((x) << 20)
#define DWC_CFGL_RELOAD_SAR	(1 << 30)
#define DWC_CFGL_RELOAD_DAR	(1 << 31)

/* Bitfields in CFG_HI */
#define DWC_CFGH_FCMODE		(1 << 0)
#define DWC_CFGH_FIFO_MODE	(1 << 1)
#define DWC_CFGH_PROTCTL(x)	((x) << 2)
#define DWC_CFGH_DS_UPD_EN	(1 << 5)
#define DWC_CFGH_SS_UPD_EN	(1 << 6)
#define DWC_CFGH_SRC_PER(x)	((x) << 7)
#define DWC_CFGH_DST_PER(x)	((x) << 11)

/* Bitfields in SGR */
#define DWC_SGR_SGI(x)		((x) << 0)
#define DWC_SGR_SGC(x)		((x) << 20)

/* Bitfields in DSR */
#define DWC_DSR_DSI(x)		((x) << 0)
#define DWC_DSR_DSC(x)		((x) << 20)

/* Bitfields in CFG */
#define DW_CFG_DMA_EN		(1 << 0)

/* DMA descriptor used by the HW version 1 */
struct dw_lli1 {
	uint32_t sar;
	uint32_t dar;
	uint32_t llp;
	uint32_t ctrl_lo;
	uint32_t ctrl_hi;
} __attribute__ ((packed));

/* DMA descriptor used by HW version 2 */
struct dw_lli2 {
	uint32_t sar;
	uint32_t dar;
	uint32_t llp;
	uint32_t ctrl_lo;
	uint32_t ctrl_hi;
	uint32_t sstat;
	uint32_t dstat;
} __attribute__ ((packed));

extern const struct dma_ops dw_dma_ops;

#endif
