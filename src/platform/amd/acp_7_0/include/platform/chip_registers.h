/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 AMD.All rights reserved.
 *
 * Author:	SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
 */
#if !defined(_ACP_7_0_REG_HEADER)
#define _ACP_7_0_REG_HEADER

typedef	union acp_dma_cntl_0 {
	struct {
	unsigned int	dmachrst:1;
	unsigned int	dmachrun:1;
	unsigned int	dmachiocen:1;
	unsigned int	:29;
	} bits;
	unsigned int	u32all;
} acp_dma_cntl_0_t;

typedef	union acp_dma_ch_sts {
	struct {
	unsigned int	dmachrunsts:8;
	unsigned int	:24;
	} bits;
	unsigned int	u32all;
} acp_dma_ch_sts_t;

typedef	union acp_external_intr_enb {
	struct {
	unsigned int	acpextintrenb:1;
	unsigned int	:31;
	} bits;
	unsigned int	u32all;
} acp_external_intr_enb_t;

typedef	union acp_dsp0_intr_cntl {
	struct {
	unsigned int	dmaiocmask:8;
	unsigned int	:8;
	unsigned int	wov_dma_intr_mask:1;
	unsigned int	:6;
	unsigned int	audio_buffer_int_mask:6;
	unsigned int	:3;
	} bits;
	unsigned int	u32all;
} acp_dsp0_intr_cntl_t;

typedef	union acp_dsp0_intr_stat {
	struct {
	unsigned int	dmaiocstat:8;
	unsigned int	:8;
	unsigned int	wov_dma_stat:1;
	unsigned int	:6;
	unsigned int	audio_buffer_int_stat:6;
	unsigned int	:3;
	} bits;
	unsigned int	u32all;
} acp_dsp0_intr_stat_t;

typedef	union acp_dsp0_intr_cntl1 {
	struct {
		unsigned int	acp_fusion_dsp_ext_timer1_timeoutmask :1;
		unsigned int	fusion_dsp_watchdog_timeoutmask :1;
		unsigned int	soundwire_mask :1;
		unsigned int	audio_buffer_int_mask :6;
		unsigned int	:23;
	} bits;
	unsigned int	u32all;
} acp_dsp0_intr_cntl1_t;

typedef union acp_dsp0_intr_stat1 {
	struct {
		unsigned int	acp_fusion_dsp_timer1_timeoutstat :1;
		unsigned int	fusion_dsp_watchdog_timeoutstat :1;
		unsigned int	soundwire_stat :1;
		unsigned int	audio_buffer_int_stat :6;
		unsigned int	:23;
	} bits;
	unsigned int	u32all;
} acp_dsp0_intr_stat1_t;

typedef	union acp_dsp_sw_intr_cntl {
	struct {
	unsigned int	:2;
	unsigned int	dsp0_to_host_intr_mask:1;
	unsigned int	:29;
	} bits;
	unsigned int	u32all;
} acp_dsp_sw_intr_cntl_t;

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

typedef	union dsp_interrupt_routing_ctrl_0 {
	struct {
		unsigned int	dma_intr_level:3;
		unsigned int	:18;
		unsigned int	watchdog_intr_level:3;
		unsigned int	az_sw_i2s_intr_level:3;
		unsigned int	sha_intr_level:3;
		unsigned int	:2;
	} bits;
	unsigned int	u32all;
} dsp_interrupt_routing_ctrl_0_t;

typedef	union dsp_interrupt_routing_ctrl_1 {
	struct {
		unsigned int	host_to_dsp_intr1_level:3;
		unsigned int	host_to_dsp_intr2_level:3;
		unsigned int	src_intr_level:3;
		unsigned int	mailbox_intr_level:3;
		unsigned int	error_intr_level:3;
		unsigned int	wov_intr_level:3;
		unsigned int	fusion_timer1_intr_level:3;
		unsigned int	fusion_watchdog_intr_level:3;
		unsigned int	p1_sw_i2s_intr_level:3;
		unsigned int	:5;
	} bits;
	unsigned int    u32all;
} dsp_interrupt_routing_ctrl_1_t;

typedef	union acp_i2s_rx_ringbufaddr {
	struct {
		unsigned int	i2s_rx_ringbufaddr:27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_ringbufaddr_t;

typedef	union acp_i2s_rx_ringbufsize {
	struct {
		unsigned int	i2s_rx_ringbufsize:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_ringbufsize_t;

typedef	union acp_i2s_rx_linkpositioncntr {
	struct {
		unsigned int	i2s_rx_linkpositioncntr:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_linkpositioncntr_t;

typedef	union acp_i2s_rx_fifoaddr {
	struct {
		unsigned int	i2s_rx_fifoaddr:27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_fifoaddr_t;

typedef	union acp_i2s_rx_fifosize {
	struct {
		unsigned int	i2s_rx_fifosize:13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_fifosize_t;

typedef	union acp_i2s_rx_dma_size {
	struct {
		unsigned int	i2s_rx_dma_size:13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_dma_size_t;

typedef	union acp_i2s_rx_linearpositioncntr_high {
	struct {
		unsigned int	i2s_rx_linearpositioncntr_high:32;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_linearpositioncntr_high_t;

typedef	union acp_i2s_rx_linearpositioncntr_low {
	struct {
		unsigned int	i2s_rx_linearpositioncntr_low:32;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_linearpositioncntr_low_t;

typedef	union acp_i2s_rx_watermark_size {
	struct {
		unsigned int	i2s_rx_intr_watermark_size:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_i2s_rx_intr_watermark_size_t;

typedef	union acp_i2s_tx_ringbufaddr {
	struct {
		unsigned int	i2s_tx_ringbufaddr:27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_ringbufaddr_t;

typedef	union acp_i2s_tx_ringbufsize {
	struct {
		unsigned int	i2s_tx_ringbufsize:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_ringbufsize_t;

typedef	union acp_i2s_tx_linkpositioncntr {
	struct {
		unsigned int	i2s_tx_linkpositioncntr:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_linkpositioncntr_t;

typedef	union acp_i2s_tx_fifoaddr {
	struct {
		unsigned int	i2s_tx_fifoaddr:27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_fifoaddr_t;

typedef	union acp_i2s_tx_fifosize {
	struct {
		unsigned int	i2s_tx_fifosize:13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_fifosize_t;

typedef	union acp_i2s_tx_dma_size {
	struct {
		unsigned int	i2s_tx_dma_size:13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_dma_size_t;

typedef	union acp_i2s_tx_linearpositioncntr_high {
	struct {
		unsigned int	i2s_tx_linearpositioncntr_high:32;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_linearpositioncntr_hight_t;

typedef	union acp_i2s_tx_linearpositioncntr_low {
	struct {
		unsigned int	i2s_tx_linearpositioncntr_low:32;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_linearpositioncntr_low_t;

typedef	union acp_i2s_tx_intr_watermark_size {
	struct {
		unsigned int	i2s_tx_intr_watermark_size:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_i2s_tx_intr_watermark_size_t;

typedef	union acp_bt_rx_ringbufaddr {
	struct {
		unsigned int	bt_rx_ringbufaddr:27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_ringbufaddr_t;

typedef	union acp_bt_rx_ringbufsize {
	struct {
		unsigned int	bt_rx_ringbufsize:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_ringbufsize_t;

typedef	union acp_bt_rx_linkpositioncntr {
	struct {
		unsigned int	bt_rx_linkpositioncntr:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_linkpositioncntr_t;

typedef	union acp_bt_rx_fifoaddr {
	struct {
		unsigned int	bt_rx_fifoaddr:27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_fifoaddr_t;

typedef	union acp_bt_rx_fifosize {
	struct {
		unsigned int	bt_rx_fifosize:13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_fifosize_t;

typedef	union acp_bt_rx_dma_size {
	struct {
		unsigned int	bt_rx_dma_size:13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_dma_size_t;

typedef	union acp_bt_rx_linearpositioncntr_high {
	struct {
		unsigned int	bt_rx_linearpositioncntr_high:32;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_linearpositioncntr_high_t;

typedef	union acp_bt_rx_linearpositioncntr_low {
	struct {
		unsigned int	bt_rx_linearpositioncntr_low:32;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_linearpositioncntr_low_t;

typedef	union acp_bt_rx_intr_watermark_size {
	struct {
		unsigned int	bt_rx_intr_watermark_size:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_bt_rx_intr_watermark_size_t;

typedef	union acp_bt_tx_ringbufaddr {
	struct {
		unsigned int	bt_tx_ringbufaddr:27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_ringbufaddr_t;

typedef	union acp_bt_tx_ringbufsize {
	struct {
		unsigned int	bt_tx_ringbufsize:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_ringbufsize_t;

typedef	union acp_bt_tx_linkpositiontcntr {
	struct {
		unsigned int	bt_tx_linkpositioncntr:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_linkpositiontcntr_t;

typedef	union acp_bt_tx_fifoaddr {
	struct {
		unsigned int	bt_tx_fifoaddr:27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_fifoaddr_t;

typedef	union acp_bt_tx_fifosize {
	struct {
		unsigned int	bt_tx_fifosize:13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_fifosize_t;

typedef	union acp_bt_tx_dmasize {
	struct {
		unsigned int	bt_tx_dma_size:13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_dmasize_t;

typedef	union acp_bt_tx_linearpositioncntr_high {
	struct {
		unsigned int	bt_tx_linearpositioncntr_high:32;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_linearpositioncntr_high_t;

typedef	union acp_bt_tx_linearpositioncntr_low {
	struct {
		unsigned int	bt_tx_linearpositioncntr_low:32;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_linearpositioncntr_low_t;

typedef	union acp_bt_tx_intr_watermark_size {
	struct {
		unsigned int	bt_tx_intr_watermark_size:26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_bt_tx_intr_watermark_size_t;

typedef	union acp_i2stdm_ier {
	struct {
		unsigned int	i2stdm_ien:1;
		unsigned int	:31;
	} bits;
	unsigned int	u32all;
} acp_i2stdm_ier_t;

typedef	union acp_i2stdm_irer {
	struct {
		unsigned int	i2stdm_rx_en:1;
		unsigned int	i2stdm_rx_protocol_mode:1;
		unsigned int	i2stdm_rx_data_path_mode:1;
		unsigned int	i2stdm_rx_samplen:3;
		unsigned int	i2stdm_rx_status:1;
		unsigned int	:25;
	} bits;
	unsigned int	u32all;
} acp_i2stdm_irer_t;

typedef	union acp_i2stdm_iter {
	struct {
		unsigned int	i2stdm_txen:1;
		unsigned int	i2stdm_tx_protocol_mode:1;
		unsigned int	i2stdm_tx_data_path_mode:1;
		unsigned int	i2stdm_tx_samp_len:3;
		unsigned int	i2stdm_tx_status:1;
		unsigned int	:25;
	} bits;
	unsigned int	u32all;
} acp_i2stdm_iter_t;

typedef	union acp_bttdm_ier {
	struct {
		unsigned int	bttdm_ien:1;
		unsigned int	:31;
	} bits;
	unsigned int	u32all;
} acp_bttdm_ier_t;

typedef	union acp_bttdm_irer {
	struct {
		unsigned int	bttdm_rx_en:1;
		unsigned int	bttdm_rx_protocol_mode:1;
		unsigned int	bttdm_rx_data_path_mode:1;
		unsigned int	bttdm_rx_samplen:3;
		unsigned int	bttdm_rx_status:1;
		unsigned int	:25;
	} bits;
	unsigned int	u32all;
} acp_bttdm_irer_t;

typedef	union acp_bttdm_iter {
	struct {
		unsigned int	bttdm_txen :1;
		unsigned int	bttdm_tx_protocol_mode :1;
		unsigned int	bttdm_tx_data_path_mode :1;
		unsigned int	bttdm_tx_samp_len :3;
		unsigned int	bttdm_tx_status :1;
		unsigned int	:25;
	} bits;
	unsigned int	u32all;
} acp_bttdm_iter_t;

typedef	union acp_wov_pdm_dma_enable {
	struct {
		unsigned int	pdm_dma_en :1;
		unsigned int	pdm_dma_en_status :1;
		unsigned int	:30;
	} bits;
unsigned int	u32all;
} acp_wov_pdm_dma_enable_t;

typedef	union acp_wov_rx_ringbufaddr {
	struct {
		unsigned int	rx_ringbufaddr :27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_wov_rx_ringbufaddr_t;

typedef	union acp_wov_rx_ringbufsize {
	struct {
		unsigned int	rx_ringbufsize :26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_wov_rx_ringbufsize_t;

typedef	union acp_wov_rx_intr_watermark_size {
	struct {
		unsigned int	rx_intr_watermark_size :26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_wov_rx_intr_watermark_size_t;

typedef	union acp_wov_pdm_no_of_channels {
	struct {
		unsigned int	pdm_no_of_channels :2;
		unsigned int	:30;
	} bits;
	unsigned int	u32all;
} acp_wov_pdm_no_of_channels_t;

typedef	union acp_wov_pdm_decimation_factor {
	struct {
		unsigned int	pdm_decimation_factor :2;
		unsigned int	:30;
	} bits;
	unsigned int	u32all;
} acp_wov_pdm_decimation_factor_t;

typedef	union acp_wov_misc_ctrl {
	struct {
		unsigned int	:3;
		unsigned int	pcm_data_shift_ctrl :2;
		unsigned int	:27;
	} bits;
	unsigned int	u32all;
} acp_wov_misc_ctrl_t;

typedef	union acp_wov_clk_ctrl {
	struct {
		unsigned int	brm_clk_ctrl :4;
		unsigned int	pdm_vad_clkdiv :2;
		unsigned int	:26;
	} bits;
	unsigned int	u32all;
} acp_wov_clk_ctrl_t;

typedef	union acp_srbm_cycle_sts {
	struct {
		unsigned int	srbm_clients_sts :1;
		unsigned int	:7;
	} bits;
	unsigned int	u32all;
} acp_srbm_cycle_sts_t;

typedef	union acp_hs_rx_ringbufaddr {
	struct {
		unsigned int	hs_rx_ringbufaddr :32;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_ringbufaddr_t;

typedef	union acp_hs_rx_ringbufsize {
	struct {
		unsigned int	hs_rx_ringbufsize :26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_ringbufsize_t;

typedef	union acp_hs_rx_linkpositioncntr {
	struct {
		unsigned int	hs_rx_linkpositioncntr :26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_linkpositioncntr_t;

typedef	union acp_hs_rx_fifoaddr {
	struct {
		unsigned int	hs_rx_fifoaddr :27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_fifoaddr_t;

typedef	union acp_hs_rx_fifosize {
	struct {
		unsigned int	hs_rx_fifosize :13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_fifosize_t;

typedef	union acp_hs_rx_dma_size {
	struct {
		unsigned int	hs_rx_dma_size :13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_dma_size_t;

typedef	union acp_hs_rx_linearpositioncntr_high {
	struct {
		unsigned int	hs_rx_linearpositioncntr_high :32;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_linearpositioncntr_high_t;

typedef	union acp_hs_rx_linearpositioncntr_low {
	struct {
		unsigned int	hs_rx_linearpositioncntr_low :32;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_linearpositioncntr_low_t;

typedef	union acp_hs_rx_intr_watermark_size {
	struct {
		unsigned int	hs_rx_intr_watermark_size :26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_hs_rx_intr_watermark_size_t;

typedef	union acp_hs_tx_ringbufaddr {
	struct {
		unsigned int	hs_tx_ringbufaddr :32;
	} bits;
	unsigned int	u32all;
} acp_hs_tx_ringbufaddr_t;

typedef	union acp_hs_tx_ringbufsize {
	struct {
		unsigned int	hs_tx_ringbufsize :26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_hs_tx_ringbufsize_t;

typedef	union acp_hs_tx_linkpositioncntr {
	struct {
		unsigned int	hs_tx_linkpositioncntr :26;
		unsigned int	:6;
	} bits;
	unsigned int    u32all;
} acp_hs_tx_linkpositioncntr_t;

typedef	union acp_hs_tx_fifoaddr {
	struct {
		unsigned int	hs_tx_fifoaddr :27;
		unsigned int	:5;
	} bits;
	unsigned int	u32all;
} acp_hs_tx_fifoaddr_t;

typedef	union acp_hs_tx_fifosize {
	struct {
		unsigned int	hs_tx_fifosize :13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_hs_tx_fifosize_t;

typedef	union acp_hs_tx_dma_size {
	struct {
		unsigned int	hs_tx_dma_size :13;
		unsigned int	:19;
	} bits;
	unsigned int	u32all;
} acp_hs_tx_dma_size_t;

typedef	union acp_hs_tx_linearpositioncntr_high {
	struct {
		unsigned int	hs_tx_linearpositioncntr_high :32;
	} bits;
	unsigned int	u32all;
} acp_hs_tx_linearpositioncntr_high_t;

typedef	union acp_hs_tx_linearpositioncntr_low {
	struct {
		unsigned int	hs_tx_linearpositioncntr_low :32;
	} bits;
	unsigned int	u32all;
} acp_hs_tx_linearpositioncntr_low_t;

typedef	union acp_hs_tx_intr_watermark_size {
	struct {
		unsigned int	hs_tx_intr_watermark_size :26;
		unsigned int	:6;
	} bits;
	unsigned int	u32all;
} acp_hs_tx_intr_watermark_size_t;

typedef	union acp_i2stdm_rxfrmt {
	struct {
		unsigned int	i2stdm_frame_len :9;
		unsigned int	:6;
		unsigned int	i2stdm_num_slots :3;
		unsigned int	i2stdm_slot_len :5;
		unsigned int	:9;
	} bits;
	unsigned int	u32all;
} acp_i2stdm_rxfrmt_t;

typedef	union acp_i2stdm_txfrmt {
	struct {
		unsigned int	i2stdm_frame_len :9;
		unsigned int	:6;
		unsigned int	i2stdm_num_slots :3;
		unsigned int	i2stdm_slot_len :5;
		unsigned int	:9;
	} bits;
	unsigned int	u32all;
} acp_i2stdm_txfrmt_t;

typedef	union acp_hstdm_ier {
	struct {
		unsigned int	hstdm_ien :1;
		unsigned int	:31;
	} bits;
	unsigned int	u32all;
} acp_hstdm_ier_t;

typedef	union acp_hstdm_irer {
	struct {
		unsigned int	hstdm_rx_en :1;
		unsigned int	hstdm_rx_protocol_mode :1;
		unsigned int	hstdm_rx_data_path_mode :1;
		unsigned int	hstdm_rx_samplen :3;
		unsigned int	hstdm_rx_status :1;
		unsigned int	:25;
	} bits;
	unsigned int	u32all;
} acp_hstdm_irer_t;

typedef	union acp_hstdm_rxfrmt {
	struct {
		unsigned int	hstdm_frame_len :9;
		unsigned int	:6;
		unsigned int	hstdm_num_slots :3;
		unsigned int	hstdm_slot_len :5;
		unsigned int	:9;
	} bits;
	unsigned int	u32all;
} acp_hstdm_rxfrmt_t;

typedef	union acp_hstdm_iter {
	struct {
		unsigned int	hstdm_txen :1;
		unsigned int	hstdm_tx_protocol_mode :1;
		unsigned int	hstdm_tx_data_path_mode :1;
		unsigned int	hstdm_tx_samp_len :3;
		unsigned int	hstdm_tx_status :1;
		unsigned int	:25;
	} bits;
	unsigned int	u32all;
} acp_hstdm_iter_t;

typedef	union acp_hstdm_txfrmt {
	struct {
		unsigned int	hstdm_frame_len :9;
		unsigned int	:6;
		unsigned int	hstdm_num_slots :3;
		unsigned int	hstdm_slot_len :5;
		unsigned int	:9;
	} bits;
	unsigned int	u32all;
} acp_hstdm_txfrmt_t;

typedef union acp_clkmux_sel {
	struct {
		unsigned int acp_clkmux_sel : 3;
		unsigned int : 13;
		unsigned int acp_clkmux_div_value : 16;
	} bits;
	unsigned int  u32all;
} acp_clkmux_sel_t;

typedef union acp_i2stdm_mstrclkgen {
	struct {
		unsigned int i2stdm_master_mode : 1;
		unsigned int i2stdm_format_mode : 1;
		unsigned int i2stdm_lrclk_div_val : 11;
		unsigned int i2stdm_bclk_div_val : 11;
		unsigned int : 8;
	} bits;
	unsigned int  u32all;
} acp_i2stdm_mstrclkgen_t;

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

#endif
