// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/drivers/esai.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <ipc/dai.h>

static void esai_regs_dump(struct dai *dai)
{
	/* Read every readable ESAI register and print it */
#define GET_ESAI_REG(reg) dai_read(dai, REG_ESAI_##reg)
#define PRINT_ESAI_REG(reg) tracev_esai("ESAI_" #reg ": %08x", GET_ESAI_REG(reg))
	PRINT_ESAI_REG(ECR);
	PRINT_ESAI_REG(ESR);
	PRINT_ESAI_REG(TFCR);
	PRINT_ESAI_REG(TFSR);
	PRINT_ESAI_REG(RFCR);
	PRINT_ESAI_REG(RFSR);
	PRINT_ESAI_REG(SAISR);
	PRINT_ESAI_REG(SAICR);
	PRINT_ESAI_REG(TCR);
	PRINT_ESAI_REG(TCCR);
	PRINT_ESAI_REG(RCR);
	PRINT_ESAI_REG(RCCR);
	PRINT_ESAI_REG(TSMA);
	PRINT_ESAI_REG(TSMB);
	PRINT_ESAI_REG(RSMA);
	PRINT_ESAI_REG(RSMB);
	PRINT_ESAI_REG(PRRC);
	PRINT_ESAI_REG(PCRC);
#undef GET_ESAI_REG
}

struct esai_data {
	/* General configuration data that isn't stored in the HW
	 * registers */
	/* Notice: External ESAI clock cannot exceed internal divided by
	 * 6 */
	/* Incomplete driver... */
	/* TODO: Clock configuration? */
	/* Bits in ECR should be controlled directly */
	/* Bits in ESR should be checked directly */
	/* ESAI_TX* will not be used directly; use TFSR */
	/* ESAI_RX* will not be used directly; use RFSR to enable
	 * channels */
	bool RLIE; /* Last slot interrupt enable (receive) */
	bool RIE; /* Receive interrupt enable */
	bool REIE; /* Receive exception interrupt enable */
	/* RPR should be used on hardware directly, as a HW command */
	uint8_t RSWS; /* Frame sync length */
	uint8_t RMOD; /* Network mode */
};

static int esai_context_store(struct dai *dai)
{
	tracev_esai("ESAI: suspending... Let's stop and cache the DAI config!");
	return 0;
}

static int esai_context_restore(struct dai *dai)
{
	tracev_esai("ESAI: resuming... Let's restore DAI from the cache");
	return 0;
}

static int esai_probe(struct dai *dai);

static inline int esai_set_config(struct dai *dai,
				 struct sof_ipc_dai_config *config)
{
	uint32_t xcr = 0, xccr = 0, mask;

	tracev_esai("ESAI: set_config");
	if (config->type != SOF_DAI_IMX_ESAI)
		trace_esai_error("ESAI: set_config got wrong type %d (expected %d)", config->type, SOF_DAI_IMX_ESAI);
	tracev_esai("ESAI: format 0x%16x (explaining...)", config->format);
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_I2S:
		tracev_esai("ESAI: format I2S");
		/* Data on rising edge of bclk, frame low, 1clk before
		 * data
		 */
		xcr |= ESAI_xCR_xFSR;
		xccr |= ESAI_xCCR_xFSP | ESAI_xCCR_xCKP | ESAI_xCCR_xHCKP;
		break;
	case SOF_DAI_FMT_RIGHT_J:
		tracev_esai("ESAI: format Right justified");
		/* Data on rising edge of bclk, frame high, right
		 * aligned
		 */
		xccr |= ESAI_xCCR_xCKP | ESAI_xCCR_xHCKP;
		xcr  |= ESAI_xCR_xWA;
		break;
	case SOF_DAI_FMT_LEFT_J:
		tracev_esai("ESAI: format Left justified");
		/* Data on rising edge of bclk, frame high */
		xccr |= ESAI_xCCR_xCKP | ESAI_xCCR_xHCKP;
		break;
	case SOF_DAI_FMT_DSP_A:
		tracev_esai("ESAI: format DSP_A");
		/* Data on rising edge of bclk, frame high, 1clk before
		 * data
		 */
		xcr |= ESAI_xCR_xFSL | ESAI_xCR_xFSR;
		xccr |= ESAI_xCCR_xCKP | ESAI_xCCR_xHCKP;
		break;
	case SOF_DAI_FMT_DSP_B:
		tracev_esai("ESAI: format DSP_B");
		/* Data on rising edge of bclk, frame high */
		xcr |= ESAI_xCR_xFSL;
		xccr |= ESAI_xCCR_xCKP | ESAI_xCCR_xHCKP;
		break;
	case SOF_DAI_FMT_PDM:
		tracev_esai("ESAI: format PDM (not supported)");
		return -EINVAL;
	default:
		trace_esai_error("ESAI: invalid format");
		return -EINVAL;
	}

	switch (config->format & SOF_DAI_FMT_CLOCK_MASK) {
	case SOF_DAI_FMT_CONT:
		tracev_esai("ESAI: Continuous clock (unhandled)");
		break;
	case SOF_DAI_FMT_GATED:
		tracev_esai("ESAI: Gated clock (unhandled)");
		break;
	default:
		trace_esai_error("ESAI: invalid clock");
		return -EINVAL;
	}
	switch (config->format & SOF_DAI_FMT_INV_MASK) {
	case SOF_DAI_FMT_NB_NF:
		tracev_esai("ESAI: Bit clock NORMAL frame NORMAL");
		 /* Nothing to do for both normal cases */
		break;
	case SOF_DAI_FMT_NB_IF:
		tracev_esai("ESAI: Bit clock NORMAL frame INVERTED");
		/* Invert frame clock */
		xccr ^= ESAI_xCCR_xFSP;
		break;
	case SOF_DAI_FMT_IB_NF:
		tracev_esai("ESAI: Bit clock INVERTED frame NORMAL");
		/* Invert bit clock */
		xccr ^= ESAI_xCCR_xCKP | ESAI_xCCR_xHCKP;
		break;
	case SOF_DAI_FMT_IB_IF:
		tracev_esai("ESAI: Bit clock INVERTED frame INVERTED");
		/* Invert both clocks */
		xccr ^= ESAI_xCCR_xCKP | ESAI_xCCR_xHCKP | ESAI_xCCR_xFSP;
		break;
	default:
		trace_esai_error("ESAI: Invalid bit inversion format");
		return -EINVAL;
	}
	switch (config->format & SOF_DAI_FMT_MASTER_MASK) {
	case SOF_DAI_FMT_CBM_CFM:
		tracev_esai("ESAI: Code clock MASTER codec frame MASTER");
		/* Nothing to do in the registers */
		break;
	case SOF_DAI_FMT_CBM_CFS:
		tracev_esai("ESAI: Code clock MASTER codec frame SLAVE");
		xccr |= ESAI_xCCR_xFSD;
		break;
	case SOF_DAI_FMT_CBS_CFM:
		tracev_esai("ESAI: Code clock SLAVE codec frame MASTER");
		xccr |= ESAI_xCCR_xCKD;
		break;
	case SOF_DAI_FMT_CBS_CFS:
		tracev_esai("ESAI: Code clock SLAVE codec frame SLAVE");
		xccr |= ESAI_xCCR_xFSD | ESAI_xCCR_xCKD;
		break;
	default:
		trace_esai_error("ESAI: Invalid master-slave configuration");
		return -EINVAL;
	}

	/* Set networked mode; we only support 2 channels now, not 1 */
	xcr |= ESAI_xCR_xMOD_NETWORK;
	xccr |= ESAI_xCCR_xDC(2);

	/* Word size */
	xcr |= ESAI_xCR_xSWS(32, 16) | ESAI_xCR_PADC;

	/* Before actually setting config we should reset the ESAI
	 * probe() should be able to do it right
	 */

	esai_probe(dai);

	dai_update_bits(dai, REG_ESAI_ECR, ESAI_ECR_ETI, ESAI_ECR_ETI);

	mask = ESAI_xCCR_xCKP | ESAI_xCCR_xHCKP | ESAI_xCCR_xFSP |
		ESAI_xCCR_xFSD | ESAI_xCCR_xCKD | ESAI_xCCR_xHCKD |
		ESAI_xCCR_xDC_MASK;

	xccr |= ESAI_xCCR_xHCKD; /* HACK, maybe doesn't even work? */

	if (~mask & xccr) {
		trace_esai_error("XCCR bits not caught by mask: 0x%08x",
				 (~mask & xccr));
		trace_esai_error("MASK 0x%08x XCCR 0x%08x", mask, xccr);
	}

	dai_update_bits(dai, REG_ESAI_xCCR(0), mask, xccr); /* rx */
	dai_update_bits(dai, REG_ESAI_xCCR(1), mask, xccr); /* tx */

	mask = ESAI_xCR_xFSL | ESAI_xCR_xFSR | ESAI_xCR_xWA |
		ESAI_xCR_xMOD_MASK | ESAI_xCR_xSWS_MASK | ESAI_xCR_PADC;

	if (~mask & xcr) {
		trace_esai_error("XCR bits not caught by mask: 0x%08x",
				 (~mask & xcr));
		trace_esai_error("MASK 0x%08x XCR 0x%08x", mask, xcr);
	}

	dai_update_bits(dai, REG_ESAI_xCR(0), mask, xcr); /* rx */
	dai_update_bits(dai, REG_ESAI_xCR(1), mask, xcr); /* tx */

	dai_write(dai, REG_ESAI_TSMA, 0);
	dai_write(dai, REG_ESAI_TSMB, 0);
	dai_write(dai, REG_ESAI_RSMA, 0);
	dai_write(dai, REG_ESAI_RSMB, 0);

	/* Program FIFOs */
	dai_update_bits(dai, REG_ESAI_RFCR, ESAI_xFCR_xFR, ESAI_xFCR_xFR);
	dai_update_bits(dai, REG_ESAI_RFCR, ESAI_xFCR_xFR, 0);
	
	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
	/* TODO use TFCR and RFCR */
	dai_update_bits(dai, REG_ESAI_TFCR,
				ESAI_xFCR_xFR_MASK,
				ESAI_xFCR_xFR);

	dai_update_bits(dai, REG_ESAI_TFCR,
				ESAI_xFCR_xFR_MASK | ESAI_xFCR_xWA_MASK | ESAI_xFCR_xFWM_MASK | ESAI_xFCR_TE_MASK | ESAI_xFCR_TIEN,
				ESAI_xFCR_xWA(16) | ESAI_xFCR_xFWM(64) | ESAI_xFCR_TE(1) | ESAI_xFCR_TIEN);
	/* ESAI_xFCR_xWA(bits) instead of 16 ^^^^ */

	dai_update_bits(dai, REG_ESAI_TCCR, ESAI_xCCR_xDC_MASK, ESAI_xCCR_xDC(2));
	dai_update_bits(dai, REG_ESAI_TCCR, ESAI_xCCR_xFP_MASK, ESAI_xCCR_xFP(8));
	dai_update_bits(dai, REG_ESAI_TCCR, ESAI_xCCR_xPSR_MASK, ESAI_xCCR_xPSR_BYPASS);


	/* Remove ESAI personal reset */
	//dai_update_bits(dai, REG_ESAI_PRRC, ESAI_PRRC_PDC_MASK,
	//		ESAI_PRRC_PDC(ESAI_GPIO));
	dai_update_bits(dai, REG_ESAI_PCRC, ESAI_PCRC_PC_MASK,
			ESAI_PCRC_PC(ESAI_GPIO));

	tracev_esai("ESAI_REGS_DUMP in esai_set_config");
	esai_regs_dump(dai);
	return 0;
}

static void esai_start(struct dai *dai, int direction)
{
	direction = direction == SOF_IPC_STREAM_PLAYBACK;
	/* FIFO enable */
	dai_update_bits(dai, REG_ESAI_xFCR(direction), ESAI_xFCR_xFEN_MASK,
			ESAI_xFCR_xFEN);

	
	dai_update_bits(dai, REG_ESAI_xCR(direction),
			direction ? ESAI_xCR_TE_MASK : ESAI_xCR_RE_MASK,
			direction ? ESAI_xCR_TE(1) : ESAI_xCR_RE(1));

	dai_update_bits(dai, REG_ESAI_xSMB(direction), ESAI_xSMB_xS_MASK,
			ESAI_xSMB_xS(0x3));

	dai_update_bits(dai, REG_ESAI_xSMA(direction), ESAI_xSMA_xS_MASK,
			ESAI_xSMA_xS(0x3));

	/* enable regular interrupt */
	dai_update_bits(dai, REG_ESAI_xCR(direction), ESAI_xCR_xIE,
			ESAI_xCR_xIE);
	/* enable exception interrupt */
	dai_update_bits(dai, REG_ESAI_xCR(direction), ESAI_xCR_xEIE,
			ESAI_xCR_xEIE);
	tracev_esai("ESAI_REGS_DUMP in esai_start");
	esai_regs_dump(dai);
}

static void esai_stop(struct dai *dai, int direction)
{
	direction = direction == SOF_IPC_STREAM_PLAYBACK;
	/* disable exception interrupt */
	dai_update_bits(dai, REG_ESAI_xCR(direction), ESAI_xCR_xEIE, 0);
	/* disable regular interrupt */
	dai_update_bits(dai, REG_ESAI_xCR(direction), ESAI_xCR_xIE, 0);

	dai_update_bits(dai, REG_ESAI_xCR(direction),
			direction ? ESAI_xCR_TE_MASK : ESAI_xCR_RE_MASK, 0);

	dai_update_bits(dai, REG_ESAI_xSMA(direction), ESAI_xSMA_xS_MASK, 0);
	dai_update_bits(dai, REG_ESAI_xSMB(direction), ESAI_xSMB_xS_MASK, 0);

	/* disable and reset FIFO */
	dai_update_bits(dai, REG_ESAI_xFCR(direction),
			ESAI_xFCR_xFR | ESAI_xFCR_xFEN, ESAI_xFCR_xFR);
	dai_update_bits(dai, REG_ESAI_xFCR(direction), ESAI_xFCR_xFR, 0);
	tracev_esai("ESAI_REGS_DUMP in esai_stop");
	esai_regs_dump(dai);
}

static int esai_trigger(struct dai *dai, int cmd, int direction)
{
	tracev_esai("ESAI: trigger");
	switch (direction) {
	case DAI_DIR_PLAYBACK:
		tracev_esai("ESAI: playback trigger");
		break;
	case DAI_DIR_CAPTURE:
		tracev_esai("ESAI: capture trigger");
		break;
	default:
		trace_esai_error("ESAI: <INVALID DIRECTION %d> trigger", direction);
	}
	switch (cmd) {
#define CASE(c) case COMP_ ## c: tracev_esai("ESAI: trigger cmd DAI_" #c ); break;
#define CASE2(c, p) case COMP_ ## c: tracev_esai("ESAI: trigger cmd DAI_" #p); break;
	case COMP_TRIGGER_START:
		tracev_esai("ESAI: trigger cmd DAI_TRIGGER_START");
		esai_start(dai, direction);
		break;
	case COMP_TRIGGER_STOP:
		tracev_esai("ESAI: trigger cmd DAI_TRIGGER_STOP");
		esai_stop(dai, direction);
		break;
	case COMP_TRIGGER_PAUSE:
		tracev_esai("ESAI: trigger cmd DAI_TRIGGER_PAUSE_PUSH");
		esai_stop(dai, direction);
		break;
	case COMP_TRIGGER_RELEASE:
		tracev_esai("ESAI: trigger cmd DAI_TRIGGER_PAUSE_RELEASE");
		esai_start(dai, direction);
		break;
	/* Remaining triggers are no-ops, just print */
	CASE(TRIGGER_SUSPEND)
	CASE(TRIGGER_RESUME)
#undef CASE
	default:
		trace_esai_error("ESAI: invalid trigger cmd %d", cmd);
		break;
	}
	return 0;
}

static void esai_irq(void *ign)
{
	tracev_esai("ESAI: irq");
}

static int esai_probe(struct dai *dai)
{
	int irq, rc;
	tracev_esai("ESAI: probe");
	/* ESAI core reset */
	dai_write(dai, REG_ESAI_ECR, ESAI_ECR_ERST | ESAI_ECR_ESAIEN);
	/* We should do ESAI individual reset for the FIFOs */
	dai_write(dai, REG_ESAI_TFCR, ESAI_xFCR_xFR);
	dai_write(dai, REG_ESAI_RFCR, ESAI_xFCR_xFR);
	/* Clear TSMA, TSMB */
	dai_write(dai, REG_ESAI_TSMA, 0);
	dai_write(dai, REG_ESAI_TSMB, 0);
	dai_write(dai, REG_ESAI_RSMA, 0);
	dai_write(dai, REG_ESAI_RSMB, 0);

	dai_write(dai, REG_ESAI_ECR, ESAI_ECR_ESAIEN);

	irq = irqstr_get_sof_int(ESAI_IRQ);
	rc = interrupt_register(irq, IRQ_AUTO_UNMASK, &esai_irq, dai);
	if (rc >= 0 || rc == -EEXIST)
		interrupt_enable(irq, dai);

	tracev_esai("ESAI_REGS_DUMP in esai_probe");
	esai_regs_dump(dai);
	return 0;
}

static int esai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	// TODO
	return 0;
}

static int esai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	switch (direction) {
	case DAI_DIR_PLAYBACK:
	case DAI_DIR_CAPTURE:
		return dai_fifo(dai, direction); // stream_id is unused
	default:
		trace_esai_error("esai_get_fifo(): Invalid direction");
		return -EINVAL;
	}
}

const struct dai_driver esai_driver = {
	.type = SOF_DAI_IMX_ESAI,
	.dma_dev = DMA_DEV_ESAI,
	.ops = {
		.trigger		= esai_trigger,
		.set_config		= esai_set_config,
		.pm_context_store	= esai_context_store,
		.pm_context_restore	= esai_context_restore,
		.probe			= esai_probe,
		.get_handshake		= esai_get_handshake,
		.get_fifo		= esai_get_fifo,
	},
};
