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

/* private data for IPC */
struct ipc_data {
	uint32_t host_msg;		/* current message from host */
	uint32_t dsp_msg;		/* current message to host */
	struct dma *dmac0, *dmac1;
	uint8_t *page_table;
	completion_t complete;
	struct ipc_stream_data *stream_data; /* points to mailbox */
	struct ipc_mixer_data *mixer_data; /* points to mailbox */
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
	trace_ipc('b');

	mailbox_outbox_write(0, &fw_version, sizeof(fw_version));

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_fw_caps(uint32_t header)
{
	trace_ipc('d');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

#if 0
/* Stream ring info */
struct sst_hsw_ipc_stream_ring {
	u32 ring_pt_address; // PHY address of page table
	u32 num_pages;	// audio DMA buffer size in pages of 4K
	u32 ring_size;	// audio DMA buffer size in bytes
	u32 ring_offset; // always 0
	u32 ring_first_pfn; //audio DMA buffer PHY address PFN
} __attribute__((packed));
#endif

static void dma_complete(void *data)
{
	struct ipc_data *ipc = (struct ipc_data *)data;

	wait_completed(&ipc->complete);
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
	list_init(&config.elem_list);

	/* set up DMA desciptor */
	elem.dest = (uint32_t *)_ipc->page_table;
	elem.src = (uint32_t *)ring->ring_pt_address;
	elem.size = (ring->num_pages * 5 + 1) / 2;/* 20 bits for each page */
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
	ret = wait_for_completion_timeout(&_ipc->complete, 2);

	/* compressed page tables now in buffer at _ipc->page_table */
out:
	dma_channel_put(dma, chan);
	return ret;
}

#if 0
/* Stream Allocate Request */
struct ipc_intel_ipc_stream_alloc_req {
	uint8_t path_id;
	uint8_t stream_type;
	uint8_t format_id;
	uint8_t reserved;
	struct ipc_intel_audio_data_format_ipc format;
	struct ipc_intel_ipc_stream_ring ringinfo;
	struct ipc_intel_module_map map;
	struct ipc_intel_memory_info persistent_mem;
	struct ipc_intel_memory_info scratch_mem;
	uint32_t number_of_notifications;
} __attribute__((packed));

/* Audio Data formats */
struct sst_hsw_audio_data_format_ipc {
	u32 frequency;
	u32 bitdepth;
	u32 map;
	u32 config;
	u32 style;
	u8 ch_num;
	u8 valid_bit;
	u8 reserved[2];
} __attribute__((packed));

/* Stream Allocate Reply */
struct ipc_intel_ipc_stream_alloc_reply {
	uint32_t stream_hw_id;
	uint32_t mixer_hw_id; // returns rate ????
	uint32_t read_position_register_address;
	uint32_t presentation_position_register_address;
	uint32_t peak_meter_register_address[IPC_INTEL_NO_CHANNELS];
	uint32_t volume_register_address[IPC_INTEL_NO_CHANNELS];
} __attribute__((packed));
#endif

/* TODO: now parse page tables and create audio DMA SG configuration and
	for host audio DMA buffer. This involves creating a dma_sg_elem for each
	page table entry and adding each elem to a list in struct dma_sg_config*/
static int parse_page_descriptors(struct dma_sg_config *config)
{
	return 0;
};

static uint32_t ipc_stream_alloc(uint32_t header)
{
	struct ipc_intel_ipc_stream_alloc_req req;
	struct ipc_intel_ipc_stream_alloc_reply reply;
	struct dma_sg_config config;
	struct stream_params params;
	struct comp_desc host, dai;
	int err, stream_id = 0, i;

	trace_ipc('A');

	/* read alloc stream IPC from the inbox */
	mailbox_inbox_read(&req, 0, sizeof(req));

	/* path_id is always SSP0 */
	dai.uuid = COMP_UUID(COMP_VENDOR_INTEL, DAI_UUID_SSP0);
	dai.id = 0;

	/* format always PCM, now check source type */
	switch (req.stream_type) {
	case IPC_INTEL_STREAM_TYPE_SYSTEM:
		host.uuid = COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_HOST);
		host.id = 0;
		break;
	case IPC_INTEL_STREAM_TYPE_RENDER:
	case IPC_INTEL_STREAM_TYPE_CAPTURE:
	case IPC_INTEL_STREAM_TYPE_LOOPBACK:
	default:
		/* TODO: use this as default atm */
		host.uuid = COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_HOST);
		host.id = 0;
		break;
	};

	/* read in format to create params */
	params.pcm.channels = req.format.ch_num;
	/* TODO: rest of params */

	/* set host PCM params */
	err = pipeline_params(pipeline_static, &host, &params);	
	if (err < 0)
		goto error;

	/* use DMA to read in compressed page table ringbuffer from host */
	err = get_page_desciptors(&req);
	if (err < 0)
		goto error;

	/* TODO: now parse page tables and create audio DMA SG configuration and
	for host audio DMA buffer. This involves creating a dma_sg_elem for each
	page table entry and adding each elem to a list in struct dma_sg_config*/
	err = parse_page_descriptors(&config);
	if (err < 0)
		goto error;

	/* now pass the parsed page tables to the host audio DMA */
	err = pipeline_host_buffer(pipeline_static, &host, &config);
	if (err < 0)
		goto error;

	/* configure pipeline audio params */
	err = pipeline_params(pipeline_static, &host, &params);	
	if (err < 0)
		goto error;

	/* Configure SSP and send to DAI*/
	err = pipeline_params(pipeline_static, &dai, &params);	
	if (err < 0)
		goto error;

	/* initialise the pipeline */
	err = pipeline_prepare(pipeline_static, &host);
	if (err < 0)
		goto error;
error:

	/* at this point pipeline is ready for command so send stream reply */
	reply.stream_hw_id = stream_id;
	reply.mixer_hw_id = 0; // returns rate ????

	/* set read pos and presentation pos address */
	reply.read_position_register_address =
		to_host_offset(_ipc->stream_data[stream_id].rpos);
	reply.presentation_position_register_address =
		to_host_offset(_ipc->stream_data[stream_id].ppos);

	/* set volume address */
	for (i = 0; i < IPC_INTEL_NO_CHANNELS; i++) {
		reply.peak_meter_register_address[i] =
			to_host_offset(_ipc->stream_data[stream_id].peak[i]);
		reply.volume_register_address[i] =
			to_host_offset(_ipc->stream_data[stream_id].volume[i]);
	}

	/* write reply to mailbox */
	mailbox_outbox_write(0, &reply, sizeof(reply));

	/* TODO */
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_free(uint32_t header)
{
	trace_ipc('a');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_info(uint32_t header)
{
	struct ipc_intel_ipc_stream_info_reply info;
	int i;

	trace_ipc('i');

	/* TODO: get data from topology */ 
	info.mixer_hw_id = 1;

	for (i = 0; i < IPC_INTEL_NO_CHANNELS; i++) {
		info.peak_meter_register_address[i] =
			to_host_offset(_ipc->mixer_data->peak[i]);
		info.volume_register_address[i] =
			to_host_offset(_ipc->mixer_data->volume[i]);
	}

	mailbox_outbox_write(0, &info, sizeof(info));

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_dump(uint32_t header)
{
	trace_ipc('D');

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
	trace_ipc('f');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_device_set_formats(uint32_t header)
{
	trace_ipc('F');

	// call
	//int pipeline_params(int pipeline_id, struct pipe_comp_desc *host_desc,
	//struct stream_params *params);

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_save(uint32_t header)
{
	trace_ipc('x');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_restore(uint32_t header)
{
	trace_ipc('X');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_set_volume(uint32_t header)
{
	trace_ipc('v');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_get_volume(uint32_t header)
{
	trace_ipc('V');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_write_pos(uint32_t header)
{
	trace_ipc('o');

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
	trace_ipc('R');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_pause(uint32_t header)
{
	trace_ipc('P');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_resume(uint32_t header)
{
	trace_ipc('p');

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_notification(uint32_t header)
{
	trace_ipc('n');

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

	header = shim_read(SHIM_IPCXL);
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
	
	trace_ipc('c');

	/* TODO: place message in Q and process later */
	_ipc->host_msg = shim_read(SHIM_IPCXL);
	status = ipc_cmd();

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
	trace_ipc('n');

	/* clear DONE bit - tell Host we have completed */
	shim_write(SHIM_IPCDH, shim_read(SHIM_IPCDH) & ~SHIM_IPCDH_DONE);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DONE);
}

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
	uint32_t isr;

	/* Interrupt arrived, check src */
	isr = shim_read(SHIM_ISRD);

	interrupt_clear(IRQ_NUM_EXT_IA);

	if (isr & SHIM_ISRD_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_DONE);
		do_notify();
	}

	if (isr & SHIM_ISRD_BUSY) {
		
		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);
		do_cmd();
	}

	interrupt_enable(IRQ_NUM_EXT_IA);
}

/* process current message */
int ipc_process_msg_queue(void)
{

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
	_ipc->dmac0 = dma_get(DMA_ID_DMAC0);
	_ipc->dmac1 = dma_get(DMA_ID_DMAC1);
	_ipc->stream_data =
		(struct ipc_stream_data*)(MAILBOX_BASE + MAILBOX_STREAM_OFFSET);
	_ipc->mixer_data = (void*)_ipc->stream_data +
		sizeof(struct ipc_stream_data) * STREAM_MAX_STREAMS;
	bzero(_ipc->stream_data,
		sizeof(struct ipc_stream_data) * STREAM_MAX_STREAMS +
		sizeof(struct ipc_mixer_data));

	/* configure interrupt */
	interrupt_register(IRQ_NUM_EXT_IA, irq_handler, NULL);
	interrupt_enable(IRQ_NUM_EXT_IA);

	/* Unmask Busy and Done interrupts */
	imrd = shim_read(SHIM_IMRD);
	imrd &= ~(SHIM_IMRD_BUSY | SHIM_IMRD_DONE);
	shim_write(SHIM_IMRD, imrd);

	return 0;
}
