// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 AMD.All rights reserved.
//
// Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		Maruthi Machani <maruthi.machani@amd.com>
//		SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
//

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
#include <platform/chip_registers.h>

/*b414df09-9e31-4c59-8657-7afc8deba70c*/
SOF_DEFINE_UUID("acp_clk", acp_clk_uuid, 0xb414df09, 0x9e31, 0x4c59,
		 0x86, 0x57, 0x7a, 0xfc, 0x8d, 0xeb, 0xa7, 0x0c);
DECLARE_TR_CTX(acp_clk_tr, SOF_UUID(acp_clk_uuid), LOG_LEVEL_INFO);

const struct freq_table platform_cpu_freq[] = {
	{600000000, 600000 },
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ,
	      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

void audio_pll_power_off(void);
void audio_pll_power_on(void);
void clk_dfs_bypassexit(void);
void audio_pll_mode_switch(uint32_t mode, uint32_t fcw_int, uint32_t fcw_frac, uint32_t fcw_denom,
			   uint32_t pllspinediv);
void clk5_init_vco(void);
void acp_6_3_reg_wait(void);
void acp_6_3_get_boot_ref_clock(float *boot_ref_clk);

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

static int acp_reg_read_via_smn(uint32_t reg_offset,
				uint32_t size)
{
	uint32_t reg_value;
	uint32_t delay_cnt = 10000;
	uint32_t smn_client_base_addr = (reg_offset >> 10);
	uint32_t region_start_addr = (smn_client_base_addr << 10);
	uint32_t apertureid = ((reg_offset >> 20) & 0xFFF);
	acp_srbm_cycle_sts_t  acp_srbm_cycle_sts;

	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_CONFIG), apertureid);
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_BASE_ADDR), smn_client_base_addr);
	reg_value = (uint32_t)io_reg_read(PU_REGISTER_BASE +
	(ACP_MASTER_REG_ACCESS_ADDRESS + reg_offset - region_start_addr + ACP_FIRST_REG_OFFSET));
	if (reg_value)
		reg_value = 0;
	acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
							       ACP_SRBM_CYCLE_STS);
	while (delay_cnt > 0) {
		if (!acp_srbm_cycle_sts.bits.srbm_clients_sts)
			return (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_SRBM_CLIENT_RDDATA);
		acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
									ACP_SRBM_CYCLE_STS);
		delay_cnt--;
	}
	return -1;
}

static void acp_reg_write_via_smn(uint32_t reg_offset,
				  uint32_t value, uint32_t size)
{
	uint32_t	delay_cnt = 10000;
	uint32_t        smn_client_base_addr    = (reg_offset >> 10);
	uint32_t        region_start_addr = (smn_client_base_addr << 10);
	uint32_t	apertureid = ((reg_offset >> 20) & 0xFFF);
	acp_srbm_cycle_sts_t      acp_srbm_cycle_sts;

	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_CONFIG), apertureid);
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_BASE_ADDR), smn_client_base_addr);
	io_reg_write((PU_REGISTER_BASE +
		(ACP_MASTER_REG_ACCESS_ADDRESS + reg_offset - region_start_addr +
		 ACP_FIRST_REG_OFFSET)),
		value);
	acp_srbm_cycle_sts =
		(acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE + ACP_SRBM_CYCLE_STS);
	while (delay_cnt > 0) {
		acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
								ACP_SRBM_CYCLE_STS);
		if (!acp_srbm_cycle_sts.bits.srbm_clients_sts)
			return;
		delay_cnt--;
	}
}

void acp_6_3_reg_wait(void)
{
	int test_count = 0;
	int val = 0;

	for (test_count = 0; test_count < 255; test_count++) {
		val = acp_reg_read_via_smn(CLK5_CLK_FSM_STATUS, sizeof(int));
		val = val * 0;
	}
}

void acp_6_3_get_boot_ref_clock(float *boot_ref_clk)
{
	clk5_clk_pll_refclk_startup_t boot_ref_clk_startup;
	clk5_clk_pll_req_u_t clk5_clk_pll_req;
	clk5_spll_field_2_t clk5_spll_field;
	uint32_t spinediv      = 1;
	float fract_part    = 0.0f;
	float final_refclk  = 0.0f;

	boot_ref_clk_startup.u32all =
		acp_reg_read_via_smn(CLK5_CLK_PLL_REFCLK_RATE_STARTUP, sizeof(int));

	clk5_spll_field_9_t  clk_spll_field_9;

	clk_spll_field_9.u32all = 0x0;
	clk_spll_field_9.u32all = acp_reg_read_via_smn(CLK5_SPLL_FIELD_9, sizeof(int));

	if (clk_spll_field_9.bitfields.spll_dpll_cfg_3 == 0x2)
		final_refclk = ((32768.0f * 128.0f) / 1000000.0f);
	else
		final_refclk = (float)boot_ref_clk_startup.bitfields.main_pll_ref_clk_rate_startup;

	clk5_clk_pll_req.u32all = acp_reg_read_via_smn(CLK5_CLK_PLL_REQ, sizeof(int));
	clk5_spll_field.u32all = acp_reg_read_via_smn(CLK5_SPLL_FIELD_2, sizeof(int));

	spinediv = (1 << clk5_clk_pll_req.bitfields.pllspinediv);

	if (clk5_spll_field.bitfields.spll_fracn_en == 1)
		fract_part = (float)(clk5_clk_pll_req.bitfields.fbmult_frac / (float)65536.0f);

	*boot_ref_clk = (float)(((final_refclk) * (clk5_clk_pll_req.bitfields.fbmult_int +
					fract_part)) / (float)spinediv);
}

void acp_change_clock_notify(uint32_t clock_freq)
{
	volatile clk5_clk1_dfs_cntl_u_t dfs_cntl;
	volatile clk5_clk1_bypass_cntl_u_t bypass_cntl;
	volatile clk5_clk1_dfs_status_u_t dfs_status;
	volatile uint32_t updated_clk;
	float did = 0.0f;
	float fraction_val = 0.0f;
	uint32_t int_did_val = 0;
	float boot_ref_clk;
	acp_clock_type_t clock_type = acp_aclk_clock;

	acp_6_3_get_boot_ref_clock(&boot_ref_clk);

	tr_info(&acp_clk_tr, "acp_change_clock_notify clock_freq : %d clock_type : %d",
		clock_freq, clock_type);

	fraction_val = (float)(clock_freq / (float)1000000.0f);
	clock_freq   = (clock_freq / 1000000);
	if (acp_aclk_clock == clock_type) {
		bypass_cntl.u32all = acp_reg_read_via_smn(CLK5_CLK1_BYPASS_CNTL, sizeof(int));
		dfs_cntl.u32all = acp_reg_read_via_smn(CLK5_CLK1_DFS_CNTL, sizeof(int));
	} else if (acp_sclk_clock == clock_type) {
		bypass_cntl.u32all = acp_reg_read_via_smn(CLK5_CLK0_BYPASS_CNTL, sizeof(int));
		dfs_cntl.u32all = acp_reg_read_via_smn(CLK5_CLK0_DFS_CNTL, sizeof(int));
	}

	bypass_cntl.bitfields.CLK1_BYPASS_DIV = 0;

	if (clock_freq == 6 || clock_freq == 0) {
		did = 128;
		dfs_cntl.bitfields.CLK1_DIVIDER = 0x7F;
		bypass_cntl.bitfields.CLK1_BYPASS_DIV = 0xF;
	} else {
		did  = (float)(boot_ref_clk / (float)fraction_val);
		tr_info(&acp_clk_tr, "acp_change_clock_notify CLK Divider : %d boot_ref_clk : %d\n",
			(uint32_t)(did * 100), (uint32_t)boot_ref_clk);

		if (did > 62.0f) {
			dfs_cntl.bitfields.CLK1_DIVIDER = 0x7F;
		} else {
			fraction_val = did - (uint8_t)(did);
			did = did - fraction_val;
			if (did <= 16.00f)
				did = (did * 4.0f);
			else if ((did > 16.0f) && (did <= 32.0f))
				did = ((did - 16.0f) * 2.0f + 64.0f);
			else if ((did > 32.0f) && (did <= 62.0f))
				did = ((did - 32.0f) + 96.0f);

			int_did_val = (uint32_t)(fraction_val * 100.0f);
			fraction_val = (float)(int_did_val / 100.0f);

			if (fraction_val == 0.0f)
				dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did);
			else if (fraction_val <= 0.25f)
				dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did) + 1;
			else if ((fraction_val > 0.25f) && (fraction_val <= 0.5f))
				dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did) + 2;
			else if ((fraction_val > 0.5f) && (fraction_val <= 0.75f))
				dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did) + 3;
			else if ((fraction_val > 0.75f))
				dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did) + 4;
		}
	}

	if (acp_aclk_clock == clock_type) {
		acp_reg_write_via_smn(CLK5_CLK1_BYPASS_CNTL, bypass_cntl.u32all, sizeof(int));
		acp_reg_write_via_smn(CLK5_CLK1_DFS_CNTL, dfs_cntl.u32all, sizeof(int));
		dfs_status.u32all = acp_reg_read_via_smn(CLK5_CLK1_DFS_STATUS, sizeof(int));
		acp_6_3_reg_wait();

		do {
			dfs_status.u32all = acp_reg_read_via_smn(CLK5_CLK1_DFS_STATUS, sizeof(int));
			tr_info(&acp_clk_tr, "acp_change_clock_notify ACLK1 CLK1_DIVIDER : %d dfsstatus %d ",
				dfs_cntl.u32all, dfs_status.u32all);
		} while (dfs_status.bitfields.CLK1_DFS_DIV_REQ_IDLE == 0);
		updated_clk = acp_reg_read_via_smn(CLK5_CLK1_CURRENT_CNT, sizeof(int));
		acp_6_3_reg_wait();

		if (updated_clk < (clock_freq * 10)) {
			dfs_cntl.bitfields.CLK1_DIVIDER -= 1;
			dfs_status.u32all               = 0;
			acp_reg_write_via_smn(CLK5_CLK1_DFS_CNTL, dfs_cntl.u32all, sizeof(int));
			do {
				dfs_status.u32all =
					acp_reg_read_via_smn(CLK5_CLK1_DFS_STATUS,
							     sizeof(int));
				dfs_cntl.u32all =
					acp_reg_read_via_smn(CLK5_CLK1_DFS_CNTL,
							     sizeof(int));
				tr_info(&acp_clk_tr, "acp_change_clock_notify ACLK2 CLK1_DIVIDER:%d dfsstatus %d ",
					dfs_cntl.u32all, dfs_status.u32all);
			} while (dfs_status.bitfields.CLK1_DFS_DIV_REQ_IDLE == 0);
		}
		updated_clk = acp_reg_read_via_smn(CLK5_CLK1_CURRENT_CNT, sizeof(int));
	} else if (acp_sclk_clock == clock_type) {
		acp_reg_write_via_smn(CLK5_CLK0_BYPASS_CNTL, bypass_cntl.u32all, sizeof(int));
		acp_reg_write_via_smn(CLK5_CLK0_DFS_CNTL, dfs_cntl.u32all, sizeof(int));
		dfs_status.u32all = acp_reg_read_via_smn(CLK5_CLK0_DFS_STATUS, sizeof(int));
		acp_6_3_reg_wait();

		do {
			dfs_status.u32all = acp_reg_read_via_smn(CLK5_CLK0_DFS_STATUS, sizeof(int));
			tr_info(&acp_clk_tr, "acp_change_clock_notify SCLK CLK1_DIVIDER: %d",
				dfs_cntl.u32all);
		} while (dfs_status.bitfields.CLK1_DFS_DIV_REQ_IDLE == 0);

		updated_clk = acp_reg_read_via_smn(CLK5_CLK0_CURRENT_CNT, sizeof(int));
	}
	tr_info(&acp_clk_tr,
		"clock_notify:CLK1_DIVIDER :%x boot_ref_clk : %d ClkReq : %d FinalClockValue: %d",
		dfs_cntl.u32all, (uint32_t)boot_ref_clk,
		clock_freq, updated_clk);
}

void audio_pll_power_off(void)
{
	volatile clk5_clk_pll_pwr_req_t clk5_clk_pll_pwr_req;
	volatile clk5_clk_fsm_status_t  clk5_clk_fsm_status;
	int count = 0;

	for (count = 0; count < 10; count++) {
		clk5_clk_pll_pwr_req.u32all =
			acp_reg_read_via_smn(CLK5_CLK_PLL_PWR_REQ, sizeof(int));
		clk5_clk_pll_pwr_req.bitfields.PLL_AUTO_STOP_REQ = 1;

		acp_reg_write_via_smn(CLK5_CLK_PLL_PWR_REQ,
				      clk5_clk_pll_pwr_req.u32all, sizeof(int));
		acp_6_3_reg_wait();

		clk5_clk_fsm_status.u32all = acp_reg_read_via_smn(CLK5_CLK_FSM_STATUS, sizeof(int));

		if (clk5_clk_fsm_status.bitfields.RO_FSM_PLL_STATUS_STOPPED == 1)
			break;
	}
}

void audio_pll_power_on(void)
{
	volatile clk5_clk_pll_pwr_req_t clk5_clk_pll_pwr_req;
	volatile clk5_clk_fsm_status_t  clk5_clk_fsm_status;
	int count = 0;

	for (count = 0; count < 10; count++) {
		clk5_clk_pll_pwr_req.u32all =
			acp_reg_read_via_smn(CLK5_CLK_PLL_PWR_REQ, sizeof(int));
		clk5_clk_pll_pwr_req.bitfields.PLL_AUTO_START_REQ = 1;

		acp_reg_write_via_smn(CLK5_CLK_PLL_PWR_REQ,
				      clk5_clk_pll_pwr_req.u32all,
				      sizeof(int));
		acp_6_3_reg_wait();

		clk5_clk_fsm_status.u32all = acp_reg_read_via_smn(CLK5_CLK_FSM_STATUS, sizeof(int));
		if (clk5_clk_fsm_status.bitfields.RO_FSM_PLL_STATUS_STARTED == 1)
			break;
		acp_6_3_reg_wait();
	}
}

void clk_dfs_bypassexit(void)
{
	volatile clk5_clk_dfsbypass_cntl_t clk5_clk_dfsbypass_cntl;

	clk5_clk_dfsbypass_cntl = (clk5_clk_dfsbypass_cntl_t)acp_reg_read_via_smn
				(CLK5_CLK_DFSBYPASS_CONTROL, sizeof(int));

	clk5_clk_dfsbypass_cntl.bitfields.exit_dfs_bypass_0 = 1;
	clk5_clk_dfsbypass_cntl.bitfields.exit_dfs_bypass_1 = 1;

	acp_reg_write_via_smn(CLK5_CLK_DFSBYPASS_CONTROL,
			      clk5_clk_dfsbypass_cntl.u32all,
			      sizeof(int));
}

void audio_pll_mode_switch(uint32_t mode, uint32_t fcw_int, uint32_t fcw_frac, uint32_t fcw_denom,
			   uint32_t pllspinediv)
{
	volatile clk5_spll_fuse_1_t  clk_spll_fuse1;
	volatile clk5_spll_fuse_2_t  clk_spll_fuse2;
	volatile clk5_spll_field_9_t  clk_spll_field_9;
	volatile clk5_spll_field_6nm_t  clk_spll_field_6nm;
	volatile clk5_spll_field_7_t  clk_spll_field_7;
	volatile clk5_spll_field_4_t  clk_spll_field_4;
	volatile clk5_spll_field_5nm_bus_ctrl_t  clk_spll_field_5nm_bus_ctrl;
	volatile clk5_spll_field_5nm_bus_wdata_t  clk_spll_field_5nm_bus_wdata;
	volatile clk5_spll_field_5nm_bus_status_t  clk_spll_field_5nm_bus_status;
	volatile clk5_rootrefclk_mux_1_t clk_rootrefclkmux;
	volatile clk5_spll_field_2_t  clk5_spll_field_2;

	clk_spll_fuse1.u32all = 0x0;
	clk_spll_fuse2.u32all = 0x0;
	clk_spll_field_9.u32all = 0x0;
	clk_spll_field_6nm.u32all = 0x0;
	clk_spll_field_7.u32all = 0x0;
	clk_spll_field_4.u32all = 0x0;
	clk_spll_field_5nm_bus_ctrl.u32all = 0x0;
	clk_spll_field_5nm_bus_wdata.u32all = 0x0;
	clk_spll_field_5nm_bus_status.u32all = 0x0;
	clk5_spll_field_2 =
		(clk5_spll_field_2_t)acp_reg_read_via_smn(CLK5_SPLL_FIELD_2, sizeof(int));

	if (clk5_spll_field_2.bitfields.spll_fracn_en == 0)
		clk5_spll_field_2.bitfields.spll_fracn_en = 1;
	acp_reg_write_via_smn(CLK5_SPLL_FIELD_2, clk5_spll_field_2.u32all, sizeof(int));

	switch (mode) {
	case PLL_MODE_32KHZ_LPPM:
		clk_rootrefclkmux.bitfields.ROOTREFCLK_MUX_1 = 0x0;
		clk_rootrefclkmux.u32all = acp_reg_read_via_smn(CLK5_ROOTREFCLKMUX_1, sizeof(int));

		clk_rootrefclkmux.bitfields.ROOTREFCLK_MUX_1    = 1;
		acp_reg_write_via_smn(CLK5_ROOTREFCLKMUX_1, clk_rootrefclkmux.u32all, sizeof(int));

		clk_spll_fuse1.bitfields.spll_gp_coarse_exp     = 0x5;
		clk_spll_fuse1.bitfields.spll_gp_coarse_mant    = 0x0;
		clk_spll_fuse1.bitfields.spll_gi_coarse_exp     = 0x7;
		clk_spll_fuse1.bitfields.spll_gi_coarse_mant    = 0x0;

		clk_spll_fuse2.bitfields.spll_tdc_resolution    = 0xe8;
		clk_spll_fuse2.bitfields.spll_freq_offset_exp   = 0xa;
		clk_spll_fuse2.bitfields.spll_freq_offset_mant  = 0xe;

		clk_spll_field_9.bitfields.spll_dpll_cfg_3      = 2;
		clk_spll_field_6nm.bitfields.spll_dpll_cfg_4    = 0x60;
		clk_spll_field_6nm.bitfields.spll_vco_pre_div   = 3;
		clk_spll_field_7.bitfields.spll_refclk_rate     = 4;
		clk_spll_field_7.bitfields.spll_pwr_state       = 1;
		clk_spll_field_4.bitfields.spll_refclk_div      = 0;

		acp_reg_write_via_smn(CLK5_SPLL_FUSE_1, clk_spll_fuse1.u32all, sizeof(int));
		acp_reg_write_via_smn(CLK5_SPLL_FUSE_2, clk_spll_fuse2.u32all, sizeof(int));
		acp_reg_write_via_smn(CLK5_SPLL_FIELD_9, clk_spll_field_9.u32all, sizeof(int));
		acp_reg_write_via_smn(CLK5_SPLL_FIELD_6nm, clk_spll_field_6nm.u32all, sizeof(int));
		acp_reg_write_via_smn(CLK5_SPLL_FIELD_7, clk_spll_field_7.u32all, sizeof(int));
		acp_reg_write_via_smn(CLK5_SPLL_FIELD_4, clk_spll_field_4.u32all, sizeof(int));

		clk_spll_field_5nm_bus_wdata.bitfields.bus_spll_wr_data = 0x00400000;
		acp_reg_write_via_smn(CLK5_SPLL_FIELD_5nm_BUS_WDATA,
				      clk_spll_field_5nm_bus_wdata.u32all,
				      sizeof(int));

		clk_spll_field_5nm_bus_ctrl.u32all =
			acp_reg_read_via_smn(CLK5_SPLL_FIELD_5nm_BUS_CTRL,
					     sizeof(int));
		clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_async_mode   = 1;
		clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_apb_mode     = 0;
		clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_addr         = 0xa;
		clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_byte_en      = 0xf;
		clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_rdtr         =
					!clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_rdtr;
		clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_resetb       = 1;
		clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_sel          = 1;
		clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_wrtr         = 1;
		acp_reg_write_via_smn(CLK5_SPLL_FIELD_5nm_BUS_CTRL,
				      clk_spll_field_5nm_bus_ctrl.u32all,
				      sizeof(int));
		do {
			clk_spll_field_5nm_bus_status.u32all =
				acp_reg_read_via_smn(CLK5_SPLL_FIELD_5nm_BUS_STATUS,
						     sizeof(int));
		} while (clk_spll_field_5nm_bus_status.bitfields.spll_bus_rd_valid !=
			 clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_rdtr);

		acp_reg_write_via_smn(CLK5_CLK_PLL_RESET_STOP_TIMER, 0xbbb11aa, sizeof(int));
		break;
	default:
		tr_err(&acp_clk_tr, "ERROR: Invalid PLL Mode");
		return;
	}

	clk5_clk_pll_req_u_t  clk5_clk_pll_req;

	clk5_clk_pll_req.u32all = 0;
	clk5_clk_pll_req =
		(clk5_clk_pll_req_u_t)acp_reg_read_via_smn(CLK5_CLK_PLL_REQ, sizeof(int));
	clk5_clk_pll_req.bitfields.fbmult_int   = fcw_int;

	if (clk5_spll_field_2.bitfields.spll_fracn_en)
		clk5_clk_pll_req.bitfields.fbmult_frac  = fcw_frac;
	clk5_clk_pll_req.bitfields.pllspinediv  = pllspinediv;
	acp_reg_write_via_smn(CLK5_CLK_PLL_REQ, clk5_clk_pll_req.u32all, sizeof(int));
}

void clk5_init_vco(void)
{
	audio_pll_power_off();
	audio_pll_mode_switch(PLL_MODE_32KHZ_LPPM, 0x125, 0, 0, 0);

	audio_pll_power_on();

	clk_dfs_bypassexit();
	acp_reg_write_via_smn(CLK5_CLK1_BYPASS_CNTL, 0, sizeof(int));
}

void platform_clock_init(struct sof *sof)
{
	int i = 0;

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
	clk5_init_vco();
}
