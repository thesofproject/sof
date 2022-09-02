// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#if CONFIG_INTEL_DMIC

#include <sof/audio/coefficients/pdm_decim/pdm_decim_fir.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/debug/panic.h>
#include <sof/drivers/dmic.h>
#include <rtos/interrupt.h>
#include <sof/drivers/timestamp.h>
#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/math/decibels.h>
#include <sof/math/numbers.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <ipc/dai.h>
#include <ipc/dai-intel.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(dmic_dai, CONFIG_SOF_LOG_LEVEL);

/* aafc26fe-3b8d-498d-8bd6-248fc72efa31 */
DECLARE_SOF_UUID("dmic-dai", dmic_uuid, 0xaafc26fe, 0x3b8d, 0x498d,
		 0x8b, 0xd6, 0x24, 0x8f, 0xc7, 0x2e, 0xfa, 0x31);

DECLARE_TR_CTX(dmic_tr, SOF_UUID(dmic_uuid), LOG_LEVEL_INFO);

/* Configuration ABI version, increment if not compatible with previous
 * version.
 */
#define DMIC_IPC_VERSION 1

/* Base addresses (in PDM scope) of 2ch PDM controllers and coefficient RAM. */
static const uint32_t base[4] = {PDM0, PDM1, PDM2, PDM3};

/* Global configuration request and state for DMIC */
static SHARED_DATA struct dmic_global_shared dmic_global;

/* this ramps volume changes over time */
static void dmic_gain_ramp(struct dai *dai)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	k_spinlock_key_t key;
	int32_t gval;
	uint32_t val;
	int i;

	/* Currently there's no DMIC HW internal mutings and wait times
	 * applied into this start sequence. It can be implemented here if
	 * start of audio capture would contain clicks and/or noise and it
	 * is not suppressed by gain ramp somewhere in the capture pipe.
	 */

	dai_dbg(dai, "dmic_gain_ramp()");

	/*
	 * At run-time dmic->gain is only changed in this function, and this
	 * function runs in the pipeline task context, so it cannot run
	 * concurrently on multiple cores, since there's always only one
	 * task associated with each DAI, so we don't need to hold the lock to
	 * read the value here.
	 */
	if (dmic->gain == DMIC_HW_FIR_GAIN_MAX << 11)
		return;

	key = k_spin_lock(&dai->lock);

	/* Increment gain with logarithmic step.
	 * Gain is Q2.30 and gain modifier is Q12.20.
	 */
	dmic->startcount++;
	dmic->gain = q_multsr_sat_32x32(dmic->gain, dmic->gain_coef, Q_SHIFT_GAIN_X_GAIN_COEF);

	/* Gain is stored as Q2.30, while HW register is Q1.19 so shift
	 * the value right by 11.
	 */
	gval = dmic->gain >> 11;

	/* Note that DMIC gain value zero has a special purpose. Value zero
	 * sets gain bypass mode in HW. Zero value will be applied after ramp
	 * is complete. It is because exact 1.0 gain is not possible with Q1.19.
	 */
	if (gval > DMIC_HW_FIR_GAIN_MAX) {
		gval = 0;
		dmic->gain = DMIC_HW_FIR_GAIN_MAX << 11;
	}

	/* Write gain to registers */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		if (!dmic->enable[i])
			continue;

		if (dmic->startcount == DMIC_UNMUTE_CIC)
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_MIC_MUTE_BIT, 0);

		if (dmic->startcount == DMIC_UNMUTE_FIR) {
			switch (dai->index) {
			case 0:
				dai_update_bits(dai, base[i] + FIR_CONTROL_A,
						FIR_CONTROL_A_MUTE_BIT, 0);
				break;
			case 1:
				dai_update_bits(dai, base[i] + FIR_CONTROL_B,
						FIR_CONTROL_B_MUTE_BIT, 0);
				break;
			}
		}
		switch (dai->index) {
		case 0:
			val = OUT_GAIN_LEFT_A_GAIN(gval);
			dai_write(dai, base[i] + OUT_GAIN_LEFT_A, val);
			dai_write(dai, base[i] + OUT_GAIN_RIGHT_A, val);
			break;
		case 1:
			val = OUT_GAIN_LEFT_B_GAIN(gval);
			dai_write(dai, base[i] + OUT_GAIN_LEFT_B, val);
			dai_write(dai, base[i] + OUT_GAIN_RIGHT_B, val);
			break;
		}
	}


	k_spin_unlock(&dai->lock, key);
}

/* get DMIC hw params */
static int dmic_get_hw_params(struct dai *dai,
			      struct sof_ipc_stream_params *params, int dir)
{
#if CONFIG_INTEL_DMIC_TPLG_PARAMS
	return dmic_get_hw_params_computed(dai, params, dir);

#elif CONFIG_INTEL_DMIC_NHLT
	return dmic_get_hw_params_nhlt(dai, params, dir);

#else
	return -EINVAL;
#endif
}

static int dmic_set_config(struct dai *dai, struct ipc_config_dai *common_config, void *spec_config)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int ret = 0;
	int di = dai->index;
	k_spinlock_key_t key;
#if CONFIG_INTEL_DMIC_TPLG_PARAMS
	struct sof_ipc_dai_config *config = spec_config;
	int i;
	int j;
#endif

	dai_info(dai, "dmic_set_config()");

	if (di >= DMIC_HW_FIFOS) {
		dai_err(dai, "dmic_set_config(): DAI index exceeds number of FIFOs");
		return -EINVAL;
	}

	if (!spec_config) {
		dai_err(dai, "dmic_set_config(): NULL config");
		return -EINVAL;
	}

	assert(dmic);
	key = k_spin_lock(&dai->lock);

#if CONFIG_INTEL_DMIC_TPLG_PARAMS
	/*
	 * "config" might contain pdm controller params for only
	 * the active controllers
	 * "prm" is initialized with default params for all HW controllers
	 */
	if (config->dmic.driver_ipc_version != DMIC_IPC_VERSION) {
		dai_err(dai, "dmic_set_config(): wrong ipc version");
		ret = -EINVAL;
		goto out;
	}

	if (config->dmic.num_pdm_active > DMIC_HW_CONTROLLERS) {
		dai_err(dai, "dmic_set_config(): the requested PDM controllers count exceeds platform capability");
		ret = -EINVAL;
		goto out;
	}

	/* Get unmute gain ramp duration. Use the value from topology
	 * if it is non-zero, otherwise use default length.
	 */
	if (config->dmic.unmute_ramp_time)
		dmic->unmute_ramp_time_ms = config->dmic.unmute_ramp_time;
	else
		dmic->unmute_ramp_time_ms = dmic_get_unmute_ramp_from_samplerate(
			config->dmic.fifo_fs);

	if (dmic->unmute_ramp_time_ms < LOGRAMP_TIME_MIN_MS ||
	    dmic->unmute_ramp_time_ms > LOGRAMP_TIME_MAX_MS) {
		dai_err(dai, "dmic_set_config(): Illegal ramp time = %d",
			dmic->unmute_ramp_time_ms);
		ret = -EINVAL;
		goto out;
	}

	/* Copy the new DMIC params header (all but not pdm[]) to persistent.
	 * The last arrived request determines the parameters.
	 */
	ret = memcpy_s(&dmic->global->prm[di], sizeof(dmic->global->prm[di]), &config->dmic,
		       offsetof(struct sof_ipc_dai_dmic_params, pdm));
	assert(!ret);

	/* copy the pdm controller params from ipc */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		dmic->global->prm[di].pdm[i].id = i;
		for (j = 0; j < config->dmic.num_pdm_active; j++) {
			/* copy the pdm controller params id the id's match */
			if (dmic->global->prm[di].pdm[i].id == config->dmic.pdm[j].id) {
				ret = memcpy_s(&dmic->global->prm[di].pdm[i],
					       sizeof(dmic->global->prm[di].pdm[i]),
					       &config->dmic.pdm[j],
					       sizeof(struct sof_ipc_dai_dmic_pdm_ctrl));
				assert(!ret);
			}
		}
	}

	ret = dmic_set_config_computed(dai);

#elif CONFIG_INTEL_DMIC_NHLT
	ret = dmic_set_config_nhlt(dai, spec_config);

	/* There's no unmute ramp duration in blob, so the default rate dependent is used. */
	dmic->unmute_ramp_time_ms = dmic_get_unmute_ramp_from_samplerate(dmic->dai_rate);
#else
	ret = -EINVAL;
#endif

	if (ret < 0) {
		dai_err(dai, "dmic_set_config(): Failed to set the requested configuration.");
		goto out;
	}

	dai_info(dai, "dmic_set_config(): unmute_ramp_time_ms = %d", dmic->unmute_ramp_time_ms);

	dmic->state = COMP_STATE_PREPARE;

out:
	k_spin_unlock(&dai->lock, key);
	return ret;
}

/* start the DMIC for capture */
static void dmic_start(struct dai *dai)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	k_spinlock_key_t key;
	int32_t step_db;
	int i;
	int mic_a;
	int mic_b;
	int fir_a;
	int fir_b;

	/* enable port */
	key = k_spin_lock(&dai->lock);
	dai_dbg(dai, "dmic_start()");
	dmic->startcount = 0;

	/*
	 * Compute unmute ramp gain update coefficient, based on DAI processing
	 * period in microseconds.
	 */
	step_db = dai->dd->dai_dev->period * (int64_t)-LOGRAMP_START_DB /
		(1000 * dmic->unmute_ramp_time_ms);
	dmic->gain_coef = db2lin_fixed(step_db);

	/* Initial gain value, convert Q12.20 to Q2.30 */
	dmic->gain = Q_SHIFT_LEFT(db2lin_fixed(LOGRAMP_START_DB), 20, 30);

	switch (dai->index) {
	case 0:
		dai_info(dai, "dmic_start(), dmic->fifo_a");
		/*  Clear FIFO A initialize, Enable interrupts to DSP,
		 *  Start FIFO A packer.
		 */
		dai_update_bits(dai, OUTCONTROL0,
				OUTCONTROL0_FINIT_BIT | OUTCONTROL0_SIP_BIT,
				OUTCONTROL0_SIP_BIT);
		break;
	case 1:
		dai_info(dai, "dmic_start(), dmic->fifo_b");
		/*  Clear FIFO B initialize, Enable interrupts to DSP,
		 *  Start FIFO B packer.
		 */
		dai_update_bits(dai, OUTCONTROL1,
				OUTCONTROL1_FINIT_BIT | OUTCONTROL1_SIP_BIT,
				OUTCONTROL1_SIP_BIT);
	}

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		mic_a = dmic->enable[i] & 1;
		mic_b = (dmic->enable[i] & 2) >> 1;
		fir_a = (dmic->enable[i] > 0) ? 1 : 0;
#if DMIC_HW_FIFOS > 1
		fir_b = (dmic->enable[i] > 0) ? 1 : 0;
#else
		fir_b = 0;
#endif
		dai_info(dai, "dmic_start(), pdm%d mic_a = %u, mic_b = %u", i, mic_a, mic_b);

		/* If both microphones are needed start them simultaneously
		 * to start them in sync. The reset may be cleared for another
		 * FIFO already. If only one mic, start them independently.
		 * This makes sure we do not clear start/en for another DAI.
		 */
		if (mic_a && mic_b) {
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_CIC_START_A_BIT |
					CIC_CONTROL_CIC_START_B_BIT,
					CIC_CONTROL_CIC_START_A(1) |
					CIC_CONTROL_CIC_START_B(1));
			dai_update_bits(dai, base[i] + MIC_CONTROL,
					MIC_CONTROL_PDM_EN_A_BIT |
					MIC_CONTROL_PDM_EN_B_BIT,
					MIC_CONTROL_PDM_EN_A(1) |
					MIC_CONTROL_PDM_EN_B(1));
		} else if (mic_a) {
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_CIC_START_A_BIT,
					CIC_CONTROL_CIC_START_A(1));
			dai_update_bits(dai, base[i] + MIC_CONTROL,
					MIC_CONTROL_PDM_EN_A_BIT,
					MIC_CONTROL_PDM_EN_A(1));
		} else if (mic_b) {
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_CIC_START_B_BIT,
					CIC_CONTROL_CIC_START_B(1));
			dai_update_bits(dai, base[i] + MIC_CONTROL,
					MIC_CONTROL_PDM_EN_B_BIT,
					MIC_CONTROL_PDM_EN_B(1));
		}

		switch (dai->index) {
		case 0:
			dai_update_bits(dai, base[i] + FIR_CONTROL_A,
					FIR_CONTROL_A_START_BIT,
					FIR_CONTROL_A_START(fir_a));
			break;
		case 1:
			dai_update_bits(dai, base[i] + FIR_CONTROL_B,
					FIR_CONTROL_B_START_BIT,
					FIR_CONTROL_B_START(fir_b));
			break;
		}
	}

	/* Clear soft reset for all/used PDM controllers. This should
	 * start capture in sync.
	 */
	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		dai_update_bits(dai, base[i] + CIC_CONTROL,
				CIC_CONTROL_SOFT_RESET_BIT, 0);
	}

	/* Set bit dai->index */
	dmic->global->active_fifos_mask |= BIT(dai->index);
	dmic->global->pause_mask &= ~BIT(dai->index);

	dmic->state = COMP_STATE_ACTIVE;
	k_spin_unlock(&dai->lock, key);

	dai_info(dai, "dmic_start(), dmic_active_fifos_mask = 0x%x",
		 dmic->global->active_fifos_mask);
}

static void dmic_stop_fifo_packers(struct dai *dai, int fifo_index)
{
	/* Stop FIFO packers and set FIFO initialize bits */
	switch (fifo_index) {
	case 0:
		dai_update_bits(dai, OUTCONTROL0,
				OUTCONTROL0_SIP_BIT | OUTCONTROL0_FINIT_BIT,
				OUTCONTROL0_FINIT_BIT);
		break;
	case 1:
		dai_update_bits(dai, OUTCONTROL1,
				OUTCONTROL1_SIP_BIT | OUTCONTROL1_FINIT_BIT,
				OUTCONTROL1_FINIT_BIT);
		break;
	}
}

/* stop the DMIC for capture */
static void dmic_stop(struct dai *dai, bool stop_is_pause)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	k_spinlock_key_t key;
	int i;

	dai_dbg(dai, "dmic_stop()");
	key = k_spin_lock(&dai->lock);

	dmic_stop_fifo_packers(dai, dai->index);

	/* Set soft reset and mute on for all PDM controllers.
	 */
	dai_info(dai, "dmic_stop(), dmic_active_fifos_mask = 0x%x",
		 dmic->global->active_fifos_mask);

	/* Clear bit dai->index for active FIFO. If stop for pause, set pause mask bit.
	 * If stop is not for pausing, it is safe to clear the pause bit.
	 */
	dmic->global->active_fifos_mask &= ~BIT(dai->index);
	if (stop_is_pause)
		dmic->global->pause_mask |= BIT(dai->index);
	else
		dmic->global->pause_mask &= ~BIT(dai->index);

	for (i = 0; i < DMIC_HW_CONTROLLERS; i++) {
		/* Don't stop CIC yet if one FIFO remains active */
		if (dmic->global->active_fifos_mask == 0) {
			dai_update_bits(dai, base[i] + CIC_CONTROL,
					CIC_CONTROL_SOFT_RESET_BIT |
					CIC_CONTROL_MIC_MUTE_BIT,
					CIC_CONTROL_SOFT_RESET_BIT |
					CIC_CONTROL_MIC_MUTE_BIT);
		}
		switch (dai->index) {
		case 0:
			dai_update_bits(dai, base[i] + FIR_CONTROL_A,
					FIR_CONTROL_A_MUTE_BIT,
					FIR_CONTROL_A_MUTE_BIT);
			break;
		case 1:
			dai_update_bits(dai, base[i] + FIR_CONTROL_B,
					FIR_CONTROL_B_MUTE_BIT,
					FIR_CONTROL_B_MUTE_BIT);
			break;
		}
	}

	k_spin_unlock(&dai->lock, key);
}

static int dmic_trigger(struct dai *dai, int cmd, int direction)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);

	dai_dbg(dai, "dmic_trigger()");

	/* dai private is set in dmic_probe(), error if not set */
	if (!dmic) {
		dai_err(dai, "dmic_trigger(): dai not set");
		return -EINVAL;
	}

	if (direction != DAI_DIR_CAPTURE) {
		dai_err(dai, "dmic_trigger(): direction != DAI_DIR_CAPTURE");
		return -EINVAL;
	}

	switch (cmd) {
	case COMP_TRIGGER_RELEASE:
	case COMP_TRIGGER_START:
		if (dmic->state == COMP_STATE_PREPARE ||
		    dmic->state == COMP_STATE_PAUSED) {
			dmic_start(dai);
		} else {
			dai_err(dai, "dmic_trigger(): state is not prepare or paused, dmic->state = %u",
				dmic->state);
		}
		break;
	case COMP_TRIGGER_STOP:
		dmic->state = COMP_STATE_PREPARE;
		dmic_stop(dai, false);
		break;
	case COMP_TRIGGER_PAUSE:
		dmic->state = COMP_STATE_PAUSED;
		dmic_stop(dai, true);
		break;
	}


	return 0;
}

/* On DMIC IRQ event trace the status register that contains the status and
 * error bit fields.
 */
static void dmic_irq_handler(void *data)
{
	struct dai *dai = data;
	uint32_t val0;
	uint32_t val1;

	/* Trace OUTSTAT0 register */
	val0 = dai_read(dai, OUTSTAT0);
	val1 = dai_read(dai, OUTSTAT1);
	dai_info(dai, "dmic_irq_handler(), OUTSTAT0 = 0x%x, OUTSTAT1 = 0x%x", val0, val1);

	if (val0 & OUTSTAT0_ROR_BIT) {
		dai_err(dai, "dmic_irq_handler(): full fifo A or PDM overrun");
		dai_write(dai, OUTSTAT0, val0);
		dmic_stop_fifo_packers(dai, 0);
	}

	if (val1 & OUTSTAT1_ROR_BIT) {
		dai_err(dai, "dmic_irq_handler(): full fifo B or PDM overrun");
		dai_write(dai, OUTSTAT1, val1);
		dmic_stop_fifo_packers(dai, 1);
	}
}

static int dmic_probe(struct dai *dai)
{
	int irq = dmic_irq(dai);
	struct dmic_pdata *dmic;
	int ret;

	dai_info(dai, "dmic_probe()");

	if (dai_get_drvdata(dai))
		return -EEXIST; /* already created */

	dmic = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dmic));
	if (!dmic) {
		dai_err(dai, "dmic_probe(): alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, dmic);

	/* Common shared data for all DMIC DAI instances */
	dmic->global = platform_shared_get(&dmic_global, sizeof(dmic_global));

	/* Set state, note there is no playback direction support */
	dmic->state = COMP_STATE_READY;

	/* register our IRQ handler */
	dmic->irq = interrupt_get_irq(irq, dmic_irq_name(dai));
	if (dmic->irq < 0) {
		ret = dmic->irq;
		rfree(dmic);
		return ret;
	}

	ret = interrupt_register(dmic->irq, dmic_irq_handler, dai);
	if (ret < 0) {
		dai_err(dai, "dmic failed to allocate IRQ");
		rfree(dmic);
		return ret;
	}

	/* Enable DMIC power */
	pm_runtime_get_sync(DMIC_POW, dai->index);

	/* Disable dynamic clock gating for dmic before touching any reg */
	pm_runtime_get_sync(DMIC_CLK, dai->index);
	interrupt_enable(dmic->irq, dai);
	return 0;
}

static int dmic_remove(struct dai *dai)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	uint32_t active_fifos_mask = dmic->global->active_fifos_mask;
	uint32_t pause_mask = dmic->global->pause_mask;

	dai_info(dai, "dmic_remove()");

	interrupt_disable(dmic->irq, dai);
	interrupt_unregister(dmic->irq, dai);

	dai_info(dai, "dmic_remove(), dmic_active_fifos_mask = 0x%x, dmic_pause_mask = 0x%x",
		 active_fifos_mask, pause_mask);
	dai_set_drvdata(dai, NULL);
	rfree(dmic);

	/* The next end tasks must be passed if another DAI FIFO still runs.
	 * Note: dai_put() function that calls remove() applies the spinlock
	 * so it is not needed here to protect access to mask bits.
	 */
	if (active_fifos_mask || pause_mask)
		return 0;

	/* Disable DMIC clock and power */
	pm_runtime_put_sync(DMIC_CLK, dai->index);
	pm_runtime_put_sync(DMIC_POW, dai->index);
	return 0;
}

static int dmic_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[SOF_IPC_STREAM_CAPTURE].handshake;
}

static int dmic_get_fifo(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[SOF_IPC_STREAM_CAPTURE].offset;
}

static int dmic_get_fifo_depth(struct dai *dai, int direction)
{
	return dai->plat_data.fifo[SOF_IPC_STREAM_CAPTURE].depth;
}

const struct dai_driver dmic_driver = {
	.type = SOF_DAI_INTEL_DMIC,
	.uid = SOF_UUID(dmic_uuid),
	.tctx = &dmic_tr,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_DMIC,
	.ops = {
		.trigger		= dmic_trigger,
		.set_config		= dmic_set_config,
		.get_hw_params		= dmic_get_hw_params,
		.get_handshake		= dmic_get_handshake,
		.get_fifo		= dmic_get_fifo,
		.get_fifo_depth		= dmic_get_fifo_depth,
		.probe			= dmic_probe,
		.remove			= dmic_remove,
		.copy			= dmic_gain_ramp,
	},
	.ts_ops = {
		.ts_config		= timestamp_dmic_config,
		.ts_start		= timestamp_dmic_start,
		.ts_get			= timestamp_dmic_get,
		.ts_stop		= timestamp_dmic_stop,
	},
};

#endif
