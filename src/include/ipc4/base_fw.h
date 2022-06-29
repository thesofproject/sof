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
 * \file include/ipc4/base_fw.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

/* Three clk src states :low power XTAL,  low power ring
 * and high power ring oscillator
 */
#define IPC4_MAX_CLK_STATES 3

/* Max src queue count count supported by ipc4 */
#define IPC4_MAX_SRC_QUEUE 8

/* Max module instance for single module count supported by ipc4 */
#define IPC4_MAX_MODULE_INSTANCES 256

/* Max ll tasks for a schedule priority count supported by ipc4 */
#define IPC4_MAX_LL_TASKS_PER_PRI_COUNT 16

/* Max dp tasks count supported by ipc4 */
#define IPC4_MAX_DP_TASKS_COUNT 16

/* Max external libraries count supported by ipc4 */
#define IPC4_MAX_LIBS_COUNT 16

/* Max pipeline count supported by ipc4 */
#define IPC4_MAX_PPL_COUNT 16

enum ipc4_basefw_params {
	/* Use LARGE_CONFIG_GET to retrieve fw properties as TLV structure
	 * with typeof AdspProperties.
	 */
	IPC4_DSP_PROPERTIES = 0,

	IPC4_DSP_RESOURCE_STATE = 1,

	IPC4_RESERVED = 2,

	/* Driver sends this request to enable/disable notifications. This message
	 * should be used by the driver in debug mode to avoid flooding host with
	 * underrun notifications when driver is stopped by breakpoint for example.
	 */
	IPC4_NOTIFICATION_MASK = 3,

	/* Driver sends A-State Table data right after the Base FW is up and ready to
	 * handle IPC communication. The table is forwarded to the Power Manager to
	 * configure available power states according to the underlying platform.
	 */
	IPC4_ASTATE_TABLE = 4,

	/* Driver sends the DMA Control parameter in order to initialize or modify
	 * DMA gateway configuration outside of a stream lifetime. Typically a DMA
	 * gateway is initialized during pipeline creation when a Copier module is
	 * instantiated and attached to that gateway. Similarly the gateway is
	 * de-initialized when the Copiers parent pipeline is being destroyed.
	 * However sometimes the driver may want to control the gateway before or
	 * after a stream is being attached to it.
	 *
	 * The data of DMA Control parameter starts with DmaControl structure
	 * optionally followed by the target gateway specific data (that may consist
	 * of two parts, the former coming from NHLT BIOS tables and the latter
	 * aux_config in form of TLV list provided by the driver).
	 */
	IPC4_DMA_CONTROL = 5,

	/* Driver sets this parameter to control state of FW logging. Driver may
	 * enable logging for each core and specify logging level. Driver also
	 * configures period of aging and FIFO full timers. Aging timer period
	 * specifies how frequently FW sends Log Buffer Status notification for new
	 * entries in case the usual notification sending criteria are not met
	 * (half of the buffer is full). Fifo full timer period specifies the latency
	 * of logging "dropped log entries" information after the content is consumed
	 * by the driver but no new log entry appears (which would trigger logging
	 * "dropped entries" as well).
	 *
	 * SystemTime property must be provided by the driver prior to enabling
	 *the logs for the first time, otherwise error is raised by FW since
	 * it will not be able to translate log event timestamps into the host
	 * CPU clock domain.
	 *
	 * Log FIFO content is reset on logs enabled by the driver, so the RP
	 * is expected to be 0, however the driver should not assume that value
	 * but just read the RP from FW Registers instead.
	 */
	IPC4_ENABLE_LOGS = 6,

	/* Use LARGE_CONFIG_SET/LARGE_CONFIG_GET to write/read FW configuration.
	 *
	 * Driver requests value of this Base FW property to discover FW configuration.
	 * Configuration data is returned in form of TLV list and contains items as
	 * defined in the next table.
	 *
	 * Driver may also set values of parameters that are marked as RW in the table.
	 * FirmwareConfig is expected to be queried/set once at the FW
	 * initialization time. Properties that are expected to be changed more
	 * frequently (e.g. current number of modules descriptors loaded or
	 * performance measurements state) are moved and became separate
	 * parameters.
	 */
	IPC4_FW_CONFIG = 7,

	/* Use LARGE_CONFIG_GET to read HW configuration.
	 *
	 * Driver requests value of this Base FW property to discover underlying HW
	 * configuration. Configuration data is returned in form of TLV list.
	 */
	IPC4_HW_CONFIG_GET = 8,

	/* Use LARGE_CONFIG_GET to read modules configuration.
	 *
	 * Driver requests value of this Base FW property to retrieve list of the
	 * module entries loaded into the FW memory space (as part of either the
	 * image manifest or library manifest).
	 *
	 * The response may be too large to fit into a single message. The driver
	 * must be prepared to handle multiple fragments.
	 */
	IPC4_MODULES_INFO_GET = 9,

	/* Use LARGE_CONFIG_GET to read pipeline list.
	 *
	 * Driver requests value of this Base FW property to retrieve list of
	 * pipelines IDs. Once the list is received driver may retrieve properties of
	 * each pipeline by querying Pipeline Info specifying IDs from the list.
	 */
	IPC4_PIPELINE_LIST_INFO_GET = 10,

	/* Use LARGE_CONFIG_GET to read pipelines properties.
	 *
	 * Driver requests value of this Base FW property to retrieve properties of a
	 * pipeline. Full parameter id wrapped by APPLICATION_PARAM into the request
	 * payload is of ExtendedParameterId type where parameter_type is set to
	 * PIPELINE_PROPS and parameter_instance is set to the target pipeline id.
	 *
	 * Properties of a single pipeline are expected to fit into a single IPC
	 * response as there is a room for ~1K of IDs of tasks and modules instances.
	 */
	IPC4_PIPELINE_PROPS_GET = 11,

	/* Use SCHEDULERS_INFO_GET to read schedulers configuration.
	 *
	 * Driver requests value of this Base FW property to retrieve list of task
	 * schedulers and tasks created inside the FW and being executed on a core.
	 * Full parameter id wrapped by APPLICATION_PARAM into the request payload
	 * is of ExtendedParameterId type where parameter_type is set to
	 * SCHEDULERS_INFO and parameter_instance is set to the target core id.
	 */
	IPC4_SCHEDULERS_INFO_GET = 12,

	/* Use LARGE_CONFIG_GET to read gateway configuration */
	IPC4_GATEWAYS_INFO_GET = 13,

	/* Use LARGE_CONFIG_GET to get information on memory state.
	 *
	 * Driver requests value of this Base FW property to retrieve information
	 * about current DSP memory state. Configuration data is returned in form of
	 * TLV list and contains items as defined in the next table.
	 */
	IPC4_MEMORY_STATE_INFO_GET = 14,

	/*  Use LARGE_CONFIG_GET to get information on power state.
	 *
	 * Driver requests value of this Base FW property to retrieve information
	 * about current DSP power state. Configuration data is returned in form of
	 * TLV list and contains items as defined in the next table.
	 */
	IPC4_POWER_STATE_INFO_GET = 15,

	/* Use LARGE_CONFIG_GET to get information about libraries
	 * loaded into the ADSP memory.
	 */
	IPC4_LIBRARIES_INFO_GET = 16,

	/* Use LARGE_CONFIG_SET to set value of this Base FW property to control
	 * performance measurements state. There is one global flag that controls the
	 * state of performance measurements process globally.
	 *
	 * Driver may set value of this Base FW property to control state of
	 * performance measurements process in the DSP FW.
	 *
	 * This state is applied to MCPS monitoring only. Memory allocation
	 * state is always available.
	 */
	IPC4_PERF_MEASUREMENTS_STATE = 17,

	/* Use LARGE_CONFIG_GET to retrieve retrieve global performance data. FW
	 * sends a list of captured performance data per enabled core and power mode.
	 * MCPS are sampled each DSP system tick (1ms in D0) and used to compute peak
	 * and average values.
	 *
	 * Driver requests value of this Base FW property to retrieve performance data
	 * captured on the DSP. FW sends a list of items reported by FW components.
	 * KCPS are sampled either each DSP system tick (1ms in D0) for low latency
	 * and infrastructure components, or each processed frame otherwise. The KCPS
	 * are used to compute reported peak and average values by FW.
	 *
	 * Data items reported for resource_id = {module_id = 0, instance_id =
	 * core_id} contain total KCPS spent on each active core.
	 *
	 *This parameter reports only KCPS, while the memory state is reported
	 * by Memory State Info parameter.
	 */
	IPC4_GLOBAL_PERF_DATA = 18,

	/* Use LARGE_CONFIG_GET to get information on l2 cache state.
	 *
	 * Driver requests value of this Base FW property to retrieve information
	 * about current state of L2 Cache. Available on platforms where L2 cache is
	 * in use, otherwise ADSP_IPC_UNAVAILABLE is returned.
	 */
	IPC4_L2_CACHE_INFO_GET = 19,

	/* Driver sets this property to pass down information about host system time.
	 *
	 * Driver sets this property to pass down information about current system
	 * time. It is used by FW to translate event timestamps (Logs, Probes packets
	 * for example) to the system time (current host time) domain.
	 *
	 * The value of system time is expressed in us
	 * Time is in UTC
	 * Epoch is 1601-01-01T00:00:00Z
	 */
	IPC4_SYSTEM_TIME = 20,

	/* Use LARGE_CONFIG_SET to configure firmware for performance */
	IPC4_PERFORMANCE_CONFIGURATION = 21,

	/* Use LARGE_CONFIG_SET to register KCPS into power manager
	 * service per core 0. Negative numbers are allowed.
	 */
	IPC4_REGISTER_KCPS = 22,

	/* Use LARGE_CONFIG_SET to request additional resource allocation */
	IPC4_RESOURCE_ALLOCATION_REQUEST = 23,

	/* Driver may set value of this Base FW property to control state of
	 * I/o performance measurements process in the DSP FW.
	 */
	IPC4_IO_PERF_MEASUREMENTS_STATE  = 24,

	/* The command returns I/O statistics when they are enabled */
	IPC4_IO_GLOBAL_PERF_DATA         = 25,

	/* The command is shorter version of Modules Info command.
	 * It is used to retrieve module ID for a specified module UUID.
	 */
	IPC4_GET_MODULE_ID               = 26,

	/* EXTENDED_SYSTEM_TIME command returns current value of UTC, RTC and HH.
	 * The system time must be set first via SYSTEM_TIME command before
	 * EXTENDED_SYSTEM_TIME can be used.
	 */
	IPC4_EXTENDED_SYSTEM_TIME        = 27,

	/* Driver may set value of this Base FW property to control state
	 * of telemetry collection process in the DSP FW.
	 * In started state, TELEMETRY_STATE command is used to change threshold
	 * and aging timer depend on system state.
	 */
	IPC4_TELEMETRY_STATE             = 28,

	/* The command to read data from the telemetry circular buffer.
	 * The telemetry data can be produced by firmware modules using
	 * System Service APIand then all telemetry is collected via one
	 * common mechanism provided by the base firmware.
	 */
	IPC4_TELEMETRY_DATA              = 29,

	/* This command is extended version of Global Performance Data which
	 * provides more details information about total number of
	 * cycles consumed by each of the modules.
	 */
	IPC4_EXTENDED_GLOBAL_PERF_DATA   = 30,

	/* Use LARGE_CONFIG_SET to change SDW ownership */
	IPC4_SDW_OWNERSHIP = 31,
};

enum ipc4_fw_config_params {
	/* Firmware version */
	IPC4_FW_VERSION_FW_CFG                  = 0,
	/* Indicates whether legacy DMA memory is managed by FW */
	IPC4_MEMORY_RECLAIMED_FW_CFG            = 1,
	/* Frequency of oscillator clock */
	IPC4_SLOW_CLOCK_FREQ_HZ_FW_CFG          = 2,
	/* Frequency of PLL clock */
	IPC4_FAST_CLOCK_FREQ_HZ_FW_CFG          = 3,
	/* List of static and dynamic DMA buffer sizes.
	 * SW may configure minimum and maximum size for each buffer.
	 */
	IPC4_DMA_BUFFER_CONFIG_FW_CFG           = 4,
	/* Audio Hub Link support level.
	 * Note: Lower 16-bits may be used in future to indicate
	 * implementation revision if necessary.
	 */
	IPC4_ALH_SUPPORT_LEVEL_FW_CFG           = 5,
	/* Size of IPC downlink mailbox */
	IPC4_DL_MAILBOX_BYTES_FW_CFG        = 6,
	/* Size of IPC uplink mailbox */
	IPC4_UL_MAILBOX_BYTES_FW_CFG        = 7,
	/* Size of trace log buffer */
	IPC4_TRACE_LOG_BYTES_FW_CFG             = 8,
	/* Maximum number of pipelines that may be instantiated at the same time */
	IPC4_MAX_PPL_CNT_FW_CFG                 = 9,
	/* Maximum number of A-state table entries that may be configured by the driver.
	 * Driver may also use this value to estimate the size of data retrieved as
	 * ASTATE_TABLE property.
	 */
	IPC4_MAX_ASTATE_COUNT_FW_CFG            = 10,
	/* Maximum number of input or output pins supported by a module */
	IPC4_MAX_MODULE_PIN_COUNT_FW_CFG        = 11,
	/* Current total number of module entries loaded into the DSP */
	IPC4_MODULES_COUNT_FW_CFG               = 12,
	/* Maximum number of module instances supported by FW */
	IPC4_MAX_MOD_INST_COUNT_FW_CFG          = 13,
	/* Maximum number of LL tasks that may be allocated with
	 * the same priority (specified by a priority of the parent pipeline).
	 */
	IPC4_MAX_LL_TASKS_PER_PRI_COUNT_FW_CFG  = 14,
	/* Number of LL priorities */
	IPC4_LL_PRI_COUNT                       = 15,
	/* Maximum number of DP tasks that may be allocated on a single core */
	IPC4_MAX_DP_TASKS_COUNT_FW_CFG          = 16,
	/* Maximum number of libraries that can be loaded into the ADSP memory */
	IPC4_MAX_LIBS_COUNT_FW_CFG              = 17,
	/* Configuration of system tick source and period */
	IPC4_SCHEDULER_CONFIGURATION            = 18,
	/* Frequency of xtal oscillator clock */
	IPC4_XTAL_FREQ_HZ_FW_CFG                = 19,
	/* Configuration of clocks */
	IPC4_CLOCKS_CONFIGURATION               = 20,
	/* USB Audio Offload support */
	IPC4_UAOL_SUPPORT                       = 21,
	/* Configuration of Dynamic Power Gating Policy */
	IPC4_POWER_GATING_POLICY                = 22,
	/* Configuration of assert mode
	 * Run-time asserts requires special handling by decoder.
	 * Asserts will be in format: "%passert", ptr_to_assert_desc.
	 *
	 * ptr_to_assert_desc will point to place in .asserts_desc section in ELF.
	 * Data must be cased to struct: struct assert_entry
	 */
	IPC4_ASSERT_MODE                        = 23,
	/* Size of telemetry buffer in bytes. The default size is 4KB  */
	IPC4_TELEMETRY_BUFFER_SIZE = 24,
	/* HW version information  */
	IPC4_BUS_HARDWARE_ID = 25,
	/* Total number of FW config parameters  */
	IPC4_FW_CFG_PARAMS_COUNT,
	/* Max config parameter id */
	IPC4_MAX_FW_CFG_PARAM = IPC4_FW_CFG_PARAMS_COUNT - 1,
};

enum ipc4_hw_config_params {
	/* Version of cAVS implemented by FW (from ROMInfo) */
	IPC4_CAVS_VER_HW_CFG             = 0,
	/* How many dsp cores are available in current audio subsystem */
	IPC4_DSP_CORES_HW_CFG            = 1,
	/* Size of a single memory page */
	IPC4_MEM_PAGE_BYTES_HW_CFG       = 2,
	/* Total number of physical pages available for allocation */
	IPC4_TOTAL_PHYS_MEM_PAGES_HW_CFG = 3,
	/*
	 * Number of items in controller_base_addr array is specified by
	 * controller_count. note Lower 16 bits of I2sVersion may be used
	 * in future to indicate implementation revision if necessary.
	 */
	IPC4_I2S_CAPS_HW_CFG             = 4,
	/* GPDMA capabilities */
	IPC4_GPDMA_CAPS_HW_CFG           = 5,
	/*Total number of DMA gateways of all types */
	IPC4_GATEWAY_COUNT_HW_CFG        = 6,
	/* Number of HP SRAM memory banks manageable by DSP */
	IPC4_HP_EBB_COUNT_HW_CFG          = 7,
	/* Number of LP SRAM memory banks manageable by DSP */
	IPC4_LP_EBB_COUNT_HW_CFG          = 8,
	/* Size of a single memory bank (EBB) in bytes */
	IPC4_EBB_SIZE_BYTES_HW_CFG        = 9,
	/* UAOL capabilities */
	IPC4_UAOL_CAPS_HW_CFG            = 10
};

struct ipc4_tuple {
	uint32_t type;
	uint32_t length;
	char data[0];
} __attribute__((packed, aligned(4)));

enum ipc4_memory_type {
	/* High power sram memory */
	IPC4_HP_SRAM_MEMORY = 0,
	/*Low power sram memory */
	IPC4_LP_SRAM_MEMORY = 1
};

enum ipc4_resource_state_request {
	/*
	 * This type is used to inform about free physical HP sram memory pages
	 * available.
	 */
	IPC4_FREE_PHYS_MEM_PAGES = 0
};

/* PhysMemPages describes current free phys memory pages */
struct ipc4_phys_mem_pages {
	uint32_t mem_type;
	/* Number of pages */
	uint32_t pages;
} __attribute__((packed, aligned(4)));

#define IPC4_UNDERRUN_AT_GATEWAY_NOTIFICATION_MASK_IDX	0
#define IPC4_UNDERRUN_AT_MIXER_NOTIFICATION_MASK_IDX	1
#define IPC4_BUDGET_VIOLATION_NOTIFICATION_MASK_IDX	2
#define IPC4_OVERRUN_AT_GATEWAY_NOTIFICATION_MASK_IDX	3

struct ipc4_notification_mask_info {
	/* Indicates which notifications are begin enabled/disabled */
	uint32_t ntfy_mask;
	/* Indicates if notifications indicated by corresponding bits in ntfy_mask
	 * are enabled (b'1) or disabled (b'0).
	 */
	uint32_t enabled_mask;
} __attribute__((packed, aligned(4)));

enum ipc4_clock_src {
	/* Low Power XTAL (oscillator) clock source */
	IPC4_CLOCK_SRC_XTAL = 0,

	/* Low Power Ring Oscillator */
	IPC4_CLOCK_SRC_LP_RING_OSC,
	/* High Power Ring Oscillator */
	IPC4_CLOCK_SRC_HP_RING_OSC,
	/* Low Power XTAL (oscillator) clock source
	 * Frequency depends on platform. This XTAL is different from CLOCK_SRC_XTAL
	 * because it is generate in IP (not given from platform), saving more power.
	 */
	IPC4_CLOCK_SRC_WOV_XTAL,
	IPC4_CLOCK_SRC_INVALID,
	IPC4_CLOCK_SRC_MAX_IDX = IPC4_CLOCK_SRC_INVALID - 1,
};

struct ipc4_astate {
	/* Kilo Cycles Per Second.
	 * Specifies core load threshold (expressed in kilo cycles per second). When
	 * load is below this threshold DSP is clocked from source specified by clk_src.
	 *
	 * Configuring 0 kcps in the first entry means that this clock source will be
	 * used in idle state only.
	 */
	uint32_t kcps;
	/* Clock source associated with kcps threshold */
	enum ipc4_clock_src clock_src;
} __attribute__((packed, aligned(4)));

/* Power Manager Astate Table */
struct ipc4_astate_table {
	/* Number of entries in astates_ array. The value does not exceed maximum
	 * number specified by MAX_ASTATE_COUNT member of Base FWs
	 * FIRMWARE_CONFIG parameter.
	 */
	uint32_t astates_count_;
	/* Array of states. */
	struct ipc4_astate astates_[3];
} __attribute__((packed, aligned(4)));

/* All members have the same meaning as in the CopierGatewayCfg structure
 * (except for the dma_buffer_size that is not used here).
 */
struct ipc4_dma_control {
	uint32_t node_id;
	uint32_t config_length;
	uint32_t config_data[1];
} __attribute__((packed, aligned(4)));

enum ipc4_perf_measurements_state_set {
	IPC4_PERF_MEASUREMENTS_DISABLED = 0,
	IPC4_PERF_MEASUREMENTS_STOPPED = 1,
	IPC4_PERF_MEASUREMENTS_STARTED = 2,
	IPC4_PERF_MEASUREMENTS_PAUSED = 3,
};

struct ipc4_perf_data_item {
	/*ID of the core running the load being reported */
	uint32_t  resource_id;
	/*
	 * 0 - D0,
	 * 1 - D0i3,
	 */
	uint32_t  power_mode : 1;
	uint32_t  rsvd       : 30;
	uint32_t  is_removed : 1;
	/* Peak KCPS (Kilo Cycles Per Second) captured */
	uint32_t  peak_kcps;
	/* Average KCPS (Kilo Cycles Per Second) measured */
	uint32_t  avg_kcps;
} __attribute__((packed, aligned(4)));

/*
 * PerfDataItem with additional fields required by module
 * instance to properly calculate its performance data.
 * NOTE: Only PerfDataItem is part of GlobalPerfData.
 */
struct ipc4_perf_data_item_mi {
	struct ipc4_perf_data_item item;
	/* Total iteration count of module instance */
	uint32_t total_iteration_count;
	/* Total cycles consumed by module instance */
	uint64_t total_cycles_consumed;
} __attribute__((packed, aligned(4)));

struct ipc4_global_perf_data {
	/* Specifies number of items in perf_items array */
	uint32_t      perf_item_count;
	/* Array of global performance measurements */
	struct ipc4_perf_data_item  perf_items[1];
} __attribute__((packed, aligned(4)));

enum ipc4_low_latency_interrupt_source {
	IPC4_LOW_POWER_TIMER_INTERRUPT_SOURCE = 1,
	IPC4_DMA_GATEWAY_INTERRUPT_SOURCE = 2
};

struct ipc4_scheduler_config {
	uint32_t sys_tick_multiplier;
	uint32_t sys_tick_divider;
	uint32_t sys_tick_source;
	uint32_t sys_tick_cfg_length;
	uint32_t sys_tick_cfg[1];
} __attribute__((packed, aligned(4)));

struct ipc4_system_time {
	/* Lower DWORD of current system time value */
	uint32_t  val_l;
	/* Upper DWORD of current system time value */
	uint32_t  val_u;
} __attribute__((packed, aligned(4)));

struct ipc4_system_time_info {
	struct ipc4_system_time host_time;
	struct ipc4_system_time dsp_time;
} __attribute__((packed, aligned(4)));

enum ipc4_pipeline_attributes {
	/* Determines whether on pipeline will be allocated module(s) with ULP capability */
	IPC4_ULTRA_LOW_POWER     = 0,
	/* Determines whether on pipeline will be allocated
	 * module(s) that can report autonomous reset.
	 */
	IPC4_AUTONOMOUS_RESET     = 1,
};

enum ipc4_resource_allocation_type {
	/* Allocate KCPS */
	IPC4_RAT_DSP_KCPS   = 0,
	IPC4_RAT_MEMORY     = 1,
};

struct ipc4_resource_kcps {
	uint32_t core_id;
	int32_t kcps;
} __attribute__((packed, aligned(4)));

struct ipc4_resource_memory {
	/* base address to allocate */
	uint32_t address;
	/* size of allocate */
	uint32_t size;
} __attribute__((packed, aligned(4)));

struct ipc4_resource_request {
	/* Type of resource to allocate */
	uint32_t ra_type;
	union {
		/* Valid for ra_type == RAT_DSP_KCPS */
		struct ipc4_resource_kcps kcps;
		/* Valid for ra_type == RAT_MEMORY */
		struct ipc4_resource_memory  memory;
	} ra_data;
} __attribute__((packed, aligned(4)));

#define IPC4_LPSRAM_STATE  0
#define IPC4_HPSRAM_STATE  1

struct ipc4_sram_state_page_alloc {
	/* Number of items in page_alloc array */
	uint32_t page_alloc_count;
	/* State of memory page allocation.
	 * bit[i] indicates whether i-th page is allocated.
	 */
	uint16_t page_alloc[1];
} __attribute__((packed, aligned(4)));

struct ipc4_sram_state_info {
	/* Number of free memory pages */
	uint32_t free_phys_mem_pages;
	/* Number of items in ebb_state array */
	uint32_t ebb_state_dword_count;
	/* State of EBBs (memory banks) */
	/* bit[i] indicates whether i-th EBB is in use (1) or powered down (0) */
	uint32_t ebb_state[1];
	struct ipc4_sram_state_page_alloc page_alloc_struct;
} __attribute__((packed, aligned(4)));

enum ipc4_alh_version {
	IPC4_ALH_NO_SUPPORT,
	IPC4_ALH_CAVS_1_8 = 0x10000,
};

struct ipc4_log_state_info {
	/*
	 * Specifies how frequently FW sends Log Buffer Status
	 * notification for new entries in case the usual notification
	 * sending criteria are not met (half of the buffer is full). The
	 * parameter is specified in number of system ticks.
	 */
	uint32_t  aging_timer_period;

	/*
	 * Specifies the latency of logging 'dropped log entries'
	 * information after the content is consumed by the driver but no
	 * new log entry appears (which would trigger logging 'dropped
	 * entries' as well). The parameter is specified in number of
	 * system ticks.
	 */
	uint32_t  fifo_full_timer_period;

	/* 0 if logging is disabled, otherwise enabled */
	uint32_t  enable;

	/*
	 * Logging mask of priorities and components for all
	 * supported providers. Nth entry in array gives priorities and
	 * components mask for Nth provider (library).
	 */
	uint32_t logs_mask[IPC4_MAX_LIBS_COUNT];
} __attribute__((packed, aligned(4)));
