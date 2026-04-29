// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 AMD. All rights reserved.
 *
 * Author: Sneha Voona <sneha.voona@amd.com>
 *         Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
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

/* Static variables */
static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

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
				mmCLK6_PLL0_DFS0_STATUS, sizeof(uint32_t));
		timeout++;
	} while ((clk_dfs_status.bitfields.PLL0_DFS0_DFS_DIV_REQ_IDLE == 0) &
		 (timeout < timeout_cnt));

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
	/* Optionally, read back the argument register if needed (not used here) */

	return resp;
}

void acp_clk_d0_sequence(uint32_t clock_freq)
{
	/* Send message to PMFW to power on Audio PLL */
	mp1_mailbox_send(ACPSMC_MSG_PllPowerState, ACP_AUDIOPLL_POWER_ON_REQ);

	change_clock_notify(clock_freq);

	/* Audio PLL DFS Output for AUDIOCLK */
	acp_reg_write_via_smn(mmCLK6_CLK0_BYPASS_CNTL, 0, sizeof(uint32_t));
	/* Audio PLL DFS Output for ACLK */
	acp_reg_write_via_smn(mmCLK6_CLK1_BYPASS_CNTL, 0, sizeof(uint32_t));

}

void acp_clk_d3_sequence(void)
{
	clk6_bypass_cntl_u_t    bypass_cntl;

	bypass_cntl.u32All = 0;
	bypass_cntl.bitfields.CLK_BYPASS_SEL = 4;

	/* SET AUDIOCLK TO LPPLL 196.608MHz */
	acp_reg_write_via_smn(mmCLK6_CLK0_BYPASS_CNTL, bypass_cntl.u32All, sizeof(uint32_t));

	bypass_cntl.u32All = 0;
	bypass_cntl.bitfields.CLK_BYPASS_SEL = 2;

	/* SET ACLK TO LPPLL 393MHz */
	acp_reg_write_via_smn(mmCLK6_CLK1_BYPASS_CNTL, bypass_cntl.u32All, sizeof(uint32_t));
	/* mp1_mailbox_send(ACPSMC_MSG_PllPowerState, ACP_AUDIOPLL_POWER_OFF_REQ); */

	/* Send message to PMFW to power off Audio PLL */
	mp1_mailbox_send(ACPSMC_MSG_PllPowerState, ACP_AUDIOPLL_POWER_OFF_REQ_WITH_WOV_EN);
}

void change_clock_notify(uint32_t clock_freq)
{
	uint32_t final_did = 4; /* Default DID value */
	float did = 0.0f;
	float fraction_val = 0.0f;
	uint32_t int_did_val = 0;
	float boot_ref_clk = 0.0f;
	acp_clock_type_t clock_type = acp_aclk_clock;

	acp_7_x_get_boot_ref_clock(&boot_ref_clk);

	tr_info(&acp_clk_tr, "acp_change_clock_notify clock_freq : %d clock_type : %d", clock_freq, clock_type);

	fraction_val = (float)(clock_freq / (float)1000000.0f);   /*converting Hz to Mhz*/
	clock_freq   = (clock_freq / 1000000);                    /*converting Hz to Mhz*/

	did  = (float)(boot_ref_clk/(float)fraction_val);
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
