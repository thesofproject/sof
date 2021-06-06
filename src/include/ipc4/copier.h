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
 * \file include/ipc4/header.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_COPIER_H__
#define __SOF_IPC4_COPIER_H__

#include <stdint.h>
#include <ipc4/base-config.h>

/**
 * This is basic module config that may serve as a base for more specialized, module
 * specific config received along with Init Module Instance from host.
 *
 * Remarks: <br>
 * The data required to build basic module configuration should be retrieved by the
 * software driver from the Module Configuration array, part of the FW Image Manifest.
 * Appropriate Module Configuration record is selected based on look up parameters
 * (specific look up parameter values are driver implementation specific and should
 * be provided as an input to the FW Image Builder).
 */
//typedef intel_adsp::LegacyModuleInitialSettings BaseModuleCfg;

//! Linear Link Position Reading Data
struct ipc4_llp_reading {
	//! lower part of 64-bit LLP
	uint32_t llp_l;
	//! upper part of 64-bit LLP
	uint32_t llp_u;
	//! lower part of 64-bit Wallclock
	uint32_t wclk_l;
	//! upper part of 64-bit Wallclock
	uint32_t wclk_u;
} __attribute__((packed, aligned(4)));

struct ipc4_llp_reading_extended {
	struct ipc4_llp_reading llp_reading;
	//! total processed data (low part)
	uint32_t tpd_low;
	//! total processed data (high part)
	uint32_t tpd_high;
} __attribute__((packed, aligned(4)));

struct ipc4_base_module_cfg_ext {
	/*!
	 * \brief Specifies number of items in input_pins array. Maximum size is 8.
	 */
	uint16_t nb_input_pins;
	/*!
	 * \brief Specifies number of items in output_pins array. Maximum size is 8.
	 */
	uint16_t nb_output_pins;
	/*!
	 * \brief Not used, set to 0.
	 */
	uint8_t reserved[8];
	/*!
	 * \brief Length of optional module specific parameters that follow BaseModuleCfgExt.
	 */
	uint32_t  priv_param_length;
	/*!
	 * \brief Specifies format of input pins.
	 * \remarks Pin format arrays may be non-continuous i.e. may contain pin #0 format followed by pin #2 format
	 *          in case pin #1 will not be in use. FW assigned format of the pin based on pin_index,
	 *          not on a position of the item in the array. Applies to both input and output pins.
	 */
	struct ipc4_input_pin_format input_pins[1];
	/*!
	 * \brief Specifies format of output pins.
	 */
	struct ipc4_output_pin_format output_pins[1];
} __attribute__((packed, aligned(4)));

/**
 * This enum defines short 16bit parameters common for all modules.
 * Value of module specific parameters have to be less than 0x3000.
 */
enum ipc4_base_module_params {
	/**
	* Handled inside LargeConfigGet of module instance
	*/
	IPC4_MOD_INST_PROPS  = 0xFE,
	/**
	* Handled inside ConfigSet of module instance
	*/
	IPC4_MOD_INST_ENABLE = 0x3000
};

//TODO: needs to be moved out of IPC ABI header
static const uint32_t INVALID_QUEUE_ID = 0xFFFFFFFF;

/*!
  \struct PinProps
  Properties of a pin.
*/
struct ipc4_pin_props {
	/*! Type of the connected stream. */
	enum ipc4_stream_type stream_type;

	/*! Audio format of the stream.
	  The content is valid in case of ePcm stream_type. */
	struct ipc4_audio_format format;

	/*! Unique ID of the physical queue connected to the pin.
	  If there is no queue connected, then -1 (invalid queue ID) is set. */
	uint32_t phys_queue_id;
} __attribute__((packed, aligned(4)));


struct ipc4_pin_list_info {
	uint32_t pin_count;
	struct ipc4_pin_props pin_info[1];
} __attribute__((packed, aligned(4)));

/**
 * Helper structure definition.
 * Reflects the last two entries in ModuleInstanceProps sttructure.
 */
struct ipc4_in_out_gateway {
	uint32_t  input_gateway;
	uint32_t  output_gateway;
} __attribute__((packed, aligned(4)));

/**
 * Structure describing module instance properties used in response
 * to module LargeConfigGet with MOD_INST_PROPS parameter.
 */
struct ipc4_module_instance_props {
	uint32_t  id;
	uint32_t  dp_queue_type;
	uint32_t  queue_alignment;
	uint32_t  cp_usage_mask;
	uint32_t  stack_bytes;
	uint32_t  bss_total_bytes;
	uint32_t  bss_used_bytes;
	uint32_t  ibs_bytes;
	uint32_t  obs_bytes;
	uint32_t  cpc;
	uint32_t  cpc_peak;
	// TODO: uncomment or remove once performance measurements collection mechanism
	// uint32_t  cpc_avg;
	// uint32_t  cpc_last;
	struct ipc4_pin_list_info input_queues;
	struct ipc4_pin_list_info output_queues;
	uint32_t  input_gateway;
	uint32_t  output_gateway;
} __attribute__((packed, aligned(4)));

/**
 * This structure may be used by modules to carry
 * short 16bit parameters as part of the IxC
 * register content.
 */
union ipc4_cfg_param_id_data {
	uint32_t dw;
	struct {
		uint32_t data16 : 16;   ///< Input/Output small config data
		uint32_t id     : 14;   ///< input parameter ID
		uint32_t _rsvd  : 2;
	} f;
} __attribute__((packed, aligned(4)));

/*!
  \name Copier Module Configuration & Interface

  <b>UUID: 9BA00C83-CA12-4A83-943C-1FA2E82F9DDA </b>

  Copier may be instantiated and bound in one of following configurations:

  - case 1:
  \dot
  digraph Module_Copier_Module {
    rankdir = "LR";
    node [shape=box style=rounded, fontsize=10, fontname="Tahoma"];
    edge [fontsize=10, fontname="Tahoma"];
    InputGateway [color=green]
    InputGateway -> Copier [color=green]

    DestinationMod [color=red]
    Copier -> DestinationMod [color=red]
  }
  \enddot

  - case 2:
  \dot
  digraph Module_Copier_Gateways {
    rankdir = "LR";
    node [shape=box style=rounded, fontsize=10, fontname="Tahoma"];
    edge [fontsize=10, fontname="Tahoma"];
    SourceMod [color=red]

    SourceMod -> Copier  [color=red]

    OutputGateway [color=green]
    Copier -> OutputGateway [color=green]
  }
  \enddot

  - case 3:
  \dot
  digraph Module_Copier_Module {
    rankdir = "LR";
    node [shape=box style=rounded, fontsize=10, fontname="Tahoma"];
    edge [fontsize=10, fontname="Tahoma"];
    SourceMod [color=red]

    SourceMod -> Copier [color=red]

    DestinationMod [color=red]
    Copier -> DestinationMod [color=red]
  }
  \enddot

  - case 4:
  \dot
  digraph Module_Copier_Module {
    rankdir = "LR";
    node [shape=box style=rounded, fontsize=10, fontname="Tahoma"];
    edge [fontsize=10, fontname="Tahoma"];
    SourceMod [color=red]

    SourceMod -> Copier [color=red]

    DestinationMod [color=red]
    OutputGateway [color=green]

    Copier -> OutputGateway [color=green]
    Copier -> DestinationMod [color=red]
  }
  \enddot

  In cases 1 and 2, the initial configuration must include Gateway Configuration
  data along with valid Node ID of the gateway to be connected on either
  Copier's end.

  \attention Gateway can only be connected to input pin "0" or output pin "0".

  Initial configuration data allows setup audio format of main Copier's pins,
  input pin "0" and output pin "0" and prepare PCM conversion routine if any is
  required. However Copier supports up to #COPIER_MODULE_OUTPUT_PINS_COUNT
  output pins. Before any additional output pin is used in binding operation,
  the host driver has to send run-time parameter to setup sink formwat
  (#COPIER_MODULE_CFG_PARAM_SET_SINK_FORMAT) first to setup a PCM conversion
  routine if any is required.

  ------------------------------------------------------------------------------
*/

//! Total number of output pins. TODO: this should be a macro
static const size_t COPIER_MODULE_OUTPUT_PINS_COUNT = 4;

enum ipc4_copier_features {
	/*!
	  If FAST_MODE bit is set in CopierModuleCfg::copier_feature_mask then
	  copier is able to transfer more than ibs. This bit shall be set only if
	  all sinks are connected to data processing queue.
	*/
	IPC4_COPIER_FAST_MODE = 0
};

struct ipc4_copier_gateway_cfg {
	//! ID of Gateway Node.
	/*!
	  If node_id is valid, i.e. != -1, copier instance is connected to the
	  specified gateway using either input pin 0 or output pin 0 depending on
	  the node's direction, otherwise the data in this structure is ignored.
	*/
	uint32_t node_id;
	//! Preferred Gateway DMA buffer size (in bytes).
	/*!
	  FW attempts to allocate DMA buffer according to this value, however it may
	  fall back to IBS/OBS * 2 in case there is no memory available for deeper
	  buffering.
	*/
	uint32_t dma_buffer_size;
	//! Length of gateway node configuration blob specified in #config_data.
	/*!
	\note Length must be specified in number of dwords.

	  Refer to the specific gateway documentation for details on the node
	  configuration blob requirements.
	*/
	uint32_t config_length;
	//! Gateway node configuration blob.
	uint32_t config_data[1];
} __attribute__((packed, aligned(4)));

struct ipc4_copier_module_cfg {
	struct ipc4_base_module_cfg base;

	//! Audio format for output pin 0.
	struct ipc4_audio_format out_fmt;
	//! Mask of allowed copier features.
	/*!
	\see CopierFeatures
	*/
	uint32_t copier_feature_mask;
	//! Gateway Configuration.
	struct ipc4_copier_gateway_cfg  gtw_cfg;
} __attribute__((packed, aligned(4)));

//TODO: is this IPC or state data ???
struct copier_data {
	struct comp_dev *host;
	struct comp_dev *dai;
	struct comp_buffer *buf;
	int direction;
	pcm_converter_func converter;

	struct ipc4_copier_module_cfg config;
};

enum ipc4_copier_module_config_params {
	/*!
	  Use LARGE_CONFIG_SET to initialize timestamp event. Ipc mailbox must
	  contain properly built CopierConfigTimestampInitData struct.
	*/
	COPIER_MODULE_CFG_PARAM_TIMESTAMP_INIT = 1,
	/*!
	  Use LARGE_CONFIG_SET to initialize copier sink. Ipc mailbox must contain
	  properly built CopierConfigSetSinkFormat struct.
	*/
	COPIER_MODULE_CFG_PARAM_SET_SINK_FORMAT = 2,
	/*!
	  Use LARGE_CONFIG_SET to initialize and enable on Copier data segment
	  event. Ipc mailbox must contain properly built DataSegmentEnabled struct.
	*/
	COPIER_MODULE_CFG_PARAM_DATA_SEGMENT_ENABLED = 3,
	/*!
	  Use LARGE_CONFIG_GET to retrieve Linear Link Position (LLP) value for non
	  HD-A gateways.

	  \see LlpReading
	*/
	COPIER_MODULE_CFG_PARAM_LLP_READING = 4,
	/*!
	  Use LARGE_CONFIG_GET to retrieve Linear Link Position (LLP) value for non
	  HD-A gateways and corresponding total processed data

	  \see LlpReadingExtended

	  Sample code to retrieve LlpReadingExtended:
	  \code
	  Message::LargeConfigOp message(true, COPIER_MODULE_ID, KPB_INSTANCE_ID);
	  message.GetBits().large_param_id = COPIER_MODULE_CFG_PARAM_LLP_READING_EXTENDED;
	  message.GetBits().init_block = true;
	  message.GetBits().final_block = true;
	  message.GetBits().data_off_size = IPC_OUTPUT_MAILBOX;

	  LlpReadingExtended* output_mailbox = NULL;
	  send_ipc(message, input_mailbox, (uint8_t**)&output_mailbox);

	  \endcode
	*/
	COPIER_MODULE_CFG_PARAM_LLP_READING_EXTENDED = 5,
	/*!
	  Use LARGE_CONFIG_SET to setup attenuation on output pins. Data is just
	  uint32_t.

	  \note Config is only allowed when output pin is set up for 32bit and
	    source is connected to Gateway
	*/
	COPIER_MODULE_CFG_ATTENUATION = 6
};

struct ipc4_copier_config_timestamp_init_data {
	//! Contains low-level configuration for timestamp init.
	/*!
	  Passed-through directly into ifc _LOCAL_TS_Control Register of
	  corresponding HW i/f from DSP Timestamping Registers.
	*/
	uint32_t tsctrl_reg;
} __attribute__((packed, aligned(4)));

struct CopierConfigSetSinkFormat
{
	//! Id of sink
	uint32_t sink_id;
	//! Input format used by the source
	/*!
	  \attention Must be the same as present if already initialized.
	*/
	struct ipc4_audio_format source_fmt;
	//! Output format used by the sink
	struct ipc4_audio_format sink_fmt;
} __attribute__((packed, aligned(4)));

static const uint32_t COPIER_DATA_SEGMENT_DISABLE = (0 << 0);
static const uint32_t COPIER_DATA_SEGMENT_ENABLE  = (1 << 0);
static const uint32_t COPIER_DATA_SEGMENT_RESTART = (1 << 1);

struct DataSegmentEnabled
{
	//! Gateway node id
	uint32_t node_id;
	//! Indicates whether notification should be enabled (!=0) or disabled (=0).
	/*!
	  Carries additional information. If bit 1 is set DS will be restarted
	  immediately.
	  Use only as logic or of COPIER_DATA_SEGMENT_*.
	  To disable:
	    COPIER_DATA_SEGMENT_DISABLE
	  To enable, but finish previous:
	    COPIER_DATA_SEGMENT_ENABLE
	  To enable, and apply right away:
	    COPIER_DATA_SEGMENT_ENABLE | COPIER_DATA_SEGMENT_RESTART
	  */
	uint32_t enabled;
	//! Data segment size (in bytes).
	uint32_t data_seg_size;
} __attribute__((packed, aligned(4)));

#endif
