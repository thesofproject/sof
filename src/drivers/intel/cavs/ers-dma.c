// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <sof/drivers/ers-dma.h>
#include <sof/lib/alloc.h>
#include <sof/audio/component.h>

#define trace_ersdma(__e, ...) \
	trace_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define tracev_ersdma(__e, ...) \
	tracev_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define trace_ersdma_error(__e, ...) \
	trace_error(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)

struct ers_dma_pdata {
	/* client callback function */
	void (*cb)(void *data, uint32_t type, struct dma_cb_data *next);
	/* client callback data */
	void *cb_data;
	/* callback type */
	int cb_type;

	struct buffer_callback src_buff_cb;
	struct comp_buffer *src_buff;

	uint32_t dst_w_ptr;
	uint32_t dst_idx;
	void *src_r_ptr;
	uint32_t src_data_size;
	struct dma_sg_elem_array *elem_array;

	struct task work;
};

static int ers_dma_channel_get(struct dma *dma, unsigned int req_channel)
{
	trace_ersdma("ers_dma_channel_get(%u)", req_channel);
	return 0;
}

static void ers_dma_channel_put(struct dma *dma, unsigned int channel)
{
	trace_ersdma("ers_dma_channel_put(%u)", channel);
}

static int ers_dma_start(struct dma *dma, unsigned int channel)
{
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	trace_ersdma("ers_dma_start(%u)", channel);

	schedule_task(&pdata->work, 0, 1000, 0);

	return 0;
}

static int ers_dma_stop(struct dma *dma, unsigned int channel)
{
	trace_ersdma("ers_dma_stop(%u)", channel);

	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	schedule_task_cancel(&pdata->work);

	return 0;
}

static inline void *ers_dma_get_dst(struct dma *dma, uint32_t *size)
{
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	*size = pdata->elem_array->elems[pdata->dst_idx].size - pdata->dst_w_ptr;

	return (void *)pdata->elem_array->elems[pdata->dst_idx].dest + pdata->dst_w_ptr;
}

static inline void ers_dma_update_dst(struct dma *dma, uint32_t used)
{
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);
	struct dma_sg_elem *elem = &pdata->elem_array->elems[pdata->dst_idx];
	uint32_t current = elem->size - pdata->dst_w_ptr;

	dcache_writeback_region((void *)elem->dest + pdata->dst_w_ptr, used);

	if (current == used) {
		pdata->dst_idx = (pdata->dst_idx + 1) % pdata->elem_array->count;
		pdata->dst_w_ptr = 0;
	} else {
		pdata->dst_w_ptr += used;
	}
}

static int ers_dma_copy(struct dma *dma, unsigned int channel, int bytes, uint32_t flags)
{
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	struct dma_cb_data next = {
		.elem = {
			.src = 0,
			.dest = 0,
			.size = bytes
		}
	};

	void *dst;
	void *src;
	uint32_t copy_size;
	uint32_t dst_size;
	uint32_t i;

	while (bytes > 0) {
		src = NULL;
		copy_size = 0;

		if (pdata->src_buff) {
			src = pdata->src_r_ptr;
			copy_size = pdata->src_buff->w_ptr <= pdata->src_r_ptr ?
				pdata->src_buff->end_addr - pdata->src_r_ptr :
				pdata->src_buff->w_ptr - pdata->src_r_ptr;
			copy_size = MIN(MIN(copy_size, bytes), pdata->src_data_size);
			pdata->src_r_ptr += copy_size;
			if (pdata->src_r_ptr == pdata->src_buff->end_addr)
				pdata->src_r_ptr = pdata->src_buff->addr;
			bytes -= copy_size;
			pdata->src_data_size -= copy_size;

			dcache_invalidate_region(src, copy_size);
		}

		/* if copy_size is zero, src_buffer is not attached or we've
		 * run over the w_ptr, fill in zeros to match bytes.
		 * if copy_size > 0, copy data to dest.
		 */
		if (copy_size == 0) {
			src = NULL;
			copy_size = bytes;
			bytes = 0;
		}

		while (copy_size > 0) {
			dst = ers_dma_get_dst(dma, &dst_size);
			dst_size = MIN(copy_size, dst_size);

			if (src) {
				memcpy_s(dst, dst_size, src, dst_size);
			} else {
				for (i = 0; i < dst_size; i++)
					((uint8_t *)dst)[i] = 0;
			}

			ers_dma_update_dst(dma, dst_size);

			copy_size -= dst_size;
		}
	}

	pdata->cb(pdata->cb_data, DMA_CB_TYPE_COPY, &next);

	return 0;
}

static int ers_dma_pause(struct dma *dma, unsigned int channel)
{
	trace_ersdma("ers_dma_pause(%u)", channel);
	return 0;
}

static int ers_dma_release(struct dma *dma, unsigned int channel)
{
	trace_ersdma("ers_dma_release(%u)", channel);
	return 0;
}

static int ers_dma_status(struct dma *dma, unsigned int channel,
			  struct dma_chan_status *status, uint8_t direction)
{
	trace_ersdma("ers_dma_status()");
	return 0;
}

static int ers_dma_set_config(struct dma *dma, unsigned int channel,
			      struct dma_sg_config *config)
{
	struct comp_buffer *buff = (struct comp_buffer *)config->src_dev;
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	trace_ersdma("ers_dma_set_config()");

	pdata->src_buff = buff;
	if (pdata->src_buff) {
		pdata->src_r_ptr = pdata->src_buff->r_ptr;
		buffer_add_callback(pdata->src_buff, &pdata->src_buff_cb);
	}

	pdata->elem_array = &config->elem_array;

	return 0;
}

static int ers_dma_set_cb(struct dma *dma, unsigned int channel, int type,
			  void (*cb)(void *data, uint32_t type, struct dma_cb_data *next),
			  void *data)
{
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);
	uint32_t flags;

	trace_ersdma("ers_dma_set_cb()");

	spin_lock_irq(&dma->lock, flags);
	pdata->cb = cb;
	pdata->cb_data = data;
	pdata->cb_type = type;
	spin_unlock_irq(&dma->lock, flags);

	return 0;
}

static int ers_dma_pm_context_restore(struct dma *dma)
{
	trace_ersdma("ers_dma_pm_context_restore()");
	return 0;
}

static int ers_dma_pm_context_store(struct dma *dma)
{
	trace_ersdma("ers_dma_pm_context_store()");
	return 0;
}

static uint64_t ers_dma_task(void *arg)
{
	struct dma *dma = arg;
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	pdata->cb(pdata->cb_data, DMA_CB_TYPE_IRQ, NULL);

	// reschedule
	return 1000;
}

static void ers_dma_src_buff_cb(void *arg, int type, void *data)
{
	struct dma *dma = arg;
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	switch (type) {
	case BUFF_CB_TYPE_FREE_COMP:
		pdata->src_buff = NULL;
		pdata->src_data_size = 0;
		break;
	case BUFF_CB_TYPE_PRODUCE:
		pdata->src_data_size += *(uint32_t *)data;
		break;
	}
}

static int ers_dma_probe(struct dma *dma)
{
	struct ers_dma_pdata *pdata;

	trace_ersdma("ers_dma_probe()");

	if (dma_get_drvdata(dma))
		return -EEXIST;

	pdata = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
		SOF_MEM_CAPS_RAM, sizeof(*pdata));

	if (!pdata) {
		trace_ersdma_error("ers_dma_probe() error: dma %d alloc failed",
				   dma->plat_data.id);
		return -ENOMEM;
	}
	dma_set_drvdata(dma, pdata);

	spinlock_init(&dma->lock);

	schedule_task_init(&pdata->work, SOF_SCHEDULE_LL, 1, ers_dma_task, dma, 0, 0);

	pdata->src_buff_cb.cb_arg = dma;
	pdata->src_buff_cb.cb_type = BUFF_CB_TYPE_PRODUCE | BUFF_CB_TYPE_FREE_COMP;
	pdata->src_buff_cb.cb = &ers_dma_src_buff_cb;
	list_init(&pdata->src_buff_cb.list);

	return 0;
}

static int ers_dma_remove(struct dma *dma)
{
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	trace_ersdma("ers_dma_remove()");

	schedule_task_free(&pdata->work);

	return 0;
}

static int ers_dma_data_size(struct dma *dma, unsigned int channel, uint32_t *avail,
			     uint32_t *free)
{
	struct ers_dma_pdata *pdata = dma_get_drvdata(dma);

	*avail = pdata->elem_array->elems[pdata->dst_idx].size;

	/* if we're falling behind too much, attempt to catch up */
	if (pdata->src_data_size >= *avail * pdata->elem_array->count)
		*avail *= 2;

	return 0;
}

const struct dma_ops ers_dma_ops = {
	.channel_get		= ers_dma_channel_get,
	.channel_put		= ers_dma_channel_put,
	.start			= ers_dma_start,
	.stop			= ers_dma_stop,
	.copy			= ers_dma_copy,
	.pause			= ers_dma_pause,
	.release		= ers_dma_release,
	.status			= ers_dma_status,
	.set_config		= ers_dma_set_config,
	.set_cb			= ers_dma_set_cb,
	.pm_context_restore	= ers_dma_pm_context_restore,
	.pm_context_store	= ers_dma_pm_context_store,
	.probe			= ers_dma_probe,
	.remove			= ers_dma_remove,
	.get_data_size		= ers_dma_data_size,
};
