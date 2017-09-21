/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *
 * IPC (InterProcessor Communication) provides a method of two way
 * communication between the host processor and the DSP. The IPC used here
 * utilises a shared mailbox and door bell between the host and DSP.
 *
 */

#include <reef/debug.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <reef/ipc.h>
#include <reef/mailbox.h>
#include <reef/reef.h>
#include <reef/stream.h>
#include <reef/dai.h>
#include <reef/dma.h>
#include <reef/alloc.h>
#include <reef/wait.h>
#include <reef/trace.h>
#include <reef/ssp.h>
#include <platform/interrupt.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <platform/dma.h>
#include <platform/timer.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <uapi/ipc.h>
#include <reef/intel-ipc.h>
#include <config.h>

#define iGS(x) ((x >> SOF_GLB_TYPE_SHIFT) & 0xf)
#define iCS(x) ((x >> SOF_CMD_TYPE_SHIFT) & 0xfff)

/* IPC context - shared with platform IPC driver */
struct ipc *_ipc;

static inline struct sof_ipc_hdr *mailbox_validate(void)
{
	struct sof_ipc_hdr *hdr = _ipc->comp_data;

	/* read component values from the inbox */
	mailbox_hostbox_read(hdr, 0, sizeof(*hdr));

	/* validate component header */
	if (hdr->size > SOF_IPC_MSG_MAX_SIZE) {
		trace_ipc_error("ebg");
		return NULL;
	}

	/* read rest of component data */
	mailbox_hostbox_read(hdr + 1, sizeof(*hdr), hdr->size - sizeof(*hdr));
	return hdr;
}

static void dma_complete(void *data, uint32_t type, struct dma_sg_elem *next)
{
	struct intel_ipc_data *iipc = (struct intel_ipc_data *)data;

	if (type == DMA_IRQ_TYPE_LLIST)
		wait_completed(&iipc->complete);
}

/*
 * Copy the audio buffer page tables from the host to the DSP max of 4K.
 */
static int get_page_descriptors(struct intel_ipc_data *iipc,
	struct sof_ipc_host_buffer *ring)
{
	struct dma_sg_config config;
	struct dma_sg_elem elem;
	struct dma *dma;
	int chan, ret = 0;

	/* get DMA channel from DMAC0 */
	chan = dma_channel_get(iipc->dmac0);
	if (chan < 0) {
		trace_ipc_error("ePC");
		return chan;
	}
	dma = iipc->dmac0;

	/* set up DMA configuration */
	config.direction = DMA_DIR_HMEM_TO_LMEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	list_init(&config.elem_list);

	/* set up DMA desciptor */
	elem.dest = (uint32_t)iipc->page_table;
	elem.src = ring->phy_addr;

	/* source buffer size is always PAGE_SIZE bytes */
	/* 20 bits for each page, round up to 32 */
	elem.size = (ring->pages * 5 * 16 + 31) / 32;
	list_item_prepend(&elem.list, &config.elem_list);

	ret = dma_set_config(dma, chan, &config);
	if (ret < 0) {
		trace_ipc_error("ePs");
		goto out;
	}

	/* set up callback */
	dma_set_cb(dma, chan, DMA_IRQ_TYPE_LLIST, dma_complete, iipc);

	wait_init(&iipc->complete);

	/* start the copy of page table to DSP */
	dma_start(dma, chan);

	/* wait for DMA to complete */
	iipc->complete.timeout = PLATFORM_HOST_DMA_TIMEOUT;
	ret = wait_for_completion_timeout(&iipc->complete);

	/* compressed page tables now in buffer at _ipc->page_table */
out:
	dma_channel_put(dma, chan);
	return ret;
}

/*
 * Parse the host page tables and create the audio DMA SG configuration
 * for host audio DMA buffer. This involves creating a dma_sg_elem for each
 * page table entry and adding each elem to a list in struct dma_sg_config.
 */
static int parse_page_descriptors(struct intel_ipc_data *iipc,
	struct sof_ipc_host_buffer *ring, void *data, uint32_t is_trace)
{
	struct comp_dev* cd = NULL;
	struct sof_ipc_comp_host *host = NULL;
	struct dma_trace_data *d = NULL;
	struct dma_sg_elem elem;
	int i, err;
	uint32_t idx, phy_addr;

	elem.size = HOST_PAGE_SIZE;
	if (is_trace)
		d = (struct dma_trace_data *)data;
	else {
		cd = (struct comp_dev *)data;
		host = (struct sof_ipc_comp_host *)&cd->comp;
	}

	for (i = 0; i < ring->pages; i++) {

		idx = (((i << 2) + i)) >> 1;
		phy_addr = iipc->page_table[idx] | (iipc->page_table[idx + 1] << 8)
				| (iipc->page_table[idx + 2] << 16);

		if (i & 0x1)
			phy_addr <<= 8;
		else
			phy_addr <<= 12;
		phy_addr &= 0xfffff000;

		if (!is_trace && host->direction == SOF_IPC_STREAM_PLAYBACK)
			elem.src = phy_addr;
		else
			elem.dest = phy_addr;

		if (is_trace)
			err = dma_trace_host_buffer(d, &elem, ring->size);
		else
			err = comp_host_buffer(cd, &elem, ring->size);
		if (err < 0) {
			trace_ipc_error("ePb");
			return err;
		}
	}

	return 0;
}

/*
 * Stream IPC Operations.
 */

/* allocate a new stream */
static int ipc_stream_pcm_params(uint32_t stream)
{
	struct intel_ipc_data *iipc = ipc_get_drvdata(_ipc);
	struct sof_ipc_pcm_params *pcm_params = _ipc->comp_data;
	struct sof_ipc_pcm_params_reply reply;
	struct ipc_comp_dev *pcm_dev;
	struct comp_dev *cd;
	int err;

	trace_ipc("SAl");

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp(_ipc, pcm_params->comp_id);
	if (pcm_dev == NULL) {
		trace_ipc_error("eAC");
		trace_value(pcm_params->comp_id);
		return -EINVAL;
	}

	/* sanity check comp */
	if (pcm_dev->cd->pipeline == NULL) {
		trace_ipc_error("eA1");
		trace_value(pcm_params->comp_id);
		return -EINVAL;
	}

	/* set params component params */
	cd = pcm_dev->cd;
	cd->params = pcm_params->params;

	/* use DMA to read in compressed page table ringbuffer from host */
	err = get_page_descriptors(iipc, &pcm_params->params.buffer);
	if (err < 0) {
		trace_ipc_error("eAp");
		goto error;
	}

	/* Parse host tables */
	err = parse_page_descriptors(iipc, &pcm_params->params.buffer,
		pcm_dev->cd, 0);
	if (err < 0) {
		trace_ipc_error("eAP");
		goto error;
	}

	/* configure pipeline audio params */
	err = pipeline_params(pcm_dev->cd->pipeline, pcm_dev->cd, pcm_params);
	if (err < 0) {
		trace_ipc_error("eAa");
		goto error;
	}

	/* prepare pipeline audio params */
	err = pipeline_prepare(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (err < 0) {
		trace_ipc_error("eAr");
		goto error;
	}


	/* write component values to the outbox */
	reply.rhdr.hdr.size = sizeof(reply);
	reply.rhdr.hdr.cmd = stream;
	reply.rhdr.error = 0;
	reply.comp_id = pcm_params->comp_id;
	reply.posn_offset = 0; /* TODO: set this up for mmaped components */
	mailbox_hostbox_write(0, &reply, sizeof(reply));
	return 1;

error:
	err = pipeline_reset(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (err < 0)
		trace_ipc_error("eA!");
	return -EINVAL;
}

/* free stream resources */
static int ipc_stream_pcm_free(uint32_t header)
{
	struct sof_ipc_stream *free_req = _ipc->comp_data;
	struct ipc_comp_dev *pcm_dev;

	trace_ipc("SFr");

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp(_ipc, free_req->comp_id);
	if (pcm_dev == NULL) {
		trace_ipc_error("eFr");
		return -ENODEV;
	}

	/* sanity check comp */
	if (pcm_dev->cd->pipeline == NULL) {
		trace_ipc_error("eF1");
		trace_value(free_req->comp_id);
		return -EINVAL;
	}

	/* reset the pipeline */
	return pipeline_reset(pcm_dev->cd->pipeline, pcm_dev->cd);
}

/* get stream position */
static int ipc_stream_position(uint32_t header)
{
	struct sof_ipc_stream *stream = _ipc->comp_data;
	struct sof_ipc_stream_posn posn;
	struct ipc_comp_dev *pcm_dev;

	trace_ipc("pos");

	memset(&posn, 0, sizeof(posn));

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp(_ipc, stream->comp_id);
	if (pcm_dev == NULL) {
		trace_ipc_error("epo");
		return -ENODEV;
	}

	/* set message fields - TODO; get others */
	posn.rhdr.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | SOF_IPC_STREAM_POSITION;
	posn.rhdr.hdr.size = sizeof(posn);
	posn.comp_id = stream->comp_id;

	/* get the stream positions and timestamps */
	pipeline_get_timestamp(pcm_dev->cd->pipeline, pcm_dev->cd, &posn);

	/* copy positions to outbox */
	mailbox_hostbox_write(0, &posn, sizeof(posn));
	return 1;
}

/* send stream position */
int ipc_stream_send_position(struct comp_dev *cdev,
	struct sof_ipc_stream_posn *posn)
{
	posn->rhdr.hdr.cmd =  SOF_IPC_GLB_STREAM_MSG | SOF_IPC_STREAM_POSITION;
	posn->rhdr.hdr.size = sizeof(*posn);
	posn->comp_id = cdev->comp.id;

	return ipc_queue_host_message(_ipc, posn->rhdr.hdr.cmd, posn,
		sizeof(*posn), NULL, 0, NULL, NULL);
}

/* send stream position TODO: send compound message  */
int ipc_stream_send_xrun(struct comp_dev *cdev,
	struct sof_ipc_stream_posn *posn)
{
	posn->rhdr.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | SOF_IPC_STREAM_TRIG_XRUN;
	posn->rhdr.hdr.size = sizeof(*posn);
	posn->comp_id = cdev->comp.id;

	return ipc_queue_host_message(_ipc, posn->rhdr.hdr.cmd, posn,
		sizeof(*posn), NULL, 0, NULL, NULL);
}

static int ipc_stream_trigger(uint32_t header)
{
	struct ipc_comp_dev *pcm_dev;
	uint32_t cmd = COMP_CMD_RELEASE;
	struct sof_ipc_stream *stream  = _ipc->comp_data;
	uint32_t ipc_cmd = (header & SOF_CMD_TYPE_MASK) >> SOF_CMD_TYPE_SHIFT;
	int ret;

	trace_ipc("tri");

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp(_ipc, stream->comp_id);
	if (pcm_dev == NULL) {
		trace_ipc_error("eRg");
		return -ENODEV;
	}

	switch (ipc_cmd) {
	case iCS(SOF_IPC_STREAM_TRIG_START):
		cmd = COMP_CMD_START;
		break;
	case iCS(SOF_IPC_STREAM_TRIG_STOP):
		cmd = COMP_CMD_STOP;
		break;
	case iCS(SOF_IPC_STREAM_TRIG_PAUSE):
		cmd = COMP_CMD_PAUSE;
		break;
	case iCS(SOF_IPC_STREAM_TRIG_RELEASE):
		cmd = COMP_CMD_RELEASE;
		break;
	/* XRUN is special case- TODO */
	case iCS(SOF_IPC_STREAM_TRIG_XRUN):
		return 0;
	}

	/* trigger the component */
	ret = pipeline_cmd(pcm_dev->cd->pipeline, pcm_dev->cd,
			cmd, NULL);
	if (ret < 0) {
		trace_ipc_error("eRc");
		trace_value(ipc_cmd);
	}

	return ret;
}

static int ipc_glb_stream_message(uint32_t header)
{
	uint32_t cmd = (header & SOF_CMD_TYPE_MASK) >> SOF_CMD_TYPE_SHIFT;

	switch (cmd) {
	case iCS(SOF_IPC_STREAM_PCM_PARAMS):
		return ipc_stream_pcm_params(header);
	case iCS(SOF_IPC_STREAM_PCM_FREE):
		return ipc_stream_pcm_free(header);
	case iCS(SOF_IPC_STREAM_TRIG_START):
	case iCS(SOF_IPC_STREAM_TRIG_STOP):
	case iCS(SOF_IPC_STREAM_TRIG_PAUSE):
	case iCS(SOF_IPC_STREAM_TRIG_RELEASE):
	case iCS(SOF_IPC_STREAM_TRIG_DRAIN):
	case iCS(SOF_IPC_STREAM_TRIG_XRUN):
		return ipc_stream_trigger(header);
	case iCS(SOF_IPC_STREAM_POSITION):
		return ipc_stream_position(header);
	default:
		return -EINVAL;
	}
}

/*
 * DAI IPC Operations.
 */

static int ipc_dai_config(uint32_t header)
{
	struct sof_ipc_dai_config *config = _ipc->comp_data;
	struct dai *dai;
	int ret;

	trace_ipc("DsF");

	/* get DAI */
	dai = dai_get(config->type, config->id);
	if (dai == NULL) {
		trace_ipc_error("eDi");
		trace_value(config->type);
		trace_value(config->id);
		return -ENODEV;
	}

	/* configure DAI */
	ret = dai_set_config(dai, config);
	if (ret < 0) {
		trace_ipc_error("eDC");
		return ret;
	}

	/* now send params to all DAI components who use that physical DAI */
	return ipc_comp_dai_config(_ipc, config);
}

static int ipc_glb_dai_message(uint32_t header)
{
	uint32_t cmd = (header & SOF_CMD_TYPE_MASK) >> SOF_CMD_TYPE_SHIFT;

	switch (cmd) {
	case iCS(SOF_IPC_DAI_CONFIG):
		return ipc_dai_config(header);
	case iCS(SOF_IPC_DAI_LOOPBACK):
		//return ipc_comp_set_value(header, COMP_CMD_LOOPBACK);
	default:
		trace_ipc_error("eDc");
		trace_value(header);
		return -EINVAL;
	}
}

/*
 * PM IPC Operations.
 */

static int ipc_pm_context_size(uint32_t header)
{
	struct sof_ipc_pm_ctx pm_ctx;

	trace_ipc("PMs");

	bzero(&pm_ctx, sizeof(pm_ctx));

	/* TODO: calculate the context and size of host buffers required */

	/* write the context to the host driver */
	mailbox_hostbox_write(0, &pm_ctx, sizeof(pm_ctx));
	return 1;
}

static int ipc_pm_context_save(uint32_t header)
{
	struct sof_ipc_pm_ctx *pm_ctx = _ipc->comp_data;

	trace_ipc("PMs");

	/* TODO: check we are inactive - all streams are suspended */

	/* TODO: mask ALL platform interrupts except DMA */

	/* TODO now save the context - create SG buffer config using */
	//mm_pm_context_save(struct dma_sg_config *sg);

	/* mask all DSP interrupts */
	arch_interrupt_disable_mask(0xffff);

	/* TODO: mask ALL platform interrupts inc DMA */

	/* TODO: clear any outstanding platform IRQs - TODO refine */

	/* TODO: stop ALL timers */
	platform_timer_stop(platform_timer);

	/* TODO: disable SSP and DMA HW */

	/* TODO: save the context */
	//reply.entries_no = 0;

	/* write the context to the host driver */
	mailbox_hostbox_write(0, pm_ctx, sizeof(*pm_ctx));

	//iipc->pm_prepare_D3 = 1;

	return 1;
}

static int ipc_pm_context_restore(uint32_t header)
{
//	struct sof_ipc_pm_ctx pm_ctx = _ipc->comp_data;

	trace_ipc("PMr");

	/* now restore the context */

	return 0;
}

static int ipc_glb_pm_message(uint32_t header)
{
	uint32_t cmd = (header & SOF_CMD_TYPE_MASK) >> SOF_CMD_TYPE_SHIFT;

	switch (cmd) {
	case iCS(SOF_IPC_PM_CTX_SAVE):
		return ipc_pm_context_save(header);
	case iCS(SOF_IPC_PM_CTX_RESTORE):
		return ipc_pm_context_restore(header);
	case iCS(SOF_IPC_PM_CTX_SIZE):
		return ipc_pm_context_size(header);
	case iCS(SOF_IPC_PM_CLK_SET):
	case iCS(SOF_IPC_PM_CLK_GET):
	case iCS(SOF_IPC_PM_CLK_REQ):
	default:
		return -EINVAL;
	}
}

/*
 * Debug IPC Operations.
 */

static int ipc_dma_trace_init(uint32_t header)
{
	struct sof_ipc_reply reply;
	int err;

	trace_ipc("Dti");

	/* Initialize DMA for Trace*/
	err = dma_trace_init(&_ipc->dmat);
	if (err < 0) {
		trace_ipc_error("eIP");
		goto error;
	}

	/* write component values to the outbox */
	reply.hdr.size = sizeof(reply);
	reply.hdr.cmd = header;
	reply.error = 0;
	mailbox_hostbox_write(0, &reply, sizeof(reply));
	return 0;

error:
	if (err < 0)
		trace_ipc_error("eA!");
	return -EINVAL;
}

static int ipc_dma_trace_config(uint32_t header)
{
	struct intel_ipc_data *iipc = ipc_get_drvdata(_ipc);
	struct sof_ipc_dma_trace_params *params = _ipc->comp_data;
	struct sof_ipc_reply reply;
	int err;

	trace_ipc("DAl");

	/* use DMA to read in compressed page table ringbuffer from host */
	err = get_page_descriptors(iipc, &params->buffer);
	if (err < 0) {
		trace_ipc_error("eCp");
		goto error;
	}

	trace_ipc("DAg");

	/* Parse host tables */
	err = parse_page_descriptors(iipc, &params->buffer,
		&_ipc->dmat, 1);
	if (err < 0) {
		trace_ipc_error("ePP");
		goto error;
	}

	trace_ipc("DAp");

	dma_trace_config_ready(&_ipc->dmat);

	/* write component values to the outbox */
	reply.hdr.size = sizeof(reply);
	reply.hdr.cmd = header;
	reply.error = 0;
	mailbox_hostbox_write(0, &reply, sizeof(reply));
	return 0;

error:
	if (err < 0)
		trace_ipc_error("eA!");
	return -EINVAL;
}

static int ipc_glb_debug_message(uint32_t header)
{
	uint32_t cmd = (header & SOF_CMD_TYPE_MASK) >> SOF_CMD_TYPE_SHIFT;

	trace_ipc("Idn");

	switch (cmd) {
	case iCS(SOF_IPC_TRACE_DMA_INIT):
		return ipc_dma_trace_init(header);
	case iCS(SOF_IPC_TRACE_DMA_PARAMS):
		return ipc_dma_trace_config(header);
	default:
		trace_ipc_error("eDc");
		trace_value(header);
		return -EINVAL;
	}
}

/*
 * Topology IPC Operations.
 */

/* get/set component values or runtime data */
static int ipc_comp_value(uint32_t header, uint32_t cmd)
{
	struct ipc_comp_dev *stream_dev;
	struct sof_ipc_ctrl_data *data = _ipc->comp_data;
	int ret;

	trace_ipc("VoG");

	/* get the component */
	stream_dev = ipc_get_comp(_ipc, data->comp_id);
	if (stream_dev == NULL){
		trace_ipc_error("eVg");
		trace_value(data->comp_id);
		return -ENODEV;
	}
	
	/* get component values */
	ret = comp_cmd(stream_dev->cd, cmd, data);
	if (ret < 0) {
		trace_ipc_error("eVG");
		return ret;
	}

	/* write component values to the outbox */
	mailbox_hostbox_write(0, data, data->rhdr.hdr.size);
	return 1;
}

static int ipc_glb_comp_message(uint32_t header)
{
	uint32_t cmd = (header & SOF_CMD_TYPE_MASK) >> SOF_CMD_TYPE_SHIFT;

	switch (cmd) {
	case iCS(SOF_IPC_COMP_SET_VALUE):
		return ipc_comp_value(header, COMP_CMD_SET_VALUE);
	case iCS(SOF_IPC_COMP_GET_VALUE):
		return ipc_comp_value(header, COMP_CMD_GET_VALUE);
	case iCS(SOF_IPC_COMP_SET_DATA):
		return ipc_comp_value(header, COMP_CMD_SET_DATA);
	case iCS(SOF_IPC_COMP_GET_DATA):
		return ipc_comp_value(header, COMP_CMD_GET_DATA);
	default:
		trace_ipc_error("eCc");
		trace_value(header);
		return -EINVAL;
	}
}

static int ipc_glb_tplg_comp_new(uint32_t header)
{
	struct sof_ipc_comp *comp = _ipc->comp_data;
	struct sof_ipc_comp_reply reply;
	int ret;

	trace_ipc("tcn");

	/* register component */
	ret = ipc_comp_new(_ipc, comp);
	if (ret < 0) {
		trace_ipc_error("cn1");
		return ret;
	}

	/* write component values to the outbox */
	reply.rhdr.hdr.size = sizeof(reply);
	reply.rhdr.hdr.cmd = header;
	reply.rhdr.error = 0;
	reply.offset = 0; /* TODO: set this up for mmaped components */
	mailbox_hostbox_write(0, &reply, sizeof(reply));
	return 1;
}

static int ipc_glb_tplg_buffer_new(uint32_t header)
{
	struct sof_ipc_buffer *ipc_buffer = _ipc->comp_data;
	struct sof_ipc_comp_reply reply;
	int ret;

	trace_ipc("Ibn");

	ret = ipc_buffer_new(_ipc, ipc_buffer);
	if (ret < 0) {
		trace_ipc_error("bn1");
		return ret;
	}

	/* write component values to the outbox */
	reply.rhdr.hdr.size = sizeof(reply);
	reply.rhdr.hdr.cmd = header;
	reply.rhdr.error = 0;
	reply.offset = 0; /* TODO: set this up for mmaped components */
	mailbox_hostbox_write(0, &reply, sizeof(reply));
	return 1;
}

static int ipc_glb_tplg_pipe_new(uint32_t header)
{
	struct sof_ipc_pipe_new *ipc_pipeline = _ipc->comp_data;
	struct sof_ipc_comp_reply reply;
	int ret;

	trace_ipc("Ipn");

	ret = ipc_pipeline_new(_ipc, ipc_pipeline);
	if (ret < 0) {
		trace_ipc_error("pn1");
		return ret;
	}

	/* write component values to the outbox */
	reply.rhdr.hdr.size = sizeof(reply);
	reply.rhdr.hdr.cmd = header;
	reply.rhdr.error = 0;
	reply.offset = 0; /* TODO: set this up for mmaped components */
	mailbox_hostbox_write(0, &reply, sizeof(reply));
	return 1;
}

static int ipc_glb_tplg_pipe_complete(uint32_t header)
{
	struct sof_ipc_pipe_ready *ipc_pipeline = _ipc->comp_data;

	trace_ipc("Ipc");

	ipc_pipeline_complete(_ipc, ipc_pipeline->comp_id);

	return 0;
}

static int ipc_glb_tplg_comp_connect(uint32_t header)
{
	struct sof_ipc_pipe_comp_connect *connect = _ipc->comp_data;

	trace_ipc("Icn");

	return ipc_comp_connect(_ipc, connect);
}

static int ipc_glb_tplg_free(uint32_t header,
		int (*free_func)(struct ipc *ipc, uint32_t id))
{
	struct sof_ipc_free *ipc_free = _ipc->comp_data;

	trace_ipc("Tcf");

	/* free the object */
	free_func(_ipc, ipc_free->id);

	return 0;
}

static int ipc_glb_tplg_message(uint32_t header)
{
	uint32_t cmd = (header & SOF_CMD_TYPE_MASK) >> SOF_CMD_TYPE_SHIFT;

	switch (cmd) {
	case iCS(SOF_IPC_TPLG_COMP_NEW):
		return ipc_glb_tplg_comp_new(header);
	case iCS(SOF_IPC_TPLG_COMP_FREE):
		return ipc_glb_tplg_free(header, ipc_comp_free);
	case iCS(SOF_IPC_TPLG_COMP_CONNECT):
		return ipc_glb_tplg_comp_connect(header);
	case iCS(SOF_IPC_TPLG_PIPE_NEW):
		return ipc_glb_tplg_pipe_new(header);
	case iCS(SOF_IPC_TPLG_PIPE_COMPLETE):
		return ipc_glb_tplg_pipe_complete(header);
	case iCS(SOF_IPC_TPLG_PIPE_FREE):
		return ipc_glb_tplg_free(header, ipc_pipeline_free);
	case iCS(SOF_IPC_TPLG_BUFFER_NEW):
		return ipc_glb_tplg_buffer_new(header);
	case iCS(SOF_IPC_TPLG_BUFFER_FREE):
		return ipc_glb_tplg_free(header, ipc_buffer_free);
	default:
		trace_ipc_error("eTc");
		trace_value(header);
		return -EINVAL;
	}
}

/*
 * Global IPC Operations.
 */

int ipc_cmd(void)
{
	struct sof_ipc_hdr *hdr;
	uint32_t type;

	hdr = mailbox_validate();
	if (hdr == NULL) {
		trace_ipc_error("hdr");
		return -EINVAL;
	}

	type = (hdr->cmd & SOF_GLB_TYPE_MASK) >> SOF_GLB_TYPE_SHIFT;

	switch (type) {
	case iGS(SOF_IPC_GLB_REPLY):
		return 0;
	case iGS(SOF_IPC_GLB_COMPOUND):
		return -EINVAL;	/* TODO */
	case iGS(SOF_IPC_GLB_TPLG_MSG):
		return ipc_glb_tplg_message(hdr->cmd);
	case iGS(SOF_IPC_GLB_PM_MSG):
		return ipc_glb_pm_message(hdr->cmd);
	case iGS(SOF_IPC_GLB_COMP_MSG):
		return ipc_glb_comp_message(hdr->cmd);
	case iGS(SOF_IPC_GLB_STREAM_MSG):
		return ipc_glb_stream_message(hdr->cmd);
	case iGS(SOF_IPC_GLB_DAI_MSG):
		return ipc_glb_dai_message(hdr->cmd);
	case iGS(SOF_IPC_GLB_TRACE_MSG):
		return ipc_glb_debug_message(hdr->cmd);
	default:
		trace_ipc_error("eGc");
		trace_value(type);
		return -EINVAL;
	}
}

/* locks held by caller */
static inline struct ipc_msg *msg_get_empty(struct ipc *ipc)
{
	struct ipc_msg *msg = NULL;

	if (!list_is_empty(&ipc->empty_list)) {
		msg = list_first_item(&ipc->empty_list, struct ipc_msg, list);
		list_item_del(&msg->list);
	}

	return msg;
}


int ipc_queue_host_message(struct ipc *ipc, uint32_t header,
	void *tx_data, size_t tx_bytes, void *rx_data,
	size_t rx_bytes, void (*cb)(void*, void*), void *cb_data)
{
	struct ipc_msg *msg;
	uint32_t flags;
	int ret = 0;

	spin_lock_irq(&ipc->lock, flags);

	/* get a free message */
	msg = msg_get_empty(ipc);
	if (msg == NULL) {
		trace_ipc_error("eQb");
		ret = -EBUSY;
		goto out;
	}

	/* prepare the message */
	msg->header = header;
	msg->tx_size = tx_bytes;
	msg->rx_size = rx_bytes;
	msg->cb_data = cb_data;
	msg->cb = cb;

	/* copy mailbox data to message */
	if (tx_bytes > 0 && tx_bytes < SOF_IPC_MSG_MAX_SIZE)
		rmemcpy(msg->tx_data, tx_data, tx_bytes);

	/* now queue the message */
	ipc->dsp_pending = 1;
	list_item_append(&msg->list, &ipc->msg_list);

out:
	spin_unlock_irq(&ipc->lock, flags);
	return ret;
}

/* process current message */
int ipc_process_msg_queue(void)
{
	if (_ipc->host_pending)
		ipc_platform_do_cmd(_ipc);
	if (_ipc->dsp_pending)
		ipc_platform_send_msg(_ipc);
	return 0;
}
