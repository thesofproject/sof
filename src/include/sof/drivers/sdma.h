/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Paul Olaru <paul.olaru@nxp.com>
 */

#ifndef __SOF_DRIVERS_SDMA_H__
#define __SOF_DRIVERS_SDMA_H__

#define SDMA_MC0PTR		0x0000 /* platform 0 control block */
#define SDMA_INTR		0x0004 /* Active interrupts, W1C */
#define SDMA_STOP_STAT		0x0008 /* Channel stop/status, W1C */
#define SDMA_HSTART		0x000C /* Channel start */

/* Prevent hardware requests from starting channels */
#define SDMA_EVTOVR		0x0010 /* Event override */

/* Set to 0 to prevent channels from ever starting */
#define SDMA_DSPOVR		0x0014 /* Channel BP override */
#define SDMA_HOSTOVR		0x0018 /* ARM platform override */

/* Channels which are pending; you can also start channels here;
 * `done` instruction will clear this.
 */
#define SDMA_EVTPEND		0x001C /* Pending events */

/* Bit 0 resets the SDMA; bit 1 forces reschedule as if `done` was done */
#define SDMA_RESET		0x0024 /* Reset register */

/* New HW request for already pending/running channel; XRUN? */
#define SDMA_EVTERR		0x0028 /* DMA request error */

#define SDMA_INTRMASK		0x002C /* ARM platform interrupt mask */
#define SDMA_PSW		0x0030 /* Schedule status */

/* Mirror of SDMA_EVTERR which doesn't clear on reads */
#define SDMA_EVTERRDBG		0x0034 /* DMA request error register */

#define SDMA_CONFIG		0x0038 /* Configuration register */
#define SDMA_CONFIG_ACR		BIT(4)
#define SDMA_CONFIG_CSM_MSK	MASK(1, 0)
#define SDMA_CONFIG_CSM_STATIC	SET_BITS(1, 0, 0) /* Static ctx switch */
#define SDMA_CONFIG_CSM_DYN_LP	SET_BITS(1, 0, 1) /* Low power; unused */
#define SDMA_CONFIG_CSM_DYN_NL	SET_BITS(1, 0, 2) /* No loops; unused */
#define SDMA_CONFIG_CSM_DYN	SET_BITS(1, 0, 3) /* Fully dynamic ctx switch */

#define SDMA_LOCK		0x003C /* Lock firmware until reset */

/* OnCE debug registers; unsupported, mentioned for completeness */
/* Should I use these for accessing contexts?! */
#define SDMA_ONCE_ENB		0x0040 /* Enable OnCE */
#define SDMA_ONCE_DATA		0x0044 /* OnCE data register */
#define SDMA_ONCE_INSTR		0x0048 /* OnCE instruction register */
#define SDMA_ONCE_STAT		0x004C /* OnCE status register */
#define SDMA_ONCE_CMD		0x0050 /* OnCE command register */

/* Illegal instruction trap handler address */
#define SDMA_ILLINSTADDR	0x0058

#define SDMA_CHN0ADDR		0x005C /* Channel 0 boot address */

/* Hardware DMA requests 0-47, useful for debug */
#define SDMA_EVT_MIRROR		0x0060 /* DMA requests */
#define SDMA_EVT_MIRROR2	0x0064 /* DMA requests */

/* Cross-trigger events configuration registers
 * Unsupported by this driver
 */
#define SDMA_XTRIG_CONF1	0x0070
#define SDMA_XTRIG_CONF2	0x0074

/* Channel priority registers; priorities 1-7 are useful, 0 means don't
 * start. Higher number priority is higher priority.
 */
#define SDMA_CHNPRI(chan)	(0x0100 + 4 * (chan))

#define SDMA_DEFPRI		4
#define SDMA_MAXPRI		7

#define SDMA_HWEVENTS_COUNT	48

/* Channel to HW mapping; no default values */
#define SDMA_CHNENBL(hwchan)	(0x0200 + 4 * (hwchan))

/* SDMA DONE0/DONE1 configurations; each of 32 events can stop channels
 * from 0 to 7
 */
#define SDMA_DONE0_CONFIG	0x1000
#define SDMA_DONE1_CONFIG	0x1004

/* Buffer descriptor first word */
/* Count: Data buffer size, in words */
#define SDMA_BD_COUNT_MASK	MASK(15, 0)
#define SDMA_BD_COUNT(n)	SET_BITS(15, 0, n)
#define SDMA_BD_MAX_COUNT	SDMA_BD_COUNT_MASK
/* Done bit, when 1 SDMA is active */
#define SDMA_BD_DONE		BIT(16)
/* Wrap bit, last BD in circular buffer */
#define SDMA_BD_WRAP		BIT(17)
/* Continuous, transfer can bleed into next BD */
#define SDMA_BD_CONT		BIT(18)
/* Interrupt, should DSP receive interrupt when this BD is complete? */
#define SDMA_BD_INT		BIT(19)
/* Error (status bit), if some error happened while processing this BD */
#define SDMA_BD_ERROR		BIT(20)
/* Last: SDMA sets it when there is no more data to transfer */
#define SDMA_BD_LAST		BIT(21)
/* Bit 22 is reserved */
/* Bit 23 is undocumented, but used in existing drivers; I think it
 * means that buf_xaddr contains a valid value
 */
#define SDMA_BD_EXTD		BIT(23)

/* CMD: Command, differentiator for functionality of scripts; can also
 * hold error codes returned by SDMA scripts.
 */
#define SDMA_BD_CMD_MASK	MASK(31, 24)
#define SDMA_BD_CMD(cmd)	SET_BITS(31, 24, cmd)

/* We don't need more than 4 buffer descriptors per channel */
#define SDMA_MAX_BDS		4

#define SDMA_CMD_C0_SET_PM		0x4
#define SDMA_CMD_C0_SET_DM		0x1
#define SDMA_CMD_C0_GET_PM		0x8
#define SDMA_CMD_C0_GET_DM		0x2
#define SDMA_CMD_C0_SETCTX(chan)	(((chan) << 3) | 7)
#define SDMA_CMD_C0_GETCTX(chan)	(((chan) << 3) | 6)
/* Used by the actual data transfer scripts, the width of each
 * elementary transfer; set in command field. Default is 32-bit.
 */
#define SDMA_CMD_XFER_SIZE(s) ((s) == 8 ? 1 : (s) == 16 ? 2 : (s) == 24 ? 3 : 0)

#define SDMA_SRAM_CONTEXTS_BASE		0x800

/* SDMA channel types; you can add more as required */
/* AP2AP is memory to memory */
#define SDMA_CHAN_TYPE_AP2AP		0
/* AP2MCU is host to DAI, slow path but works with DSP OCRAM */
#define SDMA_CHAN_TYPE_AP2MCU		1
#define SDMA_CHAN_TYPE_MCU2AP		2
/* SHP2MCU is host to DAI, faster but only works with SDRAM */
#define SDMA_CHAN_TYPE_SHP2MCU		3
#define SDMA_CHAN_TYPE_MCU2SHP		4

/* TODO check and move these to platform data */
#define SDMA_SCRIPT_AP2AP_OFF		644
#define SDMA_SCRIPT_AP2MCU_OFF		685
#define SDMA_SCRIPT_MCU2AP_OFF		749
#define SDMA_SCRIPT_SHP2MCU_OFF		893
#define SDMA_SCRIPT_MCU2SHP_OFF		962

#if CONFIG_HAVE_SDMA_FIRMWARE
#include "sdma_script_code_imx7d_4_5.h"
#endif

#endif /* __SOF_DRIVERS_SDMA_H__ */
