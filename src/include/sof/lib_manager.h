/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *         Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */

/*
 * Library manager implementation for SOF is using module adapter API for
 * loadable native and external libraries. Depends of information received from
 * it manages memory and locate accordingly to available space.
 *
 * Driver                   IPC4                       Library                     MEMORY    ENTITY
 *                         Handler                     Manager                               VERIF.
 *   |                       |                            |                          |         |
 *   | IPC4_GLB_LOAD_LIBRARY |                            |                          |         |
 *   | --------------------> | lib_manager_load_library() |                          |         |
 *   |                       | -------------------------> |   Prepare DMA transfer   |         |
 *   |                       |                            | -------                  |         |
 *   |                       |                            |        |                 |         |
 *   |                       |                            | <------                  |         |
 *   |                       |                            | -----------------------> |         |
 *   |                       |                            |                          |         |
 *   |                       |                            |   (IF AUTH_API_ENABLED)  |         |
 *   |                       |                            |  Verify Manifest         |         |
 *   |                       |                            | -------------------------|-------> |
 *   |                       |                            |  results                 |         |
 *   |                       |                            | <------------------------|-------- |
 *   |                       |                            |   (IF AUTH_API_ENABLED)  |         |
 *   |                       |                            |                          |         |
 *   |                       |                            | Parse Manifest           |         |
 *   |                       |                            | Prepare Storage Memory   |         |
 *   |                       |                            | -------                  |         |
 *   |                       |                            |        |                 |         |
 *   |                       |                            | <------                  |         |
 *   |                       |                            |                          |         |
 *   |                       |                            | Copy Library Data        |         |
 *   |                       |                            | -----------------------> |         |
 *   |                       |                            |                          |         |
 *   |                       |                            |   (IF AUTH_API_ENABLED)  |         |
 *   |                       |                            |  Verify Manifest         |         |
 *   |                       |                            | -------------------------|-------> |
 *   |                       |                            |  results                 |         |
 *   |                       |                            | <------------------------|-------- |
 *   |                       |                            |   (IF AUTH_API_ENABLED)  |         |
 *   |                       |                            |                          |         |
 *   |                       |                            | Update Library           |         |
 *   |                       |                            | descriptors table        |         |
 *   |                       |                            | -------                  |         |
 *   |                       |                            |        |                 |         |
 *   |                       |                            | <------                  |         |
 *   |                       |       return status        |                          |         |
 *   |                       | <------------------------- |                          |         |
 *   | Complete IPC request  |                            |                          |         |
 *   | <-------------------  |                            |                          |         |
 *
 * Driver                   IPC4                       Library                     MEMORY    ENTITY
 *                         Handler                     Manager                               VERIF.
 */

#ifndef __SOF_LIB_MANAGER_H__
#define __SOF_LIB_MANAGER_H__

#include <stdint.h>
#include <rimage/sof/user/manifest.h>
#if CONFIG_LIBRARY_AUTH_SUPPORT
#include <sof/auth_api_iface.h>
#endif
#include <sof/list.h>

#define LIB_MANAGER_MAX_LIBS				16
#define LIB_MANAGER_LIB_ID_SHIFT			12
#define LIB_MANAGER_LIB_NOTIX_MAX_COUNT		4

#define LIB_MANAGER_GET_LIB_ID(module_id) ((module_id) >> LIB_MANAGER_LIB_ID_SHIFT)
#define LIB_MANAGER_GET_MODULE_INDEX(module_id) ((module_id) & 0xFFF)

#ifdef CONFIG_LIBRARY_MANAGER
struct ipc_lib_msg {
	struct ipc_msg *msg;
	struct list_item list;
};

struct sof_man_module_manifest;

enum {
	LIB_MANAGER_TEXT,
	LIB_MANAGER_DATA,
	LIB_MANAGER_RODATA,
	LIB_MANAGER_BSS,
	LIB_MANAGER_N_SEGMENTS,
};

struct lib_manager_segment_desc {
	uintptr_t addr;
	size_t size;
};

struct llext;
struct llext_buf_loader;

struct lib_manager_module {
	unsigned int start_idx;	/* Index of the first driver from this module in
				 * the library-global driver list */
	const struct sof_man_module_manifest *mod_manifest;
	struct llext *llext; /* Zephyr loadable extension context */
	struct llext_buf_loader *ebl; /* Zephyr loadable extension buffer loader */
	unsigned int n_dependent; /* For auxiliary modules: number of dependents */
	bool mapped;
	struct lib_manager_segment_desc segment[LIB_MANAGER_N_SEGMENTS];
};

struct lib_manager_mod_ctx {
	void *base_addr;	/* library cold storage address (e.g. DRAM) */
	unsigned int n_mod;
	struct lib_manager_module *mod;
};

struct ext_library {
	struct k_spinlock lock;	/* last locking CPU record */
	struct lib_manager_mod_ctx *desc[LIB_MANAGER_MAX_LIBS];
#ifdef CONFIG_LIBCODE_MODULE_SUPPORT
	uint32_t mods_exec_load_cnt;
#endif
	struct ipc_lib_msg *lib_notif_pool;
	uint32_t lib_notif_count;

	/* Only needed from SOF_IPC4_GLB_LOAD_LIBRARY_PREPARE to SOF_IPC4_GLB_LOAD_LIBRARY */
	void *runtime_data;
};

/* lib manager context, used by lib_notification */
extern struct tr_ctx lib_manager_tr;

static inline struct ext_library *ext_lib_get(void)
{
	return sof_get()->ext_library;
}

static inline struct lib_manager_mod_ctx *lib_manager_get_mod_ctx(int module_id)
{
	uint32_t lib_id = LIB_MANAGER_GET_LIB_ID(module_id);
	struct ext_library *_ext_lib = ext_lib_get();

	if (!_ext_lib || lib_id >= LIB_MANAGER_MAX_LIBS)
		return NULL;

	return _ext_lib->desc[lib_id];
}

/*
 * \brief Get module manifest for given module id
 *
 * param[in] module_id - used to get library manifest
 *
 * Gets library manifest descriptor using module_id to locate it
 */
const struct sof_man_module *lib_manager_get_module_manifest(const uint32_t module_id);
#endif

/*
 * \brief Initialize library manager
 */
void lib_manager_init(void);

/*
 * \brief Register module on driver list
 *
 * param[in] component_id - component id coming from ipc config. This function reguires valid
 * lib_id and module_id fields of component id.
 *
 * Creates new comp_driver_info and initialize it for module from library
 * Adds new module to sof_get()->comp_drivers list
 */
int lib_manager_register_module(const uint32_t component_id);

/*
 * \brief Get library module manifest descriptor
 *
 * param[in] module_id - used to get text manifest offset
 *
 * Gets firmware manifest descriptor using module_id to locate it
 */
const struct sof_man_fw_desc *lib_manager_get_library_manifest(int module_id);

struct processing_module;
struct comp_ipc_config;
/*
 * \brief Allocate module
 *
 * param[in] drv - component driver
 * param[in] ipc_config - audio component base configuration from IPC at creation
 * param[in] ipc_specific_config - ipc4 base configuration
 *
 * Function is responsible to allocate module in available free memory and assigning proper address.
 * (WIP) These feature will contain module validation and proper memory management.
 */
uintptr_t lib_manager_allocate_module(const struct comp_ipc_config *ipc_config,
				      const void *ipc_specific_config);

/*
 * \brief Free module
 *
 * param[in] component_id - component id coming from ipc config. This function reguires valid
 * lib_id and module_id fields of component id.
 *
 * Function is responsible to free module resources in HP memory.
 */
int lib_manager_free_module(const uint32_t component_id);
/*
 * \brief Load library
 *
 * param[in] dma_id - channel used to transfer binary from host
 * param[in] lib_id
 * param[in] type - ipc command type
 *           (SOF_IPC4_GLB_LOAD_LIBRARY or SOF_IPC4_GLB_LOAD_LIBRARY_PREPARE)
 *
 * Function will load library manifest into temporary buffer.
 * Then it will read library parameters, allocate memory for library and load it into
 * destination memory region.
 */
int lib_manager_load_library(uint32_t dma_id, uint32_t lib_id, uint32_t type);

/*
 * \brief Initialize message
 *
 * param[in] header - IPC header provided by caller
 * param[in] size   - size of data buffer for messege to be send
 *
 * Function search lib_notif_pool for free message handler.
 * If not found allocates new message handle and returns it to
 * caller.
 */
struct ipc_msg *lib_notif_msg_init(uint32_t header, uint32_t size);

/*
 * \brief  Send message
 *
 * param[in] msg - IPC message handler
 *
 * Function sends IPC message and calls lib_notif_msg_clean() to
 * free unused message handlers. Only single message handle will
 * be kept in list while at least one loadable module is loaded.
 */
void lib_notif_msg_send(struct ipc_msg *msg);

/*
 * \brief  Clean unused message handles
 *
 * param[in] leave_one_handle - remove all unused or keep one
 *
 * Function search lib_notif_pool list for unused message handles.
 * Remove them keeping one if leave_one_handle == true.
 */
void lib_notif_msg_clean(bool leave_one_handle);

#endif /* __SOF_LIB_MANAGER_H__ */
