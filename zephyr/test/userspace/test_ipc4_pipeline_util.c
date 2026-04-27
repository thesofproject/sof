// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Test case for creation and destruction of IPC4 pipelines.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <user/mfcc.h>
#include <zephyr/logging/log.h>
#include "test_ipc4_pipeline_util.h"
#include <sof/ipc/topology.h>
#include <ipc4/pipeline.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/sof.h>
#include <sof/audio/component_ext.h>
#include <ipc4/base-config.h>
#include "../../../src/audio/copier/copier.h"
#include "../../../src/audio/copier/qemugtw_copier.h"
#include "../../../src/audio/aria/aria.h"
#include <sof/audio/module_adapter/module/generic.h>
#include <module/module/base.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

K_THREAD_STACK_DEFINE(sync_test_stack, 4096);
struct k_thread sync_test_thread;
K_SEM_DEFINE(pipeline_test_sem, 0, 1);

/* Legacy stack kept purely to prevent Xtensa MMU memory shifting bugs in standard tests */
K_THREAD_STACK_DEFINE(userspace_pipe_stack, 4096);
struct k_thread userspace_pipe_thread;
K_SEM_DEFINE(userspace_test_sem, 0, 1);

static void *dummy_refs[] __attribute__((used)) = {
	userspace_pipe_stack,
	&userspace_pipe_thread,
	&userspace_test_sem
};

static inline struct ipc4_message_request create_pipeline_delete_msg(uint16_t instance_id);
static inline struct ipc4_message_request create_pipeline_msg(uint16_t instance_id, uint8_t core_id,
							      uint8_t priority, uint16_t mem_pad);

/**
 * @brief Thread helper to test elementary IPC4 pipeline creation and destruction
 *
 * Simulates an IPC4 global message to create and then delete a pipeline (ID 1)
 * via direct function calls (`ipc4_new_pipeline` & `ipc_pipeline_free`).
 * Validates that pipeline objects are tracked correctly by the IPC core.
 */
void pipeline_create_destroy_helpers_thread(void *p1, void *p2, void *p3)
{
	struct k_sem *test_sem = p1;
	struct ipc *ipc = ipc_get();
	struct ipc4_pipeline_create msg = { 0 };
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	LOG_INF("Starting IPC4 pipeline test (helpers)");

	/* 1. Setup msg */
	msg.primary.r.instance_id = 1;
	msg.primary.r.ppl_priority = SOF_IPC4_PIPELINE_PRIORITY_0;
	msg.primary.r.ppl_mem_size = 1;
	msg.primary.r.type = SOF_IPC4_GLB_CREATE_PIPELINE;
	msg.extension.r.core_id = 0;

	struct ipc4_message_request req = { 0 };
	req.primary.dat = msg.primary.dat;
	req.extension.dat = msg.extension.dat;

	/* 2. Create pipeline */
	ret = ipc4_new_pipeline(&req);
	zassert_equal(ret, 0, "ipc_pipeline_new failed with %d", ret);

	/* 3. Validate pipeline exists */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 1);
	zassert_not_null(ipc_pipe, "pipeline 1 not found after creation");

	/* 4. Destroy pipeline */
	ret = ipc_pipeline_free(ipc, 1);
	zassert_equal(ret, 0, "ipc_pipeline_free failed with %d", ret);

	/* 5. Validate pipeline is destroyed */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 1);
	zassert_is_null(ipc_pipe, "pipeline 1 still exists after destruction");

	LOG_INF("IPC4 pipeline test (helpers) complete");
	k_sem_give(test_sem);
}



/**
 * @brief Thread helper to test IPC4 pipeline setup/teardown via full handlers
 *
 * Verifies that the native IPC4 message dispatching flow successfully
 * parses `SOF_IPC4_GLB_CREATE_PIPELINE` and `SOF_IPC4_GLB_DELETE_PIPELINE`
 * to instantiate and free a pipeline (ID 2).
 */
void pipeline_create_destroy_handlers_thread(void *p1, void *p2, void *p3)
{
	struct k_sem *test_sem = p1;
	struct ipc *ipc = ipc_get();

	struct ipc4_message_request req = { 0 };
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	(void)p2; /* Cast unused parameter */
	(void)p3; /* Cast unused parameter */

	LOG_INF("Starting IPC4 pipeline test (handlers)");

	/* 1. Setup create message */
	req = create_pipeline_msg(20, 0, SOF_IPC4_PIPELINE_PRIORITY_0, 1);

	/* 2. Create pipeline using handler */
	ret = ipc4_new_pipeline(&req);
	zassert_equal(ret, 0, "ipc4_new_pipeline failed with %d", ret);

	/* 3. Validate pipeline exists */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 20);
	zassert_not_null(ipc_pipe, "pipeline 20 not found after creation");

	/* 4. Setup delete message */
	req = create_pipeline_delete_msg(20);

	/* Destroy pipeline using handler */
	ret = ipc4_delete_pipeline(&req);
	zassert_equal(ret, 0, "ipc4_delete_pipeline failed with %d", ret);

	/* 5. Validate pipeline is destroyed */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 20);
	zassert_is_null(ipc_pipe, "pipeline 20 still exists after destruction");

	LOG_INF("IPC4 pipeline test (handlers) complete");
	k_sem_give(test_sem);
}


extern void sys_comp_module_src_interface_init(void);
extern void sys_comp_module_copier_interface_init(void);
extern void sys_comp_module_volume_interface_init(void);
extern void sys_comp_module_mixin_interface_init(void);
extern void sys_comp_module_mixout_interface_init(void);
extern void sys_comp_module_eq_iir_interface_init(void);
extern void sys_comp_module_eq_fir_interface_init(void);
extern void sys_comp_module_drc_interface_init(void);
extern void sys_comp_module_aria_interface_init(void);
extern void sys_comp_module_mfcc_interface_init(void);

/**
 * @brief ZTest test suite setup function
 *
 * Manual initialization of IPC resources because SOF_BOOT_TEST_STANDALONE skips
 * the standard framework init sequence. Required for pipeline tests to function.
 */
void *ipc4_pipeline_setup(void)
{
	struct sof *sof = sof_get();
	printk("DEBUG: Entering ipc4_pipeline_setup\n");

	sys_comp_init(sof);

	sys_comp_module_src_interface_init();
	sys_comp_module_copier_interface_init();
	sys_comp_module_volume_interface_init();
	sys_comp_module_mixin_interface_init();
	sys_comp_module_mixout_interface_init();
#if defined(CONFIG_COMP_EQ_IIR)
	sys_comp_module_eq_iir_interface_init();
#endif
#if defined(CONFIG_COMP_EQ_FIR)
	sys_comp_module_eq_fir_interface_init();
#endif
#if defined(CONFIG_COMP_DRC)
	sys_comp_module_drc_interface_init();
#endif
#if defined(CONFIG_COMP_ARIA)
	sys_comp_module_aria_interface_init();
#endif
#if defined(CONFIG_COMP_MFCC)
	sys_comp_module_mfcc_interface_init();
#endif

	/* SOF_BOOT_TEST_STANDALONE skips IPC init. We must allocate it manually for testing. */
	if (!sof->ipc) {
		printk("DEBUG: Allocating IPC\n");
		sof->ipc = rzalloc(SOF_MEM_FLAG_COHERENT, sizeof(*sof->ipc));
		zassert_not_null(sof->ipc, "IPC allocation failed");
		sof->ipc->comp_data = rzalloc(SOF_MEM_FLAG_COHERENT,
					      SOF_IPC_MSG_MAX_SIZE);
		zassert_not_null(sof->ipc->comp_data, "IPC comp_data allocation failed");
		k_spinlock_init(&sof->ipc->lock);
		list_init(&sof->ipc->msg_list);
		list_init(&sof->ipc->comp_list);
	};

	printk("DEBUG: Returning from ipc4_pipeline_setup\n");
	return NULL;
}

/* --- Pipeline Helper Utilities --- */

#define MAX_MODULE_SINKS 4
#define MAX_PIPELINE_MODULES 16

/* Global UUID declarations for commonly used audio modules */
static const struct sof_uuid __attribute__((unused)) COPIER_UUID = {
	0x9ba00c83, 0xca12, 0x4a83,
	{ 0x94, 0x3c, 0x1f, 0xa2, 0xe8, 0x2f, 0x9d, 0xda }
};

static const struct sof_uuid __attribute__((unused)) SRC_UUID = {
	0xe61bb28d, 0x149a, 0x4c1f,
	{ 0xb7, 0x09, 0x46, 0x82, 0x3e, 0xf5, 0xf5, 0xae }
};

static const struct sof_uuid __attribute__((unused)) MIXIN_UUID = {
	0x39656eb2, 0x3b71, 0x4049,
	{ 0x8d, 0x3f, 0xf9, 0x2c, 0xd5, 0xc4, 0x3c, 0x09 }
};

static const struct sof_uuid __attribute__((unused)) VOL_UUID = {
	0x8a171323, 0x94a3, 0x4e1d,
	{ 0xaf, 0xe9, 0xfe, 0x5d, 0xba, 0xa4, 0xc3, 0x93 }
};

static const struct sof_uuid __attribute__((unused)) MIXOUT_UUID = {
	0x3c56505a, 0x24d7, 0x418f,
	{ 0xbd, 0xdc, 0xc1, 0xf5, 0xa3, 0xac, 0x2a, 0xe0 }
};

static const struct sof_uuid __attribute__((unused)) ASRC_UUID = {
	0xc8ec72f6, 0x8526, 0x4faf,
	{ 0x9d, 0x39, 0xa2, 0x3d, 0x0b, 0x54, 0x1d, 0xe2 }
};

static const struct sof_uuid __attribute__((unused)) CROSSOVER_UUID = {
	0x948c9ad1, 0x806a, 0x4131,
	{ 0xad, 0x6c, 0xb2, 0xbd, 0xa9, 0xe3, 0x5a, 0x9f }
};

static const struct sof_uuid __attribute__((unused)) DCBLOCK_UUID = {
	0xb809efaf, 0x5681, 0x42b1,
	{ 0x9e, 0xd6, 0x04, 0xbb, 0x01, 0x2d, 0xd3, 0x84 }
};

static const struct sof_uuid __attribute__((unused)) DEMUX_UUID = {
	0xc4b26868, 0x1430, 0x470e,
	{ 0xa0, 0x89, 0x15, 0xd1, 0xc7, 0x7f, 0x85, 0x1a }
};

static const struct sof_uuid __attribute__((unused)) DP_SCHED_UUID = {
	0x87858bc2, 0xbaa9, 0x40b6,
	{ 0x8e, 0x4c, 0x2c, 0x95, 0xba, 0x8b, 0x15, 0x45 }
};

static const struct sof_uuid __attribute__((unused)) DRC_UUID = {
	0xb36ee4da, 0x006f, 0x47f9,
	{ 0xa0, 0x6d, 0xfe, 0xcb, 0xe2, 0xd8, 0xb6, 0xce }
};

static const struct sof_uuid __attribute__((unused)) EQ_FIR_UUID = {
	0x43a90ce7, 0xf3a5, 0x41df,
	{ 0xac, 0x06, 0xba, 0x98, 0x65, 0x1a, 0xe6, 0xa3 }
};

static const struct sof_uuid __attribute__((unused)) EQ_IIR_UUID = {
	0x5150c0e6, 0x27f9, 0x4ec8,
	{ 0x83, 0x51, 0xc7, 0x05, 0xb6, 0x42, 0xd1, 0x2f }
};

static const struct sof_uuid __attribute__((unused)) GAIN_UUID = {
	0x61bca9a8, 0x18d0, 0x4a18,
	{ 0x8e, 0x7b, 0x26, 0x39, 0x21, 0x98, 0x04, 0xb7 }
};

static const struct sof_uuid __attribute__((unused)) KPB_UUID = {
	0xd8218443, 0x5ff3, 0x4a4c,
	{ 0xb3, 0x88, 0x6c, 0xfe, 0x07, 0xb9, 0x56, 0x2e }
};

static const struct sof_uuid __attribute__((unused)) MICFIL_UUID = {
	0xdd400475, 0x35d7, 0x4045,
	{ 0xab, 0x03, 0x0c, 0x34, 0x95, 0x7d, 0x7a, 0x08 }
};

static const struct sof_uuid __attribute__((unused)) MULTIBAND_DRC_UUID = {
	0x0d9f2256, 0x8e4f, 0x47b3,
	{ 0x84, 0x48, 0x23, 0x9a, 0x33, 0x4f, 0x11, 0x91 }
};

static const struct sof_uuid __attribute__((unused)) MUX_UUID = {
	0xc607ff4d, 0x9cb6, 0x49dc,
	{ 0xb6, 0x78, 0x7d, 0xa3, 0xc6, 0x3e, 0xa5, 0x57 }
};

static const struct sof_uuid __attribute__((unused)) SELECTOR_UUID = {
	0x55a88ed5, 0x3d18, 0x46ca,
	{ 0x88, 0xf1, 0x0e, 0xe6, 0xea, 0xe9, 0x93, 0x0f }
};

static const struct sof_uuid __attribute__((unused)) STFT_PROCESS_UUID = {
	0x0d116ea6, 0x9150, 0x46de,
	{ 0x98, 0xb8, 0xb2, 0xb3, 0xa7, 0x91, 0xda, 0x29 }
};

static const struct sof_uuid __attribute__((unused)) TONE_UUID = {
	0x04e3f894, 0x2c5c, 0x4f2e,
	{ 0x8d, 0xc1, 0x69, 0x4e, 0xea, 0xab, 0x53, 0xfa }
};

static const struct sof_uuid __attribute__((unused)) WAVES_UUID = {
	0xd944281a, 0xafe9, 0x4695,
	{ 0xa0, 0x43, 0xd7, 0xf6, 0x2b, 0x89, 0x53, 0x8e }
};

static const struct sof_uuid __attribute__((unused)) ARIA_UUID = {
	0x99f7166d, 0x372c, 0x43ef,
	{ 0x81, 0xf6, 0x22, 0x00, 0x7a, 0xa1, 0x5f, 0x03 }
};

static const struct sof_uuid __attribute__((unused)) MFCC_UUID = {
	0xdb10a773, 0x1aa4, 0x4cea,
	{ 0xa2, 0x1f, 0x2d, 0x57, 0xa5, 0xc9, 0x82, 0xeb }
};

/**
 * @struct test_module_conn
 * @brief Defines a connection graph edge from a test module to a sink target
 */
struct test_module_conn {
	uint32_t sink_comp_id;
	uint16_t src_queue;
	uint16_t sink_queue;
};

/**
 * @struct test_module_def
 * @brief Blueprint describing an audio module instantiation in the test pipeline
 */
struct test_module_def {
	/* Module Driver UUID */
	const struct sof_uuid *uuid;
	
	/* IPC Configuration */
	uint32_t comp_id;
	uint16_t pipeline_id;
	uint16_t core_id;
	uint32_t proc_domain; /* 0 defaults to COMP_PROCESSING_DOMAIN_LL */
	uint32_t type;        /* 0 defaults to SOF_COMP_MODULE_ADAPTER */

	/* Configuration payload */
	const void *init_data;
	size_t init_data_size;

	/* Sink Connections */
	uint8_t num_sinks;
	struct test_module_conn sinks[MAX_MODULE_SINKS];

	/* Edge connectivity flags (to mock edge buffers) */
	bool is_pipeline_source;
	bool is_pipeline_sink;
	
	/* Scheduler flag */
	bool is_sched_comp;
};

/**
 * @struct test_pipeline_state
 * @brief State tracking structure spanning the lifecycle of an instantiated test pipeline
 */
struct test_pipeline_state {
	struct comp_dev *modules[MAX_PIPELINE_MODULES];
	uint32_t num_modules;
	struct comp_dev *sched_comp;
	struct comp_buffer *source_buf; /* Mock source buffer for edge */
	struct comp_buffer *sink_buf;   /* Mock sink buffer for edge */
	uint64_t accumulated_bytes[MAX_PIPELINE_MODULES];
	void *last_wptr[MAX_PIPELINE_MODULES];
};

/**
 * @brief Helper binding two instantiated processing modules
 * 
 * Invokes `SOF_IPC4_MOD_BIND` to link a specific source queue of an origin
 * module to a specific destination queue of a target module.
 */
static int mock_ipc4_bind_queues(struct ipc *ipc, uint32_t src_comp_id, uint32_t dst_comp_id,
				 uint16_t src_queue, uint16_t dst_queue)
{
	struct ipc4_module_bind_unbind bu;
	
	memset(&bu, 0, sizeof(bu));
	bu.primary.r.module_id = IPC4_MOD_ID(src_comp_id);
	bu.primary.r.instance_id = IPC4_INST_ID(src_comp_id);
	bu.primary.r.type = SOF_IPC4_MOD_BIND;
	
	bu.extension.r.dst_module_id = IPC4_MOD_ID(dst_comp_id);
	bu.extension.r.dst_instance_id = IPC4_INST_ID(dst_comp_id);
	bu.extension.r.src_queue = src_queue;
	bu.extension.r.dst_queue = dst_queue;
	
	return ipc_comp_connect(ipc, (ipc_pipe_comp_connect *)&bu);
}

/**
 * @brief Crafts a Global IPC4 buffer request to create a pipeline
 */
static inline struct ipc4_message_request create_pipeline_msg(uint16_t instance_id, uint8_t core_id,
							      uint8_t priority, uint16_t mem_size)
{
	struct ipc4_pipeline_create create_msg = {0};
	struct ipc4_message_request req = {0};

	create_msg.primary.r.instance_id = instance_id;
	create_msg.primary.r.ppl_priority = priority;
	create_msg.primary.r.ppl_mem_size = mem_size;
	create_msg.primary.r.type = SOF_IPC4_GLB_CREATE_PIPELINE;
	create_msg.extension.r.core_id = core_id;

	req.primary.dat = create_msg.primary.dat;
	req.extension.dat = create_msg.extension.dat;
	return req;
}

/**
 * @brief Crafts a Global IPC4 buffer request to delete a pipeline
 */
static inline struct ipc4_message_request create_pipeline_delete_msg(uint16_t instance_id)
{
	struct ipc4_pipeline_delete delete_msg = {0};
	struct ipc4_message_request req = {0};

	delete_msg.primary.r.instance_id = instance_id;
	delete_msg.primary.r.type = SOF_IPC4_GLB_DELETE_PIPELINE;

	req.primary.dat = delete_msg.primary.dat;
	req.extension.dat = delete_msg.extension.dat;
	return req;
}

/**
 * @brief Crafts a Modular IPC4 buffer request to initialize a processing module instance
 */
static inline struct ipc4_message_request create_module_msg(const struct test_module_def *def)
{
	struct ipc4_module_init_instance init_msg = {0};
	struct ipc4_message_request req = {0};

	init_msg.primary.r.module_id = IPC4_MOD_ID(def->comp_id);
	init_msg.primary.r.instance_id = IPC4_INST_ID(def->comp_id);
	init_msg.primary.r.type = SOF_IPC4_MOD_INIT_INSTANCE;

	init_msg.extension.r.core_id = def->core_id;
	init_msg.extension.r.param_block_size =
		def->init_data_size ? (def->init_data_size + sizeof(uint32_t) - 1) / sizeof(uint32_t) : 0;

	req.primary.dat = init_msg.primary.dat;
	req.extension.dat = init_msg.extension.dat;
	return req;
}

/**
 * @brief Resolves an initialized module component object from test state by its IPC4 Component ID
 */
static inline struct comp_dev *test_pipeline_get_module(struct test_pipeline_state *state, uint32_t comp_id)
{
	for (uint32_t i = 0; i < state->num_modules; i++) {
		if (state->modules[i]->ipc_config.id == comp_id) {
			return state->modules[i];
		}
	}
	return NULL;
}

static void print_module_buffers(struct test_pipeline_state *p_state)
{
	for (uint32_t i = 0; i < p_state->num_modules; i++) {
		struct comp_dev *mod = p_state->modules[i];
		uint64_t bytes_copied = 0;
		uint32_t frames_copied = 0;
		struct list_item *clist;
		uint32_t frame_bytes = 0;

		if (!list_is_empty(&mod->bsink_list)) {
			struct comp_buffer *buf = list_first_item(&mod->bsink_list, struct comp_buffer, source_list);
			frame_bytes = audio_stream_frame_bytes(&buf->stream);
			
			void *current_wptr = audio_stream_get_wptr(&buf->stream);
			void *last_wptr = p_state->last_wptr[i];
			uint32_t buffer_size = audio_stream_get_size(&buf->stream);
			
			if (last_wptr && current_wptr != last_wptr) {
				uint32_t diff;
				if (current_wptr >= last_wptr)
					diff = (char *)current_wptr - (char *)last_wptr;
				else
					diff = buffer_size - ((char *)last_wptr - (char *)current_wptr);
					
				p_state->accumulated_bytes[i] += diff;
			}
			p_state->last_wptr[i] = current_wptr;
			bytes_copied = p_state->accumulated_bytes[i];
		} else if (!list_is_empty(&mod->bsource_list)) {
			/* If it's a gateway that only has a source but no sink, query its internal counter */
			bytes_copied = comp_get_total_data_processed(mod, 0, false);
			struct comp_buffer *buf = list_first_item(&mod->bsource_list, struct comp_buffer, sink_list);
			frame_bytes = audio_stream_frame_bytes(&buf->stream);
		}

		if (frame_bytes > 0)
			frames_copied = (uint32_t)(bytes_copied / frame_bytes);

		const char *mod_name = mod->tctx.uuid_p ? mod->tctx.uuid_p->name : "Unknown";

		printk("  [Mod ID: %u | %s] Copied: %llu bytes (%u frames)\n", 
		       mod->ipc_config.id, mod_name, bytes_copied, frames_copied);

		list_for_item(clist, &mod->bsource_list) {
			struct comp_buffer *buf = container_of(clist, struct comp_buffer, sink_list);
			uint32_t avail = audio_stream_get_avail_bytes(&buf->stream);
			uint32_t free_bytes = audio_stream_get_free_bytes(&buf->stream);
			void *rptr = audio_stream_get_rptr(&buf->stream);
			void *wptr = audio_stream_get_wptr(&buf->stream);

			printk("    -> [Src Buf] Avail: %u, Free: %u, Rptr: %p, Wptr: %p\n", 
			       avail, free_bytes, rptr, wptr);
		}

		list_for_item(clist, &mod->bsink_list) {
			struct comp_buffer *buf = container_of(clist, struct comp_buffer, source_list);
			uint32_t avail = audio_stream_get_avail_bytes(&buf->stream);
			uint32_t free_bytes = audio_stream_get_free_bytes(&buf->stream);
			void *rptr = audio_stream_get_rptr(&buf->stream);
			void *wptr = audio_stream_get_wptr(&buf->stream);

			printk("    -> [Snk Buf] Avail: %u, Free: %u, Rptr: %p, Wptr: %p\n", 
			       avail, free_bytes, rptr, wptr);
		}
	}
}

/**
 * @brief Constructs a complex topology by allocating modules and interlinking them
 *
 * Parses an array of declarative `test_module_def` blueprints, iteratively locates drivers,
 * invokes their `create()` endpoints. Registers them to the IPC tracking map, injects
 * mock edge test buffers, and binds the topology edges via `mock_ipc4_bind_queues()`.
 */
static int test_pipeline_build(struct test_pipeline_state *state, 
			       const struct test_module_def *defs,
			       size_t num_defs,
			       struct pipeline *pipeline,
			       struct ipc *ipc)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;

	memset(state, 0, sizeof(*state));

	if (num_defs > MAX_PIPELINE_MODULES) {
		LOG_ERR("Too many modules in pipeline definition");
		return -EINVAL;
	};

	/* 1. Instantiate modules */
	for (size_t i = 0; i < num_defs; i++) {
		const struct comp_driver *drv = NULL;

		list_for_item(clist, &drivers->list) {
			struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
			if (!info->drv->uid) continue;
			if (!memcmp(info->drv->uid, defs[i].uuid, sizeof(struct sof_uuid))) {
				drv = info->drv;
				break;
			};
		};

		zassert_not_null(drv, "Driver for module %zu (comp_id 0x%x) not found", 
				 i, defs[i].comp_id);

		struct comp_ipc_config cfg = {
			.id = defs[i].comp_id,
			.pipeline_id = defs[i].pipeline_id,
			.core = defs[i].core_id,
			.proc_domain = defs[i].proc_domain ? defs[i].proc_domain : COMP_PROCESSING_DOMAIN_LL,
			.type = defs[i].type ? defs[i].type : SOF_COMP_MODULE_ADAPTER,
		};

		struct ipc_config_process spec = {
			.data = (unsigned char *)defs[i].init_data,
			.size = (uint32_t)defs[i].init_data_size
		};

		struct comp_dev *mod = drv->ops.create(drv, &cfg, &spec);
		zassert_not_null(mod, "Module %zu creation failed", i);

		ipc4_add_comp_dev(mod);
		state->modules[i] = mod;
		state->num_modules++;

		if (defs[i].is_sched_comp) {
			state->sched_comp = mod;
			pipeline->sched_comp = mod;
		};

		/* Setup edge buffers if requested */
		if (defs[i].is_pipeline_source) {
			struct sof_ipc_stream_params params = {0};
			params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
			state->source_buf = buffer_alloc(pipeline->heap, 1024, 0, 0, false);
			zassert_not_null(state->source_buf, "Source buffer allocation failed");
			buffer_set_params(state->source_buf, &params, true);
			pipeline_connect(mod, state->source_buf, PPL_CONN_DIR_BUFFER_TO_COMP);
		};
		if (defs[i].is_pipeline_sink) {
			struct sof_ipc_stream_params params = {0};
			params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
			state->sink_buf = buffer_alloc(pipeline->heap, 1024, 0, 0, false);
			zassert_not_null(state->sink_buf, "Sink buffer allocation failed");
			buffer_set_params(state->sink_buf, &params, true);
			pipeline_connect(mod, state->sink_buf, PPL_CONN_DIR_COMP_TO_BUFFER);
		};
	};

	/* 2. Bind modules */
	for (size_t i = 0; i < num_defs; i++) {
		for (int j = 0; j < defs[i].num_sinks; j++) {
			int ret = mock_ipc4_bind_queues(ipc, defs[i].comp_id, 
							defs[i].sinks[j].sink_comp_id,
							defs[i].sinks[j].src_queue,
							defs[i].sinks[j].sink_queue);
			zassert_true(ret >= 0, "Bind failed from mod %zu to sink %d", i, j);
		};
	};

	return 0;
}

struct userspace_dp_args {
	struct pipeline *pipeline;
	struct test_pipeline_state *p_state;
};

/**
 * @brief Re-entrant execution loop validating IPC4 DP Pipeline scheduling behaviors
 *
 * Loops explicitly feeding processing frames to an active graph. Asserts continuous 
 * expected stream pointer updates and validates pipeline doesn't xrun or halt gracefully.
 */
static void userspace_dp_pipeline_loop(void *p1, void *p2, void *p3)
{
	struct userspace_dp_args *args = p1;


	/* 1. Simulate a data processing run loop, acting as the LL scheduler ticking for N iterations */
	/* Mock 10 iterations running */
	for (int i = 0; i < 10; ++i) {
		/* Drive the pipeline explicitly since scheduler ticks are absent for LL */
		pipeline_copy(args->pipeline);
		
		printk("  Iteration %d:\n", i);
		print_module_buffers(args->p_state);

		zassert_equal(args->pipeline->status, COMP_STATE_ACTIVE,
			      "pipeline error in iteration %d, status %d", i, args->pipeline->status);
	};

	struct k_sem *test_sem = p2;
	if (test_sem) {
		k_sem_give(test_sem);
	};
	return;
}

/**
 * @brief Data Processing Graph test harnessing QEMU Native Host Simulation
 *
 * Assembles a complicated pipeline featuring QEMU Gateway Input Copier 
 * (feeding sine waves), chained through `SRC -> Volume -> Mixin -> Mixout`.
 * Final Copier executes a validator verifying host gateway outputs matched
 * simulated bitstreams perfectly without processing anomalies.
 */
void pipeline_with_dp_thread(void *p1, void *p2, void *p3)
{
	struct k_sem *test_sem = p1;
	struct k_mem_domain *pipeline_domain_ptr = p2;
	bool is_userspace = (bool)p3;
	struct ipc *ipc = ipc_get();
	struct ipc4_message_request req = { 0 };
	struct ipc_comp_dev *ipc_pipe;
	struct comp_dev *copier1_mod, *src_mod, *mixin_mod, *vol_mod, *mixout_mod, *copier2_mod;
	int ret;

	LOG_INF("Starting IPC4 pipeline with DP test thread");

	/* 1. Setup create pipeline message */
	req = create_pipeline_msg(3, 0, SOF_IPC4_PIPELINE_PRIORITY_0, 1);

	/* 2. Create pipeline */
	ret = ipc4_new_pipeline(&req);
	zassert_equal(ret, 0, "ipc4_new_pipeline failed with %d", ret);

	/* 3. Validate pipeline exists */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 3);
	zassert_not_null(ipc_pipe, "pipeline 3 not found after creation");


	struct custom_qemu_cfg {
		struct ipc4_copier_module_cfg cfg;
		uint32_t extra_data[2];
	};

	struct custom_qemu_cfg c_cfg = {
		.cfg = {
			.base = {
				.cpc = 1, .ibs = 100, .obs = 100, .is_pages = 0,
				.audio_fmt = {
					.sampling_frequency = IPC4_FS_48000HZ,
					.depth = IPC4_DEPTH_16BIT,
					.ch_map = 0, .ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
					.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
					.channels_count = 2, .valid_bit_depth = 16,
					.s_type = IPC4_TYPE_SIGNED_INTEGER,
				}
			},
			.out_fmt = {
				.sampling_frequency = IPC4_FS_48000HZ,
				.depth = IPC4_DEPTH_16BIT,
				.ch_map = 0, .ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
				.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
				.channels_count = 2, .valid_bit_depth = 16,
				.s_type = IPC4_TYPE_SIGNED_INTEGER,
			},
			.gtw_cfg = {
				.node_id = { .f = { .dma_type = ipc4_qemu_input_class, .v_index = 0 } },
				.config_length = 3,
				.config_data = { 0 }, /* Sine mode */
			}
		},
		.extra_data = { 10000, 440 } /* Amplitude, Frequency */
	};
	

	struct custom_qemu_cfg c2_cfg = c_cfg;
	c2_cfg.cfg.gtw_cfg.node_id.f.dma_type = ipc4_qemu_output_class;

	struct ipc4_base_module_cfg base_cfg = c_cfg.cfg.base;

	struct custom_ipc4_config_src {
		struct ipc4_base_module_cfg base;
		uint32_t sink_rate;
	};

	struct custom_ipc4_config_src src_init_data;
	memset(&src_init_data, 0, sizeof(src_init_data));
	src_init_data.base = base_cfg;
	src_init_data.sink_rate = 48000;

	struct test_module_def defs[] = {
		{
			.uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(0, 0), .pipeline_id = 3,
			.init_data = &c_cfg, .init_data_size = sizeof(c_cfg),
			.is_pipeline_source = true, .is_sched_comp = true,
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(1, 0) } }
		},
		{
			.uuid = &SRC_UUID, .comp_id = IPC4_COMP_ID(1, 0), .pipeline_id = 3,
			.init_data = &src_init_data, .init_data_size = sizeof(src_init_data),
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(3, 0) } }
		},
		{
			.uuid = &VOL_UUID, .comp_id = IPC4_COMP_ID(3, 0), .pipeline_id = 3,
			.init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(2, 0) } }
		},
		{
			.uuid = &MIXIN_UUID, .comp_id = IPC4_COMP_ID(2, 0), .pipeline_id = 3,
			.init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(4, 0) } }
		},
		{
			.uuid = &MIXOUT_UUID, .comp_id = IPC4_COMP_ID(4, 0), .pipeline_id = 3,
			.init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(5, 0) } }
		},
		{
			.uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(5, 0), .pipeline_id = 3,
			.init_data = &c2_cfg, .init_data_size = sizeof(c2_cfg),
			.is_pipeline_sink = true,
		}
	};

	struct test_pipeline_state state;
	ret = test_pipeline_build(&state, defs, ARRAY_SIZE(defs), ipc_pipe->pipeline, ipc);
	zassert_equal(ret, 0, "test_pipeline_build failed with %d", ret);

	copier1_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(0, 0));
	src_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(1, 0));
	vol_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(3, 0));
	mixin_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(2, 0));
	mixout_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(4, 0));
	copier2_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(5, 0));

	ret = pipeline_complete(ipc_pipe->pipeline, copier1_mod, copier2_mod);
	zassert_true(ret >= 0, "pipeline complete failed %d", ret);
	ret = pipeline_prepare(ipc_pipe->pipeline, copier1_mod);
	zassert_true(ret >= 0, "pipeline prepare failed %d", ret);

	printk("--------------------------------------------------\n");
	printk("Starting test: Pipeline DP\n");
	printk("Flow: [QEMU IN] -> Copier 1 -> SRC -> Volume -> Mixin -> Mixout -> Copier 2 -> [QEMU OUT]\n");
	printk("--------------------------------------------------\n");

	ret = pipeline_trigger_run(ipc_pipe->pipeline, copier1_mod, COMP_TRIGGER_PRE_START);
	zassert_true(ret >= 0, "pipeline trigger start failed %d", ret);
	ipc_pipe->pipeline->status = COMP_STATE_ACTIVE;

	struct userspace_dp_args u_args = { 
		ipc_pipe->pipeline, 
		&state
	};
	k_sem_reset(&userspace_test_sem);

	uint32_t flags = is_userspace ? (K_USER | K_INHERIT_PERMS) : 0;
	k_thread_create(&userspace_pipe_thread, userspace_pipe_stack, K_THREAD_STACK_SIZEOF(userspace_pipe_stack),
			userspace_dp_pipeline_loop, &u_args, &userspace_test_sem, NULL,
			K_PRIO_COOP(1), flags, K_FOREVER);

	if (is_userspace && pipeline_domain_ptr) {
		k_mem_domain_add_thread(pipeline_domain_ptr, &userspace_pipe_thread);
		k_thread_access_grant(&userspace_pipe_thread, &userspace_test_sem);
	};
	k_thread_start(&userspace_pipe_thread);
	k_sem_take(&userspace_test_sem, K_FOREVER);

	struct processing_module *mod = comp_mod(copier2_mod);
	struct copier_data *cd = module_get_private_data(mod);
	struct qemugtw_data *qemugtw_data = cd->qemugtw_data;
	zassert_not_null(qemugtw_data, "qemugtw_data is null in copier2");
	zassert_equal(qemugtw_data->error_count, 0,
		      "QEMU Gateway output validation had %u errors",
		      qemugtw_data->error_count);
	zassert_true(qemugtw_data->validated_bytes > 0, "QEMU Gateway did not validate any bytes");
	printk("Successfully validated %u bytes through Copier 2 supervised (DP Graph)\n",
	       qemugtw_data->validated_bytes);

	/* 15. Teardown and Cleanup Pipeline Resources */
	ret = pipeline_trigger_run(ipc_pipe->pipeline, copier1_mod, COMP_TRIGGER_STOP);
	ipc_pipe->pipeline->status = COMP_STATE_PAUSED;
	ret = pipeline_reset(ipc_pipe->pipeline, copier1_mod);
	ipc_pipe->pipeline->status = COMP_STATE_READY;
	zassert_true(ret >= 0, "pipeline reset failed %d", ret);

	ipc_pipe->pipeline->pipe_task = NULL;
	ret = ipc_pipeline_free(ipc, 3);
	zassert_equal(ret, 0, "ipc_pipeline_free failed with %d", ret);
	LOG_INF("IPC4 pipeline with DP test complete");
	k_sem_give(test_sem);
}


struct userspace_args {
	struct pipeline *pipeline;
	struct test_pipeline_state *p_state;
};

/**
 * @brief Re-entrant execution loop to execute generic pipeline nodes repeatedly
 */
static void userspace_pipeline_loop(void *p1, void *p2, void *p3)
{
	struct userspace_args *args = p1;


	/* 1. Simulate a data processing run loop, acting as the LL scheduler ticking for N iterations */
	/* Mock 10 iterations running */
	for (int i = 0; i < 10; ++i) {
		/* Drive the pipeline explicitly since scheduler ticks are absent for LL */
		pipeline_copy(args->pipeline);
		
		printk("  Iteration %d:\n", i);
		print_module_buffers(args->p_state);

		zassert_equal(args->pipeline->status, COMP_STATE_ACTIVE,
			      "pipeline error in iteration %d, status %d", i, args->pipeline->status);
	};

	struct k_sem *test_sem = p2;
	if (test_sem) {
		k_sem_give(test_sem);
	};
	return;
}

/**
 * @brief Comprehensive audio end-to-end pipeline graph simulator
 *
 * Assembles a functional audio topology featuring QEMU Gateway Input Copier 
 * chained through `Volume -> Mixin -> Mixout` down to a QEMU Gateway Output Copier.
 * Manages states: Setup, Bind, Prepare, Run (validation), Pause, Reset and Teardown.
 */
void pipeline_full_run_thread(void *p1, void *p2, void *p3)
{
	struct k_sem *test_sem = p1;
	struct k_mem_domain *pipeline_domain_ptr = p2;
	bool is_userspace = (bool)p3;
	struct ipc *ipc = ipc_get();
	struct ipc4_message_request req = { 0 };
	struct ipc_comp_dev *ipc_pipe;
	struct comp_dev *copier1_mod, *vol_mod, *mixin_mod, *mixout_mod, *copier2_mod;
	int ret;

	LOG_INF("Starting IPC4 pipeline full run test thread");

	/* 1. Setup create pipeline message */
	req = create_pipeline_msg(2, 0, SOF_IPC4_PIPELINE_PRIORITY_0, 1);

	/* 2. Create pipeline */
	ret = ipc4_new_pipeline(&req);
	zassert_equal(ret, 0, "ipc4_new_pipeline failed with %d", ret);

	/* 3. Validate pipeline exists */
	ipc_pipe = ipc_get_pipeline_by_id(ipc, 2);
	zassert_not_null(ipc_pipe, "pipeline 2 not found after creation");


	struct custom_qemu_cfg {
		struct ipc4_copier_module_cfg cfg;
		uint32_t extra_data[2];
	};

	struct custom_qemu_cfg c_cfg = {
		.cfg = {
			.base = {
				.cpc = 1, .ibs = 100, .obs = 100, .is_pages = 0,
				.audio_fmt = {
					.sampling_frequency = IPC4_FS_48000HZ,
					.depth = IPC4_DEPTH_16BIT,
					.ch_map = 0, .ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
					.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
					.channels_count = 2, .valid_bit_depth = 16,
					.s_type = IPC4_TYPE_SIGNED_INTEGER,
				}
			},
			.out_fmt = {
				.sampling_frequency = IPC4_FS_48000HZ,
				.depth = IPC4_DEPTH_16BIT,
				.ch_map = 0, .ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
				.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
				.channels_count = 2, .valid_bit_depth = 16,
				.s_type = IPC4_TYPE_SIGNED_INTEGER,
			},
			.gtw_cfg = {
				.node_id = { .f = { .dma_type = ipc4_qemu_input_class, .v_index = 0 } },
				.config_length = 3,
				.config_data = { 0 }, /* Sine mode */
			}
		},
		.extra_data = { 10000, 440 } /* Amplitude, Frequency */
	};
	
	struct custom_qemu_cfg c2_cfg = c_cfg;
	c2_cfg.cfg.gtw_cfg.node_id.f.dma_type = ipc4_qemu_output_class;

	struct ipc4_base_module_cfg base_cfg = c_cfg.cfg.base;

	struct test_module_def defs[] = {
		{
			.uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(0, 0), .pipeline_id = 2,
			.init_data = &c_cfg, .init_data_size = sizeof(c_cfg),
			.is_pipeline_source = true, .is_sched_comp = true,
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(1, 0) } }
		},
		{
			.uuid = &VOL_UUID, .comp_id = IPC4_COMP_ID(1, 0), .pipeline_id = 2,
			.init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(2, 0) } }
		},
		{
			.uuid = &MIXIN_UUID, .comp_id = IPC4_COMP_ID(2, 0), .pipeline_id = 2,
			.init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(3, 0) } }
		},
		{
			.uuid = &MIXOUT_UUID, .comp_id = IPC4_COMP_ID(3, 0), .pipeline_id = 2,
			.init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
			.num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(4, 0) } }
		},
		{
			.uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(4, 0), .pipeline_id = 2,
			.init_data = &c2_cfg, .init_data_size = sizeof(c2_cfg),
			.is_pipeline_sink = true,
		}
	};

	struct test_pipeline_state state;
	ret = test_pipeline_build(&state, defs, ARRAY_SIZE(defs), ipc_pipe->pipeline, ipc);
	zassert_equal(ret, 0, "test_pipeline_build failed with %d", ret);

	copier1_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(0, 0));
	vol_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(1, 0));
	mixin_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(2, 0));
	mixout_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(3, 0));
	copier2_mod = test_pipeline_get_module(&state, IPC4_COMP_ID(4, 0));

	/* Complete pipeline */
	ret = pipeline_complete(ipc_pipe->pipeline, copier1_mod, copier2_mod);
	zassert_true(ret >= 0, "pipeline complete failed %d", ret);

	/* Prepare pipeline */
	ret = pipeline_prepare(ipc_pipe->pipeline, copier1_mod);
	zassert_true(ret >= 0, "pipeline prepare failed %d", ret);

	/* 15. Print Pipeline Layout */
	printk("--------------------------------------------------\n");
	printk("Starting test: Pipeline 4\n");
	printk("Flow: [QEMU GTW IN (Sine)] -> Copier 1 -> Volume -> Mixin -> Mixout" \
	       " -> Copier 2 -> [QEMU GTW OUT (Validate)]\n");
	printk("--------------------------------------------------\n");

	/* Trigger pipeline schedule */
	ret = pipeline_trigger_run(ipc_pipe->pipeline, copier1_mod, COMP_TRIGGER_PRE_START);
	zassert_true(ret >= 0, "pipeline trigger start failed %d", ret);
	ipc_pipe->pipeline->status = COMP_STATE_ACTIVE;

	struct userspace_args u_args = { 
		ipc_pipe->pipeline, 
		&state
	};
	k_sem_reset(&userspace_test_sem);

	uint32_t flags = is_userspace ? (K_USER | K_INHERIT_PERMS) : 0;
	k_thread_create(&userspace_pipe_thread, userspace_pipe_stack, K_THREAD_STACK_SIZEOF(userspace_pipe_stack),
			userspace_pipeline_loop, &u_args, &userspace_test_sem, NULL,
			K_PRIO_COOP(1), flags, K_FOREVER);

	if (is_userspace && pipeline_domain_ptr) {
		k_mem_domain_add_thread(pipeline_domain_ptr, &userspace_pipe_thread);
		k_thread_access_grant(&userspace_pipe_thread, &userspace_test_sem);
	};
	k_thread_start(&userspace_pipe_thread);

	k_sem_take(&userspace_test_sem, K_FOREVER);
	k_thread_join(&userspace_pipe_thread, K_FOREVER);

	/* Validate QEMU output gateway Sink Copier from supervisor mode */
	struct processing_module *mod = comp_mod(copier2_mod);
	struct copier_data *cd = module_get_private_data(mod);
	struct qemugtw_data *qemugtw_data = cd->qemugtw_data;
	zassert_not_null(qemugtw_data, "qemugtw_data is null in copier2");
	zassert_equal(qemugtw_data->error_count, 0,
		      "QEMU Gateway output validation had %u errors",
		      qemugtw_data->error_count);
	zassert_true(qemugtw_data->validated_bytes > 0, "QEMU Gateway did not validate any bytes");
	printk("Successfully validated %u bytes through Copier 2 supervised\n", qemugtw_data->validated_bytes);

	/* 15. Teardown and Cleanup Pipeline Resources */
	ret = pipeline_trigger_run(ipc_pipe->pipeline, copier1_mod, COMP_TRIGGER_STOP);
	ipc_pipe->pipeline->status = COMP_STATE_PAUSED;
	zassert_true(ret >= 0, "pipeline trigger stop failed %d", ret);

	ret = pipeline_reset(ipc_pipe->pipeline, copier1_mod);
	ipc_pipe->pipeline->status = COMP_STATE_READY;
	zassert_true(ret >= 0, "pipeline reset failed %d", ret);
	
	/* internal buffers are pipeline_connected so ipc_pipeline_module_free frees them smoothly!
	 * however, the edge mock ones weren't connected to a source/sink logic natively, so wait, actually
	 * mock_src_buf and mock_sink_buf were pipeline_connected!
     * So no buffer_free() is needed here because ipc_pipeline_free implicitly deletes them all! */


	/* Prevent scheduler execution on standalone dummy task during teardown */
	ipc_pipe->pipeline->pipe_task = NULL;

	/* Now delete pipeline */
	ret = ipc_pipeline_free(ipc, 2);
	zassert_equal(ret, 0, "ipc_pipeline_free failed with %d", ret);
	LOG_INF("IPC4 pipeline full run test complete");
	k_sem_give(test_sem);
}

struct userspace_multi_args {
	struct pipeline *pipeline;
	struct test_pipeline_state *p_state;
};

static void userspace_multi_pipeline_loop(void *p1, void *p2, void *p3)
{
	struct userspace_multi_args *p1_args = p1;
	struct userspace_multi_args *p2_args = p2;
	struct userspace_multi_args *p3_args = p3;

	/* Simulate a single multi-graph loop */
	for (int i = 0; i < 5; ++i) {
		printk("  -- Multi-Pipeline Iteration [%d] --\n", i);
		if (p1_args->pipeline) {
			pipeline_copy(p1_args->pipeline);
			print_module_buffers(p1_args->p_state);
			zassert_equal(p1_args->pipeline->status, COMP_STATE_ACTIVE, "p1 error");
		};
		if (p2_args->pipeline) {
			pipeline_copy(p2_args->pipeline);
			print_module_buffers(p2_args->p_state);
			zassert_equal(p2_args->pipeline->status, COMP_STATE_ACTIVE, "p2 error");
		};
		if (p3_args->pipeline) {
			pipeline_copy(p3_args->pipeline);
			print_module_buffers(p3_args->p_state);
			zassert_equal(p3_args->pipeline->status, COMP_STATE_ACTIVE, "p3 error");
		};
	};

	k_sem_give(&userspace_test_sem);
}

/**
 * @brief Complex audio graph pipeline simulator executing 3 parallel pipelines
 */
void multiple_pipelines_thread(void *p1, void *p2, void *p3)
{
	struct k_sem *test_sem = p1;
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *pipe1 = NULL, *pipe2 = NULL, *pipe3 = NULL;

	struct comp_dev *p3_c1 = NULL, *p3_c2 = NULL;
	struct test_pipeline_state p1_state = {0};
	struct test_pipeline_state p2_state = {0};
	struct test_pipeline_state p3_state = {0};
	int ret;

	LOG_INF("Starting IPC4 multiple pipelines test thread");

	/* Create 3 Pipelines */
#if defined(CONFIG_COMP_EQ_IIR) && defined(CONFIG_COMP_DRC)
	struct ipc4_message_request req1 = create_pipeline_msg(10, 0, SOF_IPC4_PIPELINE_PRIORITY_0, 1);
	ret = ipc4_new_pipeline(&req1);
	zassert_equal(ret, 0, "ipc4_new_pipeline (1) failed with %d", ret);
	pipe1 = ipc_get_pipeline_by_id(ipc, 10);
	zassert_not_null(pipe1, "pipeline 10 not found after creation");
#endif
	
#if defined(CONFIG_COMP_EQ_FIR) && defined(CONFIG_COMP_ARIA)
	struct ipc4_message_request req2 = create_pipeline_msg(11, 0, SOF_IPC4_PIPELINE_PRIORITY_0, 1);
	ret = ipc4_new_pipeline(&req2);
	zassert_equal(ret, 0, "ipc4_new_pipeline (2) failed with %d", ret);
	pipe2 = ipc_get_pipeline_by_id(ipc, 11);
	zassert_not_null(pipe2, "pipeline 11 not found after creation");
#endif
	
#if defined(CONFIG_COMP_MFCC)
	struct ipc4_message_request req3 = create_pipeline_msg(12, 0, SOF_IPC4_PIPELINE_PRIORITY_0, 1);
	ret = ipc4_new_pipeline(&req3);
	zassert_equal(ret, 0, "ipc4_new_pipeline (3) failed with %d", ret);
	pipe3 = ipc_get_pipeline_by_id(ipc, 12);
	zassert_not_null(pipe3, "pipeline 12 not found after creation");
#endif

	/* Mock setup structures */
	struct custom_qemu_cfg {
		struct ipc4_copier_module_cfg cfg;
		uint32_t extra_data[2];
	};

	struct custom_qemu_cfg c_cfg = {
		.cfg = {
			.base = {
				.cpc = 1, .ibs = 100, .obs = 100, .is_pages = 0,
				.audio_fmt = {
					.sampling_frequency = IPC4_FS_48000HZ,
					.depth = IPC4_DEPTH_16BIT,
					.ch_map = 0, .ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
					.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
					.channels_count = 2, .valid_bit_depth = 16,
					.s_type = IPC4_TYPE_SIGNED_INTEGER,
				}
			},
			.out_fmt = {
				.sampling_frequency = IPC4_FS_48000HZ,
				.depth = IPC4_DEPTH_16BIT,
				.ch_map = 0, .ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
				.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
				.channels_count = 2, .valid_bit_depth = 16,
				.s_type = IPC4_TYPE_SIGNED_INTEGER,
			},
			.gtw_cfg = {
				.node_id = { .f = { .dma_type = ipc4_qemu_input_class, .v_index = 0 } },
				.config_length = 3,
				.config_data = { 0 }, /* Sine mode */
			}
		},
		.extra_data = { 10000, 440 } /* Amplitude, Frequency */
	};

	struct custom_qemu_cfg c_out_cfg = c_cfg;
	c_out_cfg.cfg.gtw_cfg.node_id.f.dma_type = ipc4_qemu_output_class;

	struct ipc4_base_module_cfg base_cfg = c_cfg.cfg.base;

#if defined(CONFIG_COMP_EQ_IIR) && defined(CONFIG_COMP_DRC)
	struct custom_ipc4_config_src {
		struct ipc4_base_module_cfg base;
		uint32_t sink_rate;
	};

	struct custom_ipc4_config_src src_init_data;
	memset(&src_init_data, 0, sizeof(src_init_data));
	src_init_data.base = base_cfg;
	src_init_data.sink_rate = 48000;

	struct test_module_def p1_defs[] = {
		{ .uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(0, 0), .pipeline_id = 10,
		  .init_data = &c_cfg, .init_data_size = sizeof(c_cfg), .is_pipeline_source = true, .is_sched_comp = true,
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(1, 0) } } },
		{ .uuid = &EQ_IIR_UUID, .comp_id = IPC4_COMP_ID(1, 0), .pipeline_id = 10,
		  .init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(2, 0) } } },
		{ .uuid = &SRC_UUID, .comp_id = IPC4_COMP_ID(2, 0), .pipeline_id = 10,
		  .init_data = &src_init_data, .init_data_size = sizeof(src_init_data),
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(3, 0) } } },
		{ .uuid = &DRC_UUID, .comp_id = IPC4_COMP_ID(3, 0), .pipeline_id = 10,
		  .init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(4, 0) } } },
		{ .uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(4, 0), .pipeline_id = 10,
		  .init_data = &c_out_cfg, .init_data_size = sizeof(c_out_cfg), .is_pipeline_sink = true }
	};
	struct test_pipeline_state p1_state;
	test_pipeline_build(&p1_state, p1_defs, ARRAY_SIZE(p1_defs), pipe1->pipeline, ipc);
	p1_c1 = test_pipeline_get_module(&p1_state, IPC4_COMP_ID(0, 0));
	p1_c2 = test_pipeline_get_module(&p1_state, IPC4_COMP_ID(4, 0));
	pipeline_complete(pipe1->pipeline, p1_c1, p1_c2);
	pipeline_prepare(pipe1->pipeline, p1_c1);
#endif

#if defined(CONFIG_COMP_EQ_FIR) && defined(CONFIG_COMP_ARIA)
	struct ipc4_aria_module_cfg aria_cfg;
	memset(&aria_cfg, 0, sizeof(aria_cfg));
	aria_cfg.base_cfg = base_cfg;
	aria_cfg.attenuation = 10;

	struct test_module_def p2_defs[] = {
		{ .uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(5, 0), .pipeline_id = 11,
		  .init_data = &c_cfg, .init_data_size = sizeof(c_cfg), .is_pipeline_source = true, .is_sched_comp = true,
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(6, 0) } } },
		{ .uuid = &EQ_FIR_UUID, .comp_id = IPC4_COMP_ID(6, 0), .pipeline_id = 11,
		  .init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(7, 0) } } },
		{ .uuid = &VOL_UUID, .comp_id = IPC4_COMP_ID(7, 0), .pipeline_id = 11,
		  .init_data = &base_cfg, .init_data_size = sizeof(base_cfg),
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(8, 0) } } },
		{ .uuid = &ARIA_UUID, .comp_id = IPC4_COMP_ID(8, 0), .pipeline_id = 11,
		  .init_data = &aria_cfg, .init_data_size = sizeof(aria_cfg),
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(9, 0) } } },
		{ .uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(9, 0), .pipeline_id = 11,
		  .init_data = &c_out_cfg, .init_data_size = sizeof(c_out_cfg), .is_pipeline_sink = true }
	};
	struct test_pipeline_state p2_state;
	test_pipeline_build(&p2_state, p2_defs, ARRAY_SIZE(p2_defs), pipe2->pipeline, ipc);
	p2_c1 = test_pipeline_get_module(&p2_state, IPC4_COMP_ID(5, 0));
	p2_c2 = test_pipeline_get_module(&p2_state, IPC4_COMP_ID(9, 0));
	pipeline_complete(pipe2->pipeline, p2_c1, p2_c2);
	pipeline_prepare(pipe2->pipeline, p2_c1);
#endif

#if defined(CONFIG_COMP_MFCC)
	struct sof_mfcc_config mfcc_init;
	memset(&mfcc_init, 0, sizeof(mfcc_init));

	/* MFCC implicitly overlays the base configuration over its `size` + `reserved` fields */
	memcpy(&mfcc_init, &base_cfg, sizeof(base_cfg));
	/* We set cpc (aliasing config->size) so MFCC parses the entire incoming configuration payload */
	mfcc_init.size = sizeof(struct sof_mfcc_config);

	mfcc_init.sample_frequency = 48000;
	mfcc_init.channel = 0;
	mfcc_init.frame_length = 400;
	mfcc_init.frame_shift = 160;
	mfcc_init.num_mel_bins = 23;
	mfcc_init.num_ceps = 13;
	mfcc_init.dct = MFCC_DCT_II;
	mfcc_init.window = MFCC_BLACKMAN_WINDOW;
	mfcc_init.blackman_coef = MFCC_BLACKMAN_A0;
	mfcc_init.cepstral_lifter = 22 << 9;
	mfcc_init.round_to_power_of_two = true;
	mfcc_init.snip_edges = true;

	struct test_module_def p3_defs[] = {
		{ .uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(13, 0), .pipeline_id = 12,
		  .init_data = &c_cfg, .init_data_size = sizeof(c_cfg), .is_pipeline_source = true, .is_sched_comp = true,
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(14, 0) } } },
		{ .uuid = &MFCC_UUID, .comp_id = IPC4_COMP_ID(14, 0), .pipeline_id = 12,
		  .init_data = &mfcc_init, .init_data_size = sizeof(mfcc_init),
		  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(15, 0) } } },
		{ .uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(15, 0), .pipeline_id = 12,
		  .init_data = &c_out_cfg, .init_data_size = sizeof(c_out_cfg), .is_pipeline_sink = true }
	};
	test_pipeline_build(&p3_state, p3_defs, ARRAY_SIZE(p3_defs), pipe3->pipeline, ipc);
	p3_c1 = test_pipeline_get_module(&p3_state, IPC4_COMP_ID(13, 0));
	p3_c2 = test_pipeline_get_module(&p3_state, IPC4_COMP_ID(15, 0));
	pipeline_complete(pipe3->pipeline, p3_c1, p3_c2);
	pipeline_prepare(pipe3->pipeline, p3_c1);
#endif

	printk("--------------------------------------------------\n");
	printk("Starting test: Concurrent Pipelines Setup\n");
	printk("--------------------------------------------------\n");

#if defined(CONFIG_COMP_EQ_IIR) && defined(CONFIG_COMP_DRC)
	pipeline_trigger_run(pipe1->pipeline, p1_c1, COMP_TRIGGER_PRE_START);
	pipe1->pipeline->status = COMP_STATE_ACTIVE;
#endif
#if defined(CONFIG_COMP_EQ_FIR) && defined(CONFIG_COMP_ARIA)
	pipeline_trigger_run(pipe2->pipeline, p2_c1, COMP_TRIGGER_PRE_START);
	pipe2->pipeline->status = COMP_STATE_ACTIVE;
#endif
#if defined(CONFIG_COMP_MFCC)
	pipeline_trigger_run(pipe3->pipeline, p3_c1, COMP_TRIGGER_PRE_START);
	pipe3->pipeline->status = COMP_STATE_ACTIVE;
#endif

	/* Launch thread only if there's at least one pipeline to process */
	if (pipe1 || pipe2 || pipe3) {
		struct userspace_multi_args u_p1 = { pipe1 ? pipe1->pipeline : NULL, &p1_state };
		struct userspace_multi_args u_p2 = { pipe2 ? pipe2->pipeline : NULL, &p2_state };
		struct userspace_multi_args u_p3 = { pipe3 ? pipe3->pipeline : NULL, &p3_state };

		k_sem_reset(&userspace_test_sem);
		k_thread_create(&userspace_pipe_thread, userspace_pipe_stack, K_THREAD_STACK_SIZEOF(userspace_pipe_stack),
				userspace_multi_pipeline_loop, &u_p1, &u_p2, &u_p3,
				K_PRIO_COOP(1), 0, K_FOREVER);
		k_thread_start(&userspace_pipe_thread);
		k_sem_take(&userspace_test_sem, K_FOREVER);
		k_thread_join(&userspace_pipe_thread, K_FOREVER);
	};

#if defined(CONFIG_COMP_EQ_IIR) && defined(CONFIG_COMP_DRC)
	pipeline_trigger_run(pipe1->pipeline, p1_c1, COMP_TRIGGER_STOP);
	pipeline_reset(pipe1->pipeline, p1_c1);
	pipe1->pipeline->pipe_task = NULL;
	ipc_pipeline_free(ipc, 10);
#endif
#if defined(CONFIG_COMP_EQ_FIR) && defined(CONFIG_COMP_ARIA)
	pipeline_trigger_run(pipe2->pipeline, p2_c1, COMP_TRIGGER_STOP);
	pipeline_reset(pipe2->pipeline, p2_c1);
	pipe2->pipeline->pipe_task = NULL;
	ipc_pipeline_free(ipc, 11);
#endif
#if defined(CONFIG_COMP_MFCC)
	pipeline_trigger_run(pipe3->pipeline, p3_c1, COMP_TRIGGER_STOP);
	pipeline_reset(pipe3->pipeline, p3_c1);
	pipe3->pipeline->pipe_task = NULL;
	ipc_pipeline_free(ipc, 12);
#endif

	LOG_INF("IPC4 %s pipeline test complete", "multi");
	k_sem_give(test_sem);
}




struct test_module_info {
	const struct sof_uuid *uuid;
	void *init_data;
	size_t init_data_size;
	const char *name;
};

void all_modules_ll_pipeline_thread(void *p1, void *p2, void *p3)
{
	struct k_sem *test_sem = p1;
	struct ipc *ipc = ipc_get();
	int ret;

	LOG_INF("Starting IPC4 LL all modules test thread");

	struct custom_qemu_cfg {
		struct ipc4_copier_module_cfg cfg;
		uint32_t extra_data[2];
	};

	struct custom_qemu_cfg c_cfg = {
		.cfg = {
			.base = {
				.cpc = 1, .ibs = 100, .obs = 100, .is_pages = 0,
				.audio_fmt = {
					.sampling_frequency = IPC4_FS_48000HZ,
					.depth = IPC4_DEPTH_16BIT,
					.ch_map = 0, .ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
					.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
					.channels_count = 2, .valid_bit_depth = 16,
					.s_type = IPC4_TYPE_SIGNED_INTEGER,
				}
			},
			.out_fmt = {
				.sampling_frequency = IPC4_FS_48000HZ,
				.depth = IPC4_DEPTH_16BIT,
				.ch_map = 0, .ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
				.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
				.channels_count = 2, .valid_bit_depth = 16,
				.s_type = IPC4_TYPE_SIGNED_INTEGER,
			},
			.gtw_cfg = {
				.node_id = { .f = { .dma_type = ipc4_qemu_input_class, .v_index = 0 } },
				.config_length = 3,
				.config_data = { 0 },
			}
		},
		.extra_data = { 10000, 440 }
	};

	struct custom_qemu_cfg c_out_cfg = c_cfg;
	c_out_cfg.cfg.gtw_cfg.node_id.f.dma_type = ipc4_qemu_output_class;

	struct ipc4_base_module_cfg base_cfg = c_cfg.cfg.base;

#if defined(CONFIG_COMP_ARIA)
	struct ipc4_aria_module_cfg aria_cfg;
	memset(&aria_cfg, 0, sizeof(aria_cfg));
	aria_cfg.base_cfg = base_cfg;
	aria_cfg.attenuation = 10;
#endif

#if defined(CONFIG_COMP_MFCC)
	struct sof_mfcc_config mfcc_init;
	memset(&mfcc_init, 0, sizeof(mfcc_init));

	/* MFCC implicitly overlays the base configuration over its `size` + `reserved` fields */
	memcpy(&mfcc_init, &base_cfg, sizeof(base_cfg));
	/* We set cpc (aliasing config->size) so MFCC parses the entire incoming configuration payload */
	mfcc_init.size = sizeof(struct sof_mfcc_config);

	mfcc_init.sample_frequency = 48000;
	mfcc_init.channel = 0;
	mfcc_init.frame_length = 400;
	mfcc_init.frame_shift = 160;
	mfcc_init.num_mel_bins = 23;
	mfcc_init.num_ceps = 13;
	mfcc_init.dct = MFCC_DCT_II;
	mfcc_init.window = MFCC_BLACKMAN_WINDOW;
	mfcc_init.blackman_coef = MFCC_BLACKMAN_A0;
	mfcc_init.cepstral_lifter = 22 << 9;
	mfcc_init.round_to_power_of_two = true;
	mfcc_init.snip_edges = true;
#endif

	struct test_module_info modules_to_test[] = {
#if defined(CONFIG_COMP_EQ_IIR)
		{ &EQ_IIR_UUID, &base_cfg, sizeof(base_cfg), "EQ_IIR" },
#endif
#if defined(CONFIG_COMP_EQ_FIR)
		{ &EQ_FIR_UUID, &base_cfg, sizeof(base_cfg), "EQ_FIR" },
#endif
#if defined(CONFIG_COMP_DRC)
		{ &DRC_UUID, &base_cfg, sizeof(base_cfg), "DRC" },
#endif
#if defined(CONFIG_COMP_ARIA)
		{ &ARIA_UUID, &aria_cfg, sizeof(aria_cfg), "ARIA" },
#endif
#if defined(CONFIG_COMP_MFCC)
		{ &MFCC_UUID, &mfcc_init, sizeof(mfcc_init), "MFCC" },
#endif
// Skip testing Volume standalone since test struct isn't fully defined yet 
	};

	int num_modules = ARRAY_SIZE(modules_to_test);

	for (int i = 0; i < num_modules; ++i) {
		LOG_INF("Testing LL pipeline: copier -> %s -> copier", modules_to_test[i].name);
		int pipe_id = 30 + i;
		
		/* LL Pipeline creation: lp=0 */
		struct ipc4_message_request req = create_pipeline_msg(pipe_id, 0, SOF_IPC4_PIPELINE_PRIORITY_0, 0);
		ret = ipc4_new_pipeline(&req);
		zassert_equal(ret, 0, "ipc4_new_pipeline failed for %s", modules_to_test[i].name);

		struct ipc_comp_dev *pipe = ipc_get_pipeline_by_id(ipc, pipe_id);
		zassert_not_null(pipe, "pipeline not found for %s", modules_to_test[i].name);

		struct test_module_def p_defs[] = {
			{ .uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(100+i, 0), .pipeline_id = pipe_id,
			  .init_data = &c_cfg, .init_data_size = sizeof(c_cfg), .is_pipeline_source = true, .is_sched_comp = true,
			  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(100+i, 1) } } },
			{ .uuid = modules_to_test[i].uuid, .comp_id = IPC4_COMP_ID(100+i, 1), .pipeline_id = pipe_id,
			  .init_data = modules_to_test[i].init_data, .init_data_size = modules_to_test[i].init_data_size,
			  .num_sinks = 1, .sinks = { { .sink_comp_id = IPC4_COMP_ID(100+i, 2) } } },
			{ .uuid = &COPIER_UUID, .comp_id = IPC4_COMP_ID(100+i, 2), .pipeline_id = pipe_id,
			  .init_data = &c_out_cfg, .init_data_size = sizeof(c_out_cfg), .is_pipeline_sink = true }
		};

		struct test_pipeline_state p_state;
		test_pipeline_build(&p_state, p_defs, ARRAY_SIZE(p_defs), pipe->pipeline, ipc);

		struct comp_dev *c1 = test_pipeline_get_module(&p_state, IPC4_COMP_ID(100+i, 0));
		struct comp_dev *c2 = test_pipeline_get_module(&p_state, IPC4_COMP_ID(100+i, 2));

		pipeline_complete(pipe->pipeline, c1, c2);
		pipeline_prepare(pipe->pipeline, c1);
		
		pipeline_trigger_run(pipe->pipeline, c1, COMP_TRIGGER_PRE_START);
		pipe->pipeline->status = COMP_STATE_ACTIVE;
		
		/* Simulate LL loop since no dedicated scheduler threads exist naturally */
		for (int j = 0; j < 5; ++j) {
			pipeline_copy(pipe->pipeline);
			print_module_buffers(&p_state);
			zassert_equal(pipe->pipeline->status, COMP_STATE_ACTIVE, "%s error", modules_to_test[i].name);
		};

		pipeline_trigger_run(pipe->pipeline, c1, COMP_TRIGGER_STOP);
		pipeline_reset(pipe->pipeline, c1);
		pipe->pipeline->pipe_task = NULL;
		ipc_pipeline_free(ipc, pipe_id);
	};

	LOG_INF("IPC4 LL all modules pipeline test complete");
	k_sem_give(test_sem);
}



ZTEST_SUITE(userspace_ipc4_pipeline, NULL, ipc4_pipeline_setup, NULL, NULL, NULL);




