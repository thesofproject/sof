/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Intel IPC.
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
#include <platform/interrupt.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <platform/dma.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <uapi/intel-ipc.h>

/* memory mapped stream info for static pipeline - map to stream alloc reply */
struct ipc_stream_data {
	uint32_t rpos;	/* read_position_register_address */
	uint32_t ppos;	/* presentation_position_register_address */
	uint32_t peak[IPC_INTEL_NO_CHANNELS];	/* peak_meter_register_address */
	uint32_t volume[IPC_INTEL_NO_CHANNELS];	/* register_address */
};

struct ipc_mixer_data {
	uint32_t peak[IPC_INTEL_NO_CHANNELS];	/* peak_meter_register_address */
	uint32_t volume[IPC_INTEL_NO_CHANNELS];	/* register_address */
};

// TODO these should be added by static pipeline
struct host_pcm_dev {
	uint32_t pipeline_id;
	struct pipeline *p;
	struct stream_params params;
	struct comp_desc desc;
	struct ipc_stream_data *stream_data;
	struct list_head list;		/* list in ipc data */
};

// TODO these should be added by static pipeline
struct host_mixer_dev {
	struct comp_desc desc;
	struct ipc_mixer_data *mixer_data;
	struct list_head list;		/* list in ipc data */
};

/* private data for IPC */
struct ipc_data {
	/* messaging */
	uint32_t host_msg;		/* current message from host */
	uint32_t dsp_msg;		/* current message to host */
	uint16_t host_pending;
	uint16_t dsp_pending;

	/* DMA */
	struct dma *dmac0, *dmac1;
	uint8_t *page_table;
	completion_t complete;

	/* host mixers and pcms */
	struct list_head pcm_list;	/* list of host pcm devices */
	struct list_head mixer_list;	/* list of host mixers */
};

#define to_host_offset(_s) \
	(((uint32_t)&_s) - MAILBOX_BASE + MAILBOX_HOST_OFFSET)

static struct ipc_data *_ipc;
/*
 * BDW IPC dialect.
 *
 * This IPC ABI only supports a fixed static pipeline using the upstream BDW
 * driver. The stream and mixer IDs are hard coded (must match the static
 * pipeline definition).
 */

static inline uint32_t msg_get_global_type(uint32_t msg)
{
	return (msg & IPC_INTEL_GLB_TYPE_MASK) >> IPC_INTEL_GLB_TYPE_SHIFT;
}

static inline uint32_t msg_get_stream_type(uint32_t msg)
{
	return (msg & IPC_INTEL_STR_TYPE_MASK) >> IPC_INTEL_STR_TYPE_SHIFT;
}

static inline uint32_t msg_get_stage_type(uint32_t msg)
{
	return (msg & IPC_INTEL_STG_TYPE_MASK) >> IPC_INTEL_STG_TYPE_SHIFT;
}

/* TODO: pick values from autotools */
static const struct ipc_intel_ipc_fw_version fw_version = {
	.build = 1,
	.minor = 2,
	.major = 3,
	.type = 4,
	//.fw_build_hash[IPC_INTEL_BUILD_HASH_LENGTH];
	.fw_log_providers_hash = 0,
};

static uint32_t ipc_fw_version(uint32_t header)
{
	trace_ipc("Ver");

	mailbox_outbox_write(0, &fw_version, sizeof(fw_version));

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_fw_caps(uint32_t header)
{
	trace_ipc("Cap");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static void dma_complete(void *data, uint32_t type)
{
	struct ipc_data *ipc = (struct ipc_data *)data;

	wait_completed(&ipc->complete);
}

static struct host_pcm_dev *get_pcm(uint32_t id)
{
	struct host_pcm_dev *pcm;
	struct list_head *plist;

	list_for_each(plist, &_ipc->pcm_list) {

		pcm = container_of(plist, struct host_pcm_dev, list);

		if (pcm->desc.id == id)
			return pcm;
	}

	return NULL;
}

static struct host_mixer_dev *get_mixer(uint32_t id)
{
	struct host_mixer_dev *mixer;
	struct list_head *mlist;

	list_for_each(mlist, &_ipc->mixer_list) {

		mixer = container_of(mlist, struct host_mixer_dev, list);

		if (mixer->desc.id == id)
			return mixer;
	}

	return NULL;
}

/* this function copies the audio buffer page tables from the host to the DSP */
/* the page table is a max of 4K */
static int get_page_desciptors(struct ipc_intel_ipc_stream_alloc_req *req)
{
	struct ipc_intel_ipc_stream_ring *ring = &req->ringinfo;
	struct dma_sg_config config;
	struct dma_sg_elem elem;
	struct dma *dma;
	int chan, ret = 0;

	/* get DMA channel from DMAC1 */
	chan = dma_channel_get(_ipc->dmac1);
	if (chan >= 0)
		dma = _ipc->dmac1;
	else
		return chan;

	/* set up DMA configuration */
	config.direction = DMA_DIR_MEM_TO_MEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	list_init(&config.elem_list);

	/* set up DMA desciptor */
	elem.dest = (uint32_t)_ipc->page_table;
	elem.src = ring->ring_pt_address;

	/* source buffer size is always PAGE_SIZE bytes */
	elem.size = ring->num_pages * 4; /* 20 bits for each page, round up to 32 */
	list_add(&elem.list, &config.elem_list);

	ret = dma_set_config(dma, chan, &config);
	if (ret < 0)
		goto out;

	/* set up callback */
	dma_set_cb(dma, chan, dma_complete, _ipc);

	wait_init(&_ipc->complete);

	/* start the copy of page table to DSP */
	dma_start(dma, chan);

	/* wait 2 msecs for DMA to finish */
	_ipc->complete.timeout = 2;
	ret = wait_for_completion_timeout(&_ipc->complete);

	/* compressed page tables now in buffer at _ipc->page_table */
out:
	dma_channel_put(dma, chan);
	return ret;
}

/* now parse page tables and create the audio DMA SG configuration
 for host audio DMA buffer. This involves creating a dma_sg_elem for each
 page table entry and adding each elem to a list in struct dma_sg_config*/
static int parse_page_descriptors(struct ipc_intel_ipc_stream_alloc_req *req,
	struct comp_desc *host, struct stream_params *params)
{
	struct ipc_intel_ipc_stream_ring *ring = &req->ringinfo;
	struct dma_sg_elem elem;
	int i, err;
	uint32_t idx;

	elem.size = 4096;

	for (i = 0; i < ring->num_pages; i++) {
		idx = (((i << 2) + i)) >> 1;

		elem.src = *((uint32_t*)(_ipc->page_table + idx));
		if (i & 0x1)
			elem.src <<= 8;
		else
			elem.src <<= 12;
		elem.src &= 0xfffff000;

		err = pipeline_host_buffer(pipeline_static, host, params, &elem);
		if (err < 0)
			return err;
	}

	return 0;
}

static uint32_t ipc_stream_alloc(uint32_t header)
{
	struct ipc_intel_ipc_stream_alloc_req req;
	struct ipc_intel_ipc_stream_alloc_reply reply;
	struct stream_params *params;
	struct comp_desc dai, host;
	struct host_pcm_dev *pcm_dev;
	int err;
	uint8_t direction;

	trace_ipc("SAl");

	/* read alloc stream IPC from the inbox */
	mailbox_inbox_read(&req, 0, sizeof(req));

	host.uuid = COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_HOST);

	/* path_id is always SSP0 */
	dai.uuid = COMP_UUID(COMP_VENDOR_INTEL, DAI_UUID_SSP0);
	dai.id = 0;

	/* format always PCM, now check source type */
	switch (req.stream_type) {
	case IPC_INTEL_STREAM_TYPE_SYSTEM:
		host.id = 0;
		direction = STREAM_DIRECTION_PLAYBACK;
		break;
	case IPC_INTEL_STREAM_TYPE_RENDER:
		host.id = 1;
		direction = STREAM_DIRECTION_PLAYBACK;
		break;
	case IPC_INTEL_STREAM_TYPE_CAPTURE:
		host.id = 2;
		direction = STREAM_DIRECTION_CAPTURE;
		break;
	case IPC_INTEL_STREAM_TYPE_LOOPBACK:
		host.id = 3;
		direction = STREAM_DIRECTION_CAPTURE;
	default:
		goto error;
	};

	/* get the pcm_dev */
	pcm_dev = get_pcm(host.id);
	if (pcm_dev == NULL)
		goto error; 

	params = &pcm_dev->params;

	/* read in format to create params */
	params->channels = req.format.ch_num;
	params->pcm.rate = req.format.frequency;
	params->direction = direction;

	/* work out bitdepth and framesize */
	switch (req.format.bitdepth) {
	case 16:
		params->pcm.format = STREAM_FORMAT_S16_LE;
		params->frame_size = 2 * params->channels;
		break;
	default:
		goto error;
	}

	params->period_frames = 4096 / params->frame_size;

	/* use DMA to read in compressed page table ringbuffer from host */
	err = get_page_desciptors(&req);
	if (err < 0)
		goto error;

	/* TODO: now parse page tables and create audio DMA SG configuration and
	for host audio DMA buffer. This involves creating a dma_sg_elem for each
	page table entry and adding each elem to a list in struct dma_sg_config*/
	err = parse_page_descriptors(&req, &host, params);
	if (err < 0)
		goto error;

	/* configure pipeline audio params */
	err = pipeline_params(pipeline_static, &host, params);	
	if (err < 0)
		goto error;

	/* Configure SSP and send to DAI*/
	err = pipeline_params(pipeline_static, &dai, params);	
	if (err < 0)
		goto error;

	/* initialise the pipeline */
	err = pipeline_prepare(pipeline_static, &host, params);
	if (err < 0)
		goto error;


	/* at this point pipeline is ready for command so send stream reply */
	reply.stream_hw_id = host.id;
	reply.mixer_hw_id = 0; // returns rate ????

// TODO: set up pointers to read data directly.
#if 0
	/* set read pos and presentation pos address */
	reply.read_position_register_address =
		to_host_offset(pcm_dev->stream_data[stream_id].rpos);
	reply.presentation_position_register_address =
		to_host_offset(pcm_dev->stream_data[stream_id].ppos);

	/* set volume address */
	for (i = 0; i < IPC_INTEL_NO_CHANNELS; i++) {
		reply.peak_meter_register_address[i] =
			to_host_offset(pcm_dev->stream_data[stream_id].peak[i]);
		reply.volume_register_address[i] =
			to_host_offset(pcm_dev->stream_data[stream_id].volume[i]);
	}
#endif

	/* write reply to mailbox */
	mailbox_outbox_write(0, &reply, sizeof(reply));

	/* TODO */
error:
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_free(uint32_t header)
{
	struct ipc_intel_ipc_stream_free_req free_req;
	struct host_pcm_dev *pcm_dev;
	struct stream_params *params;
	int err;

	trace_ipc("SFr");

	/* read alloc stream IPC from the inbox */
	mailbox_inbox_read(&free_req, 0, sizeof(free_req));

	/* get the pcm_dev */
	pcm_dev = get_pcm(free_req.stream_id);
	if (pcm_dev == NULL)
		goto error; 

	params = &pcm_dev->params;

	/* initialise the pipeline */
	err = pipeline_reset(pipeline_static, &pcm_dev->desc, params);
	if (err < 0)
		goto error;

error:
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_info(uint32_t header)
{
	struct ipc_intel_ipc_stream_info_reply info;
	struct host_mixer_dev *mixer_dev;
//	int i;

	trace_ipc("SIn");

	/* TODO: get data from topology */ 
	info.mixer_hw_id = 1;

	/* get the pcm_dev */
	mixer_dev = get_mixer(info.mixer_hw_id);
	if (mixer_dev == NULL)
		goto error; 
	
#if 0
	for (i = 0; i < IPC_INTEL_NO_CHANNELS; i++) {
		info.peak_meter_register_address[i] =
			to_host_offset(_ipc->mixer_data->peak[i]);
		info.volume_register_address[i] =
			to_host_offset(_ipc->mixer_data->volume[i]);
	}
#endif
	mailbox_outbox_write(0, &info, sizeof(info));

// TODO: define error paths
error:
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_dump(uint32_t header)
{
	trace_ipc("Dum");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

#if 0
/* Device Configuration Request */
struct ipc_intel_ipc_device_config_req {
	uint32_t ssp_interface;
	uint32_t clock_frequency;
	uint32_t mode;
	uint16_t clock_divider;
	uint8_t channels;
	uint8_t reserved;
} __attribute__((packed));
#endif
static uint32_t ipc_device_get_formats(uint32_t header)
{
	trace_ipc("DgF");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_device_set_formats(uint32_t header)
{
	trace_ipc("DsF");

	// call
	//int pipeline_params(int pipeline_id, struct pipe_comp_desc *host_desc,
	//struct stream_params *params);

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_save(uint32_t header)
{
	trace_ipc("PMs");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_restore(uint32_t header)
{
	trace_ipc("PMr");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_set_volume(uint32_t header)
{
	trace_ipc("VoS");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_get_volume(uint32_t header)
{
	trace_ipc("VoG");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_write_pos(uint32_t header)
{
	trace_ipc("PoW");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_message(uint32_t header)
{
	uint32_t stg = msg_get_stage_type(header);

	switch (stg) {
	case IPC_INTEL_STG_GET_VOLUME:
		return ipc_stage_get_volume(header);
	case IPC_INTEL_STG_SET_VOLUME:
		return ipc_stage_set_volume(header);
	case IPC_INTEL_STG_SET_WRITE_POSITION:
		return ipc_stage_write_pos(header);
	case IPC_INTEL_STG_MUTE_LOOPBACK:
	case IPC_INTEL_STG_SET_FX_ENABLE:
	case IPC_INTEL_STG_SET_FX_DISABLE:
	case IPC_INTEL_STG_SET_FX_GET_PARAM:
	case IPC_INTEL_STG_SET_FX_SET_PARAM:
	case IPC_INTEL_STG_SET_FX_GET_INFO:
	default:
		return IPC_INTEL_GLB_REPLY_UNKNOWN_MESSAGE_TYPE;
	}
}

static uint32_t ipc_stream_reset(uint32_t header)
{
	trace_ipc("SRe");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_pause(uint32_t header)
{
	trace_ipc("SPa");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_resume(uint32_t header)
{
	trace_ipc("SRe");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_notification(uint32_t header)
{
	trace_ipc("SNo");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_message(uint32_t header)
{
	uint32_t str = msg_get_stream_type(header);

	switch (str) {
	case IPC_INTEL_STR_RESET:
		return ipc_stream_reset(header);
	case IPC_INTEL_STR_PAUSE:
		return ipc_stream_pause(header);
	case IPC_INTEL_STR_RESUME:
		return ipc_stream_resume(header);
	case IPC_INTEL_STR_STAGE_MESSAGE:
		return ipc_stage_message(header);
	case IPC_INTEL_STR_NOTIFICATION:
		return ipc_stream_notification(header);
	default:
		return IPC_INTEL_GLB_REPLY_UNKNOWN_MESSAGE_TYPE;
	}
}

static uint32_t ipc_cmd(void)
{
	uint32_t type, header;

	header = _ipc->host_msg;
	type = msg_get_global_type(header);

	switch (type) {
	case IPC_INTEL_GLB_GET_FW_VERSION:
		return ipc_fw_version(header);
	case IPC_INTEL_GLB_ALLOCATE_STREAM:
		return ipc_stream_alloc(header);
	case IPC_INTEL_GLB_FREE_STREAM:
		return ipc_stream_free(header);
	case IPC_INTEL_GLB_GET_FW_CAPABILITIES:
		return ipc_fw_caps(header);
	case IPC_INTEL_GLB_REQUEST_DUMP:
		return ipc_dump(header);
	case IPC_INTEL_GLB_GET_DEVICE_FORMATS:
		return ipc_device_get_formats(header);
	case IPC_INTEL_GLB_SET_DEVICE_FORMATS:
		return ipc_device_set_formats(header);
	case IPC_INTEL_GLB_ENTER_DX_STATE:
		return ipc_context_save(header);
	case IPC_INTEL_GLB_GET_MIXER_STREAM_INFO:
		return ipc_stream_info(header);
	case IPC_INTEL_GLB_RESTORE_CONTEXT:
		return ipc_context_restore(header);
	case IPC_INTEL_GLB_STREAM_MESSAGE:
		return ipc_stream_message(header);
	default:
		return IPC_INTEL_GLB_REPLY_UNKNOWN_MESSAGE_TYPE;
	}
}

static void do_cmd(void)
{
	uint32_t ipcxh, status;
	
	trace_ipc("Cmd");
	trace_value(_ipc->host_msg);

	status = ipc_cmd();
	_ipc->host_pending = 0;

	/* clear BUSY bit and set DONE bit - accept new messages */
	ipcxh = shim_read(SHIM_IPCXH);
	ipcxh &= ~SHIM_IPCXH_BUSY;
	ipcxh |= SHIM_IPCXH_DONE | status;
	shim_write(SHIM_IPCXH, ipcxh);

	/* unmask busy interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_BUSY);
}

static void do_notify(void)
{
	trace_ipc("Not");

	/* clear DONE bit - tell Host we have completed */
	shim_write(SHIM_IPCDH, shim_read(SHIM_IPCDH) & ~SHIM_IPCDH_DONE);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DONE);
}

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
	uint32_t isr;

	trace_ipc("IRQ");

	/* Interrupt arrived, check src */
	isr = shim_read(SHIM_ISRD);

	if (isr & SHIM_ISRD_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_DONE);
		do_notify();
	}

	if (isr & SHIM_ISRD_BUSY) {
		
		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);
		/* place message in Q and process later */
		_ipc->host_msg = shim_read(SHIM_IPCXL);
		_ipc->host_pending = 1;
	}
}

/* process current message */
int ipc_process_msg_queue(void)
{
	if (_ipc->host_pending)
		do_cmd();
	return 0;
}

int ipc_send_msg(struct ipc_msg *msg)
{

	return 0;
}

int platform_ipc_init(void)
{
	uint32_t imrd;

	/* init ipc data */
	_ipc = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*_ipc));
	_ipc->page_table = rballoc(RZONE_DEV, RMOD_SYS,
		IPC_INTEL_PAGE_TABLE_SIZE);
	if (_ipc->page_table)
		bzero(_ipc->page_table, IPC_INTEL_PAGE_TABLE_SIZE);

	_ipc->dmac0 = dma_get(DMA_ID_DMAC0);
	_ipc->dmac1 = dma_get(DMA_ID_DMAC1);
	_ipc->host_pending = 0;
	list_init(&_ipc->pcm_list);
	list_init(&_ipc->mixer_list);

	/* configure interrupt */
	interrupt_register(IRQ_NUM_EXT_IA, irq_handler, NULL);
	interrupt_enable(IRQ_NUM_EXT_IA);

	/* Unmask Busy and Done interrupts */
	imrd = shim_read(SHIM_IMRD);
	imrd &= ~(SHIM_IMRD_BUSY | SHIM_IMRD_DONE);
	shim_write(SHIM_IMRD, imrd);

	return 0;
}
