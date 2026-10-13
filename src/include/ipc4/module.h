/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/**
 * \file include/ipc4/module.h
 * \brief IPC4 module instantiation, binding, pin configuration, and parameter definitions.
 * \ingroup ipc4_module_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __IPC4_MODULE_H__
#define __IPC4_MODULE_H__

#include <ipc4/error_status.h>

#include <stdint.h>

/** Maximum number of dynamically loadable LLEXT library containers. */
#define IPC4_MAX_SUPPORTED_LIBRARIES 16

/** Maximum distinct module class types registered in the FW image manifest. */
#define IPC4_MAX_MODULE_COUNT 128

/** Number of bits allocated for destination queue/pin ID in bind commands (3 bits, supporting pins 0..7). */
#define SOF_IPC4_DST_QUEUE_ID_BITFIELD_SIZE	3
/** Number of bits allocated for source queue/pin ID in bind commands (3 bits, supporting pins 0..7). */
#define SOF_IPC4_SRC_QUEUE_ID_BITFIELD_SIZE	3

/** Special parameter ID (0xFF) designating vendor-specific large configuration payloads. */
#define VENDOR_CONFIG_PARAM 0xFF

/**
 * \brief Module message command opcodes (5-bit opcode in Primary DWORD when msg_tgt == MODULE_MSG).
 * \ingroup ipc4_module_api
 */
enum sof_ipc4_module_type {
	SOF_IPC4_MOD_INIT_INSTANCE		= 0,  /**< Instantiate a new module object and attach to a pipeline. */
	SOF_IPC4_MOD_CONFIG_GET			= 1,  /**< Retrieve a short 16-bit module parameter packed in header. */
	SOF_IPC4_MOD_CONFIG_SET			= 2,  /**< Set a short 16-bit module parameter packed in header. */
	SOF_IPC4_MOD_LARGE_CONFIG_GET		= 3,  /**< Retrieve multi-block configuration or state data from module. */
	SOF_IPC4_MOD_LARGE_CONFIG_SET		= 4,  /**< Send multi-block configuration or filter coefficients to module. */
	SOF_IPC4_MOD_BIND			= 5,  /**< Connect a source pin of this module to a destination pin. */
	SOF_IPC4_MOD_UNBIND			= 6,  /**< Disconnect a runtime binding between two module instances. */
	SOF_IPC4_MOD_SET_DX			= 7,  /**< Request core power state transition (D0/D3). */
	SOF_IPC4_MOD_SET_D0IX			= 8,  /**< Request Base FW or module to enter low-power D0ix state. */
	SOF_IPC4_MOD_ENTER_MODULE_RESTORE	= 9,  /**< Notify module to prepare for context restoration after wake. */
	SOF_IPC4_MOD_EXIT_MODULE_RESTORE	= 10, /**< Signal completion of module state restoration. */
	SOF_IPC4_MOD_DELETE_INSTANCE		= 11, /**< Destroy module instance and release allocated memory. */
};

/**
 * \brief Compound parameter identifier encoding type and instance index.
 * \ingroup ipc4_module_api
 */
union ipc4_extended_param_id {
	uint32_t full; /**< Full 32-bit parameter identifier. */
	struct {
		uint32_t parameter_type     : 8;  /**< Parameter type identifier. */
		uint32_t parameter_instance : 24; /**< Parameter instance index. */
	} part;
} __attribute__((packed, aligned(4)));

/**
 * \brief Vendor error reporting payload structure.
 * \ingroup ipc4_module_api
 */
struct ipc4_vendor_error {
	uint32_t param_idx; /**< Index of the failed parameter in vendor configuration list. */
	uint32_t err_code;  /**< Vendor-specific or hardware error code. */
};

/**
 * \brief Global object type identifiers for extended initialization payloads.
 * \ingroup ipc4_module_api
 */
enum ipc4_mod_init_data_glb_id {
	IPC4_MOD_INIT_DATA_ID_INVALID = 0,     /**< Invalid initialization object identifier. */
	IPC4_MOD_INIT_DATA_ID_DP_DATA = 1,     /**< Data Processing (DP) thread stack and heap requirements. */
	IPC4_MOD_INIT_DATA_ID_MODULE_DATA = 2, /**< Module-specific bespoke initialization data. */
	IPC4_MOD_INIT_DATA_ID_MAX = IPC4_MOD_INIT_DATA_ID_MODULE_DATA, /**< Maximum supported object identifier. */
};

/**
 * \brief Header for extended initialization data objects.
 * \ingroup ipc4_module_api
 */
struct ipc4_module_init_ext_object {
	uint32_t last_object : 1;	/**< Set to 1 if this is the terminating object in the array. */
	uint32_t object_id : 15;	/**< Unique object identifier (one of enum ipc4_mod_init_data_glb_id). */
	uint32_t object_words : 16;	/**< Payload size in 32-bit DWORDs following this header. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Memory requirements for Data Processing (DP) module threads.
 * \ingroup ipc4_module_api
 */
struct ipc4_module_init_ext_obj_dp_data {
	uint32_t domain_id;		/**< Userspace domain identifier. */
	uint32_t stack_bytes;		/**< Required thread stack size in bytes. */
	uint32_t heap_bytes;		/**< Required thread heap allocation size in bytes. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Extended initialization flags and coprocessor requirements.
 * \ingroup ipc4_module_api
 */
struct ipc4_module_init_ext_init {
	/**< If set to 1, proc_domain is ignored and processing domain is RTOS scheduling. */
	uint32_t rtos_domain : 1;
	/**< Indicates that GNA coprocessor is used by module and gna_config follows. */
	uint32_t gna_used    : 1;
	uint32_t data_obj_array : 1;	/**< 1 if followed by struct ipc4_module_init_ext_object array. */
	uint32_t rsvd_0      : 29;	/**< Reserved bits. */
	uint32_t rsvd_1[2];		/**< Reserved padding words. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Extended module initialization container.
 * \ingroup ipc4_module_api
 */
struct ipc4_module_init_ext_data {
	struct ipc4_module_init_ext_init extended_init; /**< Extended flags. */
	uint32_t param_data[]; /**< Module configuration payload words. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Neural network accelerator (GNA) compute cycle configuration.
 * \ingroup ipc4_module_api
 */
struct ipc4_module_init_gna_config {
	/**< Number of GNA cycles required to process one input frame. */
	uint32_t gna_cpc;
	uint32_t rsvd; /**< Reserved padding. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Module initialization payload buffer.
 * \ingroup ipc4_module_api
 */
struct ipc4_module_init_data {
	uint32_t param_data[0]; /**< Configuration payload words (sized by param_block_size). */
} __attribute__((packed, aligned(4)));

/** Reserved pipeline ID (0xFF) indicating module has no parent pipeline (e.g. probe). */
#define IPC4_INVALID_PIPELINE_ID 0xFF

/**
 * \brief Module instance creation command structure (SOF_IPC4_MOD_INIT_INSTANCE).
 * \ingroup ipc4_module_api
 */
struct ipc4_module_init_instance {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			/**< Module class ID index from the FW manifest. */
			uint32_t module_id          : 16;
			/**< Module instance ID unique within the module class (0..max_instances-1). */
			uint32_t instance_id        : 8;
			/**< Opcode: SOF_IPC4_MOD_INIT_INSTANCE (0). */
			uint32_t type               : 5;
			/**< Direction: 0 for request (SOF_IPC4_MESSAGE_DIR_MSG_REQUEST). */
			uint32_t rsp                : 1;
			/**< Target: 1 for module message (SOF_IPC4_MESSAGE_TARGET_MODULE_MSG). */
			uint32_t msg_tgt            : 1;
			uint32_t _reserved_0        : 1; /**< Reserved hardware control bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			/**< Size of initialization payload in 32-bit DWORDs. */
			uint32_t param_block_size   : 16;
			/**< ID of parent pipeline, or #IPC4_INVALID_PIPELINE_ID. */
			uint32_t ppl_instance_id    : 8;
			/**< Target DSP core ID on which this instance will execute (0..3). */
			uint32_t core_id            : 4;
			/**< Processing domain (0 = Low-Latency LL, 1 = Data Processing DP). */
			uint32_t proc_domain        : 1;
			/**< 1 if extended initialization structures follow. */
			uint32_t extended_init      : 1;
			uint32_t _hw_reserved_2     : 2; /**< Reserved hardware bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Module pin binding and unbinding command structure (SOF_IPC4_MOD_BIND / UNBIND).
 * \ingroup ipc4_module_api
 *
 * Sent by host to connect two module instances across pins (queues), or sever an existing link.
 */
struct ipc4_module_bind_unbind {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			/**< Source module class identifier. */
			uint32_t module_id : 16;
			/**< Source module instance identifier. */
			uint32_t instance_id : 8;
			/**< Opcode: SOF_IPC4_MOD_BIND (5) or SOF_IPC4_MOD_UNBIND (6). */
			uint32_t type : 5;
			/**< Direction: 0 for request (SOF_IPC4_MESSAGE_DIR_MSG_REQUEST). */
			uint32_t rsp : 1;
			/**< Target: 1 for module message (SOF_IPC4_MESSAGE_TARGET_MODULE_MSG). */
			uint32_t msg_tgt : 1;
			uint32_t _reserved_0 : 1; /**< Reserved hardware control bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			/**< Destination module class identifier. */
			uint32_t dst_module_id : 16;
			/**< Destination module instance identifier. */
			uint32_t dst_instance_id : 8;
			/**< Destination input queue/pin index (0..7). */
			uint32_t dst_queue : SOF_IPC4_DST_QUEUE_ID_BITFIELD_SIZE;
			/**< Source output queue/pin index (0..7). */
			uint32_t src_queue : SOF_IPC4_SRC_QUEUE_ID_BITFIELD_SIZE;
			uint32_t _reserved_2 : 2; /**< Reserved hardware control bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Small parameter configuration command structure (SOF_IPC4_MOD_CONFIG_GET / SET).
 * \ingroup ipc4_module_api
 */
struct ipc4_module_config {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t module_id      : 16;	/**< Target module class ID. */
			uint32_t instance_id    : 8;	/**< Target module instance ID. */
			uint32_t type           : 5;	/**< Opcode: SOF_IPC4_MOD_CONFIG_GET or SET. */
			uint32_t rsp            : 1;	/**< 0 for request. */
			uint32_t msg_tgt        : 1;	/**< 1 for module message. */
			uint32_t _reserved_0    : 1;	/**< Reserved bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t param_id_data  : 30;	/**< Packed 16-bit parameter data and 14-bit parameter ID. */
			uint32_t _reserved_2    : 2;	/**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Reply structure for small parameter query (SOF_IPC4_MOD_CONFIG_GET).
 * \ingroup ipc4_module_api
 */
struct ipc4_module_config_reply {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t status		: IPC4_IXC_STATUS_BITS;	/**< Command processing status (enum ipc4_status). */
			uint32_t type		: 5;			/**< Opcode: SOF_IPC4_MOD_CONFIG_GET. */
			uint32_t rsp		: 1;			/**< 1 for reply. */
			uint32_t msg_tgt	: 1;			/**< 1 for module message. */
			uint32_t _reserved_0	: 1;			/**< Reserved bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t param_id_data	: 30;	/**< Returned parameter value if fitting in 30 bits. */
			uint32_t _reserved_2	: 2;	/**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Large configuration parameter transfer envelope (SOF_IPC4_MOD_LARGE_CONFIG_GET / SET).
 * \ingroup ipc4_module_api
 *
 * Implements the multi-block fragmentation handshake for exchanging configuration payloads
 * exceeding the mailbox window size (e.g. FIR coefficients, EQ curves, model blobs).
 */
struct ipc4_module_large_config {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t module_id : 16;   /**< Target module class ID. */
			uint32_t instance_id : 8;  /**< Target module instance ID. */
			uint32_t type : 5;         /**< LARGE_CONFIG_GET (3) or LARGE_CONFIG_SET (4). */
			uint32_t rsp : 1;          /**< 0 for request, 1 for reply. */
			uint32_t msg_tgt : 1;      /**< 1 for module target. */
			uint32_t _reserved_0 : 1;  /**< Reserved bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			/**< Chunk byte size (single block) or cumulative byte offset (multi-block). */
			uint32_t data_off_size : 20;
			/**< Parameter type identifier (module-specific or #VENDOR_CONFIG_PARAM). */
			uint32_t large_param_id : 8;
			/**< Set to 1 on the terminating chunk of a multi-block transfer. */
			uint32_t final_block : 1;
			/**< Set to 1 on the initial chunk of a multi-block transfer. */
			uint32_t init_block : 1;
			uint32_t _reserved_2 : 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Reply structure for large configuration transfer.
 * \ingroup ipc4_module_api
 */
struct ipc4_module_large_config_reply {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t status : IPC4_IXC_STATUS_BITS; /**< Processing status (enum ipc4_status). */
			uint32_t type : 5;         /**< Symmetric opcode. */
			uint32_t rsp : 1;          /**< 1 for reply. */
			uint32_t msg_tgt : 1;      /**< Symmetric target. */
			uint32_t _reserved_0 : 1;  /**< Reserved bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t data_off_size : 20;  /**< Processed byte size or offset. */
			uint32_t large_param_id : 8;  /**< Parameter type ID. */
			uint32_t final_block : 1;     /**< 1 if final block processed. */
			uint32_t init_block : 1;      /**< 1 if first block processed. */
			uint32_t _reserved_2 : 2;     /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Module instance deletion command structure (SOF_IPC4_MOD_DELETE_INSTANCE).
 * \ingroup ipc4_module_api
 */
struct ipc4_module_delete_instance {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t module_id : 16;   /**< Module class ID of instance to destroy. */
			uint32_t instance_id : 8;  /**< Instance ID to destroy. */
			uint32_t type : 5;         /**< Opcode: SOF_IPC4_MOD_DELETE_INSTANCE (11). */
			uint32_t rsp : 1;          /**< 0 for request. */
			uint32_t msg_tgt : 1;      /**< 1 for module target. */
			uint32_t _reserved_0 : 1;  /**< Reserved bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t rsvd : 30;        /**< Reserved padding. */
			uint32_t _reserved_1 : 2;   /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Module D0ix low-power state entry command (SOF_IPC4_MOD_SET_D0IX).
 * \ingroup ipc4_module_api
 */
struct ipc4_module_set_d0ix {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t module_id		: 16; /**< Module ID (must be 0 for Base FW). */
			uint32_t instance_id	: 8;  /**< Instance ID (must be 0 for core 0). */
			uint32_t type			: 5;  /**< Opcode: SOF_IPC4_MOD_SET_D0IX (8). */
			uint32_t rsp			: 1;  /**< 0 for request. */
			uint32_t msg_tgt		: 1;  /**< 1 for module message. */
			uint32_t _reserved_0	: 1;  /**< Reserved bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t wake			: 1;  /**< Legacy wake type flag. */
			uint32_t streaming		: 1;  /**< Active audio streaming indicator. */
			uint32_t prevent_power_gating : 1; /**< Set to 1 to inhibit DSP core power gating. */
			uint32_t prevent_local_clock_gating : 1; /**< Set to 1 to inhibit local clock gating. */
			uint32_t rsvd1			: 26; /**< Reserved padding. */
			uint32_t _reserved_2	: 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief DSP core power state bitmask descriptor.
 * \ingroup ipc4_module_api
 */
struct ipc4_dx_state_info {
	uint32_t core_mask; /**< Bitmask indicating which DSP cores are subject to state change. */
	/**< Target state bitmask: bit[core_id] = 0 -> put to D3, bit[core_id] = 1 -> put to D0. */
	uint32_t dx_mask;
} __attribute__((packed, aligned(4)));

/**
 * \brief Core DX power state transition command structure (SOF_IPC4_MOD_SET_DX).
 * \ingroup ipc4_module_api
 */
struct ipc4_module_set_dx {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t module_id			: 16; /**< Module ID (0 for Base FW). */
			uint32_t instance_id		: 8;  /**< Instance ID (0 for Core 0). */
			uint32_t type				: 5;  /**< Opcode: SOF_IPC4_MOD_SET_DX (7). */
			uint32_t rsp				: 1;  /**< 0 for request. */
			uint32_t msg_tgt			: 1;  /**< 1 for module message. */
			uint32_t _reserved_0		: 1;  /**< Reserved bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t rsvd				: 30; /**< Reserved padding. */
			uint32_t _reserved_2		: 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Dynamic library loading command structure (SOF_IPC4_GLB_LOAD_LIBRARY).
 * \ingroup ipc4_module_api
 */
struct ipc4_module_load_library {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t dma_id				: 5;  /**< ID of Host Output DMA used to load code. */
			uint32_t rsvd0				: 11; /**< Reserved padding. */
			uint32_t lib_id				: 4;  /**< Dynamic library container ID (0..15). */
			uint32_t rsvd1				: 4;  /**< Reserved padding. */
			uint32_t type				: 5;  /**< Opcode: SOF_IPC4_GLB_LOAD_LIBRARY (24). */
			uint32_t rsp				: 1;  /**< 0 for request. */
			uint32_t msg_tgt			: 1;  /**< 0 for global firmware message. */
			uint32_t _reserved_0		: 1;  /**< Reserved bit. */
		} r;
	} header;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t load_offset		: 30; /**< Load offset into host DMA buffer. */
			uint32_t _reserved_2		: 2;  /**< Reserved bits. */
		} r;
	} data;
} __attribute__((packed, aligned(4)));

/** Construct a 32-bit component token from module class ID (x) and instance ID (y). */
#define IPC4_COMP_ID(x, y)	((y) << 16 | (x))
/** Extract 16-bit module class ID from a 32-bit component token. */
#define IPC4_MOD_ID(x)	(IS_ENABLED(CONFIG_IPC_MAJOR_4) ? ((x) & 0xffff) : 0)
/** Extract 16-bit instance ID from a 32-bit component token. */
#define IPC4_INST_ID(x)	((x) >> 16)
/** Extract 16-bit source queue/pin ID from a 32-bit queue token. */
#define IPC4_SRC_QUEUE_ID(x)	((x) & 0xffff)
/** Extract 16-bit sink queue/pin ID from a 32-bit queue token. */
#define IPC4_SINK_QUEUE_ID(x)	(((x) >> 16) & 0xffff)

#endif
