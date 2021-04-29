// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <rimage/cavs/cavs_ext_manifest.h>
#include <rimage/sof/user/manifest.h>

#include <ipc4/header.h>
#include <ipc4/pipeline.h>
#include <ipc4/module.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern struct tr_ctx comp_tr;

void ipc_build_stream_posn(struct sof_ipc_stream_posn *posn, uint32_t type,
			   uint32_t id)
{
}

void ipc_build_comp_event(struct sof_ipc_comp_event *event, uint32_t type,
			  uint32_t id)
{
}

void ipc_build_trace_posn(struct sof_ipc_dma_trace_posn *posn)
{
}

/* used with IPC4 - placeholder atm */
int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params)
{
	return 0;
}

int32_t ipc_comp_pipe_id(const struct ipc_comp_dev *icd)
{
#if 0
	switch (icd->type) {
	case COMP_TYPE_COMPONENT:
		return dev_comp_pipe_id(icd->cd);
	case COMP_TYPE_BUFFER:
		return icd->cb->pipeline_id;
	case COMP_TYPE_PIPELINE:
		return icd->pipeline->pipeline_id;
	default:
		tr_err(&ipc_tr, "Unknown ipc component type %u", icd->type);
		return -EINVAL;
	};
#endif
	return 0;
}

/* used with IPC4 new module - placeholder atm */
struct comp_dev *comp_new(struct sof_ipc_comp *comp)
{
	return NULL;
}

int ipc_pipeline_new(struct ipc *ipc, ipc_pipe_new *_pipe_desc)
{
	struct ipc4_pipeline_create *pipe_desc = ipc_from_pipe_new(_pipe_desc);
	struct ipc_comp_dev *ipc_pipe;
	struct pipeline *pipe;

	/* check whether pipeline id is already taken or in use */
	ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
					  pipe_desc->header.r.instance_id);
	if (ipc_pipe) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline id is already taken, pipe_desc->instance_id = %u",
		       (uint32_t)pipe_desc->header.r.instance_id);
		return -EINVAL;
	}

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc->header.r.instance_id,
			    pipe_desc->header.r.ppl_priority, 0);
	if (!pipe) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline_new() failed");
		return -ENOMEM;
	}

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			   sizeof(struct ipc_comp_dev));
	if (!ipc_pipe) {
		pipeline_free(pipe);
		return -ENOMEM;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;
	//ipc_pipe->core = pipe_desc->core;
	ipc_pipe->id = pipe_desc->header.r.instance_id;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->comp_list);

	return 0;
}

int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_pipe)
		return -ENODEV;

	/* check core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc_process_on_core(ipc_pipe->core);

	/* free buffer and remove from list */
	ret = pipeline_free(ipc_pipe->pipeline);
	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): pipeline_free() failed");
		return ret;
	}
	ipc_pipe->pipeline = NULL;
	list_item_del(&ipc_pipe->list);
	rfree(ipc_pipe);

	return 0;
}

/* not used with IPC4 - placeholder atm */
int ipc_pipeline_complete(struct ipc *ipc, uint32_t comp_id)
{
	return 0;
}

/* not used with IPC4 - placeholder atm */
int ipc_buffer_new(struct ipc *ipc, struct sof_ipc_buffer *desc)
{
	return 0;
}

/* not used with IPC4 - placeholder atm */
int ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id)
{
	return 0;
}

/* used with IPC4 - placeholder atm */
int ipc_comp_connect(struct ipc *ipc, ipc_pipe_comp_connect *_connect)
{

	return 0;
}

/* used with IPC4 - placeholder atm */
int ipc_comp_new(struct ipc *ipc, ipc_comp *_comp)
{
	return 0;
}

/* used with IPC4 - placeholder atm */
int ipc_comp_free(struct ipc *ipc, uint32_t comp_id)
{
	return 0;
}

/* not used with IPC4 - placeholder atm */
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc)
{
	return NULL;
}

static const struct comp_driver *module_driver[FW_MAX_EXT_MODULE_NUM] = {0};

const struct comp_driver *ipc4_get_drv(uint8_t *uuid)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;
	const struct comp_driver *drv = NULL;
	struct comp_driver_info *info;
	uint32_t flags;

	irq_local_disable(flags);

	/* search driver list with UUID */
	list_for_item(clist, &drivers->list) {
		info = container_of(clist, struct comp_driver_info,
				    list);
		if (!memcmp(info->drv->uid, uuid, UUID_SIZE)) {
			tr_dbg(&comp_tr,
			       "get_drv_from_uuid(), found driver type %d, uuid %pU",
			       info->drv->type,
			       info->drv->tctx->uuid_p);
			drv = info->drv;
			goto out;
		}
	}

	tr_err(&comp_tr, "get_drv(): the provided UUID (%8x%8x%8x%8x) doesn't match to any driver!",
	       *(uint32_t *)(&uuid[0]),
	       *(uint32_t *)(&uuid[4]),
	       *(uint32_t *)(&uuid[8]),
	       *(uint32_t *)(&uuid[12]));

out:
	irq_local_enable(flags);
	return drv;
}

const struct comp_driver *ipc4_get_comp_drv(int module_id)
{
	struct sof_man_fw_desc *desc = (struct sof_man_fw_desc *)IMR_BOOT_LDR_MANIFEST_BASE;
	struct sof_man_module *mod;
	const struct comp_driver *drv;

	if (module_driver[module_id])
		return module_driver[module_id];

	/* skip basefw of module 0 in manifest */
	mod = (struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(module_id + 1));
	drv = ipc4_get_drv(mod->uuid);
	module_driver[module_id] = drv;

	return drv;
}

struct comp_dev *ipc4_get_comp_dev(int comp_id)
{
	return NULL;
}

void ipc4_add_comp_dev(struct comp_dev *dev)
{
};
