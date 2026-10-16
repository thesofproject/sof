// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 AMD. All rights reserved.
 *
 * Author: Sneha Voona <sneha.voona@amd.com>
 *         Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *         DineshKumar Kalva <DineshKumar.Kalva@amd.com>
 */

#include <sof/common.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <rtos/sof.h>
#include <rtos/atomic.h>
#include <sof/audio/component.h>
#include <rtos/bit.h>
#include <sof/drivers/acp_dai_dma.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/wait.h>
#include <rtos/alloc.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <rtos/spinlock.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/fw_scratch_mem.h>

SOF_DEFINE_REG_UUID(acp_clk);
LOG_MODULE_REGISTER(acp_clk, CONFIG_SOF_LOG_LEVEL);
/* unused with Zephyr, generates no output */
DECLARE_TR_CTX(acp_clk_tr, SOF_UUID(acp_clk_uuid), LOG_LEVEL_INFO);

/* Frequency tables */
const struct freq_table platform_cpu_freq[] = {
	{600000000, 600000 },
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ,
			invalid_number_of_cpu_frequencies);

/* Type definitions */
typedef enum  {
	PLL_MODE_100MHZ_NORMAL,
	PLL_MODE_48MHZ_NORMAL,
	PLL_MODE_32KHZ_LPPM,
	PLL_MODE_48MHZ_LPPM,
	PLL_MODE_100MHZ_LPPM
} PLL_MODE;

/* Enumeration for the Clock Types */
typedef enum _acp_clock_type_ {
	acp_aclk_clock,
	acp_sclk_clock,
	acp_clock_type_max,
	acp_clock_type_force = 0xFF
} acp_clock_type_t;

/* AGT measures CLK6 for 100 us against the 48 MHz reference. A threshold of
 * 4800 counts gives the same 0.01 MHz resolution as agt_internal.
 */
#define ACP_CLK_MEASURE_THRESHOLD	4800
#define ACP_CLK_MEASURE_REFCLK_MHZ	48
#define ACP_CLK_MEASURE_SETTLE_US	120
#define CLK_POLL_DELAY_US		10
#define CLK_POLL_COUNT			10
#define ACP_DFS_BYPASS_EXIT_RETRIES	4

/* Static variables */
static struct clock_info platform_clocks_info[NUM_CLOCKS];

/* Function prototypes */
void audio_pll_power_off(void);
void audio_pll_power_on(void);
void clk_dfs_bypassexit(void);
void audio_pll_mode_switch(uint32_t mode, uint32_t fcw_int, uint32_t fcw_frac,
	uint32_t fcw_denom, uint32_t pllspinediv);
void clk7_init_vco(void);
void acp_7_x_reg_wait(void);
void acp_7_x_get_boot_ref_clock(float *boot_ref_clk);
uint32_t acp_clk_update_dfs_did(uint32_t did, uint32_t clk_type);
void acp_cpl_aclk_dfs_did_update(bool poll, uint32_t timeout_cnt, uint8_t did);
void acp_cpl_audioclk_dfs_did_update(bool poll, uint32_t timeout_cnt, uint8_t did);
void acp_cpl_aclk_poll_dfs_div_req_idle(uint32_t timeout_cnt);
void acp_cpl_audioclk_poll_dfs_div_req_idle(uint32_t timeout_cnt);
void change_clock_notify(uint32_t clock_freq);
int acp_get_current_clk(acp_clock_type_t clk_type,
			 float *clk_value);

/* Static function prototypes */
static int acp_reg_read_via_smn(uint32_t reg_offset, uint32_t size);
static void acp_reg_write_via_smn(uint32_t reg_offset, uint32_t reg_value, uint32_t size);
static void acp_log_current_aclk(const char *state);
static void acp_log_dfs_dividers(const char *state);
static void acp_log_clk_sources(const char *state);

static int acp_reg_read_via_smn(uint32_t reg_offset,
				uint32_t size)
{
	uint32_t reg_value;
	uint32_t delay_cnt = 10000;
	uint32_t smn_client_base_addr = (reg_offset >> 10);
	uint32_t region_start_addr = (smn_client_base_addr << 10);
	uint32_t apertureid = ((reg_offset >> 20) & 0xFFF);
	acp_srbm_cycle_sts_t acp_srbm_cycle_sts;

	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_CONFIG), apertureid);
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_BASE_ADDR), smn_client_base_addr);
	reg_value = (uint32_t)io_reg_read(PU_REGISTER_BASE +
	(ACP_MASTER_REG_ACCESS_ADDRESS + reg_offset - region_start_addr + ACP_FIRST_REG_OFFSET));
	if (reg_value) {
		reg_value = 0;
	}
	acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
				ACP_SRBM_CYCLE_STS);
	while (delay_cnt > 0) {
		if (!acp_srbm_cycle_sts.bits.srbm_clients_sts) {
			return (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_SRBM_CLIENT_RDDATA);
		}
		acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
				ACP_SRBM_CYCLE_STS);
		delay_cnt--;
	}
	return -1;
}

static void acp_reg_write_via_smn(uint32_t reg_offset,
					uint32_t value, uint32_t size)
{
	uint32_t delay_cnt = 10000;
	uint32_t smn_client_base_addr = (reg_offset >> 10);
	uint32_t region_start_addr = (smn_client_base_addr << 10);
	uint32_t apertureid = ((reg_offset >> 20) & 0xFFF);
	acp_srbm_cycle_sts_t acp_srbm_cycle_sts;

	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_CONFIG), apertureid);
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_BASE_ADDR), smn_client_base_addr);
	io_reg_write((PU_REGISTER_BASE + (ACP_MASTER_REG_ACCESS_ADDRESS +
		reg_offset - region_start_addr +
				ACP_FIRST_REG_OFFSET)),	value);
	acp_srbm_cycle_sts =
		(acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE + ACP_SRBM_CYCLE_STS);
	while (delay_cnt > 0) {
		acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
				ACP_SRBM_CYCLE_STS);
		if (!acp_srbm_cycle_sts.bits.srbm_clients_sts) {
			return;
		}
		delay_cnt--;
	}
}

void platform_clock_init(struct sof *sof)
{
	int i;

	sof->clocks = platform_clocks_info;
	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info) {
			.freqs_num = NUM_CPU_FREQ,
			.freqs = platform_cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = NULL,
		};
	}
	/* acp_reg_write_via_smn(CLK6_PLL0_DFS0_CNTL, 0x19, sizeof(int)); */
}
void acp_cpl_audioclk_poll_dfs_div_req_idle(uint32_t timeout_cnt)
{
	uint32_t timeout = 0;
	clk_dfs_status_u_t clk_dfs_status;

	do {
		clk_dfs_status.u32All = acp_reg_read_via_smn(
				ACP_CLK6_PLL0_DFS0_STATUS, sizeof(uint32_t));
		timeout++;
	} while ((clk_dfs_status.bitfields.PLL0_DFS0_DFS_DIV_REQ_IDLE == 0) &
		 (timeout < timeout_cnt));

}

/**
 * \brief Name a PMFW response code read from MP1_C2PMSG_38.
 *
 * \param resp Response register value.
 * \return Printable description of the response.
 */
static const char *mp1_resp_name(uint32_t resp)
{
	switch (resp) {
	case 0x0:
		return "no response";
	case 0x1:
		return "OK";
	case 0xFF:
		return "FAIL";
	case 0xFE:
		return "UNKNOWN CMD";
	case 0xFD:
		return "BAD PREREQ";
	case 0xFC:
		return "BUSY";
	default:
		return "unexpected";
	}
}

uint32_t mp1_mailbox_send(uint32_t _reg_id, uint32_t _value)
{
	uint32_t ready = 0;
	uint32_t wait_cnt = 0;

	while (wait_cnt < 0x1000) {
		ready = acp_reg_read_via_smn(mmMP1_C2PMSG_38, sizeof(ready));
		if (ready != 0) {
			break;
		}
		wait_cnt++;
	}
		tr_info(&acp_clk_tr, "MP1 send msg 0x%x arg 0x%x: C2PMSG_38 before 0x%x (%s) after %u polls",
		_reg_id, _value, ready, mp1_resp_name(ready), wait_cnt);
	if (ready == 0) {
		return 0;
	}
	/* Clear the response register */
	acp_reg_write_via_smn(mmMP1_C2PMSG_38, 0, sizeof(uint32_t));
	/* Write the argument (value) to the argument register */
	acp_reg_write_via_smn(mmMP1_C2PMSG_39, _value, sizeof(uint32_t));
	/* Write the message ID to the message register */
	acp_reg_write_via_smn(mmMP1_C2PMSG_37, _reg_id, sizeof(uint32_t));
	/* Poll until response is non-zero */
	uint32_t resp = 0;
	/* TODO: Enable this code after bringup is complete. This is commented to avoid hang */
	int count = 0;

	do {
		resp = acp_reg_read_via_smn(mmMP1_C2PMSG_38, sizeof(uint32_t));
		count++;
		if (count >= 100) {
			break;
		}
	} while (resp == 0);
	tr_info(&acp_clk_tr, "PllPowerState msg 0x%x arg 0x%x response 0x%x (%s) after %d polls, "
		"expect 0x1 (OK)",
		_reg_id, _value, resp, mp1_resp_name(resp), count);

	return resp;
}

void acp_clk_tick_cnt_enable(void)
{
	clk_tick_cnt_config_reg_t clk_tick_config;

	clk_tick_config.u32All = acp_reg_read_via_smn(
			CLK_TICK_CNT_CONFIG_REG, sizeof(uint32_t));
	clk_tick_config.bitfields.TIMER_ENABLE = 1;
	clk_tick_config.bitfields.TIMER_THRESHOLD = ACP_CLK_MEASURE_THRESHOLD;
	acp_reg_write_via_smn(CLK_TICK_CNT_CONFIG_REG,
			      clk_tick_config.u32All, sizeof(uint32_t));
}

/* Stop waiting for PMFW 500 us after the mailbox request is posted. */
#define ACP_PMFW_POLL_US	500

/**
 * \brief Send one PMFW mailbox message and wait up to 500 us for a reply.
 *
 * \param msg Message id written to MP1_C2PMSG_37.
 * \param arg Argument written to MP1_C2PMSG_39.
 * \return Last value read from MP1_C2PMSG_38. Non-zero means PMFW answered.
 */
static uint32_t acp_pmfw_poll_response(uint32_t msg, uint32_t arg)
{
	uint32_t resp = 0;
	int try = 0;
	uint64_t start;
	uint64_t deadline;
	uint32_t elapsed_us;

	acp_reg_write_via_smn(mmMP1_C2PMSG_38, 0, sizeof(uint32_t));
	acp_reg_write_via_smn(mmMP1_C2PMSG_39, arg, sizeof(uint32_t));
	acp_reg_write_via_smn(mmMP1_C2PMSG_37, msg, sizeof(uint32_t));

	start = sof_cycle_get_64();
	deadline = start + k_us_to_cyc_ceil64(ACP_PMFW_POLL_US);

	do {
		resp = acp_reg_read_via_smn(mmMP1_C2PMSG_38, sizeof(uint32_t));
		try++;
		if (resp || sof_cycle_get_64() >= deadline) {
			break;
		}
		wait_delay_us(1000);/*1ms delay to avoid busy loop*/
	} while (sof_cycle_get_64() < deadline);

	elapsed_us = (uint32_t)k_cyc_to_us_floor64(sof_cycle_get_64() - start);
	tr_info(&acp_clk_tr,
		"PMFW mailbox response: msg=0x%x arg=0x%x resp=0x%x tries=%d elapsed_us=%u limit_us=%u",
		msg, arg, resp, try, elapsed_us, ACP_PMFW_POLL_US);

	return resp;
}

/* ACP_CLKMUX_SEL divides ACLK inside ACP; slice select stays with CLK6. */
static void acp_clkmux_sel(uint32_t div_val)
{
	acp7x_clkmux_sel_t mux_sel;

	mux_sel.u32All = (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_CLKMUX_SEL);
	mux_sel.bitfields.DIV_SEL = div_val;
	io_reg_write(PU_REGISTER_BASE + ACP_CLKMUX_SEL, mux_sel.u32All);
}

/* Post a DFS bypass request and wait for CLK6 to clear it. */
static void acp_clk_dfsbypass_request(uint32_t req_bit, uint32_t timeout_cnt)
{
	uint32_t value;
	uint32_t count;

	value = acp_reg_read_via_smn(ACP_CLK6_PLL0_DFSBYPASS_CONTROL, sizeof(uint32_t));
	acp_reg_write_via_smn(ACP_CLK6_PLL0_DFSBYPASS_CONTROL, value | req_bit,
			      sizeof(uint32_t));

	for (count = 0; count < timeout_cnt; count++) {
		value = acp_reg_read_via_smn(ACP_CLK6_PLL0_DFSBYPASS_CONTROL,
					    sizeof(uint32_t));
		if (!(value & req_bit)) {
			return;
		}
		wait_delay_us(CLK_POLL_DELAY_US);
	}

	tr_warn(&acp_clk_tr, "DFS bypass request 0x%x still pending: cntl 0x%x",
		req_bit, value);
}

/* Configure CLK6 slice 5, which feeds S5_REFCLK. */
static void acp_s5_refclk_config(uint32_t postdiv_id, uint32_t bypass_sel,
				 uint32_t bypass_div)
{
	clk6_clk_postdiv_cntl_u_t postdiv_cntl;
	clk6_bypass_cntl_u_t bypass_cntl;

	postdiv_cntl.u32All = 0;
	postdiv_cntl.bitfields.CLK0_POSTDIV_DIV_ID = postdiv_id;
	postdiv_cntl.bitfields.CLK0_POSTDIV_RAMP_STEP = 1;
	acp_reg_write_via_smn(ACP_CLK6_CLK5_POSTDIV_CNTL, postdiv_cntl.u32All,
			      sizeof(uint32_t));

	bypass_cntl.u32All = 0;
	bypass_cntl.bitfields.CLK_BYPASS_SEL = bypass_sel;
	bypass_cntl.bitfields.CLK_BYPASS_DIV = bypass_div;
	acp_reg_write_via_smn(ACP_CLK6_CLK5_BYPASS_CNTL, bypass_cntl.u32All,
			      sizeof(uint32_t));
}

/* Keep ACLK out of deep sleep while a stream is active. */
static void acp_aclk_deepsleep_disable(void)
{
	clk6_clk_ds_cntl_u_t ds_cntl;

	io_reg_write(PU_REGISTER_BASE + ACP_ACLK_AUDIOCLK_CTRL, ACP_DEEPSLEEP_DISABLE);

	ds_cntl.u32All = acp_reg_read_via_smn(ACP_CLK6_CLK1_DS_CNTL, sizeof(uint32_t));
	ds_cntl.bitfields.CLK_ALLOW_DS = 0;
	ds_cntl.bitfields.CLK_DS_DIV_ID = 3;
	ds_cntl.bitfields.CLK_DS_DEGL_SET = 1;
	acp_reg_write_via_smn(ACP_CLK6_CLK1_DS_CNTL, ds_cntl.u32All, sizeof(uint32_t));
}

/* Restore the clock inputs used by PDM and SoundWire consumers. */
static void acp_io_clk_sel_restore(void)
{
	io_reg_write(PU_REGISTER_BASE + ACP_SW_48MHz_CLK_SEL, ACP_SW_CLK_SEL_49152KHZ);
	io_reg_write(PU_REGISTER_BASE + ACP_PDM_CORE_CLK_SEL, 0);
	io_reg_write(PU_REGISTER_BASE + ACP_AUDIO_CLK_SEL, 0);
}

/**
 * \brief Power the audio PLL and move AUDIOCLK and ACLK onto it.
 *
 * Called once from platform_init(). PDM and SoundWire run from these
 * clocks, so DMA start and stop must not run this sequence again while
 * any stream is active.
 *
 * \param clock_freq Requested ACLK frequency in Hz.
 */
void acp_clk_d0_sequence(uint32_t clock_freq)
{
	uint32_t retry;

	if (!mp1_mailbox_send(ACPSMC_MSG_PllPowerState, ACP_AUDIOPLL_POWER_ON_REQ)) {
		tr_err(&acp_clk_tr, "Audio PLL power on failed, staying on bypass clock");
		return;
	}

	/* Take AUDIOCLK (slice 0) and ACLK (slice 1) out of DFS bypass */
	for (retry = 0; retry < ACP_DFS_BYPASS_EXIT_RETRIES; retry++) {
		acp_clk_dfsbypass_request(ACP_DFS_BYPASS_EXIT_AUDIOCLK, CLK_POLL_COUNT / 2);
		acp_clk_dfsbypass_request(ACP_DFS_BYPASS_EXIT_ACLK, CLK_POLL_COUNT / 2);
	}

	/* Audio PLL DFS Output for AUDIOCLK */
	acp_reg_write_via_smn(ACP_CLK6_CLK0_BYPASS_CNTL, 0, sizeof(uint32_t));
	/* Audio PLL DFS Output for ACLK */
	acp_reg_write_via_smn(ACP_CLK6_CLK1_BYPASS_CNTL, 0, sizeof(uint32_t));
	/* Run ACP off the undivided ACLK enable acp_clkmux_sel(ACP_CLK_DIV_BY_1);*/
	/* Only now program the DFS divider: the slice is off bypass */
	change_clock_notify(clock_freq);
	/* S5_REFCLK back on the 24 MHz reference with the postdiv reset to enable it acp_s5_refclk_config(0, ACP_CLK_BYPASS_SEL_REFCLK_24MHZ, 0);*/
	/* restore io clk acp_io_clk_sel_restore();*/
}

void acp_clk_d3_sequence(void)
{
	clk6_bypass_cntl_u_t    bypass_cntl;

	bypass_cntl.u32All = 0;
	bypass_cntl.bitfields.CLK_BYPASS_SEL = 4;

	/* SET AUDIOCLK TO LPPLL 196.608MHz */
	acp_reg_write_via_smn(ACP_CLK6_CLK0_BYPASS_CNTL, bypass_cntl.u32All, sizeof(uint32_t));

	bypass_cntl.u32All = 0;
	bypass_cntl.bitfields.CLK_BYPASS_SEL = 2;

	/* SET ACLK TO LPPLL 393MHz */
	acp_reg_write_via_smn(ACP_CLK6_CLK1_BYPASS_CNTL, bypass_cntl.u32All, sizeof(uint32_t));

	/* Both slices must be in DFS bypass before the PLL goes down */
	acp_clk_dfsbypass_request(ACP_DFS_BYPASS_ENTER_AUDIOCLK, CLK_POLL_COUNT);
	acp_clk_dfsbypass_request(ACP_DFS_BYPASS_ENTER_ACLK, CLK_POLL_COUNT);

	/* Send message to PMFW to power off Audio PLL */
	mp1_mailbox_send(ACPSMC_MSG_PllPowerState, ACP_AUDIOPLL_POWER_OFF_REQ_WITH_WOV_EN);
}

void change_clock_notify(uint32_t clock_freq)
{
	uint32_t final_did = 4; /* Default DID value */
	float did = 0.0f;
	float fraction_val = 0.0f;
	uint32_t int_did_val = 0;
	uint32_t raw_did_x100;
	float boot_ref_clk = 0.0f;
	acp_clock_type_t clock_type = acp_aclk_clock;
	uint32_t err_before = io_reg_read(PU_REGISTER_BASE + ACP_ERROR_STATUS);

	acp_7_x_get_boot_ref_clock(&boot_ref_clk);

	tr_info(&acp_clk_tr, "acp_change_clock_notify clock_freq : %d clock_type : %d", clock_freq, clock_type);
	tr_info(&acp_clk_tr, "boot_ref_clk SMN: ACP_ERROR_STATUS 0x%x -> 0x%x smn_refused %u",
		err_before, io_reg_read(PU_REGISTER_BASE + ACP_ERROR_STATUS),
		(io_reg_read(PU_REGISTER_BASE + ACP_ERROR_STATUS) >> 20) & 1);

	fraction_val = (float)(clock_freq / (float)1000000.0f);   /*converting Hz to Mhz*/
	clock_freq   = (clock_freq / 1000000);                    /*converting Hz to Mhz*/

	did  = (float)(boot_ref_clk/(float)fraction_val);
	raw_did_x100 = (uint32_t)(did * 100.0f);
	tr_info(&acp_clk_tr, "aclk req %d MHz boot_ref_clk %d MHz raw_did %d/%dMHz",
		clock_freq, (uint32_t)boot_ref_clk, raw_did_x100, clock_freq);
	if (did > 62.0f) {
		final_did = 0x7F;
	} else {
		/* Fractional part of the divider value. Based on fractional
		 * value increment the did value.
		 */
		/* Refer DID value sheet for reference. */
		/* Extracting the fractional value from float divider value */
		fraction_val = did - (uint8_t)(did);

		/* Extracting only integer part of the divider */
		did = did - fraction_val;
		if (did <= 16.00f) {
			did = (did * 4.0f);
		} else if ((did > 16.0f) && (did <= 32.0f)) {
			did = ((did - 16.0f) * 2.0f + 64.0f);
		} else if ((did > 32.0f) && (did <= 62.0f)) {
			did = ((did - 32.0f) + 96.0f);
		}

		/* Following logic is to ensure the fractional divider value is
		 * only limited to 2 decimal places, in order to ensure correct
		 * calculation of DID value
		 */
		int_did_val = (uint32_t)(fraction_val * 100.0f);
		fraction_val = (float)(int_did_val / 100.0f);

		if (fraction_val == 0.0f) {
			final_did = (uint8_t)(did);
		} else if (fraction_val <= 0.25f) {
			final_did = (uint8_t)(did)+1;
		} else if ((fraction_val > 0.25f) && (fraction_val <= 0.5f)) {
			final_did = (uint8_t)(did)+2;
		} else if ((fraction_val > 0.5f) && (fraction_val <= 0.75f)) {
			final_did = (uint8_t)(did)+3;
		} else if ((fraction_val > 0.75f)) {
			final_did = (uint8_t)(did)+4;
		}
	}

	tr_info(&acp_clk_tr, "aclk final_did 0x%x", final_did);
	acp_clk_update_dfs_did((uint32_t)final_did, clock_type);

}
void acp_change_clock_notify(uint32_t clock_freq)
{
	if (clock_freq) {
		/* d0 sequence */
		acp_clk_d0_sequence(clock_freq);
	} else {
		/* d3 sequence */
		acp_clk_d3_sequence();
	}

}

uint32_t acp_clk_update_dfs_did(uint32_t did, uint32_t clk_type)
{
	uint32_t updated_clk = 0;

	if (clk_type == acp_aclk_clock) {
		acp_cpl_aclk_dfs_did_update(true, 10, did);
	} else if (clk_type == acp_sclk_clock) {
		acp_cpl_audioclk_dfs_did_update(true, 10, did);
	}
	wait_delay_us(ACP_CLK_MEASURE_SETTLE_US);
	acp_log_current_aclk("D0");
	acp_get_current_clk(acp_aclk_clock, (float *)&updated_clk);
	return updated_clk;

}
void acp_cpl_aclk_dfs_did_update(bool poll, uint32_t timeout_cnt, uint8_t did)
{ /* CLK instance #6, clock slice #1 */
	clk6_pll_dfs_cntl_u_t clk6_pll0_dfs1_cntl;

	clk6_pll0_dfs1_cntl.u32All = acp_reg_read_via_smn(CLK6_PLL0_DFS1_CNTL, sizeof(int));
	clk6_pll0_dfs1_cntl.bitfields.PLL0_DFS0_DIVIDER = did;
	acp_reg_write_via_smn(CLK6_PLL0_DFS1_CNTL, clk6_pll0_dfs1_cntl.u32All, sizeof(int));
	if (poll) {
		acp_cpl_aclk_poll_dfs_div_req_idle(10);
	}
}
void acp_cpl_audioclk_dfs_did_update(bool poll, uint32_t timeout_cnt, uint8_t did)
{ /* CLK instance #6, clock slice #0 */
	clk6_pll_dfs_cntl_u_t clk6_pll0_dfs0_cntl;

	clk6_pll0_dfs0_cntl.u32All = acp_reg_read_via_smn(CLK6_PLL0_DFS0_CNTL, sizeof(uint32_t));
	clk6_pll0_dfs0_cntl.bitfields.PLL0_DFS0_DIVIDER = did;
	acp_reg_write_via_smn(CLK6_PLL0_DFS0_CNTL, clk6_pll0_dfs0_cntl.u32All, sizeof(uint32_t));
	/* polling DID change done */
	if (poll) {
		acp_cpl_audioclk_poll_dfs_div_req_idle(timeout_cnt);
	}
}

void acp_cpl_aclk_poll_dfs_div_req_idle(uint32_t timeout_cnt)
{
	uint32_t timeout = 0;
	clk_dfs_status_u_t clk6_pll0_dfs1_status;

	do {
		clk6_pll0_dfs1_status.u32All = acp_reg_read_via_smn(
				CLK6_PLL0_DFS1_STATUS, sizeof(uint32_t));
		timeout++;
	} while ((clk6_pll0_dfs1_status.bitfields.PLL0_DFS0_DFS_DIV_REQ_IDLE == 0) &&
		 (timeout < timeout_cnt));
}

void acp_7_x_get_boot_ref_clock(float *_boot_ref_clk)
{
	volatile SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_NON_AI_CTRL0_BRDS_u_t non_ai_ctrl0;
	volatile SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_REFCLK_BRDS_u_t clk_pll_refclk;
	volatile SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL6_1_BRDS_u_t ai_freq_ctrl6_1_brds;
	volatile SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL7_BRDS_u_t ai_freq_ctrl7_brds;
	volatile SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL9_BRDS_u_t ai_freq_ctrl9_brds;

	non_ai_ctrl0.u32All = acp_reg_read_via_smn(
			SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_NON_AI_CTRL0_BRDS, sizeof(uint32_t));
	clk_pll_refclk.u32All = acp_reg_read_via_smn(
			SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_REFCLK_BRDS, sizeof(uint32_t));
	ai_freq_ctrl6_1_brds.u32All = acp_reg_read_via_smn(
			SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL6_1_BRDS, sizeof(uint32_t));
	ai_freq_ctrl7_brds.u32All = acp_reg_read_via_smn(
			SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL7_BRDS, sizeof(uint32_t));
	ai_freq_ctrl9_brds.u32All = acp_reg_read_via_smn(
			SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL9_BRDS, sizeof(uint32_t));

	*_boot_ref_clk = 0.0f;
	/* 16777216 = 2^24 */
	*_boot_ref_clk =
		(((float)(ai_freq_ctrl6_1_brds.bitfields.fcw_int) +
		  ((float)(ai_freq_ctrl7_brds.bitfields.fcw_frac << 8 |
			   ai_freq_ctrl9_brds.bitfields.fcw0_frac_lsb) /
			   (16777216.0f))) *
		 (clk_pll_refclk.bitfields.refclk_rate / 2) *
		 (1 << non_ai_ctrl0.bitfields.vco_pre_div));
}

int acp_get_current_clk(acp_clock_type_t clk_type, float *clk_value)
{
	uint32_t temp_clk_value = 0;
	clk_tick_cnt_config_reg_t clk_tick_config;

	clk_tick_config.u32All = acp_reg_read_via_smn(CLK_TICK_CNT_CONFIG_REG, sizeof(uint32_t));

	/* Get current output clock based on CLK TYPE */
	if (acp_sclk_clock == clk_type) {
		temp_clk_value = acp_reg_read_via_smn(CLK0_CURRENT_CNT, sizeof(uint32_t));
	} else if (acp_aclk_clock == clk_type) {
		temp_clk_value = acp_reg_read_via_smn(CLK1_CURRENT_CNT, sizeof(uint32_t));
	} else {
		return -EINVAL; /* Invalid parameter */
	}

	*clk_value = temp_clk_value / (clk_tick_config.bitfields.TIMER_THRESHOLD / 48.0f);
	return 0; /* Success */
}

/**
 * \brief Print one CLK6 slice the way agt_internal prints Current <clk>: MHz.
 *
 * Frequency is count * 48 MHz / TIMER_THRESHOLD, shown to 0.01 MHz.
 *
 * \param state Clock transition that initiated the measurement.
 * \param name ACLK or AUDIOCLK.
 * \param clk_type CLK6 slice to read.
 */
static void acp_log_agt_clk(const char *state, const char *name, acp_clock_type_t clk_type)
{
	clk_tick_cnt_config_reg_t clk_tick_config;
	uint32_t clk_mhz_x100;
	uint32_t threshold;
	int count;
	uint32_t cnt_reg;

	cnt_reg = (clk_type == acp_sclk_clock) ? CLK0_CURRENT_CNT : CLK1_CURRENT_CNT;
	clk_tick_config.u32All = acp_reg_read_via_smn(CLK_TICK_CNT_CONFIG_REG, sizeof(uint32_t));
	count = acp_reg_read_via_smn(cnt_reg, sizeof(uint32_t));
	threshold = clk_tick_config.bitfields.TIMER_THRESHOLD;

	if (count < 0 || !threshold) {
		tr_warn(&acp_clk_tr, "%s Current %s: measurement unavailable count %d threshold %u",
			state, name, count, threshold);
		return;
	}

	/* AGT: MHz = CURRENT_CNT * 48 MHz / TIMER_THRESHOLD, printed to 0.01 MHz. */
	clk_mhz_x100 = (uint32_t)(((uint64_t)count * ACP_CLK_MEASURE_REFCLK_MHZ * 100U +
				   (threshold / 2U)) / threshold);
	tr_info(&acp_clk_tr, "%s Current %s: %u.%02u MHz (count %u threshold %u)",
		state, name, clk_mhz_x100 / 100U, clk_mhz_x100 % 100U,
		(uint32_t)count, threshold);
}

/**
 * \brief Log ACLK and AUDIOCLK with the agt_internal tick-counter measurement.
 *
 * \param state Clock transition that initiated the measurement.
 */
static void acp_log_current_aclk(const char *state)
{
	acp_clk_tick_cnt_enable();
	wait_delay_us(ACP_CLK_MEASURE_SETTLE_US);
	acp_log_agt_clk(state, "ACLK", acp_aclk_clock);
	acp_log_agt_clk(state, "AUDIOCLK", acp_sclk_clock);
}
