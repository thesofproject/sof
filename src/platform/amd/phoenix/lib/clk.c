// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 AMD. All rights reserved.
//
// Author: Basavaraj Hiregoudar<basavaraj.hiregoudar@amd.com>

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

#include <platform/chip_registers.h>

/*b414df09-9e31-4c59-8657-7afc8deba70c*/
DECLARE_SOF_UUID("acp-clk", acp_clk_uuid, 0xb414df09, 0x9e31, 0x4c59,
		0x86, 0x57, 0x7a, 0xfc, 0x8d, 0xeb, 0xa7, 0x0c);
DECLARE_TR_CTX(acp_clk_tr, SOF_UUID(acp_clk_uuid), LOG_LEVEL_INFO);

const struct freq_table platform_cpu_freq[] = {
	{600000000, 600000 },
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
			      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

 void Audio_PLL_Power_Off(void);
void Audio_PLL_Power_ON(void);
void CLK_DfsBypassExit(void);
void audio_pll_mode_switch(uint32_t mode, uint32_t fcw_int, uint32_t fcw_frac, uint32_t fcw_denom, uint32_t pllspinediv);
void CLK5Init_VCO(void);
void delay1(void);
void AcpGetPhxGetBootRefClock(float *BootRefClck);

typedef enum  {
   PLL_MODE_100MHZ_NORMAL,
   PLL_MODE_48MHZ_NORMAL,
   PLL_MODE_32KHZ_LPPM,
   PLL_MODE_48MHZ_LPPM,
   PLL_MODE_100MHZ_LPPM
} PLL_MODE;

/** \enum  AcpClockType_t
    \brief Enumeration for the Clock Types */
typedef enum _AcpClockType_
{
    /** \e 0x00000000 specifies Aclk clock */
    AcpAclkClock,

    /** \e 0x00000001 specifies Sclk clock */
    AcpSclkClock,/*For the future use*/

    /** \e 0x00000002 Max */
    AcpClockType_Max,

    /** \e 0x0000FF Force */
    AcpClockType_Force = 0xFF

}AcpClockType_t;


static int acp_reg_read_via_smn(uint32_t reg_offset,
				uint32_t size)
{
	uint32_t reg_value;
	uint32_t delay_cnt = 10000;
	uint32_t smn_client_base_addr = (reg_offset >> 10);
	uint32_t region_start_addr = (smn_client_base_addr << 10);
	uint32_t apertureid = ((reg_offset >> 20) & 0xFFF);
	acp_srbm_cycle_sts_t  acp_srbm_cycle_sts;

	/* Configuring the MP1 Aperture Id in SRB_client_config register */
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_CONFIG), apertureid);
	/* Configuring the base address of MP1 */
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_BASE_ADDR), smn_client_base_addr);
	/* dummy read to the to the address offset 0x3400 */
	/* Master config offset to access register outside of ACP */
	reg_value = (uint32_t)io_reg_read(PU_REGISTER_BASE +
	(ACP_MASTER_REG_ACCESS_ADDRESS + reg_offset - region_start_addr + ACP_FIRST_REG_OFFSET));
	if (reg_value)
		reg_value = 0;
	/* reading status register to check above read request is completed or not */
	acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
							       ACP_SRBM_CYCLE_STS);
	/* Waiting for status to be set to '0' */
	while (delay_cnt > 0) {
		if (!acp_srbm_cycle_sts.bits.srbm_clients_sts) {
			/* when status is '0' from above read the data from data register */
			return (uint32_t) io_reg_read(PU_REGISTER_BASE + ACP_SRBM_CLIENT_RDDATA);
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
	uint32_t	delay_cnt = 10000;
	uint32_t        smn_client_base_addr    = (reg_offset >> 10);
	uint32_t        region_start_addr = (smn_client_base_addr << 10);
	uint32_t	apertureid = ((reg_offset >> 20) & 0xFFF);
	acp_srbm_cycle_sts_t      acp_srbm_cycle_sts;

	/* Configuring the MP1 Aperture Id in SRB_client_config register */
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_CONFIG), apertureid);
	/* Configuring the base address of MP1 */
	io_reg_write((PU_REGISTER_BASE + ACP_SRBM_CLIENT_BASE_ADDR), smn_client_base_addr);
	/* write to the address offset 0x3400
	 *  Master config offset to access register outside of ACP
	 */
	io_reg_write((PU_REGISTER_BASE +
		(ACP_MASTER_REG_ACCESS_ADDRESS + reg_offset - region_start_addr + ACP_FIRST_REG_OFFSET)),
		value);
	/* reading status register to check above read request is completed or not */
	acp_srbm_cycle_sts =
		(acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE + ACP_SRBM_CYCLE_STS);
	/* Waiting for status to be set to '0' */
	while (delay_cnt > 0) {
		acp_srbm_cycle_sts = (acp_srbm_cycle_sts_t)io_reg_read(PU_REGISTER_BASE +
								ACP_SRBM_CYCLE_STS);
		if (!acp_srbm_cycle_sts.bits.srbm_clients_sts)
			return;
		delay_cnt--;
	}
}

#if 0
static void  get_response_from_smu(void)
{
	uint32_t ret_value;
	uint32_t count = 0;

	do {
		ret_value = acp_reg_read_via_smn(MP1_SMN_C2PMSG_93, sizeof(int));
		if (count > 0x007fffff)
			count = 0;
		count++;
	} while (ret_value == 0);
}
#endif

void delay1(void )
{
    // Delay
    int testCount = 0;
    int val = 0;
    for(testCount = 0; testCount < 255; testCount++){
        // This reg read is purely for simulating delay.
        val = acp_reg_read_via_smn(CLK5_CLK_FSM_STATUS, sizeof(int));
    }
}


/**
 * @brief AcpGetPhxGetBootRefClock
 *
 * @param *RefClock
 * @return None
 */
void AcpGetPhxGetBootRefClock(float *BootRefClck)
{
    clk5_clk_pll_refclk_startup_t boot_ref_clk_startup;
    clk5_clk_pll_req_u_t clk5_clk_pll_req;
    clk5_spll_field_2_t clk5_spll_field;
    uint32_t spinediv      = 1;
    float fract_part    = 0.0f;
    float final_refclk  = 0.0f;

    boot_ref_clk_startup.u32All = acp_reg_read_via_smn(CLK5_CLK_PLL_REFCLK_RATE_Startup, sizeof(int));
    
    clk5_spll_field_9_t  clk_spll_field_9;
    clk_spll_field_9.u32All = 0x0;
    clk_spll_field_9.u32All = acp_reg_read_via_smn(CLK5_SPLL_FIELD_9, sizeof(int));

    if(clk_spll_field_9.bitfields.spll_dpll_cfg_3 == 0x2){
        final_refclk = ((32768.0f * 128.0f)/1000000.0f); // 32KHz * vco_pre_div (2^7 = 128)
    } else {
        final_refclk = (float)boot_ref_clk_startup.bitfields.MainPllRefclkRateStartup;
    }

    clk5_clk_pll_req.u32All = acp_reg_read_via_smn(CLK5_CLK_PLL_REQ, sizeof(int));
    clk5_spll_field.u32All = acp_reg_read_via_smn(CLK5_SPLL_FIELD_2, sizeof(int));

    spinediv = (1 << clk5_clk_pll_req.bitfields.PllSpineDiv);
	
    if(clk5_spll_field.bitfields.spll_fracn_en == 1){
        fract_part = (float)(clk5_clk_pll_req.bitfields.FbMult_frac/(float)65536.0f);
    }

    *BootRefClck = (float)(((final_refclk) * (clk5_clk_pll_req.bitfields.FbMult_int + fract_part))/ (float)spinediv);
	//BootRefClck1 = *BootRefClck;
	//var1 = clk5_spll_field.u32All;
	//var2 = (uint32_t)final_refclk;
    //tr_info(&acp_clk_tr, "boot_ref_clk startup : %d Mhz VCO : %d Mhz fractbit : %d ox%x", var2, BootRefClck1, var1, clk_spll_field_9);
}

/*********************************************************************************\
* AcpChangeClockNotify_Phx()
*/
/**  \note      The \b AcpChangeCLock function details are provided in
*               aos.h file.
*
***********************************************************************************/

void acp_change_clock_notify(uint32_t clockFreq)
{
    //int                    evcStatus   = 0;
    volatile clk5_clk1_dfs_cntl_u_t          dfs_cntl;
    volatile clk5_clk1_bypass_cntl_u_t       bypass_cntl;
    volatile clk5_clk1_dfs_status_u_t        dfs_status;
    volatile uint32_t                       updatedClk;
    float                           did = 0.0f;
    float                           fractionVal = 0.0f;
    uint32_t                       intDIDVal = 0;
    float                       BootRefClck ;
    AcpClockType_t                  clockType = AcpAclkClock;

	//clockFreq = clockFreq/1000000; //clock freq to MHz

#ifdef FUNC_ENTRY
    funcEntry((uint32_t)AcpChangeClockNotify_Phx);
#endif

	io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0x40404040);
	AcpGetPhxGetBootRefClock(&BootRefClck);

    tr_info(&acp_clk_tr, "AcpChangeClockNotify_Phx clockFreq : %d clockType : %d", clockFreq, clockType);

    fractionVal = (float)(clockFreq / (float)1000000.0f);/*converting Hz to Mhz*/
    clockFreq   = (clockFreq / 1000000);/*converting Hz to Mhz*/
    if (AcpAclkClock == clockType)
    {
        bypass_cntl.u32All = acp_reg_read_via_smn(CLK5_CLK1_BYPASS_CNTL, sizeof(int));
        dfs_cntl.u32All = acp_reg_read_via_smn(CLK5_CLK1_DFS_CNTL, sizeof(int));
    }
    else if (AcpSclkClock == clockType){

        bypass_cntl.u32All = acp_reg_read_via_smn(CLK5_CLK0_BYPASS_CNTL, sizeof(int));
        dfs_cntl.u32All = acp_reg_read_via_smn(CLK5_CLK0_DFS_CNTL, sizeof(int));
    }

	io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0x50505050);
	
    // Restore by clearing Bypass Divider to 0 (means Divide by 1)
    bypass_cntl.bitfields.CLK1_BYPASS_DIV = 0;

    if((clockFreq == 6) || (clockFreq == 0))
    {
        did = 128;
        dfs_cntl.bitfields.CLK1_DIVIDER = 0x7F;
        bypass_cntl.bitfields.CLK1_BYPASS_DIV = 0xF; //Maximum Possible Value for lowest clock
    }
    else
    {
        did  = (float)(BootRefClck/(float)fractionVal);
        tr_info(&acp_clk_tr, "AcpChangeClockNotify_Phx CLK Divider : %d BootRefClck : %d \n", (uint32_t)(did*100), (uint32_t)BootRefClck);

        if(did > 62.0f)
        {
            dfs_cntl.bitfields.CLK1_DIVIDER = 0x7F;
        }
        else
        {
            // Fractional part of the divider value. Based on fractional value increment the did value.
            // Refer DID value sheet for reference.
            // Extracting the fractional value from float divider value
            fractionVal = did - (uint8_t)(did);
			
			io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0x60606060);

            // Extracting only integer part of the divider
            did = did - fractionVal;
            if (did <= 16.00f)
            {
                did = (did * 4.0f);
            }
            else if ((did > 16.0f) && (did <= 32.0f))
            {
                did = ((did - 16.0f) * 2.0f + 64.0f);
            }
            else if ((did > 32.0f) && (did <= 62.0f))
            {
                did = ((did - 32.0f) + 96.0f);
            }
            
            // Following logic is to ensure the fractional divider value is only limited to 2 decimal places, in order to ensure correct calculation of DID value
            intDIDVal = (uint32_t)(fractionVal * 100.0f);
            fractionVal = (float)(intDIDVal / 100.0f);

            if (fractionVal == 0.0f){
                dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did);
            }
            else if(fractionVal <= 0.25f){
                dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did)+1;
            }
            else if((fractionVal > 0.25f) && (fractionVal <= 0.5f )){
                dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did)+2;
            }
            else if((fractionVal > 0.5f) && (fractionVal <= 0.75f )){
                dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did)+3;
            }
            else if((fractionVal > 0.75f)){
                dfs_cntl.bitfields.CLK1_DIVIDER = (uint8_t)(did)+4;
            }
        }
    }

	io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0x70707070);
	
    if (AcpAclkClock == clockType)
    {
        acp_reg_write_via_smn(CLK5_CLK1_BYPASS_CNTL, bypass_cntl.u32All, sizeof(int));
        acp_reg_write_via_smn(CLK5_CLK1_DFS_CNTL, dfs_cntl.u32All, sizeof(int));
        dfs_status.u32All = acp_reg_read_via_smn(CLK5_CLK1_DFS_STATUS, sizeof(int));

		io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0x80808080);

        // Adding this delay between CLK operations to ensure smooth operation. Fix for SWDEV-373361
        delay1();

        do {
            dfs_status.u32All = acp_reg_read_via_smn(CLK5_CLK1_DFS_STATUS, sizeof(int));
            tr_info(&acp_clk_tr, "AcpChangeClockNotify_Phx ACLK1 CLK1_DIVIDER : %d dfsstatus %d ", dfs_cntl.u32All, dfs_status.u32All);
        } while (0 == dfs_status.bitfields.CLK1_DFS_DIV_REQ_IDLE);
        updatedClk = acp_reg_read_via_smn(CLK5_CLK1_CURRENT_CNT, sizeof(int));

		io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0x90909090);
		
        // Adding this delay between CLK operations to ensure smooth operation. Fix for SWDEV-373361
        delay1();

        if(updatedClk < (clockFreq*10))
        {
            dfs_cntl.bitfields.CLK1_DIVIDER-= 1;
            dfs_status.u32All               = 0;
            acp_reg_write_via_smn(CLK5_CLK1_DFS_CNTL, dfs_cntl.u32All, sizeof(int));
            do {
                dfs_status.u32All = acp_reg_read_via_smn(CLK5_CLK1_DFS_STATUS, sizeof(int));
                dfs_cntl.u32All = acp_reg_read_via_smn(CLK5_CLK1_DFS_CNTL, sizeof(int));
                tr_info(&acp_clk_tr, "AcpChangeClockNotify_Phx ACLK2 CLK1_DIVIDER : %d dfsstatus %d ", dfs_cntl.u32All, dfs_status.u32All);
            } while (0 == dfs_status.bitfields.CLK1_DFS_DIV_REQ_IDLE);
        }
        // Read the Final updated clock again.
        updatedClk = acp_reg_read_via_smn(CLK5_CLK1_CURRENT_CNT, sizeof(int));
    }
    else if (AcpSclkClock == clockType){
		io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0xA0A0A0A0);
        acp_reg_write_via_smn(CLK5_CLK0_BYPASS_CNTL, bypass_cntl.u32All, sizeof(int));
        acp_reg_write_via_smn(CLK5_CLK0_DFS_CNTL, dfs_cntl.u32All, sizeof(int));
        dfs_status.u32All = acp_reg_read_via_smn(CLK5_CLK0_DFS_STATUS, sizeof(int));

        // Adding this delay between CLK operations to ensure smooth operation. Fix for SWDEV-373361
        delay1();

        do {
            dfs_status.u32All = acp_reg_read_via_smn(CLK5_CLK0_DFS_STATUS, sizeof(int));
            tr_info(&acp_clk_tr, "AcpChangeClockNotify_Phx SCLK CLK1_DIVIDER : %d", dfs_cntl.u32All);
        } while (0 == dfs_status.bitfields.CLK1_DFS_DIV_REQ_IDLE);

        // Read the final updated clock again.
        updatedClk = acp_reg_read_via_smn(CLK5_CLK0_CURRENT_CNT, sizeof(int));
    }
    tr_info(&acp_clk_tr, "AcpChangeClockNotify_Phx:  CLK1_DIVIDER : %x boot_ref_clk : %d ClkReq : %d  FinalClockValue: %d", dfs_cntl.u32All, (uint32_t)BootRefClck, clockFreq, updatedClk);

	io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0xB0B0B0B0);
}

#if 0
void acp_change_clock_notify(uint32_t	clock_freq)
{
	/* converting Hz to Mhz */
	clock_freq = (clock_freq / 1000000);
	/* Writing response zero to the response register
	 * so that it cab reset to '0' for further response
	 */
	acp_reg_write_via_smn(MP1_SMN_C2PMSG_93,
				0, sizeof(int));
	/* Writing clock frequency value in Mhz in the Argument register */
	acp_reg_write_via_smn(MP1_SMN_C2PMSG_85,
				clock_freq, sizeof(int));
	/* Writing Message to message register */
	/* Writing Aclk change message to message register */
	acp_reg_write_via_smn(MP1_SMN_C2PMSG_69,
				ACP_SMU_MSG_SET_ACLK, sizeof(int));
	/* Waiting for SMU response */
	get_response_from_smu();
}
#endif


/**
 * @brief Audio_PLL_Power_Off
 * Power Off Audio PLL
 * @param None
 * @return None
 */

void Audio_PLL_Power_Off(void)
{
    volatile clk5_clk_pll_pwr_req_t clk5_clk_pll_pwr_req;
    volatile clk5_clk_fsm_status_t  clk5_clk_fsm_status;
    int count = 0;
    for(count = 0; count < 10; count++){
        clk5_clk_pll_pwr_req.u32All = acp_reg_read_via_smn(CLK5_CLK_PLL_PWR_REQ, sizeof(int));
        clk5_clk_pll_pwr_req.bitfields.PLL_AUTO_STOP_REQ = 1;

        acp_reg_write_via_smn(CLK5_CLK_PLL_PWR_REQ, clk5_clk_pll_pwr_req.u32All, sizeof(int));
        // Delay
        delay1();
        
        clk5_clk_fsm_status.u32All = acp_reg_read_via_smn(CLK5_CLK_FSM_STATUS, sizeof(int));
        
        if(clk5_clk_fsm_status.bitfields.RO_FSM_PLL_STATUS_STOPPED == 1){
            break;
        }
    }
}

/**
 * @brief Audio_PLL_Power_ON
 * Power On Audio PLL
 * @param None
 * @return None
 */

void Audio_PLL_Power_ON(void)
{
    volatile clk5_clk_pll_pwr_req_t clk5_clk_pll_pwr_req;
    volatile clk5_clk_fsm_status_t  clk5_clk_fsm_status;
    int count = 0;
	
    for(count = 0; count < 10; count++){

        clk5_clk_pll_pwr_req.u32All = acp_reg_read_via_smn(CLK5_CLK_PLL_PWR_REQ, sizeof(int));
        clk5_clk_pll_pwr_req.bitfields.PLL_AUTO_START_REQ = 1;

        acp_reg_write_via_smn(CLK5_CLK_PLL_PWR_REQ, clk5_clk_pll_pwr_req.u32All, sizeof(int));

        // Delay
        delay1();

        clk5_clk_fsm_status.u32All = acp_reg_read_via_smn(CLK5_CLK_FSM_STATUS, sizeof(int));
        if(clk5_clk_fsm_status.bitfields.RO_FSM_PLL_STATUS_STARTED == 1){
            break;
        }
        delay1();
    }
}

void CLK_DfsBypassExit(void)
{
    volatile clk5_clk_dfsbypass_cntl_t clk5_clk_dfsbypass_cntl;

    clk5_clk_dfsbypass_cntl = (clk5_clk_dfsbypass_cntl_t)acp_reg_read_via_smn(CLK5_CLK_DFSBYPASS_CONTROL, sizeof(int));

    /* Set DFS Bypass Bits for ACLK and SCLK*/
    clk5_clk_dfsbypass_cntl.bitfields.ExitDFSBypass_0 = 1;
    clk5_clk_dfsbypass_cntl.bitfields.ExitDFSBypass_1 = 1;

    acp_reg_write_via_smn(CLK5_CLK_DFSBYPASS_CONTROL, clk5_clk_dfsbypass_cntl.u32All, sizeof(int));
}

/**
 * @brief Audio PLL Mode Switch
 *          This function should only be called after PLL Power Off and must to PLL power ON + bypass exit afterwards for changes to take effect 
 * @param mode 
 * @param fcw_int 
 * @param fcw_frac 
 * @param fcw_denom 
 * @param pllspinediv 
 */

void audio_pll_mode_switch(uint32_t mode, uint32_t fcw_int, uint32_t fcw_frac, uint32_t fcw_denom, uint32_t pllspinediv)
{
//mode:
//0: 100MHz refclk normal
//1: 100MHz refclk LPPM
//2: 48MHz refclk normal
//3: 48MHz refclk LPPM
//4: 32KHz refclk LPPM
    volatile clk5_spll_fuse_1_t  clk_spll_fuse1;
    clk_spll_fuse1.u32All = 0x0;
    volatile clk5_spll_fuse_2_t  clk_spll_fuse2;
    clk_spll_fuse2.u32All = 0x0;
    volatile clk5_spll_field_9_t  clk_spll_field_9;
    clk_spll_field_9.u32All = 0x0;
    volatile clk5_spll_field_6nm_t  clk_spll_field_6nm;
    clk_spll_field_6nm.u32All = 0x0;
    volatile clk5_spll_field_7_t  clk_spll_field_7;
    clk_spll_field_7.u32All = 0x0;
    volatile clk5_spll_field_4_t  clk_spll_field_4;
    clk_spll_field_4.u32All = 0x0;

    //WA for vco_pre_div[2]
    volatile clk5_spll_field_5nm_bus_ctrl_t  clk_spll_field_5nm_bus_ctrl;
    clk_spll_field_5nm_bus_ctrl.u32All = 0x0;
    volatile clk5_spll_field_5nm_bus_wdata_t  clk_spll_field_5nm_bus_wdata;
    clk_spll_field_5nm_bus_wdata.u32All = 0x0;
    volatile clk5_spll_field_5nm_bus_status_t  clk_spll_field_5nm_bus_status;
    clk_spll_field_5nm_bus_status.u32All = 0x0;
    //WA for vco_pre_div[2]
    
    volatile clk5_rootrefclk_mux_1_t clk_rootrefclkmux;

    volatile clk5_spll_field_2_t  clk5_spll_field_2;
    clk5_spll_field_2 = (clk5_spll_field_2_t) acp_reg_read_via_smn(CLK5_SPLL_FIELD_2, sizeof(int));

    /* Check if fractional clock generate bit is set otherwise set it*/
    if(clk5_spll_field_2.bitfields.spll_fracn_en == 0){
        clk5_spll_field_2.bitfields.spll_fracn_en = 1;
    }
    acp_reg_write_via_smn(CLK5_SPLL_FIELD_2,clk5_spll_field_2.u32All, sizeof(int));

    switch (mode) {
        case PLL_MODE_32KHZ_LPPM:
            clk_rootrefclkmux.bitfields.ROOTREFCLK_MUX_1 = 0x0;
            clk_rootrefclkmux.u32All = acp_reg_read_via_smn(CLK5_ROOTREFCLKMUX_1, sizeof(int));

            clk_rootrefclkmux.bitfields.ROOTREFCLK_MUX_1    = 1;  // Change ROOT REFCLK MUX
            acp_reg_write_via_smn(CLK5_ROOTREFCLKMUX_1, clk_rootrefclkmux.u32All, sizeof(int));

            clk_spll_fuse1.bitfields.spll_gp_coarse_exp     = 0x5;
            clk_spll_fuse1.bitfields.spll_gp_coarse_mant    = 0x0;
            clk_spll_fuse1.bitfields.spll_gi_coarse_exp     = 0x7;
            clk_spll_fuse1.bitfields.spll_gi_coarse_mant    = 0x0;

            clk_spll_fuse2.bitfields.spll_tdc_resolution    = 0xe8;
            clk_spll_fuse2.bitfields.spll_freq_offset_exp   = 0xa;
            clk_spll_fuse2.bitfields.spll_freq_offset_mant  = 0xe;

            clk_spll_field_9.bitfields.spll_dpll_cfg_3      = 2; // Setting mode to LPPM means VCO range less than 1.6GHz
            clk_spll_field_6nm.bitfields.spll_dpll_cfg_4    = 0x60;
            // Actually value should be 7 here but due to limitation of vco_prediv (only 2 bits hence can store value only till 3), workaround is added.
            clk_spll_field_6nm.bitfields.spll_vco_pre_div   = 3;  
            clk_spll_field_7.bitfields.spll_refclk_rate     = 4; // means 4MHz or 4.194MHz = 32768 * 2^7 (7 is vco_pre_div)
            clk_spll_field_7.bitfields.spll_pwr_state       = 1;
            clk_spll_field_4.bitfields.spll_refclk_div      = 0;

            acp_reg_write_via_smn(CLK5_SPLL_FUSE_1    ,clk_spll_fuse1.u32All, sizeof(int));
            acp_reg_write_via_smn(CLK5_SPLL_FUSE_2    ,clk_spll_fuse2.u32All, sizeof(int));
            acp_reg_write_via_smn(CLK5_SPLL_FIELD_9   ,clk_spll_field_9.u32All, sizeof(int));
            acp_reg_write_via_smn(CLK5_SPLL_FIELD_6nm ,clk_spll_field_6nm.u32All, sizeof(int));
            acp_reg_write_via_smn(CLK5_SPLL_FIELD_7   ,clk_spll_field_7.u32All, sizeof(int));
            acp_reg_write_via_smn(CLK5_SPLL_FIELD_4   ,clk_spll_field_4.u32All, sizeof(int));

            //Workaround for vco_pre_div[2]
            clk_spll_field_5nm_bus_wdata.bitfields.bus_spll_wr_data = 0x00400000;
            acp_reg_write_via_smn(CLK5_SPLL_FIELD_5nm_BUS_WDATA, clk_spll_field_5nm_bus_wdata.u32All, sizeof(int));

            clk_spll_field_5nm_bus_ctrl.u32All = acp_reg_read_via_smn(CLK5_SPLL_FIELD_5nm_BUS_CTRL, sizeof(int));
            clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_async_mode   = 1;
            clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_apb_mode     = 0;
            clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_addr         = 0xa;
            clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_byte_en      = 0xf;
            clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_rdtr         = !clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_rdtr;
            clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_resetb       = 1;
            clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_sel          = 1;
            clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_wrtr         = 1;
            acp_reg_write_via_smn(CLK5_SPLL_FIELD_5nm_BUS_CTRL, clk_spll_field_5nm_bus_ctrl.u32All, sizeof(int));
            //polling the write status
            do {
                clk_spll_field_5nm_bus_status.u32All = acp_reg_read_via_smn(CLK5_SPLL_FIELD_5nm_BUS_STATUS, sizeof(int));
            } while (clk_spll_field_5nm_bus_status.bitfields.spll_bus_rd_valid != clk_spll_field_5nm_bus_ctrl.bitfields.bus_spll_rdtr);
            //workaround for vco_pre_div[2]

            // Setting Reset Stop timer for PLL. This is needed before we start PLL.
            acp_reg_write_via_smn(CLK5_CLK_Pll_RESET_STOP_TIMER, 0xbbb11aa, sizeof(int));

            break;

        /*
        case PLL_MODE_100MHZ_NORMAL:
            break;
        case PLL_MODE_48MHZ_NORMAL:
            break;
        case PLL_MODE_48MHZ_LPPM:
            break;
        case PLL_MODE_100MHZ_LPPM:
            break;
        */
        default:
            tr_err(&acp_clk_tr, "ERROR: Invalid PLL Mode");
            return;
    }

    clk5_clk_pll_req_u_t  clk5_clk_pll_req;
    clk5_clk_pll_req.u32All = 0;
    clk5_clk_pll_req = (clk5_clk_pll_req_u_t)acp_reg_read_via_smn(CLK5_CLK_PLL_REQ, sizeof(int));

    clk5_clk_pll_req.bitfields.FbMult_int   = fcw_int;

    if (clk5_spll_field_2.bitfields.spll_fracn_en){
        clk5_clk_pll_req.bitfields.FbMult_frac  = fcw_frac;
    }
    clk5_clk_pll_req.bitfields.PllSpineDiv  = pllspinediv;
    acp_reg_write_via_smn(CLK5_CLK_PLL_REQ, clk5_clk_pll_req.u32All, sizeof(int));
/**
    clk5_spll_field_10_t  clk_spll_field_10;
    clk_spll_field_10.u32All = 0;
    clk_spll_field_10.bitfields.spll_fcw_denom = fcw_denom;
    acp_reg_write_via_smn(CLK5_SPLL_FIELD_10, clk_spll_field_10.u32All, sizeof(int));
*/
}

/**
 * @brief CLK5Init_VCO
 *  Initialize VCO clock frequency
 * @param None
 * @return None
 */
 
void CLK5Init_VCO(void)
{
    /* Power off PLL */
    Audio_PLL_Power_Off();

    /**
     *  4.194MHz *  0x125 (fbmult) = VCO (1228.9MHz)   ----> This VCO is essential value within LPPM Mode range to achieve accurate I2S Clock.
     *  Following is the calcuation for I2S Clock.
     *       VCO / dfscntl(0x19 i.e. 6.25 divider) =  0x7AE = 196.608
     *  Hence the specific values are selected below while setting VCO multiplication factore
     */
    audio_pll_mode_switch(PLL_MODE_32KHZ_LPPM, 0x125, 0, 0, 0); //?TBD?//

    /* Power On PLL */
    Audio_PLL_Power_ON();

    /* DFS Bypass Exit for ACLK and SCLK */
    CLK_DfsBypassExit();
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
	io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0x10101010);
	CLK5Init_VCO();
	io_reg_write((PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_1), 0x30303030);
}
