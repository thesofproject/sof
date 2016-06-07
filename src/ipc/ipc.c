/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/ipc.h>
#include <reef/debug.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

static struct ipc *_ipc;

/* each componnet has unique ID within the pipeline */
struct ipc_comp_dev *ipc_get_comp(uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_head *plist;

	list_for_each(plist, &_ipc->comp_list) {

		icd = container_of(plist, struct ipc_comp_dev, list);

		if (icd->cd->id == id)
			return icd;
	}

	return NULL;
}

/* each buffer has unique ID within the pipeline */
struct ipc_buffer_dev *ipc_get_buffer(uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_head *plist;

	list_for_each(plist, &_ipc->buffer_list) {

		icd = container_of(plist, struct ipc_comp_dev, list);

		if (icd->cb->id == id)
			return (struct ipc_buffer_dev *)icd;
	}

	return NULL;
}

// todo make this type safe and provide new calls and accessor for each type
int ipc_comp_new(int pipeline_id, uint32_t type, uint32_t index,
	uint8_t direction)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);
	struct ipc_comp_dev *icd;

	if (p == NULL)
		return -EINVAL;

	/* allocate correct size for structure type */
	switch (type) {
	case COMP_TYPE_DAI_SSP:
	case COMP_TYPE_DAI_HDA:
		icd = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(struct ipc_dai_dev));
		break;
	case COMP_TYPE_HOST:
		icd = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(struct ipc_pcm_dev));
		break;
	default:
		icd = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(*icd));
		break;
	}
	if (icd == NULL)
		return -ENOMEM;

	/* register component with piepline */
	icd->cd = pipeline_comp_new(p, type, index, direction);
	if (icd->cd == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, icd);
		return -ENOMEM;
	}

	/* init IPC comp */
	icd->pipeline_id = pipeline_id;
	icd->p = p;

	list_add(&icd->list, &_ipc->comp_list);
	return icd->cd->id;
}

void ipc_comp_free(uint32_t comp_id)
{
	struct ipc_comp_dev *icd = ipc_get_comp(comp_id);

	if (icd == NULL)
		return;

	pipeline_comp_free(icd->p, icd->cd);
	list_del(&icd->list);
	rfree(RZONE_MODULE, RMOD_SYS, icd);
}

int ipc_buffer_new(int pipeline_id, struct buffer_desc *desc)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);
	struct ipc_comp_dev *icb;

	if (p == NULL)
		return -EINVAL;

	icb = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(struct ipc_buffer_dev));
	if (icb == NULL)
		return -ENOMEM;

	/* register buffer with piepline */
	icb->cb = pipeline_buffer_new(p, desc);
	if (icb->cb == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, icb);
		return -ENOMEM;
	}

	/* init IPC buffer */
	icb->pipeline_id = pipeline_id;
	icb->p = p;

	list_add(&icb->list, &_ipc->buffer_list);
	return icb->cb->id;
}

void ipc_buffer_free(uint32_t comp_id)
{
	struct ipc_buffer_dev *icb = ipc_get_buffer(comp_id);

	if (icb == NULL)
		return;

	pipeline_buffer_free(icb->dev.p, icb->dev.cb);
	list_del(&icb->dev.list);
	rfree(RZONE_MODULE, RMOD_SYS, icb);
}

int ipc_comp_connect(uint32_t source_id, uint32_t sink_id, uint32_t buffer_id)
{
	struct ipc_comp_dev *icd_source = ipc_get_comp(source_id);
	struct ipc_comp_dev *icd_sink = ipc_get_comp(sink_id);
	struct ipc_buffer_dev *ibd = ipc_get_buffer(buffer_id);

	/* valid source and sink */
	if (icd_sink == NULL || icd_source == NULL || ibd == NULL)
		return -EINVAL;

	/* must be on same pipeline */
	if (icd_sink->p != icd_source->p)
		return -EINVAL;

	return pipeline_comp_connect(icd_sink->p,
		icd_source->cd, icd_sink->cd, ibd->dev.cb);
}

int ipc_init(void)
{
	/* init ipc data */
	_ipc = rzalloc(RZONE_DEV, RMOD_SYS, sizeof(*_ipc));
	list_init(&_ipc->comp_list);
	list_init(&_ipc->buffer_list);
	_ipc->host_pending = 0;

	return platform_ipc_init(_ipc);
}
