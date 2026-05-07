/* SPDX-License-Identifier: BSD-3-Clause
 *
 *  Copyright(c) 2026 AMD. All rights reserved.
 *
 *
 */

//Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//				Sivasubramanian <sravisar@amd.com>

#define ACP_SW_INTR_TRIG					0x1241810
#define ACP_DSP0_INTR_CNTL					0x1241800
#define ACP_DSP0_INTR_STAT					0x1241804
#define ACP_DSP_SW_INTR_CNTL				0x1241808
#define ACP_DSP_SW_INTR_STAT				0x124180C
#define ACP_AXI2DAGB_SEM_0					0x1241874
#define ACP_SRBM_CLIENT_BASE_ADDR			0x12419EC
#define ACP_SRBM_CLIENT_RDDATA				0x12419F0
#define ACP_SRBM_CYCLE_STS					0x12419F4
#define ACP_SRBM_CLIENT_CONFIG				0x12419F8
#define MP1_SMN_C2PMSG_69					0x58A14
#define MP1_SMN_C2PMSG_85					0x58A54
#define MP1_SMN_C2PMSG_93					0x58A74
#define CLK7_ROOTREFCLK_MUX_1				0x6C0C8
#define CLK7_CLK_PLL_REFCLK_RATE_STARTUP	0x6C0D0
#define CLK7_CLK_PLL_REQ					0x6C0DC
#define CLK7_CLK1_DFS_CNTL					0x6C1B0
#define CLK7_CLK1_CURRENT_CNT				0x6C378
#define CLK7_CLK0_DFS_CNTL					0x6C1A4
#define CLK7_CLK0_CURRENT_CNT				0x6C374
#define CLK7_CLK0_BYPASS_CNTL				0x6C210
#define CLK7_CLK1_BYPASS_CNTL				0x6C234
#define CLK7_CLK0_DFS_STATUS				0x6C1AC
#define CLK7_CLK1_DFS_STATUS				0x6C1B8
#define CLK7_SPLL_FIELD_2					0x6C114
#define CLK7_CLK2_CURRENT_CNT				0x6C37C
#define CLK7_CLK2_BYPASS_CNTL				0x6C258
#define CLK7_CLK2_DFS_STATUS				0x6C1C4
#define CLK7_CLK_PLL_PWR_REQ				0x6C2F0
#define CLK7_CLK_DFSBYPASS_CONTROL			0x6C2F8
#define CLK7_CLK_FSM_STATUS					0x6C304
#define CLK7_SPLL_FUSE_1					0x6C0F8
#define CLK7_SPLL_FUSE_2					0x6C0FC
#define CLK7_SPLL_FIELD_7					0x6C128
#define CLK7_SPLL_FIELD_9					0x6C130
#define CLK7_SPLL_FIELD_6nm					0x6C138
#define CLK7_SPLL_FIELD_4					0x6C11C
#define CLK7_SPLL_FIELD_5nm_BUS_CTRL		0x6C140
#define CLK7_SPLL_FIELD_5nm_BUS_WDATA		0x6C144
#define CLK7_SPLL_FIELD_5nm_BUS_STATUS		0x6C148
#define CLK7_CLK_PLL_RESET_STOP_TIMER		0x6C180

typedef	union acp_srbm_cycle_sts {
	struct {
		unsigned int	srbm_clients_sts :1;
		unsigned int	:7;
	} bits;
	unsigned int	u32all;
} acp_srbm_cycle_sts_t;

typedef union acp_clkmux_sel {
	struct {
		unsigned int acp_clkmux_sel : 3;
		unsigned int : 13;
		unsigned int acp_clkmux_div_value : 16;
	} bits;
	unsigned int  u32all;
} acp_clkmux_sel_t;

typedef union clk7_clk1_dfs_cntl_u {
	struct {
		unsigned int CLK1_DIVIDER : 7;
		unsigned int : 25;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk1_dfs_cntl_u_t;

typedef union clk7_clk1_dfs_status_u {
	struct {
		unsigned int	: 16;
		unsigned int	CLK1_DFS_DIV_REQ_IDLE   : 1;
		unsigned int	: 2;
		unsigned int	RO_CLK1_DFS_STATE_IDLE  : 1;
		unsigned int	CLK1_CURRENT_DFS_DID    : 7;
		unsigned int	: 5;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk1_dfs_status_u_t;

typedef  union clk7_clk1_bypass_cntl_u {
	struct {
		unsigned int CLK1_BYPASS_SEL : 3;
		unsigned int : 13;
		unsigned int CLK1_BYPASS_DIV : 4;
		unsigned int : 12;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk1_bypass_cntl_u_t;

typedef union clk7_clk_fsm_status_u {
	struct {
		unsigned int AUTOLAUCH_FSM_FULL_SPEED_IDLE : 1;
		unsigned int : 3;
		unsigned int AUTOLAUCH_FSM_BYPASS_IDLE : 1;
		unsigned int : 3;
		unsigned int RO_FSM_PLL_STATUS_STARTED : 1;
		unsigned int : 3;
		unsigned int RO_FSM_PLL_STATUS_STOPPED : 1;
		unsigned int : 3;
		unsigned int RO_EARLY_FSM_DONE : 1;
		unsigned int : 3;
		unsigned int RO_DFS_GAP_ACTIVE : 1;
		unsigned int : 3;
		unsigned int RO_DID_FSM_IDLE : 1;
		unsigned int : 7;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk_fsm_status_t;

typedef union clk7_clk_pll_req_u {
	struct {
		unsigned int fbmult_int : 9;
		unsigned int : 3;
		unsigned int pllspinediv : 4;
		unsigned int fbmult_frac : 16;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk_pll_req_u_t;

typedef  union clk7_clk_pll_refclk_startup {
	struct {
		unsigned int  main_pll_ref_clk_rate_startup : 8;
		unsigned int  main_pll_cfg_4_startup       : 8;
		unsigned int  main_pll_ref_clk_div_startup : 2;
		unsigned int  main_pll_cfg_3_startup       : 10;
		unsigned int                              : 1;
		unsigned int  main_pll_refclk_src_mux0_startup : 1;
		unsigned int  main_pll_refclk_src_mux1_startup : 1;
		unsigned int                              : 1;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk_pll_refclk_startup_t;

typedef union clk7_spll_field_2 {
	struct{
		unsigned int                             : 3;
		unsigned int spll_fbdiv_mask_en          : 1;
		unsigned int spll_fracn_en               : 1;
		unsigned int spll_freq_jump_en           : 1;
		unsigned int : 25;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_2_t;

typedef union clk7_clk_dfsbypass_cntl {
	struct {
		unsigned int enter_dfs_bypass_0            : 1;
		unsigned int enter_dfs_bypass_1            : 1;
		unsigned int                             : 14;
		unsigned int exit_dfs_bypass_0             : 1;
		unsigned int exit_dfs_bypass_1             : 1;
		unsigned int                             : 14;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_clk_dfsbypass_cntl_t;

typedef union clk7_clk_pll_pwr_req {
	struct {
		unsigned int PLL_AUTO_START_REQ          : 1;
		unsigned int                             : 3;
		unsigned int PLL_AUTO_STOP_REQ           : 1;
		unsigned int                             : 3;
		unsigned int PLL_AUTO_STOP_NOCLK_REQ     : 1;
		unsigned int                             : 3;
		unsigned int PLL_AUTO_STOP_REFBYPCLK_REQ : 1;
		unsigned int                             : 3;
		unsigned int PLL_FORCE_RESET_HIGH        : 1;
		unsigned int                             : 15;
	} bitfields, bits;
	uint32_t    u32all;
	int32_t     i32all;
	float       f32all;
} clk7_clk_pll_pwr_req_t;

typedef union clk7_spll_fuse_1 {
	struct {
		unsigned int                             : 8;
		unsigned int spll_gp_coarse_exp          : 4;
		unsigned int spll_gp_coarse_mant         : 4;
		unsigned int                             : 4;
		unsigned int spll_gi_coarse_exp          : 4;
		unsigned int                             : 1;
		unsigned int spll_gi_coarse_mant         : 2;
		unsigned int                             : 5;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_fuse_1_t;

typedef union clk7_spll_fuse_2 {
	struct {
		unsigned int spll_tdc_resolution        : 8;
		unsigned int spll_freq_offset_exp       : 4;
		unsigned int spll_freq_offset_mant      : 5;
		unsigned int                            : 15;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_fuse_2_t;

typedef union clk7_spll_field_9 {
	struct {
		unsigned int                        : 16;
		unsigned int spll_dpll_cfg_3        : 10;
		unsigned int spll_fll_mode          : 1;
		unsigned int                        : 5;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_9_t;

typedef union clk7_spll_field_6nm {
	struct {
		unsigned int spll_dpll_cfg_4        : 8;
		unsigned int spll_reg_tim_exp       : 3;
		unsigned int spll_reg_tim_mant      : 1;
		unsigned int spll_ref_tim_exp       : 3;
		unsigned int spll_ref_tim_mant      : 1;
		unsigned int spll_vco_pre_div       : 2;
		unsigned int                        : 14;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_6nm_t;

typedef union clk7_spll_field_7 {
	struct {
		unsigned int                            : 7;
		unsigned int spll_pllout_sel            : 1;
		unsigned int spll_pllout_req            : 1;
		unsigned int spll_pllout_state          : 2;
		unsigned int spll_postdiv_ovrd          : 4;
		unsigned int spll_postdiv_pllout_ovrd   : 4;
		unsigned int spll_postdiv_sync_enable   : 1;
		unsigned int                            : 1;
		unsigned int spll_pwr_state             : 2;
		unsigned int                            : 1;
		unsigned int spll_refclk_rate           : 8;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_7_t;

typedef union clk7_spll_field_4 {
	struct {
		unsigned int spll_fcw0_frac_ovrd    : 16;
		unsigned int pll_out_sel              : 1;
		unsigned int                        : 3;
		unsigned int pll_pwr_dn_state          : 2;
		unsigned int                        : 2;
		unsigned int spll_refclk_div        : 2;
		unsigned int                        : 6;
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
		unsigned int                        :14;
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
		unsigned int ROOTREFCLK_MUX_1   : 1;
		unsigned int reserved           : 31;
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
		unsigned int                    :29;
	} bitfields, bits;
	uint32_t   u32all;
	int32_t    i32all;
	float      f32all;
} clk7_spll_field_5nm_bus_status_t;

typedef union acp_dsp0_sdw_intr_cntl {
	struct {
		unsigned int	soundwire_mask :17;
		unsigned int	:15;
	} bits;
	unsigned int	u32all;
} acp_dsp0_sdw_intr_cntl_t;

typedef	union acp_dsp_sw_intr_stat {
	struct {
	unsigned int	host_to_dsp0_intr1_stat:1;
	unsigned int	host_to_dsp0_intr2_stat:1;
	unsigned int	dsp0_to_host_intr_stat:1;
	unsigned int	host_to_dsp0_intr3_stat:1;
	unsigned int	:28;
	} bits;
	unsigned int	u32all;
} acp_dsp_sw_intr_stat_t;

typedef	union acp_sw_intr_trig {
	struct {
	unsigned int	trig_host_to_dsp0_intr1:1;
	unsigned int	:1;
	unsigned int	trig_dsp0_to_host_intr:1;
	unsigned int	:29;
	} bits;
	unsigned int	u32all;
} acp_sw_intr_trig_t;

typedef void (*dma_callback_t)(const struct device *dev, void *user_data,
			       uint32_t channel, int status);

#define ACP_DMA_CHAN_COUNT      12
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
	void *priv_data;//unused
};

struct dma_context1 {
	/** magic code to identify the context */
	int32_t magic;
	/** number of dma channels */
	int dma_channels;
	/** atomic holding bit flags for each channel to mark as used/unused */
	atomic_t *atomic;
};

struct sdw_pin_data {
	uint32_t pin_num;
	uint32_t pin_dir;
	uint32_t dma_channel;
	uint32_t index;
	uint32_t instance;
};

struct tdm_context {
	uint32_t tdm_instance;
	uint32_t pin_dir;
	uint32_t dma_channel;
	uint32_t index;
};

struct dmic_context {
	uint32_t dmic_instance;
	uint32_t pin_dir;
	uint32_t dma_channel;
	uint32_t index;
};

struct acp_dma_dev_data {
	struct dma_context1 dma_ctx;
	struct acp_dma_chan_data chan_data[ACP_DMA_CHAN_COUNT];

	ATOMIC_DEFINE(atomic, ACP_DMA_CHAN_COUNT);
	struct dma_config  *dma_config;
	void *dai_index_ptr;
};
