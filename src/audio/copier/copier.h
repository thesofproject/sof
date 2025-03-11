/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/**
 * \file include/ipc4/copier.h
 * \brief IPC4 copier definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_COPIER_H__
#define __SOF_IPC4_COPIER_H__

#include <stdint.h>
#include <ipc4/base-config.h>
#include <ipc4/gateway.h>
#include <ipc4/alh.h>

#include <sof/compiler_attributes.h>
#include <sof/audio/buffer.h>
#include <sof/audio/pcm_converter.h>
#if CONFIG_INTEL_ADSP_MIC_PRIVACY
#include <sof/lib/notifier.h>
#include <sof/audio/mic_privacy_manager.h>
#endif

static const uint32_t INVALID_QUEUE_ID = 0xFFFFFFFF;

/* copier Module Configuration & Interface
 * UUID: 9BA00C83-CA12-4A83-943C-1FA2E82F9DDA
 *
 * Copier may be instantiated and bound in one of following configurations:
 *
 * - case 1:
 * digraph Module_Copier_Module {
 *   InputGateway
 *   InputGateway -> Copier
 *
 *   DestinationMod
 *   Copier -> DestinationMod
 * }
 *
 * digraph Module_Copier_Gateways {
 *   SourceMod
 *   SourceMod -> Copier
 *
 *   OutputGateway
 *   Copier -> OutputGateway
 * }
 * - case 3:
 * digraph Module_Copier_Module {
 *   SourceMod
 *   SourceMod -> Copier
 *
 *   DestinationMod
 *   Copier -> DestinationMod
 * }
 *
 * - case 4:
 * digraph Module_Copier_Module {
 *   SourceMod
 *
 *   SourceMod -> Copier
 *
 *   DestinationMod
 *   OutputGateway
 *
 *   Copier -> OutputGateway
 *   Copier -> DestinationMod
 * }
 *
 * In cases 1 and 2, the initial configuration must include Gateway Configuration
 * data along with valid Node ID of the gateway to be connected on either
 * Copier's end.
 *
 * Gateway can only be connected to input pin "0" or output pin "0".
 *
 * Initial configuration data allows setup audio format of main Copier's pins,
 * input pin "0" and output pin "0" and prepare PCM conversion routine if any is
 * required. However Copier supports up to #COPIER_MODULE_OUTPUT_PINS_COUNT
 * output pins. Before any additional output pin is used in binding operation,
 * the host driver has to send run-time parameter to setup sink formwat
 * (#COPIER_MODULE_CFG_PARAM_SET_SINK_FORMAT) first to setup a PCM conversion
 * routine if any is required.
 */

#define IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT		4

/*
 * Gateway can only be connected to input pin "0" or output pin "0".
 */
#define IPC4_COPIER_GATEWAY_PIN 0

enum ipc4_copier_features {
	/* ff FAST_MODE bit is set in CopierModuleCfg::copier_feature_mask then
	 * copier is able to transfer more than ibs. This bit shall be set only if
	 * all sinks are connected to data processing queue.
	 */
	IPC4_COPIER_FAST_MODE = 0
};

#if CONFIG_HOST_DMA_STREAM_SYNCHRONIZATION
#define HDA_SYNC_FPI_UPDATE_GROUP 0
struct ipc4_copier_sync_group {
	uint32_t group_id;
	uint32_t fpi_update_period_usec;
} __packed __aligned(4);
#endif

struct ipc4_copier_gateway_cfg {
	/* ID of Gateway Node. If node_id is valid, i.e. != -1, copier instance is connected to the
	 * specified gateway using either input pin 0 or output pin 0 depending on
	 * the node's direction, otherwise the data in this structure is ignored.
	 */
	union ipc4_connector_node_id node_id;
	/* preferred Gateway DMA buffer size (in bytes).
	 * FW attempts to allocate DMA buffer according to this value, however it may
	 * fall back to IBS/OBS * 2 in case there is no memory available for deeper
	 * buffering.
	 */
	uint32_t dma_buffer_size;
	/* length of gateway node configuration blob specified in #config_data.
	 * Length must be specified in number of dwords.
	 * Refer to the specific gateway documentation for details on the node
	 * configuration blob requirements.
	 */
	uint32_t config_length;
	/* gateway node configuration blob */
	uint32_t config_data[1];
} __attribute__((packed, aligned(4)));

struct ipc4_copier_module_cfg {
	struct ipc4_base_module_cfg base;

	/* audio format for output pin 0 */
	struct ipc4_audio_format out_fmt;
	uint32_t copier_feature_mask;
	struct ipc4_copier_gateway_cfg gtw_cfg;
} __attribute__((packed, aligned(4)));

enum ipc4_copier_module_config_params {
	/* Use LARGE_CONFIG_SET to initialize timestamp event. Ipc mailbox must
	 * contain properly built CopierConfigTimestampInitData struct.
	 */
	IPC4_COPIER_MODULE_CFG_PARAM_TIMESTAMP_INIT = 1,
	/* Use LARGE_CONFIG_SET to initialize copier sink. Ipc mailbox must contain
	 * properly built CopierConfigSetSinkFormat struct.
	 */
	IPC4_COPIER_MODULE_CFG_PARAM_SET_SINK_FORMAT = 2,
	/* Use LARGE_CONFIG_SET to initialize and enable on Copier data segment
	 * event. Ipc mailbox must contain properly built DataSegmentEnabled struct.
	 */
	IPC4_COPIER_MODULE_CFG_PARAM_DATA_SEGMENT_ENABLED = 3,
	/* Use LARGE_CONFIG_GET to retrieve Linear Link Position (LLP) value for non
	 * HD-A gateways.
	 */
	IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING = 4,
	/* Use LARGE_CONFIG_GET to retrieve Linear Link Position (LLP) value for non
	 * HD-A gateways and corresponding total processed data
	 * Sample code to retrieve LlpReadingExtended:
	 * Message::LargeConfigOp message(true, COPIER_MODULE_ID, KPB_INSTANCE_ID);
	 * message.GetBits().large_param_id = COPIER_MODULE_CFG_PARAM_LLP_READING_EXTENDED;
	 * message.GetBits().init_block = true;
	 * message.GetBits().final_block = true;
	 * message.GetBits().data_off_size = IPC_OUTPUT_MAILBOX;
	 * LlpReadingExtended* output_mailbox = NULL;
	 * send_ipc(message, input_mailbox, (uint8_t**)&output_mailbox);
	 */
	IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING_EXTENDED = 5,
	/* Use LARGE_CONFIG_SET to setup attenuation on output pins. Data is just
	 * uint32_t. Config is only allowed when output pin is set up for 32bit and
	 * source is connected to Gateway
	 */
	IPC4_COPIER_MODULE_CFG_ATTENUATION = 6,
	/* Use LARGE_CONFIG_SET to setup new channel map, which allows to map channels
	 * from gateway buffer to copier with any order.
	 * Same mapping will be applied for all copier sinks.
	 */
	IPC4_COPIER_MODULE_CFG_PARAM_CHANNEL_MAP = 7
};

struct ipc4_copier_config_timestamp_init_data {
	/* Contains low-level configuration for timestamp init.
	 * Passed-through directly into ifc _LOCAL_TS_Control Register of
	 * corresponding HW i/f from DSP Timestamping Registers.
	 */
	uint32_t tsctrl_reg;
} __attribute__((packed, aligned(4)));

struct ipc4_copier_config_set_sink_format {
	uint32_t sink_id;
	/* Input format used by the source. Must be the same as present
	 * if already initialized.
	 */
	struct ipc4_audio_format source_fmt;
	/* Output format used by the sink */
	struct ipc4_audio_format sink_fmt;
} __attribute__((packed, aligned(4)));

struct ipc4_copier_config_channel_map {
	/* Each half-byte of the channel map is an index of the DMA stream channel that
	 * should be copied to the position determined by this half-byte index.
	 */
	uint32_t channel_map;
} __attribute__((packed, aligned(4)));

#define IPC4_COPIER_DATA_SEGMENT_DISABLE	(0 << 0)
#define IPC4_COPIER_DATA_SEGMENT_ENABLE	(1 << 0)
#define IPC4_COPIER_DATA_SEGMENT_RESTART	(1 << 1)

struct ipc4_data_segment_enabled {
	/* Gateway node id */
	uint32_t node_id;
	/* Indicates whether notification should be enabled (!=0) or disabled (=0).
	 * Carries additional information. If bit 1 is set DS will be restarted
	 * immediately.
	 * Use only as logic or of COPIER_DATA_SEGMENT_*.
	 * To disable:
	 *   COPIER_DATA_SEGMENT_DISABLE
	 * To enable, but finish previous:
	 *   COPIER_DATA_SEGMENT_ENABLE
	 * To enable, and apply right away:
	 *   COPIER_DATA_SEGMENT_ENABLE | COPIER_DATA_SEGMENT_RESTART
	 */
	uint32_t enabled;
	/* Data segment size (in bytes) */
	uint32_t data_seg_size;
} __attribute__((packed, aligned(4)));

struct copier_data {
	/*
	 * struct ipc4_copier_module_cfg actually has variable size, but we
	 * don't need the variable size array at the end, we won't be copying it
	 * from the IPC data.
	 */
	struct ipc4_copier_module_cfg config;
	void *gtw_cfg;
	enum ipc4_gateway_type gtw_type;
	uint32_t endpoint_num;

	/* buffer to mux/demux data from/to multiple endpoints for ALH multi-gateway case */
	struct comp_buffer *multi_endpoint_buffer;

	bool bsource_buffer;

	int direction;
	/* sample data >> attenuation in range of [1 - 31] */
	uint32_t attenuation;

	/* pipeline register offset in memory windows 0 */
	uint32_t pipeline_reg_offset;
	uint64_t host_position;

	struct ipc4_audio_format out_fmt[IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT];
	pcm_converter_func converter[IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT];
	uint64_t input_total_data_processed;
	uint64_t output_total_data_processed;
	struct host_data *hd;
	bool ipc_gtw;
	struct dai_data *dd[IPC4_ALH_MAX_NUMBER_OF_GTW];
	uint32_t channels[IPC4_ALH_MAX_NUMBER_OF_GTW];
	uint32_t chan_map[IPC4_ALH_MAX_NUMBER_OF_GTW];
	struct ipcgtw_data *ipcgtw_data;
#if CONFIG_INTEL_ADSP_MIC_PRIVACY
	struct mic_privacy_data *mic_priv;
#endif
};

int apply_attenuation(struct comp_dev *dev, struct copier_data *cd,
		      struct comp_buffer *sink, int frame);

pcm_converter_func get_converter_func(const struct ipc4_audio_format *in_fmt,
				      const struct ipc4_audio_format *out_fmt,
				      enum ipc4_gateway_type type,
				      enum ipc4_direction_type dir,
				      uint32_t chmap);

struct comp_ipc_config;
int create_multi_endpoint_buffer(struct comp_dev *dev,
				 struct copier_data *cd,
				 const struct ipc4_copier_module_cfg *copier_cfg);

enum sof_ipc_stream_direction
	get_gateway_direction(enum ipc4_connector_node_id_type node_id_type);

void copier_update_params(struct copier_data *cd, struct comp_dev *dev,
			  struct sof_ipc_stream_params *params);
#endif
