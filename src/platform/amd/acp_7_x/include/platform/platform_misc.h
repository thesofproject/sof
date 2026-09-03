/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 AMD. All rights reserved.
 *
 * Author: Sneha Voona <sneha.voona@amd.com>
 */

#include <zephyr/drivers/dma.h>

#define PU_REGISTER_BASE               (0x9FD00000 - 0x01240000)
#define PU_SCRATCH_REG_BASE            (0x9FF00000 - 0x01250000)
#define ACP_FUTURE_REG_ACLK_0          0x12418E0 /* don't use - reserved for driver gsync */
#define ACP_FUTURE_REG_ACLK_1          0x12418E4
#define ACP_FUTURE_REG_ACLK_2          0x12418E8
#define ACP_FUTURE_REG_ACLK_3          0x12418EC
#define ACP_FUTURE_REG_ACLK_4          0x12418F0
#define ACP_SW_INTR_TRIG               0x1241890
#define ACP_DSP0_INTR_CNTL             0x1241800
#define ACP_DSP0_INTR_STAT             0x1241818
#define ACP_DSP_SW_INTR_CNTL           0x1241860
#define ACP_DSP_SW_INTR_STAT           0x1241864
#define ACP_AXI2DAGB_SEM_0             0x12418F4
#define ACP_SRBM_CLIENT_BASE_ADDR      0x1241BF0
#define ACP_SRBM_CLIENT_RDDATA         0x1241BF4
#define ACP_SRBM_CYCLE_STS             0x1241BF8
#define ACP_SRBM_CLIENT_CONFIG         0x1241BFC

/* Registers from SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6 block ACP_7X */
#define ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_REFCLK_BRDS            0x6D204
#define ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_NON_AI_CTRL0_BRDS      0x6D214
#define ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL6_1_BRDS   0x6D21C
#define ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL7_BRDS     0x6D220
#define ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL9_BRDS     0x6D20C

/* Registers from CLK6 block */
#define mmCLK6_PLL0_DFS0_CNTL                           0x6D014
#define mmCLK6_PLL0_DFS0_STATUS                         0x6D018
#define mmCLK6_PLL0_DFS1_CNTL                           0x6D01C
#define mmCLK6_PLL0_DFS1_STATUS                         0x6D020
#define mmCLK6_CLK0_BYPASS_CNTL                         0x6D06C
#define mmCLK6_CLK1_BYPASS_CNTL                         0x6D08C
#define mmCLK6_CLK_TICK_CNT_CONFIG_REG                  0x6D188
#define mmCLK6_CLK0_CURRENT_CNT                         0x6D190
#define mmCLK6_CLK1_CURRENT_CNT                         0x6D194

/* CLK6 register aliases without mm prefix for clock management code */
#define CLK6_PLL0_DFS0_CNTL                    mmCLK6_PLL0_DFS0_CNTL
#define CLK6_PLL0_DFS1_CNTL                    mmCLK6_PLL0_DFS1_CNTL
#define CLK6_PLL0_DFS1_STATUS                  mmCLK6_PLL0_DFS1_STATUS
#define CLK0_CURRENT_CNT                       mmCLK6_CLK0_CURRENT_CNT
#define CLK1_CURRENT_CNT                       mmCLK6_CLK1_CURRENT_CNT
#define CLK_TICK_CNT_CONFIG_REG                mmCLK6_CLK_TICK_CNT_CONFIG_REG

/* SYSTEMPLL2P0 register aliases without ACP_7X prefix */
#define SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_NON_AI_CTRL0_BRDS    ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_NON_AI_CTRL0_BRDS
#define SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_REFCLK_BRDS          ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_REFCLK_BRDS
#define SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL6_1_BRDS ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL6_1_BRDS
#define SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL7_BRDS   ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL7_BRDS
#define SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL9_BRDS   ACP_7X_SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL9_BRDS

#define mmMP1_C2PMSG_37                                 0x3B10994
#define mmMP1_C2PMSG_38                                 0x3B10998
#define mmMP1_C2PMSG_39                                 0x3B1099C
#define ACPSMC_MSG_PllPowerState           0x3 /* Toggle PLL power state ON/OFF */
#define ACP_AUDIOPLL_POWER_OFF_REQ          0x0 /* ACP Power Down Request Value */
#define ACP_AUDIOPLL_POWER_ON_REQ           0x1 /* ACP Power Up Request Value */
#define ACP_AUDIOPLL_POWER_OFF_REQ_WITH_WOV_EN          0x2 /* ACP Power Down Request Value */
#define ACP_AUDIOPLL_POWER_ON_REQ_WITH_WOV_EN           0x3 /* ACP Power Up Request Value */
#define ACP_AUDIO_CLK_SEL      0x1241038
#define ACP_PDM_CORE_CLK_SEL    0x124103C

typedef	union acp_srbm_cycle_sts {
	struct {
		unsigned int	srbm_clients_sts :1;
		unsigned int:7;
	} bits;
	unsigned int	u32all;
} acp_srbm_cycle_sts_t;

typedef union acp_clkmux_sel {
	struct {
		unsigned int acp_clkmux_sel:3;
		unsigned int:13;
		unsigned int acp_clkmux_div_value:16;
	} bits;
	unsigned int  u32all;
} acp_clkmux_sel_t;

typedef union clk7_clk1_dfs_cntl_u {
	struct {
		unsigned int CLK1_DIVIDER:7;
		unsigned int:25;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk1_dfs_cntl_u_t;
typedef union clk7_clk1_dfs_status_u {
	struct {
		unsigned int	: 16;
		unsigned int	CLK1_DFS_DIV_REQ_IDLE  :1;
		unsigned int	: 2;
		unsigned int	RO_CLK1_DFS_STATE_IDLE :1;
		unsigned int	CLK1_CURRENT_DFS_DID   :7;
		unsigned int	: 5;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk1_dfs_status_u_t;

typedef  union clk7_clk1_bypass_cntl_u {
	struct {
		unsigned int CLK1_BYPASS_SEL:3;
		unsigned int:13;
		unsigned int CLK1_BYPASS_DIV:4;
		unsigned int:12;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk1_bypass_cntl_u_t;

typedef union clk7_clk_fsm_status_u {
	struct {
		unsigned int AUTOLAUCH_FSM_FULL_SPEED_IDLE:1;
		unsigned int:3;
		unsigned int AUTOLAUCH_FSM_BYPASS_IDLE:1;
		unsigned int:3;
		unsigned int RO_FSM_PLL_STATUS_STARTED:1;
		unsigned int:3;
		unsigned int RO_FSM_PLL_STATUS_STOPPED:1;
		unsigned int:3;
		unsigned int RO_EARLY_FSM_DONE:1;
		unsigned int:3;
		unsigned int RO_DFS_GAP_ACTIVE:1;
		unsigned int:3;
		unsigned int RO_DID_FSM_IDLE:1;
		unsigned int:7;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk_fsm_status_t;

typedef union clk7_clk_pll_req_u {
	struct {
		unsigned int fbmult_int:9;
		unsigned int:3;
		unsigned int pllspinediv:4;
		unsigned int fbmult_frac:16;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk_pll_req_u_t;

typedef  union clk7_clk_pll_refclk_startup {
	struct {
		unsigned int  main_pll_ref_clk_rate_startup:8;
		unsigned int  main_pll_cfg_4_startup      :8;
		unsigned int  main_pll_ref_clk_div_startup:2;
		unsigned int  main_pll_cfg_3_startup      :10;
		unsigned int:1;
		unsigned int  main_pll_refclk_src_mux0_startup:1;
		unsigned int  main_pll_refclk_src_mux1_startup:1;
		unsigned int:1;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk_pll_refclk_startup_t;
typedef union clk7_spll_field_2 {
	struct{
		unsigned int:3;
		unsigned int spll_fbdiv_mask_en         :1;
		unsigned int spll_fracn_en              :1;
		unsigned int spll_freq_jump_en          :1;
		unsigned int:25;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_2_t;

typedef union clk7_clk_dfsbypass_cntl {
	struct {
		unsigned int enter_dfs_bypass_0           :1;
		unsigned int enter_dfs_bypass_1           :1;
		unsigned int:14;
		unsigned int exit_dfs_bypass_0            :1;
		unsigned int exit_dfs_bypass_1            :1;
		unsigned int:14;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_clk_dfsbypass_cntl_t;

typedef union clk7_clk_pll_pwr_req {
	struct {
		unsigned int PLL_AUTO_START_REQ         :1;
		unsigned int:3;
		unsigned int PLL_AUTO_STOP_REQ          :1;
		unsigned int:3;
		unsigned int PLL_AUTO_STOP_NOCLK_REQ    :1;
		unsigned int:3;
		unsigned int PLL_AUTO_STOP_REFBYPCLK_REQ:1;
		unsigned int:3;
		unsigned int PLL_FORCE_RESET_HIGH       :1;
		unsigned int:15;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk_pll_pwr_req_t;

typedef union clk7_spll_fuse_1 {
	struct {
		unsigned int:8;
		unsigned int spll_gp_coarse_exp         :4;
		unsigned int spll_gp_coarse_mant        :4;
		unsigned int:4;
		unsigned int spll_gi_coarse_exp         :4;
		unsigned int:1;
		unsigned int spll_gi_coarse_mant        :2;
		unsigned int:5;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_fuse_1_t;

typedef union clk7_spll_fuse_2 {
	struct {
		unsigned int spll_tdc_resolution       :8;
		unsigned int spll_freq_offset_exp      :4;
		unsigned int spll_freq_offset_mant     :5;
		unsigned int:15;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_fuse_2_t;

typedef union clk7_spll_field_9 {
	struct {
		unsigned int:16;
		unsigned int spll_dpll_cfg_3       :10;
		unsigned int spll_fll_mode         :1;
		unsigned int:5;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_9_t;

typedef union clk7_spll_field_6nm {
	struct {
		unsigned int spll_dpll_cfg_4       :8;
		unsigned int spll_reg_tim_exp      :3;
		unsigned int spll_reg_tim_mant     :1;
		unsigned int spll_ref_tim_exp      :3;
		unsigned int spll_ref_tim_mant     :1;
		unsigned int spll_vco_pre_div      :2;
		unsigned int:14;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_6nm_t;

typedef union clk7_spll_field_7 {
	struct {
		unsigned int:7;
		unsigned int spll_pllout_sel           :1;
		unsigned int spll_pllout_req           :1;
		unsigned int spll_pllout_state         :2;
		unsigned int spll_postdiv_ovrd         :4;
		unsigned int spll_postdiv_pllout_ovrd  :4;
		unsigned int spll_postdiv_sync_enable  :1;
		unsigned int:1;
		unsigned int spll_pwr_state            :2;
		unsigned int:1;
		unsigned int spll_refclk_rate          :8;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_7_t;

typedef union clk7_spll_field_4 {
	struct {
		unsigned int spll_fcw0_frac_ovrd   :16;
		unsigned int pll_out_sel             :1;
		unsigned int:3;
		unsigned int pll_pwr_dn_state         :2;
		unsigned int:2;
		unsigned int spll_refclk_div       :2;
		unsigned int:6;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_4_t;

typedef union clk7_spll_field_5nm_bus_ctrl {
	struct {
		unsigned int bus_spll_async_mode    :1;
		unsigned int bus_spll_apb_mode	    :1;
		unsigned int bus_spll_addr	        :8;
		unsigned int bus_spll_byte_en	    :4;
		unsigned int bus_spll_rdtr	        :1;
		unsigned int bus_spll_resetb	    :1;
		unsigned int bus_spll_sel	        :1;
		unsigned int bus_spll_wrtr	        :1;
		unsigned int:14;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_5nm_bus_ctrl_t;

typedef union clk7_spll_field_5nm_bus_wdata {
	struct {
		unsigned int bus_spll_wr_data;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_5nm_bus_wdata_t;

typedef union clk7_rootrefclk_mux_1 {
	struct {
		unsigned int ROOTREFCLK_MUX_1  :1;
		unsigned int reserved          :31;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_rootrefclk_mux_1_t;

typedef union clk7_spll_field_5nm_bus_status {
	struct {
		unsigned int spll_bus_error     :1;
		unsigned int spll_bus_rd_valid  :1;
		unsigned int spll_bus_wr_ack    :1;
		unsigned int:29;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_5nm_bus_status_t;

typedef union clk_tick_cnt_config_reg {
	struct {
	    uint32_t                 TIMER_THRESHOLD:24;
	    uint32_t                    TIMER_ENABLE:1;
	    uint32_t                  HISTORY_ENABLE:1;
	    uint32_t                HISTORY_SP_RESET:1;
	    uint32_t                 HISTORY_CLK_SEL:5;
	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} clk_tick_cnt_config_reg_t;
typedef union SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_NON_AI_CTRL0_BRDS_u {
	struct {
	    uint32_t                vco_pre_div:2;
	    uint32_t                dac_ibiastrim:4;
	    uint32_t:26;
	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_NON_AI_CTRL0_BRDS_u_t;

typedef union SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_REFCLK_BRDS_u {
	struct {
	    uint32_t                refclk_rate:8;
	    uint32_t                refclk_div: 6;
	    uint32_t:1;
	    uint32_t                 cml_sel:1;
	    uint32_t:16;

	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_REFCLK_BRDS_u_t;

typedef union SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL6_1_BRDS_u {
	struct {
	    uint32_t                fcw_int:9;
	    uint32_t:23;
	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL6_1_BRDS_u_t;

typedef union SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL7_BRDS_u {
	struct {
	    uint32_t                fcw_frac:16;
	    uint32_t:16;
	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL7_BRDS_u_t;

typedef union SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL9_BRDS_u {
	struct {
	    uint32_t                fcw0_frac_lsb:8;
	    uint32_t                fcw1_frac_lsb:8;
	    uint32_t:16;
	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} SYSTEMPLL2P0_N3_BUS_CSR_SNAP_6_AI_FREQ_CTRL9_BRDS_u_t;

typedef union clk6_pll_dfs_cntl_u {
	struct {
	    uint32_t                    PLL0_DFS0_DIVIDER:7;
	    uint32_t                    PLL0_DFS0_DDCA:2;
	    uint32_t:3;
	    uint32_t                    PLL0_DFS0_ADCA:4;
	    uint32_t                    PLL0_DFS0_AllowZeroDID	:1;
	    uint32_t                    PLL0_DFS0_EnableCLKOffInBypass:1;
	    uint32_t                    PLL0_DFS0_EnableVCO48OffInBypass	:1;
	    uint32_t:1;
	    uint32_t                    PLL0_DFS0_DiDtWait:3;
	    uint32_t:1;
	    uint32_t                    PLL0_DFS0_DiDtFloor	: 2;
	    uint32_t:2;
	    uint32_t                    PLL0_DFS0_SlamDid:1;
	    uint32_t                    PLL0_DFS0_FastRamp	:1;
	    uint32_t:2;
	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} clk6_pll_dfs_cntl_u_t;

typedef union clk_dfs_status_u {
	struct {
	    uint32_t           PLL0_DFS0_CURRENT_DFS_DID:7;
	    uint32_t:1;
	    uint32_t           PLL0_DFS0_AUTOLAUCH_FSM_FULL_SPEED_IDLE:1;
	    uint32_t           PLL0_DFS0_AUTOLAUCH_FSM_BYPASS_IDLE: 1;
	    uint32_t:6;
	    uint32_t          PLL0_DFS0_DFS_STATE_IDLE:1;
	    uint32_t            PLL0_DFS0_DID_FSM_IDLE:1;
	    uint32_t             PLL0_DFS0_DFS_DIV_REQ_IDLE: 1;
	    uint32_t:13;
	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} clk_dfs_status_u_t;

typedef  union clk6_clk_bypass_cntl_u {
	struct {
	    uint32_t                 CLK_BYPASS_SEL:3;
	    uint32_t                 CLK_BYPASS_DIV:4;
	    uint32_t                 CLK_PMUX_SGMode:8;
	    uint32_t                 CLK_BYPASS_SEL_STARTUP:3;
	    uint32_t:1;
	    uint32_t                 CLK_BYPASS_DIV_STARTUP:4;
	    uint32_t:8;
	} bitfields, bits;
	uint32_t    u32All;
	int32_t     i32All;
	float       f32All;
} clk6_bypass_cntl_u_t;

typedef	union acp_dsp_sw_intr_stat {
	struct {
	unsigned int	host_to_dsp0_intr1_stat:1;
	unsigned int	host_to_dsp0_intr2_stat:1;
	unsigned int	dsp0_to_host_intr_stat:1;
	unsigned int	host_to_dsp0_intr3_stat:1;
	unsigned int:28;
	} bits;
	unsigned int	u32all;
} acp_dsp_sw_intr_stat_t;

typedef	union acp_sw_intr_trig {
	struct {
	unsigned int	trig_host_to_dsp0_intr1:1;
	unsigned int:1;
	unsigned int	trig_dsp0_to_host_intr:1;
	unsigned int:29;
	} bits;
	unsigned int	u32all;
} acp_sw_intr_trig_t;

typedef void (*dma_callback_t)(const struct device *dev, void *user_data,
			       uint32_t channel, int status);

#define ACP_DMA_CHAN_COUNT      64
struct acp_dma_ptr_data {
	/* base address of dma  buffer */
	uint32_t base;
	/* size of dma  buffer */
	uint32_t size;
	/* write pointer of dma  buffer */
	uint32_t wr_ptr;
	/* read pointer of dma  buffer */
	uint32_t rd_ptr;
	/* read size of dma  buffer */
	uint32_t rd_size;
	/* write size of dma  buffer */
	uint32_t wr_size;
	/* system memory size defined for the stream */
	uint32_t sys_buff_size;
	/* virtual system memory offset for system memory buffer */
	uint32_t phy_off;
	/* probe_channel id  */
	uint32_t probe_channel;
};

enum acp_dma_state {
	ACP_DMA_READY,
	ACP_DMA_PREPARED,
	ACP_DMA_SUSPENDED,
	ACP_DMA_ACTIVE,
};

struct acp_dma_chan_data {
	uint32_t direction;
	enum acp_dma_state state;
	struct acp_dma_ptr_data ptr_data;	/* pointer data */
	dma_callback_t dma_tfrcallback;
	void *priv_data; /* unused */
};


#define SDW_INSTANCES 4
struct sdw_pin_data {
	uint32_t pin_num;
	uint32_t pin_dir;
	uint32_t dma_channel;
	uint32_t dma_channel1;
	uint32_t index[SDW_INSTANCES];
	uint32_t instance;
	uint32_t instance1;
};

struct tdm_context {
	uint64_t prev_pos;
	uint32_t buff_size;
	uint32_t tdm_instance;
	uint32_t pin_dir;
	uint32_t dma_channel;
	uint32_t index;
	uint32_t frame_fmt;
};

struct dmic_context {
	uint64_t prev_pos;
	uint32_t dmic_instance;
	uint32_t pin_dir;
	uint32_t dma_channel;
	uint32_t index;
};

struct acp_dma_dev_data {
	struct dma_context dma_ctx;
	struct acp_dma_chan_data chan_data[ACP_DMA_CHAN_COUNT];

	ATOMIC_DEFINE(atomic, ACP_DMA_CHAN_COUNT);
	struct dma_config  *dma_config;
	void *dai_index_ptr;
};

