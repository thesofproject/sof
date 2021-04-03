/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/ipc/control.h
 * \brief IPC control commands
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __IPC_CONTROL_H__
#define __IPC_CONTROL_H__

#include <ipc/header.h>
#include <ipc/stream.h>
#include <kernel/header.h>
#include <stdint.h>

/** \addtogroup sof_uapi_control uAPI Control
 *  SOF uAPI specification - component controls.
 *  @{
 */

/** \name Component Mixers and Controls
 *  @{
 */

/** Channel positions - uses same values as ALSA */
enum sof_ipc_chmap {
	SOF_CHMAP_UNKNOWN = 0,
	SOF_CHMAP_NA,		/**< N/A, silent */
	SOF_CHMAP_MONO,		/**< mono stream */
	SOF_CHMAP_FL,		/**< front left */
	SOF_CHMAP_FR,		/**< front right */
	SOF_CHMAP_RL,		/**< rear left */
	SOF_CHMAP_RR,		/**< rear right */
	SOF_CHMAP_FC,		/**< front centre */
	SOF_CHMAP_LFE,		/**< LFE */
	SOF_CHMAP_SL,		/**< side left */
	SOF_CHMAP_SR,		/**< side right */
	SOF_CHMAP_RC,		/**< rear centre */
	SOF_CHMAP_FLC,		/**< front left centre */
	SOF_CHMAP_FRC,		/**< front right centre */
	SOF_CHMAP_RLC,		/**< rear left centre */
	SOF_CHMAP_RRC,		/**< rear right centre */
	SOF_CHMAP_FLW,		/**< front left wide */
	SOF_CHMAP_FRW,		/**< front right wide */
	SOF_CHMAP_FLH,		/**< front left high */
	SOF_CHMAP_FCH,		/**< front centre high */
	SOF_CHMAP_FRH,		/**< front right high */
	SOF_CHMAP_TC,		/**< top centre */
	SOF_CHMAP_TFL,		/**< top front left */
	SOF_CHMAP_TFR,		/**< top front right */
	SOF_CHMAP_TFC,		/**< top front centre */
	SOF_CHMAP_TRL,		/**< top rear left */
	SOF_CHMAP_TRR,		/**< top rear right */
	SOF_CHMAP_TRC,		/**< top rear centre */
	SOF_CHMAP_TFLC,		/**< top front left centre */
	SOF_CHMAP_TFRC,		/**< top front right centre */
	SOF_CHMAP_TSL,		/**< top side left */
	SOF_CHMAP_TSR,		/**< top side right */
	SOF_CHMAP_LLFE,		/**< left LFE */
	SOF_CHMAP_RLFE,		/**< right LFE */
	SOF_CHMAP_BC,		/**< bottom centre */
	SOF_CHMAP_BLC,		/**< bottom left centre */
	SOF_CHMAP_BRC,		/**< bottom right centre */
	SOF_CHMAP_LAST = SOF_CHMAP_BRC,
};

/**
 * Control data type and direction.
 * SOF_CTRL_TYPE_VALUE_CHAN uses struct sof_ipc_ctrl_value_chan.
 * SOF_CTRL_TYPE_VALUE_COMP uses struct sof_ipc_ctrl_value_comp.
 * SOF_CTRL_TYPE_DATA_GET uses sof_abi_hdr.
 */
enum sof_ipc_ctrl_type {
	SOF_CTRL_TYPE_VALUE_CHAN_GET = 0,
	SOF_CTRL_TYPE_VALUE_CHAN_SET,

	SOF_CTRL_TYPE_VALUE_COMP_GET,
	SOF_CTRL_TYPE_VALUE_COMP_SET,

	SOF_CTRL_TYPE_DATA_GET,
	SOF_CTRL_TYPE_DATA_SET,
};

/** Control command type. */
enum sof_ipc_ctrl_cmd {
	SOF_CTRL_CMD_VOLUME = 0, /**< maps to ALSA volume style controls */
	SOF_CTRL_CMD_ENUM,	/**< maps to ALSA enum style controls */
	SOF_CTRL_CMD_SWITCH,	/**< maps to ALSA switch style controls */
	SOF_CTRL_CMD_BINARY,	/**< maps to ALSA binary style controls */
};

/** Generic channel mapped value data. */
struct sof_ipc_ctrl_value_chan {
	uint32_t channel;	/**< channel map - enum sof_ipc_chmap */
	uint32_t value;
} __attribute__((packed, aligned(4)));

/**
 * Generic component mapped value data.
 */
struct sof_ipc_ctrl_value_comp {
	uint32_t index;	/**< component source/sink/control index in control */
	union {
		uint32_t uvalue;
		int32_t svalue;
	};
} __attribute__((packed, aligned(4)));

/**
 * Generic control data.
 */
struct sof_ipc_ctrl_data {
	struct sof_ipc_reply rhdr;
	uint32_t comp_id;

	/* control access and data type */
	uint32_t type;		/**< enum sof_ipc_ctrl_type */
	uint32_t cmd;		/**< enum sof_ipc_ctrl_cmd */
	uint32_t index;		/**< control index for comps > 1 control */

	/* control data - can either be appended or DMAed from host */
	struct sof_ipc_host_buffer buffer;
	uint32_t num_elems;	/**< in array elems or bytes for data type */
	uint32_t elems_remaining;	/**< elems remaining if sent in parts */

	uint32_t msg_index;	/**< index for large messages sent in parts */

	/* reserved for future use */
	uint32_t reserved[6];

	/* control data - add new types if needed */
	union {
		/* channel values can be used by volume type controls */
		struct sof_ipc_ctrl_value_chan chanv[0];
		/* component values used by routing controls like mux, mixer */
		struct sof_ipc_ctrl_value_comp compv[0];
		/* data can be used by binary controls */
		struct sof_abi_hdr data[0];
	};
} __attribute__((packed, aligned(4)));

/** Event type */
enum sof_ipc_ctrl_event_type {
	SOF_CTRL_EVENT_GENERIC = 0,	/**< generic event */
	SOF_CTRL_EVENT_GENERIC_METADATA,	/**< generic event with metadata */
	SOF_CTRL_EVENT_KD,	/**< keyword detection event */
	SOF_CTRL_EVENT_VAD,	/**< voice activity detection event */
};

/**
 * Generic notification data.
 */
struct sof_ipc_comp_event {
	struct sof_ipc_reply rhdr;
	uint16_t src_comp_type;	/**< COMP_TYPE_ */
	uint32_t src_comp_id;	/**< source component id */
	uint32_t event_type;	/**< event type - SOF_CTRL_EVENT_* */
	uint32_t num_elems;	/**< in array elems or bytes for data type */

	/* reserved for future use */
	uint32_t reserved[8];

	/* control data - add new types if needed */
	union {
		/* data can be used by binary controls */
		struct sof_abi_hdr data[0];
		/* event specific values */
		uint32_t event_value;
	};
} __attribute__((packed, aligned(4)));

/** @}*/

/** @}*/

#endif /* __IPC_CONTROL_H__ */
