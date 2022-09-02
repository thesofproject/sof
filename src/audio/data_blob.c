// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Jyri Sarha <jyri.sarha@intel.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/sof.h>
#include <rtos/alloc.h>
#include <ipc/topology.h>
#include <ipc/control.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>

LOG_MODULE_REGISTER(data_blob, CONFIG_SOF_LOG_LEVEL);

/** \brief Struct handler for large component configs */
struct comp_data_blob_handler {
	struct comp_dev *dev;	/**< audio component device */
	uint32_t data_size;	/**< size of component's data blob */
	uint32_t new_data_size;	/**< size of component's new data blob */
	void *data;		/**< pointer to data blob */
	void *data_new;		/**< pointer to new data blob */
	bool data_ready;	/**< set when data blob is fully received */
	uint32_t data_pos;	/**< indicates a data position in data
				  *  sending/receiving process
				  */
	uint32_t single_blob:1; /**< Allocate only one blob. Module can not
				  *  be active while reconfguring.
				  */
	void *(*alloc)(size_t size);	/**< alternate allocator, maybe null */
	void (*free)(void *buf);	/**< alternate free(), maybe null */
};

static void comp_free_data_blob(struct comp_data_blob_handler *blob_handler)
{
	assert(blob_handler);

	if (!blob_handler->data)
		return;

	blob_handler->free(blob_handler->data);
	blob_handler->free(blob_handler->data_new);
	blob_handler->data = NULL;
	blob_handler->data_new = NULL;
	blob_handler->data_size = 0;
}

void *comp_get_data_blob(struct comp_data_blob_handler *blob_handler,
			 size_t *size, uint32_t *crc)
{
	assert(blob_handler);

	comp_dbg(blob_handler->dev, "comp_get_data_blob()");

	if (size)
		*size = 0;

	/* Function returns new data blob if available */
	if (comp_is_new_data_blob_available(blob_handler)) {
		comp_dbg(blob_handler->dev, "comp_get_data_blob(): new data available");

		/* Free "old" data blob and set data to data_new pointer */
		blob_handler->free(blob_handler->data);
		blob_handler->data = blob_handler->data_new;
		blob_handler->data_size = blob_handler->new_data_size;

		blob_handler->data_new = NULL;
		blob_handler->data_ready = false;
		blob_handler->new_data_size = 0;
		blob_handler->data_pos = 0;
	}

	/* If data is available we calculate crc32 when crc pointer is given */
	if (blob_handler->data) {
		if (crc)
			*crc = crc32(0, blob_handler->data,
				     blob_handler->data_size);
	} else {
		/* If blob_handler->data is equal to NULL and there is no new
		 * data blob it means that component hasn't got any config yet.
		 * Function returns NULL in that case.
		 */
		comp_warn(blob_handler->dev, "comp_get_data_blob(): blob_handler->data is not set.");
	}

	if (size)
		*size = blob_handler->data_size;

	return blob_handler->data;
}

bool comp_is_new_data_blob_available(struct comp_data_blob_handler
					*blob_handler)
{
	assert(blob_handler);

	comp_dbg(blob_handler->dev, "comp_is_new_data_blob_available()");

	/* New data blob is available when new data blob is allocated (data_new
	 * is not NULL), and the component has received all required chunks of data
	 * (data_ready is set to TRUE)
	 */
	if (blob_handler->data_new && blob_handler->data_ready)
		return true;

	return false;
}

bool comp_is_current_data_blob_valid(struct comp_data_blob_handler
				     *blob_handler)
{
	return !!blob_handler->data;
}

int comp_init_data_blob(struct comp_data_blob_handler *blob_handler,
			uint32_t size, void *init_data)
{
	int ret;

	assert(blob_handler);

	comp_free_data_blob(blob_handler);

	if (!size)
		return 0;

	/* Data blob allocation */
	blob_handler->data = blob_handler->alloc(size);
	if (!blob_handler->data) {
		comp_err(blob_handler->dev, "comp_init_data_blob(): model->data allocation failed");
		return -ENOMEM;
	}

	/* If init_data is given, data will be initialized with it. In other
	 * case, data will be set to zero.
	 */
	if (init_data) {
		ret = memcpy_s(blob_handler->data, size, init_data, size);
		assert(!ret);
	} else {
		bzero(blob_handler->data, size);
	}

	blob_handler->data_new = NULL;
	blob_handler->data_size = size;
	blob_handler->new_data_size = 0;

	return 0;
}

int comp_data_blob_set(struct comp_data_blob_handler *blob_handler,
		       enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		       const uint8_t *fragment, size_t fragment_size)
{
	int ret;

	if (!blob_handler)
		return -EINVAL;

	comp_dbg(blob_handler->dev, "comp_data_blob_set_cmd() pos = %d, fragment size = %d",
		 pos, fragment_size);

	/* Check that there is no work-in-progress previous request */
	if (blob_handler->data_new &&
	    (pos == MODULE_CFG_FRAGMENT_FIRST || pos == MODULE_CFG_FRAGMENT_SINGLE)) {
		comp_err(blob_handler->dev, "comp_data_blob_set_cmd(), busy with previous request");
		return -EBUSY;
	}

	/* In single blob mode the component can not be reconfigured if the component is active.
	 */
	if (blob_handler->single_blob && blob_handler->dev->state == COMP_STATE_ACTIVE) {
		comp_err(blob_handler->dev, "comp_data_blob_set_cmd(), on the fly updates forbidden in single blob mode");
		return -EBUSY;
	}

	/* in case when the current package is the first, we should allocate
	 * memory for whole model data
	 */
	if (pos == MODULE_CFG_FRAGMENT_FIRST || pos == MODULE_CFG_FRAGMENT_SINGLE) {
		/* in case when required model size is equal to zero we do not
		 * allocate memory and should just return 0.
		 *
		 * Set cmd with cdata->data->size equal to 0 is possible in
		 * following situation:
		 * 1. At first boot and topology parsing stage, the driver will
		 * read all initial values of DSP kcontrols via IPC. Driver send
		 * get_model() cmd to components. If we do not initialize
		 * component earlier driver will get "model" with size 0.
		 * 2. When resuming from runtime suspended, the driver will
		 * restore all pipelines and kcontrols, for the tlv binary
		 * kcontrols, it will call the set_model() with the cached value
		 * and size (0 if it is not updated by any actual end user
		 * sof-ctl settings) - basically driver will send set_model()
		 * command with size equal to 0.
		 */
		if (!fragment_size)
			return 0;

		if (blob_handler->single_blob) {
			if (data_offset_size != blob_handler->data_size) {
				blob_handler->free(blob_handler->data);
				blob_handler->data = NULL;
			} else {
				blob_handler->data_new = blob_handler->data;
				blob_handler->data = NULL;
			}
		}

		if (!blob_handler->data_new) {
			blob_handler->data_new = blob_handler->alloc(data_offset_size);
			if (!blob_handler->data_new) {
				comp_err(blob_handler->dev, "comp_data_blob_set_cmd(): blob_handler->data_new allocation failed.");
				return -ENOMEM;
			}
		}

		blob_handler->new_data_size = data_offset_size;
		blob_handler->data_ready = false;
		blob_handler->data_pos = 0;
	}

	/* return an error in case when we do not have allocated memory for model data */
	if (!blob_handler->data_new) {
		comp_err(blob_handler->dev, "comp_data_blob_set_cmd(): buffer not allocated");
		return -ENOMEM;
	}

	ret = memcpy_s((char *)blob_handler->data_new + blob_handler->data_pos,
		       blob_handler->new_data_size - blob_handler->data_pos,
		       fragment, fragment_size);
	if (ret) {
		comp_err(blob_handler->dev, "comp_data_blob_set_cmd(): failed to copy fragment");
		return ret;
	}

	blob_handler->data_pos += fragment_size;

	if (pos == MODULE_CFG_FRAGMENT_SINGLE || pos == MODULE_CFG_FRAGMENT_LAST) {
		comp_dbg(blob_handler->dev, "comp_data_blob_set_cmd(): final package received");

		/* If component state is READY we can omit old
		 * configuration immediately. When in playback/capture
		 * the new configuration presence is checked in copy().
		 */
		if (blob_handler->dev->state ==  COMP_STATE_READY) {
			blob_handler->free(blob_handler->data);
			blob_handler->data = NULL;
		}

		/* If there is no existing configuration the received
		 * can be set to current immediately. It will be
		 * applied in prepare() when streaming starts.
		 */
		if (!blob_handler->data) {
			blob_handler->data = blob_handler->data_new;
			blob_handler->data_size = blob_handler->new_data_size;

			blob_handler->data_new = NULL;

			/* The new configuration has been applied */
			blob_handler->data_ready = false;
			blob_handler->new_data_size = 0;
			blob_handler->data_pos = 0;
		} else {
			/* The new configuration is ready to be applied */
			blob_handler->data_ready = true;
		}
	}

	return 0;
}

int comp_data_blob_set_cmd(struct comp_data_blob_handler *blob_handler,
			   struct sof_ipc_ctrl_data *cdata)
{
	int ret = 0;

	assert(blob_handler);

	comp_dbg(blob_handler->dev, "comp_data_blob_set_cmd() msg_index = %d, num_elems = %d, remaining = %d ",
		 cdata->msg_index, cdata->num_elems,
		 cdata->elems_remaining);

	/* Check that there is no work-in-progress previous request */
	if (blob_handler->data_new && cdata->msg_index == 0) {
		comp_err(blob_handler->dev, "comp_data_blob_set_cmd(), busy with previous request");
		return -EBUSY;
	}

	/* In single blob mode the component can not be reconfigured if
	 * the component is active.
	 */
	if (blob_handler->single_blob &&
	    blob_handler->dev->state == COMP_STATE_ACTIVE) {
		comp_err(blob_handler->dev, "comp_data_blob_set_cmd(), on the fly updates forbidden in single blob mode");
		return -EBUSY;
	}

	/* in case when the current package is the first, we should allocate
	 * memory for whole model data
	 */
	if (!cdata->msg_index) {
		/* in case when required model size is equal to zero we do not
		 * allocate memory and should just return 0.
		 *
		 * Set cmd with cdata->data->size equal to 0 is possible in
		 * following situation:
		 * 1. At first boot and topology parsing stage, the driver will
		 * read all initial values of DSP kcontrols via IPC. Driver send
		 * get_model() cmd to components. If we do not initialize
		 * component earlier driver will get "model" with size 0.
		 * 2. When resuming from runtime suspended, the driver will
		 * restore all pipelines and kcontrols, for the tlv binary
		 * kcontrols, it will call the set_model() with the cached value
		 * and size (0 if it is not updated by any actual end user
		 * sof-ctl settings) - basically driver will send set_model()
		 * command with size equal to 0.
		 */
		if (!cdata->data->size)
			return 0;

		if (blob_handler->single_blob) {
			if (cdata->data->size != blob_handler->data_size) {
				blob_handler->free(blob_handler->data);
				blob_handler->data = NULL;
			} else {
				blob_handler->data_new = blob_handler->data;
				blob_handler->data = NULL;
			}
		}

		if (!blob_handler->data_new) {
			blob_handler->data_new =
				blob_handler->alloc(cdata->data->size);
			if (!blob_handler->data_new) {
				comp_err(blob_handler->dev, "comp_data_blob_set_cmd(): blob_handler->data_new allocation failed.");
				return -ENOMEM;
			}
		}

		blob_handler->new_data_size = cdata->data->size;
		blob_handler->data_ready = false;
		blob_handler->data_pos = 0;
	}

	/* return an error in case when we do not have allocated memory for
	 * model data
	 */
	if (!blob_handler->data_new) {
		comp_err(blob_handler->dev, "comp_data_blob_set_cmd(): buffer not allocated");
		return -ENOMEM;
	}

	ret = memcpy_s((char *)blob_handler->data_new + blob_handler->data_pos,
		       blob_handler->new_data_size - blob_handler->data_pos,
		       cdata->data->data, cdata->num_elems);
	assert(!ret);

	blob_handler->data_pos += cdata->num_elems;

	if (!cdata->elems_remaining) {
		comp_dbg(blob_handler->dev, "comp_data_blob_set_cmd(): final package received");

		/* If component state is READY we can omit old
		 * configuration immediately. When in playback/capture
		 * the new configuration presence is checked in copy().
		 */
		if (blob_handler->dev->state ==  COMP_STATE_READY) {
			blob_handler->free(blob_handler->data);
			blob_handler->data = NULL;
		}

		/* If there is no existing configuration the received
		 * can be set to current immediately. It will be
		 * applied in prepare() when streaming starts.
		 */
		if (!blob_handler->data) {
			blob_handler->data = blob_handler->data_new;
			blob_handler->data_size = blob_handler->new_data_size;

			blob_handler->data_new = NULL;

			/* The new configuration has been applied */
			blob_handler->data_ready = false;
			blob_handler->new_data_size = 0;
			blob_handler->data_pos = 0;
		} else {
			/* The new configuration is ready to be applied */
			blob_handler->data_ready = true;
		}
	}

	return 0;
}

int comp_data_blob_get_cmd(struct comp_data_blob_handler *blob_handler,
			   struct sof_ipc_ctrl_data *cdata, int size)
{
	int ret = 0;

	assert(blob_handler);

	comp_dbg(blob_handler->dev, "comp_data_blob_get_cmd() msg_index = %d, num_elems = %d, remaining = %d ",
		 cdata->msg_index, cdata->num_elems,
		 cdata->elems_remaining);

	/* Copy back to user space */
	if (blob_handler->data) {
		/* reset data_pos variable in case of copying first element */
		if (!cdata->msg_index) {
			blob_handler->data_pos = 0;
			comp_dbg(blob_handler->dev, "comp_data_blob_get_cmd() model data_size = 0x%x",
				 blob_handler->data_size);
		}

		/* return an error in case of mismatch between num_elems and
		 * required size
		 */
		if (cdata->num_elems > size) {
			comp_err(blob_handler->dev, "comp_data_blob_get_cmd(): invalid cdata->num_elems %d",
				 cdata->num_elems);
			return -EINVAL;
		}

		/* copy required size of data */
		ret = memcpy_s(cdata->data->data, size,
			       (char *)blob_handler->data + blob_handler->data_pos,
			       cdata->num_elems);
		assert(!ret);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = blob_handler->data_size;
		blob_handler->data_pos += cdata->num_elems;
	} else {
		comp_warn(blob_handler->dev, "comp_data_blob_get_cmd(): model->data not allocated yet.");
		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = 0;
	}

	return ret;
}

static void *default_alloc(size_t size)
{
	return rballoc(0, SOF_MEM_CAPS_RAM, size);
}

static void default_free(void *buf)
{
	rfree(buf);
}

struct comp_data_blob_handler *
comp_data_blob_handler_new_ext(struct comp_dev *dev, bool single_blob,
			       void *(*alloc)(size_t size),
			       void (*free)(void *buf))
{
	struct comp_data_blob_handler *handler;

	comp_dbg(dev, "comp_data_blob_handler_new_ext()");

	handler = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			  sizeof(struct comp_data_blob_handler));

	if (handler) {
		handler->dev = dev;
		handler->single_blob = single_blob;
		handler->alloc = alloc ? alloc : default_alloc;
		handler->free = free ? free : default_free;
	}

	return handler;
}

void comp_data_blob_handler_free(struct comp_data_blob_handler *blob_handler)
{
	if (!blob_handler)
		return;

	comp_free_data_blob(blob_handler);

	rfree(blob_handler);
}
