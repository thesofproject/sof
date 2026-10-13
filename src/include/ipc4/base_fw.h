/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 */

/**
 * \file include/ipc4/base_fw.h
 * \brief IPC4 Base Firmware parameters, memory configuration, and status definitions.
 * \ingroup ipc4_base_fw_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_BASE_FW_H__
#define __SOF_IPC4_BASE_FW_H__

/**
 * \brief Clock states count.
 *
 * SOF FW performs autonomous management of clock sources.
 * Set MAX_CLK_STATES to 0 in order to propagate this information through IPC and thus
 * prevent reception of unsupported clock configuration.
 */
#define IPC4_MAX_CLK_STATES 0

/** \brief Maximum source queue count supported by IPC4. */
#define IPC4_MAX_SRC_QUEUE 8

/** \brief Maximum destination queue count supported by IPC4. */
#define IPC4_MAX_DST_QUEUE 8

/** \brief Maximum module instance count for a single module supported by IPC4. */
#define IPC4_MAX_MODULE_INSTANCES 256

/** \brief Maximum low-latency tasks for a schedule priority count supported by IPC4. */
#define IPC4_MAX_LL_TASKS_PER_PRI_COUNT 16

/** \brief Maximum data-processing tasks count supported by IPC4. */
#define IPC4_MAX_DP_TASKS_COUNT 16

/** \brief Maximum external libraries count supported by IPC4. */
#define IPC4_MAX_LIBS_COUNT 16

/** \brief Maximum pipeline count supported by IPC4. */
#define IPC4_MAX_PPL_COUNT 32

/**
 * \brief IPC4 Base Firmware configuration and query parameter IDs.
 * \ingroup ipc4_base_fw_api
 *
 * Parameter IDs used with LARGE_CONFIG_GET and LARGE_CONFIG_SET commands
 * addressed to the Base FW module instance (module ID 0, instance ID 0).
 */
enum ipc4_basefw_params {
	/**
	 * Retrieve firmware properties as TLV structure with typeof AdspProperties
	 * via LARGE_CONFIG_GET.
	 */
	IPC4_DSP_PROPERTIES = 0,

	/** Retrieve DSP resource state via LARGE_CONFIG_GET. */
	IPC4_DSP_RESOURCE_STATE = 1,

	/** Reserved parameter identifier. */
	IPC4_RESERVED = 2,

	/**
	 * Enable or disable asynchronous notifications. Used in debug mode
	 * to prevent flooding the host with underrun notifications during breakpoints.
	 */
	IPC4_NOTIFICATION_MASK = 3,

	/**
	 * Configure available power states according to the underlying platform.
	 * Driver sends A-State table data right after Base FW is up.
	 */
	IPC4_ASTATE_TABLE = 4,

	/**
	 * Initialize or modify DMA gateway configuration outside stream lifetime.
	 * Starts with struct ipc4_dma_control followed by optional gateway-specific data.
	 */
	IPC4_DMA_CONTROL = 5,

	/**
	 * Configure firmware logging state (enable/disable per core, log level,
	 * aging timer, FIFO full timer).
	 */
	IPC4_ENABLE_LOGS = 6,

	/**
	 * Discover or set firmware configuration in TLV format (read/write
	 * via LARGE_CONFIG_GET / LARGE_CONFIG_SET).
	 */
	IPC4_FW_CONFIG = 7,

	/**
	 * Discover underlying hardware configuration in TLV format
	 * via LARGE_CONFIG_GET.
	 */
	IPC4_HW_CONFIG_GET = 8,

	/**
	 * Retrieve list of module entries loaded into firmware memory space
	 * (image manifest or library manifest) via LARGE_CONFIG_GET.
	 */
	IPC4_MODULES_INFO_GET = 9,

	/** Retrieve list of instantiated pipeline IDs via LARGE_CONFIG_GET. */
	IPC4_PIPELINE_LIST_INFO_GET = 10,

	/**
	 * Retrieve pipeline properties for a given pipeline ID
	 * via LARGE_CONFIG_GET.
	 */
	IPC4_PIPELINE_PROPS_GET = 11,

	/**
	 * Retrieve list of task schedulers and tasks running on a core
	 * via LARGE_CONFIG_GET.
	 */
	IPC4_SCHEDULERS_INFO_GET = 12,

	/** Retrieve gateway configuration information via LARGE_CONFIG_GET. */
	IPC4_GATEWAYS_INFO_GET = 13,

	/** Retrieve information about current DSP memory state via LARGE_CONFIG_GET. */
	IPC4_MEMORY_STATE_INFO_GET = 14,

	/** Retrieve information about current DSP power state via LARGE_CONFIG_GET. */
	IPC4_POWER_STATE_INFO_GET = 15,

	/** Retrieve information about external libraries loaded into DSP memory. */
	IPC4_LIBRARIES_INFO_GET = 16,

	/** Control global performance measurement state (MCPS monitoring). */
	IPC4_PERF_MEASUREMENTS_STATE = 17,

	/** Retrieve global performance data (KCPS sampled per tick/frame). */
	IPC4_GLOBAL_PERF_DATA = 18,

	/** Retrieve L2 cache state information. */
	IPC4_L2_CACHE_INFO_GET = 19,

	/**
	 * Pass down host system time in microseconds (UTC, epoch 1601-01-01T00:00:00Z)
	 * to synchronize log and trace timestamps.
	 */
	IPC4_SYSTEM_TIME = 20,

	/** Configure firmware for performance mode via LARGE_CONFIG_SET. */
	IPC4_PERFORMANCE_CONFIGURATION = 21,

	/** Register KCPS into power manager service per core 0 (negative values allowed). */
	IPC4_REGISTER_KCPS = 22,

	/** Request additional resource allocation via LARGE_CONFIG_SET. */
	IPC4_RESOURCE_ALLOCATION_REQUEST = 23,

	/** Control I/O performance measurement state in firmware. */
	IPC4_IO_PERF_MEASUREMENTS_STATE  = 24,

	/** Retrieve I/O global performance statistics. */
	IPC4_IO_GLOBAL_PERF_DATA         = 25,

	/** Retrieve module ID for a specified module UUID. */
	IPC4_GET_MODULE_ID               = 26,

	/** Retrieve current UTC, RTC, and ART timestamps (requires SYSTEM_TIME set first). */
	IPC4_EXTENDED_SYSTEM_TIME        = 27,

	/** Control telemetry collection process state and thresholds. */
	IPC4_TELEMETRY_STATE             = 28,

	/** Read data from circular telemetry buffer. */
	IPC4_TELEMETRY_DATA              = 29,

	/** Extended global performance data with per-module DSP cycle statistics. */
	IPC4_EXTENDED_GLOBAL_PERF_DATA   = 30,

	/** Configure SoundWire controller ownership via LARGE_CONFIG_SET. */
	IPC4_SDW_OWNERSHIP               = 31,

	/** Notify firmware of hardware-managed microphone privacy state change. */
	IPC4_MIC_PRIVACY_HW_MANAGED_STATE_CHANGE = 35,

	/** Set policy mask for microphone privacy in firmware-managed mode. */
	IPC4_SET_MIC_PRIVACY_FW_MANAGED_POLICY_MASK = 36,
};

/**
 * \brief Firmware configuration parameter TLV IDs returned by IPC4_FW_CONFIG.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_fw_config_params {
	/** Firmware version. */
	IPC4_FW_VERSION_FW_CFG                  = 0,
	/** Indicates whether legacy DMA memory is managed by FW. */
	IPC4_MEMORY_RECLAIMED_FW_CFG            = 1,
	/** Frequency of oscillator clock in Hz. */
	IPC4_SLOW_CLOCK_FREQ_HZ_FW_CFG          = 2,
	/** Frequency of PLL clock in Hz. */
	IPC4_FAST_CLOCK_FREQ_HZ_FW_CFG          = 3,
	/**
	 * List of static and dynamic DMA buffer sizes.
	 * Host software may configure minimum and maximum size for each buffer.
	 */
	IPC4_DMA_BUFFER_CONFIG_FW_CFG           = 4,
	/**
	 * Audio Hub Link (ALH) support level.
	 * Lower 16 bits may indicate implementation revision.
	 */
	IPC4_ALH_SUPPORT_LEVEL_FW_CFG           = 5,
	/** Size of IPC downlink mailbox in bytes. */
	IPC4_DL_MAILBOX_BYTES_FW_CFG            = 6,
	/** Size of IPC uplink mailbox in bytes. */
	IPC4_UL_MAILBOX_BYTES_FW_CFG            = 7,
	/** Size of trace log buffer in bytes. */
	IPC4_TRACE_LOG_BYTES_FW_CFG             = 8,
	/** Maximum number of pipelines that may be instantiated simultaneously. */
	IPC4_MAX_PPL_CNT_FW_CFG                 = 9,
	/**
	 * Maximum number of A-state table entries that may be configured by the driver.
	 * Driver may use this value to estimate the size of ASTATE_TABLE data.
	 */
	IPC4_MAX_ASTATE_COUNT_FW_CFG            = 10,
	/** Maximum number of input or output pins supported by a module. */
	IPC4_MAX_MODULE_PIN_COUNT_FW_CFG        = 11,
	/** Current total number of module entries loaded into the DSP. */
	IPC4_MODULES_COUNT_FW_CFG               = 12,
	/** Maximum number of module instances supported by FW. */
	IPC4_MAX_MOD_INST_COUNT_FW_CFG          = 13,
	/**
	 * Maximum number of LL tasks that may be allocated with
	 * the same priority (specified by parent pipeline priority).
	 */
	IPC4_MAX_LL_TASKS_PER_PRI_COUNT_FW_CFG  = 14,
	/** Number of Low Latency (LL) scheduling priorities. */
	IPC4_LL_PRI_COUNT                       = 15,
	/** Maximum number of DP tasks that may be allocated on a single core. */
	IPC4_MAX_DP_TASKS_COUNT_FW_CFG          = 16,
	/** Maximum number of external libraries that can be loaded into ADSP memory. */
	IPC4_MAX_LIBS_COUNT_FW_CFG              = 17,
	/** Configuration of system tick source and period. */
	IPC4_SCHEDULER_CONFIGURATION            = 18,
	/** Frequency of XTAL oscillator clock in Hz. */
	IPC4_XTAL_FREQ_HZ_FW_CFG                = 19,
	/** Configuration of clocks. */
	IPC4_CLOCKS_CONFIGURATION               = 20,
	/** USB Audio Offload (UAOL) support indicator. */
	IPC4_UAOL_SUPPORT                       = 21,
	/** Configuration of Dynamic Power Gating Policy. */
	IPC4_POWER_GATING_POLICY                = 22,
	/**
	 * Configuration of assert mode.
	 * Asserts use format "%passert", ptr_to_assert_desc in .asserts_desc section.
	 */
	IPC4_ASSERT_MODE                        = 23,
	/** Size of telemetry circular buffer in bytes (default 4KB). */
	IPC4_TELEMETRY_BUFFER_SIZE              = 24,
	/** Hardware version and bus hardware identifier. */
	IPC4_BUS_HARDWARE_ID                    = 25,
	/** Total memory size available in debug window. */
	IPC4_DEBUG_WINDOW_SIZE                  = 26,
	/** Number of real-time Key Phrase Buffer (KPB) output sink pins. */
	IPC4_KPB_RT_SINK_COUNT                  = 27,
	/** Manual control of DMI L1 state exit. */
	IPC4_DMI_FORCE_L1_EXIT                  = 28,
	/** Firmware context save on D3 power state entry. */
	IPC4_FW_CONTEXT_SAVE                    = 29,
	/** Minimum size of host buffer in milliseconds. */
	IPC4_FW_MIN_HOST_BUFFER_PERIODS         = 33,
	/** SOF specific decoder/encoder codec information. */
	IPC4_FW_SOF_INFO                        = 35,
	/** Total number of FW config parameters. */
	IPC4_FW_CFG_PARAMS_COUNT,
	/** Maximum FW config parameter ID value. */
	IPC4_MAX_FW_CFG_PARAM = IPC4_FW_CFG_PARAMS_COUNT - 1,
};

/**
 * \brief Tuple-based parameters for SOF specific info under IPC4_FW_SOF_INFO.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_fw_sof_info_params {
	/** Decoder/encoder codec information. */
	IPC4_SOF_CODEC_INFO = 0,

	/** Total number of SOF config parameters. */
	IPC4_SOF_CFG_PARAMS_COUNT,
	/** Maximum SOF config parameter ID value. */
	IPC4_MAX_SOF_CFG_PARAM = IPC4_SOF_CFG_PARAMS_COUNT - 1,
};

/**
 * \brief Hardware configuration parameter TLV IDs returned by IPC4_HW_CONFIG_GET.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_hw_config_params {
	/** Version of CAVS/ACE architecture implemented by FW (from ROMInfo). */
	IPC4_CAVS_VER_HW_CFG             = 0,
	/** Number of DSP cores available in the audio subsystem. */
	IPC4_DSP_CORES_HW_CFG            = 1,
	/** Size of a single memory page in bytes (typically 4096). */
	IPC4_MEM_PAGE_BYTES_HW_CFG       = 2,
	/** Total number of physical memory pages available for allocation. */
	IPC4_TOTAL_PHYS_MEM_PAGES_HW_CFG = 3,
	/**
	 * I2S controller capabilities. Array count given by controller_count.
	 * Lower 16 bits may indicate implementation revision.
	 */
	IPC4_I2S_CAPS_HW_CFG             = 4,
	/** General Purpose DMA (GPDMA) controller capabilities. */
	IPC4_GPDMA_CAPS_HW_CFG           = 5,
	/** Total number of DMA gateways of all types. */
	IPC4_GATEWAY_COUNT_HW_CFG        = 6,
	/** Number of High Power (HP) SRAM memory banks (EBBs) manageable by DSP. */
	IPC4_HP_EBB_COUNT_HW_CFG         = 7,
	/** Number of Low Power (LP) SRAM memory banks (EBBs) manageable by DSP. */
	IPC4_LP_EBB_COUNT_HW_CFG         = 8,
	/** Size of a single memory bank (EBB) in bytes. */
	IPC4_EBB_SIZE_BYTES_HW_CFG       = 9,
	/** USB Audio Offload (UAOL) controller capabilities. */
	IPC4_UAOL_CAPS_HW_CFG            = 10,
	/** Microphone privacy hardware capabilities. */
	IPC4_INTEL_MIC_PRIVACY_CAPS_HW_CFG = 11
};

/**
 * \brief SRAM power memory type.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_memory_type {
	/** High Power (HP) SRAM memory. */
	IPC4_HP_SRAM_MEMORY = 0,
	/** Low Power (LP) SRAM memory. */
	IPC4_LP_SRAM_MEMORY = 1
};

/**
 * \brief DSP resource state request types.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_resource_state_request {
	/** Query free physical HP SRAM memory pages available. */
	IPC4_FREE_PHYS_MEM_PAGES = 0
};

/**
 * \brief Free physical memory page descriptor.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_phys_mem_pages {
	uint32_t mem_type; /**< Memory type (enum ipc4_memory_type). */
	uint32_t pages;    /**< Number of free memory pages available. */
} __attribute__((packed, aligned(4)));

/** \brief Notification bit index: gateway underrun notification. */
#define IPC4_UNDERRUN_AT_GATEWAY_NOTIFICATION_MASK_IDX	0
/** \brief Notification bit index: mixer underrun notification. */
#define IPC4_UNDERRUN_AT_MIXER_NOTIFICATION_MASK_IDX	1
/** \brief Notification bit index: budget violation notification. */
#define IPC4_BUDGET_VIOLATION_NOTIFICATION_MASK_IDX	2
/** \brief Notification bit index: gateway overrun notification. */
#define IPC4_OVERRUN_AT_GATEWAY_NOTIFICATION_MASK_IDX	3

/**
 * \brief Notification mask configuration structure (IPC4_NOTIFICATION_MASK).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_notification_mask_info {
	/** Bitmask indicating which notifications are being modified. */
	uint32_t ntfy_mask;
	/**
	 * Bitmask indicating enable (1) or disable (0) state for notifications
	 * selected in ntfy_mask.
	 */
	uint32_t enabled_mask;
} __attribute__((packed, aligned(4)));

/**
 * \brief Hardware clock source selection.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_clock_src {
	/** Low Power XTAL oscillator clock source. */
	IPC4_CLOCK_SRC_XTAL = 0,
	/** Low Power Ring Oscillator (RO). */
	IPC4_CLOCK_SRC_LP_RING_OSC,
	/** High Power Ring Oscillator (RO). */
	IPC4_CLOCK_SRC_HP_RING_OSC,
	/**
	 * Low Power Wake-on-Voice (WoV) XTAL oscillator.
	 * Generated internal to IP for optimal low-power listening.
	 */
	IPC4_CLOCK_SRC_WOV_XTAL,
	/** Invalid clock source sentinel. */
	IPC4_CLOCK_SRC_INVALID,
	/** Maximum valid clock source index. */
	IPC4_CLOCK_SRC_MAX_IDX = IPC4_CLOCK_SRC_INVALID - 1,
};

/**
 * \brief A-State frequency and load threshold entry.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_astate {
	/**
	 * Core load threshold in Kilo-Cycles Per Second (KCPS).
	 * Below this threshold DSP is clocked from source specified by clock_src.
	 * 0 KCPS indicates clock source used in idle state only.
	 */
	uint32_t kcps;
	/** Clock source associated with kcps threshold (enum ipc4_clock_src). */
	enum ipc4_clock_src clock_src;
} __attribute__((packed, aligned(4)));

/**
 * \brief Power Manager A-State configuration table (IPC4_ASTATE_TABLE).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_astate_table {
	/** Number of active entries in astates_ array. */
	uint32_t astates_count_;
	/** Array of power states. */
	struct ipc4_astate astates_[3];
} __attribute__((packed, aligned(4)));

/**
 * \brief Direct DMA Gateway Control structure (IPC4_DMA_CONTROL).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_dma_control {
	uint32_t node_id;       /**< Target DMA gateway node ID (union ipc4_connector_node_id). */
	uint32_t config_length; /**< Configuration blob length in DWORDS. */
	uint32_t config_data[0]; /**< Gateway-specific configuration payload (NHLT blob / TLV). */
} __attribute__((packed, aligned(4)));

/**
 * \brief Performance measurement process states.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_perf_measurements_state_set {
	IPC4_PERF_MEASUREMENTS_DISABLED = 0, /**< Performance measurements disabled. */
	IPC4_PERF_MEASUREMENTS_STOPPED = 1,  /**< Performance measurements stopped. */
	IPC4_PERF_MEASUREMENTS_STARTED = 2,  /**< Performance measurements actively running. */
	IPC4_PERF_MEASUREMENTS_PAUSED = 3,   /**< Performance measurements paused. */
};

/**
 * \brief Single component performance measurement data item.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_perf_data_item {
	uint32_t  resource_id; /**< Resource ID: core ID or {module_id, instance_id}. */
	uint32_t  power_mode : 1; /**< Power mode: 0 = D0, 1 = D0i3. */
	uint32_t  rsvd       : 30; /**< Reserved padding. */
	uint32_t  is_removed : 1;  /**< 1 if component was removed/uninstantiated. */
	uint32_t  peak_kcps;  /**< Peak Kilo Cycles Per Second captured. */
	uint32_t  avg_kcps;   /**< Average Kilo Cycles Per Second measured. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Module instance performance data with internal iteration metrics.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_perf_data_item_mi {
	struct ipc4_perf_data_item item; /**< Base performance item header. */
	uint32_t total_iteration_count;  /**< Total execution iteration count of module instance. */
	uint64_t total_cycles_consumed;  /**< Total DSP cycles consumed by module instance. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Global DSP performance report payload (IPC4_GLOBAL_PERF_DATA).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_global_perf_data {
	uint32_t perf_item_count; /**< Number of items in perf_items array. */
	struct ipc4_perf_data_item perf_items[1]; /**< Array of performance measurement items. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Low latency interrupt source types.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_low_latency_interrupt_source {
	IPC4_LOW_POWER_TIMER_INTERRUPT_SOURCE = 1, /**< Low Power Timer interrupt. */
	IPC4_DMA_GATEWAY_INTERRUPT_SOURCE = 2      /**< DMA Gateway completion interrupt. */
};

/**
 * \brief Scheduler tick and timer configuration (IPC4_SCHEDULER_CONFIGURATION).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_scheduler_config {
	uint32_t sys_tick_multiplier; /**< System tick multiplier factor. */
	uint32_t sys_tick_divider;    /**< System tick divider factor. */
	uint32_t sys_tick_source;     /**< System tick source clock. */
	uint32_t sys_tick_cfg_length; /**< Length of extra tick configuration. */
	uint32_t sys_tick_cfg[1];     /**< Extra tick configuration data. */
} __attribute__((packed, aligned(4)));

/**
 * \brief 64-bit system timestamp split into lower and upper 32-bit words.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_system_time {
	uint32_t val_l; /**< Lower 32 bits of microsecond timestamp. */
	uint32_t val_u; /**< Upper 32 bits of microsecond timestamp. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Host and DSP system time pairing for clock synchronization.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_system_time_info {
	struct ipc4_system_time host_time; /**< Host CPU timestamp. */
	struct ipc4_system_time dsp_time;  /**< Corresponding DSP timestamp. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Extended host/DSP system time with RTC and ART timestamps.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_ext_system_time {
	uint32_t utc_l; /**< Lower DWORD of UTC system time. */
	uint32_t utc_u; /**< Upper DWORD of UTC system time. */
	uint32_t rtc_l; /**< Lower DWORD of Real Time Clock. */
	uint32_t rtc_u; /**< Upper DWORD of Real Time Clock. */
	uint32_t art_l; /**< Lower DWORD of Always Running Timer (ART). */
	uint32_t art_u; /**< Upper DWORD of Always Running Timer (ART). */
};

/**
 * \brief Pipeline attributes flags.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_pipeline_attributes {
	/** Pipeline contains modules with Ultra Low Power (ULP) capability. */
	IPC4_ULTRA_LOW_POWER     = 0,
	/** Pipeline contains modules capable of autonomous reset reporting. */
	IPC4_AUTONOMOUS_RESET     = 1,
};

/**
 * \brief Resource allocation request types.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_resource_allocation_type {
	IPC4_RAT_DSP_KCPS   = 0, /**< Allocate DSP processing capacity in KCPS. */
	IPC4_RAT_MEMORY     = 1, /**< Allocate physical SRAM memory. */
};

/**
 * \brief KCPS resource allocation descriptor.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_resource_kcps {
	uint32_t core_id; /**< DSP core identifier. */
	int32_t kcps;     /**< KCPS capacity to allocate or release (signed). */
} __attribute__((packed, aligned(4)));

/**
 * \brief Memory resource allocation descriptor.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_resource_memory {
	uint32_t address; /**< Base physical address to allocate. */
	uint32_t size;    /**< Size of memory allocation in bytes. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Resource allocation request structure (IPC4_RESOURCE_ALLOCATION_REQUEST).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_resource_request {
	uint32_t ra_type; /**< Resource allocation type (enum ipc4_resource_allocation_type). */
	union {
		struct ipc4_resource_kcps kcps;     /**< KCPS data when ra_type == RAT_DSP_KCPS. */
		struct ipc4_resource_memory memory; /**< Memory data when ra_type == RAT_MEMORY. */
	} ra_data; /**< Resource payload union. */
} __attribute__((packed, aligned(4)));

/** \brief LP-SRAM state indicator index. */
#define IPC4_LPSRAM_STATE  0
/** \brief HP-SRAM state indicator index. */
#define IPC4_HPSRAM_STATE  1

/**
 * \brief SRAM page allocation bitmask array.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_sram_state_page_alloc {
	uint32_t page_alloc_count; /**< Number of items in page_alloc array. */
	/** Bit[i] indicates whether i-th memory page is allocated. */
	uint16_t page_alloc[1];
} __attribute__((packed, aligned(4)));

/**
 * \brief Detailed SRAM state and power bank information (IPC4_MEMORY_STATE_INFO_GET).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_sram_state_info {
	uint32_t free_phys_mem_pages;    /**< Number of free memory pages available. */
	uint32_t ebb_state_dword_count;  /**< Number of items in ebb_state array. */
	/** Bit[i] indicates whether i-th EBB bank is powered up (1) or off (0). */
	uint32_t ebb_state[1];
	struct ipc4_sram_state_page_alloc page_alloc_struct; /**< Page allocation state. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Audio Hub Link (ALH) interface versions.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_alh_version {
	IPC4_ALH_NO_SUPPORT,        /**< ALH not supported. */
	IPC4_ALH_CAVS_1_8 = 0x10000, /**< CAVS 1.8+ ALH implementation. */
};

/**
 * \brief Properties of a dynamic or preloaded external library.
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_library_props {
	uint32_t id;             /**< Library runtime ID (Base FW is always 0). */
	uint8_t name[8];         /**< Library identifier name string. */
	uint16_t major_version;  /**< Major version number. */
	uint16_t minor_version;  /**< Minor version number. */
	uint16_t hotfix_version; /**< Hotfix version number. */
	uint16_t build_version;  /**< Build version number. */
	uint32_t num_module_entries; /**< Number of modules provided by this library. */
} __packed __aligned(4);

/**
 * \brief Library information report payload (IPC4_LIBRARIES_INFO_GET).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_libraries_info {
	uint32_t library_count; /**< Number of libraries reported. */
	struct ipc4_library_props libraries[0]; /**< Array of library property structures. */
} __packed __aligned(4);

/**
 * \brief Logging configuration and state control (IPC4_ENABLE_LOGS).
 * \ingroup ipc4_base_fw_api
 */
struct ipc4_log_state_info {
	/** Aging timer period in system ticks for log buffer flush notification. */
	uint32_t  aging_timer_period;
	/** Latency in system ticks for logging dropped entries after buffer read. */
	uint32_t  fifo_full_timer_period;
	/** 1 to enable logging, 0 to disable. */
	uint32_t  enable;
	/** Priority and component logging masks per library provider. */
	uint32_t logs_mask[IPC4_MAX_LIBS_COUNT];
} __attribute__((packed, aligned(4)));

/**
 * \brief Power state query types.
 * \ingroup ipc4_base_fw_api
 */
enum ipc4_power_state_type {
	IPC4_ACTIVE_CORES_MASK = 0, /**< Query bitmask of currently active DSP cores. */
	IPC4_CORE_KCPS = 1,         /**< Query core clock KCPS settings. */
};

/**
 * \brief Single task properties within scheduler information.
 * \ingroup ipc4_base_fw_api
 */
struct task_props {
	uint32_t  task_id; /**< Unique identifier of the task. */
	uint32_t  module_instance_count; /**< Number of module instances attached to task. */
	uint32_t  module_instance_id[];  /**< Array of module instance IDs executing in task. */
} __packed __aligned(4);

/**
 * \brief Core scheduler properties and scheduled tasks.
 * \ingroup ipc4_base_fw_api
 */
struct scheduler_props {
	uint32_t   processing_domain; /**< Domain: COMP_PROCESSING_DOMAIN_LL or DP. */
	uint32_t   core_id;           /**< Core ID the scheduler executes on. */
	uint32_t   task_count;        /**< Number of tasks managed by scheduler. */
	struct task_props task_info[]; /**< Array of task descriptor structures. */
} __packed __aligned(4);

/**
 * \brief Complete scheduler topology report (IPC4_SCHEDULERS_INFO_GET).
 * \ingroup ipc4_base_fw_api
 */
struct schedulers_info {
	uint32_t scheduler_count; /**< Number of schedulers reported. */
	struct scheduler_props scheduler_info[]; /**< Array of scheduler properties. */
} __packed __aligned(4);

/**
 * \brief Get current DSP system time information for host-DSP clock synchronization.
 * \ingroup ipc4_base_fw_api
 * \return Pointer to struct ipc4_system_time_info containing current 64-bit timestamp.
 */
struct ipc4_system_time_info *basefw_get_system_time_info(void);

/** \brief I2S IPC4 interface version for Panther Lake (PTL) platforms. */
static const uint32_t I2S_VER_30_PTL       = 0x40000;

/**
 * \brief FW component performance measurement item.
 * \ingroup ipc4_base_fw_api
 */
struct perf_data_item {
	uint32_t resource_id; /**< Component resource ID. */
	uint32_t power_mode : 1; /**< Power mode: 0 = D0, 1 = D0i3. */
	uint32_t rsvd : 30;      /**< Reserved bits. */
	uint32_t is_removed : 1; /**< 0 if component exists, 1 if deleted. */
	uint32_t peak_kcps;      /**< Peak KCPS captured. */
	uint32_t avg_kcps;       /**< Average KCPS measured. */
} __packed __aligned(4);

/**
 * \brief Extended component performance metrics with DSP cycle counts.
 * \ingroup ipc4_base_fw_api
 */
struct ext_perf_data_item {
	uint32_t resource_id; /**< Component resource identifier. */
	uint32_t power_mode : 1; /**< Power mode: 0 = D0, 1 = D0i3. */
	uint32_t rsvd : 30;      /**< Reserved bits. */
	uint32_t is_removed : 1; /**< 0 if component exists, 1 if deleted. */
	uint32_t module_peak_dsp_cycles; /**< Peak DSP cycles used in a single run. */
	uint64_t module_total_dsp_cycles_consumed; /**< Total cumulative DSP cycles consumed. */
	uint64_t module_total_dsp_iterations; /**< Total execution iteration count. */
	uint32_t module_peak_restricted_cycles; /**< Peak restricted cycles against wall clock. */
	uint64_t module_total_restricted_cycles_consumed; /**< Total restricted cycles consumed. */
	uint64_t module_total_restricted_iterations; /**< Total restricted iteration count. */
} __packed __aligned(4);

/**
 * \brief Extended global performance data report (IPC4_EXTENDED_GLOBAL_PERF_DATA).
 * \ingroup ipc4_base_fw_api
 */
struct extended_global_perf_data {
	uint32_t perf_item_count; /**< Number of items in perf_items array. */
	struct ext_perf_data_item perf_items[0]; /**< Array of extended performance items. */
} __packed __aligned(4);

/**
 * \brief Component performance data with wall clock cycle metrics.
 * \ingroup ipc4_base_fw_api
 */
struct perf_data_item_comp {
	struct perf_data_item item; /**< Base performance item header. */
	uint32_t total_iteration_count; /**< Total module iteration count. */
	uint64_t total_cycles_consumed; /**< Total DSP cycles consumed. */
	uint32_t restricted_peak_cycles; /**< Peak cycles measured against wall clock. */
	uint64_t restricted_total_iterations; /**< Total restricted iteration count. */
	uint64_t restricted_total_cycles; /**< Total restricted cycles consumed. */
} __packed __aligned(4);

/**
 * \brief Global DSP performance summary.
 * \ingroup ipc4_base_fw_api
 */
struct global_perf_data {
	uint32_t perf_item_count; /**< Number of items in perf_items array. */
	struct perf_data_item perf_items[0]; /**< Array of performance items. */
} __packed __aligned(4);

#endif /* __SOF_IPC4_BASE_FW_H__ */
