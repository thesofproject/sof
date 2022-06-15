/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@intel.com>
 */

#ifndef __SOF_AUDIO_DATA_BLOB_H__
#define __SOF_AUDIO_DATA_BLOB_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/sof.h>

struct comp_dev;

struct comp_data_blob_handler;

/**
 * Returns data blob. In case when new data blob is available it returns new
 * one. Function returns also data blob size in case when size pointer is given.
 *
 * @param blob_handler Data blob handler
 * @param size Pointer to data blob size variable
 * @param crc Pointer to data blolb crc value
 */
void *comp_get_data_blob(struct comp_data_blob_handler *blob_handler,
			 size_t *size, uint32_t *crc);

/**
 * Checks whether new data blob is available. Function allows to check (even
 * during streaming - in copy() function) whether new config is available and
 * if it is, component can perform reconfiguration.
 *
 * @param blob_handler Data blob handler
 */
bool comp_is_new_data_blob_available(struct comp_data_blob_handler
					*blob_handler);

/**
 * Checks whether there is a valid data blob is available.
 *
 * @param blob_handler Data blob handler
 */
bool comp_is_current_data_blob_valid(struct comp_data_blob_handler
				     *blob_handler);

/**
 * Initializes data blob with given value. If init_data is not specified,
 * function will zero data blob.
 *
 * @param blob_handler Data blob handler
 * @param size Data blob size
 * @param init_data Initial data blob values
 */
int comp_init_data_blob(struct comp_data_blob_handler *blob_handler,
			uint32_t size, void *init_data);

/**
 * Handles IPC set command.
 *
 * @param blob_handler Data blob handler
 * @param cdata IPC ctrl data
 */
int comp_data_blob_set_cmd(struct comp_data_blob_handler *blob_handler,
			   struct sof_ipc_ctrl_data *cdata);

/**
 * Handles IPC set command.
 *
 * @param blob_handler: Data blob handler
 * @param pos position: of the data fragment
 * @param data_offset_size: offset of the fragment in the whole data
 * @param fragment: pointer to the fragment data
 * @param fragment_size: size of the fragment data
 */
int comp_data_blob_set(struct comp_data_blob_handler *blob_handler,
		       enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		       const uint8_t *fragment, size_t fragment_size);
/**
 * Handles IPC get command.
 * @param blob_handler Data blob handler
 * @param cdata IPC ctrl data
 * @param size Required size
 */
int comp_data_blob_get_cmd(struct comp_data_blob_handler *blob_handler,
			   struct sof_ipc_ctrl_data *cdata, int size);

/**
 * Returns new data blob handler.
 *
 * Data blob handler has two modes of operation, single_blob == false
 * stands for double blob mode that allow seamless configuration
 * updates on the fly. In single_blob == true mode there is at most
 * one blob allocated at one time and configuration update is not
 * allowed if the component is active. The single blob mode should be
 * used with components with very big configuration blobs to save DSP
 * memory.
 *
 * @param dev Component device
 * @param single_blob Set true for single configuration blob operation
 * @param alloc Optional blob memory allocator function pointer
 * @param free Optional blob memory free function pointer
 */
struct comp_data_blob_handler *
comp_data_blob_handler_new_ext(struct comp_dev *dev, bool single_blob,
			       void *(*alloc)(size_t size),
			       void (*free)(void *buf));

/**
 * Returns new data blob handler.
 *
 * This is a simplified version of comp_data_blob_handler_new_ext() using
 * default memory allocator and double blob mode for on the fly updates.
 *
 * @param dev Component device
 */
static inline
struct comp_data_blob_handler *comp_data_blob_handler_new(struct comp_dev *dev)
{
	return comp_data_blob_handler_new_ext(dev, false, NULL, NULL);
}

/**
 * Free data blob handler.
 *
 * @param blob_handler Data blob handler
 */
void comp_data_blob_handler_free(struct comp_data_blob_handler *blob_handler);

#endif /* __SOF_AUDIO_DATA_BLOB_H__ */
