/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/uapi/ipc/control.h
 * \brief IPC control commands
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_IPC_CONTROL_H__
#define __INCLUDE_UAPI_IPC_CONTROL_H__

#include <uapi/user/header.h>
#include <uapi/ipc/header.h>

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
} __attribute__((packed));

/**
 * Generic component mapped value data.
 */
struct sof_ipc_ctrl_value_comp {
	uint32_t index;	/**< component source/sink/control index in control */
	union {
		uint32_t uvalue;
		int32_t svalue;
	};
} __attribute__((packed));

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
} __attribute__((packed));

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
} __attribute__((packed));

/** @}*/

/** @}*/

#endif
