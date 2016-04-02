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

/* private data for IPC */
struct intel_ipc_data {
	/* DMA */
	struct dma *dmac0;
	uint8_t *page_table;
	completion_t complete;

	/* SSP port - TODO driver only support 1 atm */
	uint32_t dai[2];

	/* PM */
	int pm_prepare_D3;	/* do we need to prepare for D3 */
};

#define to_host_offset(_s) \
	(((uint32_t)&_s) - MAILBOX_BASE + MAILBOX_HOST_OFFSET)

static struct ipc *_ipc;

/* hard coded stream offset TODO: make this into programmable map */
static struct sst_intel_ipc_stream_data *_stream_data =
	(struct sst_intel_ipc_stream_data *)(MAILBOX_BASE + MAILBOX_STREAM_OFFSET);

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
	struct intel_ipc_data *iipc = (struct intel_ipc_data *)data;

	if (type == DMA_IRQ_TYPE_LLIST)
		wait_completed(&iipc->complete);
}

/* this function copies the audio buffer page tables from the host to the DSP */
/* the page table is a max of 4K */
static int get_page_desciptors(struct intel_ipc_data *iipc, 
	struct ipc_intel_ipc_stream_alloc_req *req)
{
	struct ipc_intel_ipc_stream_ring *ring = &req->ringinfo;
	struct dma_sg_config config;
	struct dma_sg_elem elem;
	struct dma *dma;
	int chan, ret = 0;

	/* get DMA channel from DMAC0 */
	chan = dma_channel_get(iipc->dmac0);
	if (chan < 0)
		return chan;
	dma = iipc->dmac0;

	/* set up DMA configuration */
	config.direction = DMA_DIR_MEM_TO_MEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	list_init(&config.elem_list);

	/* set up DMA desciptor */
	elem.dest = (uint32_t)iipc->page_table;
	elem.src = ring->ring_pt_address;

	/* source buffer size is always PAGE_SIZE bytes */
	/* 20 bits for each page, round up to 32 */
	elem.size = (ring->num_pages * 5 * 16 + 31) / 32;
	list_add(&elem.list, &config.elem_list);

	ret = dma_set_config(dma, chan, &config);
	if (ret < 0)
		goto out;

	/* set up callback */
	dma_set_cb(dma, chan, DMA_IRQ_TYPE_LLIST, dma_complete, iipc);

	wait_init(&iipc->complete);

	/* start the copy of page table to DSP */
	dma_start(dma, chan);

	/* wait 2 msecs for DMA to finish */
	iipc->complete.timeout = 2000;
	ret = wait_for_completion_timeout(&iipc->complete);

	/* compressed page tables now in buffer at _ipc->page_table */
out:
	dma_channel_put(dma, chan);
	return ret;
}

/* now parse page tables and create the audio DMA SG configuration
 for host audio DMA buffer. This involves creating a dma_sg_elem for each
 page table entry and adding each elem to a list in struct dma_sg_config*/
static int parse_page_descriptors(struct intel_ipc_data *iipc,
	struct ipc_intel_ipc_stream_alloc_req *req,
	struct comp_dev *host)
{
	struct ipc_intel_ipc_stream_ring *ring = &req->ringinfo;
	struct dma_sg_elem elem;
	int i, err;
	uint32_t idx;

	elem.size = HOST_PAGE_SIZE;

	for (i = 0; i < ring->num_pages; i++) {

		idx = (((i << 2) + i)) >> 1;
		elem.src = iipc->page_table[idx] | (iipc->page_table[idx + 1] << 8)
				| (iipc->page_table[idx + 2] << 16);

		if (i & 0x1)
			elem.src <<= 8;
		else
			elem.src <<= 12;
		elem.src &= 0xfffff000;

		err = pipeline_host_buffer(pipeline_static, host, &elem);
		if (err < 0)
			return err;
	}

	return 0;
}

static uint32_t ipc_stream_alloc(uint32_t header)
{
	struct intel_ipc_data *iipc = ipc_get_drvdata(_ipc);
	struct ipc_intel_ipc_stream_alloc_req req;
	struct ipc_intel_ipc_stream_alloc_reply reply;
	struct stream_params *params;
	uint32_t host_id, dai_id;
	struct ipc_pcm_dev *pcm_dev;
	struct ipc_dai_dev *dai_dev;
	int err, i;
	uint8_t direction;

	trace_ipc("SAl");

	/* read alloc stream IPC from the inbox */
	mailbox_inbox_read(&req, 0, sizeof(req));

	/* format always PCM, now check source type */
	switch (req.stream_type) {
	case IPC_INTEL_STREAM_TYPE_SYSTEM:
	default:
		host_id = 0;
		dai_id = 2;
		direction = STREAM_DIRECTION_PLAYBACK;
		break;
#if 0
	case IPC_IPC_HOST_TYPE_RENDER:
		host_id = 0;
		dai_id = 2;
		direction = STREAM_DIRECTION_PLAYBACK;
		break;
	case IPC_IPC_HOST_TYPE_CAPTURE:
		host_id = 0;
		//dai_id = 3;
		//direction = STREAM_DIRECTION_CAPTURE;
		direction = STREAM_DIRECTION_PLAYBACK;
		dai_id = 2;
		break;
	case IPC_IPC_HOST_TYPE_LOOPBACK:
		host_id = 0;
		dai_id = 3;
		direction = STREAM_DIRECTION_CAPTURE;
	default:
		goto error;
#endif
	};

	/* get the pcm_dev */
	pcm_dev = ipc_get_pcm_comp(host_id);
	if (pcm_dev == NULL)
		goto error; 

	/* get the pcm_dev */
	dai_dev = ipc_get_dai_comp(dai_id);
	if (dai_dev == NULL)
		goto error; 

	params = &pcm_dev->params;
	//ipc_set_drvdata(&pcm_dev->dev, stream_data);

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

	params->period_frames = req.format.period_frames;

	/* use DMA to read in compressed page table ringbuffer from host */
	err = get_page_desciptors(iipc, &req);
	if (err < 0)
		goto error;

	/* Parse host tables */
	err = parse_page_descriptors(iipc, &req, pcm_dev->dev.cd);
	if (err < 0)
		goto error;

	/* configure pipeline audio params */
	err = pipeline_params(pipeline_static, pcm_dev->dev.cd, params);	
	if (err < 0)
		goto error;

	/* pass the IPC presentation posn pointer to the DAI */
	err = comp_cmd(dai_dev->dev.cd, COMP_CMD_IPC_MMAP_PPOS,
		&_stream_data->presentation_posn);
	if (err < 0)
		goto error;

	/* pass the IPC read posn pointer to the host */
	err = comp_cmd(pcm_dev->dev.cd, COMP_CMD_IPC_MMAP_RPOS,
		&_stream_data->read_posn);
	if (err < 0)
		goto error;

	/* at this point pipeline is ready for command so send stream reply */
	reply.stream_hw_id = host_id;
	reply.mixer_hw_id = 48000; // returns rate ????

	/* set read pos and presentation pos address */
	reply.read_position_register_address =
		to_host_offset(_stream_data->read_posn);
	reply.presentation_position_register_address =
		to_host_offset(_stream_data->presentation_posn);

	/* set volume address */
	for (i = 0; i < IPC_INTEL_NO_CHANNELS; i++) {
		reply.peak_meter_register_address[i] =
			to_host_offset(_stream_data->vol[i].peak);
		reply.volume_register_address[i] =
			to_host_offset(_stream_data->vol[i].vol);
	}

	/* write reply to mailbox */
	mailbox_outbox_write(0, &reply, sizeof(reply));

	/* update stream state */
	pcm_dev->state = IPC_HOST_ALLOC;
	return IPC_INTEL_GLB_REPLY_SUCCESS;

error:
	pipeline_reset(pipeline_static, pcm_dev->dev.cd);
	return IPC_INTEL_GLB_REPLY_ERROR_INVALID_PARAM;
}

/* free stream resources */
static uint32_t ipc_stream_free(uint32_t header)
{
	struct ipc_intel_ipc_stream_free_req free_req;
	struct ipc_pcm_dev *pcm_dev;

	trace_ipc("SFr");

	/* read alloc stream IPC from the inbox */
	mailbox_inbox_read(&free_req, 0, sizeof(free_req));

	// HACK - fix stream IDs
	free_req.stream_id = 0;

	/* get the pcm_dev */
	pcm_dev = ipc_get_pcm_comp(free_req.stream_id);
	if (pcm_dev == NULL)
		goto error; 

	/* check stream state */
	if (pcm_dev->state == IPC_HOST_RUNNING ||
		pcm_dev->state == IPC_HOST_PAUSED)
		goto error;

	return IPC_INTEL_GLB_REPLY_SUCCESS;

error:
	return IPC_INTEL_GLB_REPLY_ERROR_INVALID_PARAM;
}

static uint32_t ipc_stream_info(uint32_t header)
{
	struct ipc_intel_ipc_stream_info_reply info;
	struct ipc_comp_dev *comp_dev;
	int i;

	trace_ipc("SIn");

	/* TODO: get data from topology */ 
	info.mixer_hw_id = 1;

	/* get the pcm_dev */
	comp_dev = ipc_get_comp(info.mixer_hw_id);
	if (comp_dev == NULL)
		goto error; 
	
	// TODO: this is duplicating standard stream alloc mixer 
	for (i = 0; i < IPC_INTEL_NO_CHANNELS; i++) {
		info.peak_meter_register_address[i] =
			to_host_offset(_stream_data->vol[i].peak);
		info.volume_register_address[i] =
			to_host_offset(_stream_data->vol[i].vol);
	}

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

struct dai_config {
	uint16_t format;
	uint16_t frame_size;	/* in BCLKs */
	struct dai_slot_map tx_slot_map[DAI_NUM_SLOT_MAPS];
	struct dai_slot_map rx_slot_map[DAI_NUM_SLOT_MAPS];
	uint16_t bclk_fs;	/* ratio between frame size and BCLK */
	uint16_t mclk_fs;	/* ratio between frame size and MCLK */
	uint32_t mclk;		/* mclk frequency in Hz */
	uint16_t clk_src;	/* DAI specific clk source */
};

#endif
static uint32_t ipc_device_get_formats(uint32_t header)
{
	trace_ipc("DgF");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_device_set_formats(uint32_t header)
{
	struct ipc_intel_ipc_device_config_req config_req;
	struct ipc_dai_dev *dai_dev;
	struct intel_ipc_data *iipc = ipc_get_drvdata(_ipc);
//	struct IPC_HOST_data *stream_data;

	trace_ipc("DsF");

	/* read alloc stream IPC from the inbox */
	mailbox_inbox_read(&config_req, 0, sizeof(config_req));

	/* get SSP port TODO: align ports*/
	switch (config_req.ssp_interface) {
	case IPC_INTEL_DEVICE_SSP_0:
		iipc->dai[0] = 2;
		iipc->dai[1] = 3;
		break;
	case IPC_INTEL_DEVICE_SSP_1:
		iipc->dai[0] = 2;
		iipc->dai[1] = 3;
		break;
	case IPC_INTEL_DEVICE_SSP_2:
		iipc->dai[0] = 2;
		iipc->dai[1] = 3;

		/* set SSP M/N clock dividers TODO: program this from IPC */
		shim_write(SHIM_SSP2_DIVL, 0x000061a8); // 3.072MHz
		shim_write(SHIM_SSP2_DIVH, 0x60000c00);

		break;
	default:
		goto error;
	};

	/* get the pcm_dev */
	dai_dev = ipc_get_dai_comp(iipc->dai[0]);
	if (dai_dev == NULL)
		goto error;

	/* setup the DAI HW config - TODO hard coded due to IPC limitations */
	dai_dev->dai_config.mclk = config_req.clock_frequency;
	dai_dev->dai_config.format = DAI_FMT_I2S;
	dai_dev->dai_config.frame_size = 32;
	dai_dev->dai_config.bclk_fs = 32;
	dai_dev->dai_config.mclk_fs = 256;
	dai_dev->dai_config.clk_src = 0;// audio

	comp_dai_config(dai_dev->dev.cd, &dai_dev->dai_config);

error:
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_save(uint32_t header)
{
	struct intel_ipc_data *iipc = ipc_get_drvdata(_ipc);
	struct ipc_intel_ipc_dx_reply reply;

	trace_ipc("PMs");

	/* TODO: check we are inactive */

	/* TODO: stop timers */

	/* TODO: save the context */
	reply.entries_no = 0;

	/* mask all interrupts */
	interrupt_global_disable();

	mailbox_outbox_write(0, &reply, sizeof(reply));

	iipc->pm_prepare_D3 = 1;
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_restore(uint32_t header)
{
	trace_ipc("PMr");

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_set_volume(uint32_t header)
{
	struct ipc_comp_dev *mixer_dev;
	uint32_t stream_id;
	int err;

	trace_ipc("VoS");

	// TODO: finish implementation, get correct ID from drv
	return IPC_INTEL_GLB_REPLY_SUCCESS;

	/* the driver uses stream ID to also identify certain mixers */
	stream_id = header & IPC_INTEL_STR_ID_MASK;
	stream_id >>= IPC_INTEL_STR_ID_SHIFT;

	/* get the pcm_dev */
	mixer_dev = ipc_get_comp(stream_id);
	if (mixer_dev == NULL)
		goto error; 
	
	/* TODO: complete call with private volume data */
	err = comp_cmd(mixer_dev->cd, COMP_CMD_VOLUME, NULL);
	if (err < 0)
		goto error;

// TODO: define error paths
error:

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_get_volume(uint32_t header)
{
	struct ipc_comp_dev *mixer_dev;
	uint32_t stream_id;
	int err;

	trace_ipc("VoG");

	// TODO: finish implementation, get correct ID from drv
	return IPC_INTEL_GLB_REPLY_SUCCESS;

	/* the driver uses stream ID to also identify certain mixers */
	stream_id = header & IPC_INTEL_STR_ID_MASK;
	stream_id >>= IPC_INTEL_STR_ID_SHIFT;

	/* get the pcm_dev */
	mixer_dev = ipc_get_comp(stream_id);
	if (mixer_dev == NULL)
		goto error; 
	
	/* TODO: complete call with private volume data */
	err = comp_cmd(mixer_dev->cd, COMP_CMD_VOLUME, NULL);
	if (err < 0)
		goto error;

// TODO: define error paths
error:

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
	struct ipc_pcm_dev *pcm_dev;
	uint32_t stream_id;
	int err;

	trace_ipc("SRt");

	// HACK - fix stream IDs
	//stream_id = header & IPC_INTEL_STR_ID_MASK;
	//stream_id >>= IPC_INTEL_STR_ID_SHIFT;
	stream_id = 0;

	/* get the pcm_dev */
	pcm_dev = ipc_get_pcm_comp(stream_id);
	if (pcm_dev == NULL)
		goto error; 

	/* send stop TODO: this should be done in trigger */
	err = pipeline_cmd(pcm_dev->dev.p, pcm_dev->dev.cd,
		COMP_CMD_STOP, NULL);
	if (err < 0)
		goto error;
	pcm_dev->state = IPC_HOST_PAUSED; // TODO: fix to stopped

	/* initialise the pipeline */
	err = pipeline_reset(pcm_dev->dev.p, pcm_dev->dev.cd);
	if (err < 0)
		goto error;

error:
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_pause(uint32_t header)
{
	struct ipc_pcm_dev *pcm_dev;
	uint32_t stream_id;
	int err;

	trace_ipc("SPa");

	// HACK - fix stream IDs
//	stream_id = header & IPC_INTEL_STR_ID_MASK;
//	stream_id >>= IPC_INTEL_STR_ID_SHIFT;
	stream_id = 0;

	/* get the pcm_dev */
	pcm_dev = ipc_get_pcm_comp(stream_id);
	if (pcm_dev == NULL)
		goto error; 

	/* driver IPC design is broken ... */
	if (pcm_dev->state == IPC_HOST_ALLOC)
		return IPC_INTEL_GLB_REPLY_SUCCESS;

	err = pipeline_cmd(pcm_dev->dev.p, pcm_dev->dev.cd,
		COMP_CMD_PAUSE, NULL);
	if (err < 0)
		goto error;
	pcm_dev->state = IPC_HOST_PAUSED;

error:
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_resume(uint32_t header)
{
	struct ipc_pcm_dev *pcm_dev;
	uint32_t stream_id;
	int err;

	trace_ipc("SRe");

	// HACK - fix stream IDs
	//stream_id = header & IPC_INTEL_STR_ID_MASK;
	//stream_id >>= IPC_INTEL_STR_ID_SHIFT;
	stream_id = 0;

	/* get the pcm_dev */
	pcm_dev = ipc_get_pcm_comp(stream_id);
	if (pcm_dev == NULL)
		goto error;

	/* TODO: add check if need prepare */
	/* initialise the pipeline, preparing pcm data */
	err = pipeline_prepare(pipeline_static, pcm_dev->dev.cd);
	if (err < 0)
		goto error;

	/* initialise the pipeline */
	err = pipeline_cmd(pcm_dev->dev.p, pcm_dev->dev.cd,
			COMP_CMD_START, NULL);
	if (err < 0)
		goto error;

	pcm_dev->state = IPC_HOST_RUNNING;

error:
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
	struct intel_ipc_data *iipc = ipc_get_drvdata(_ipc);
	uint32_t ipcxh, status;
	
	trace_ipc("Cmd");
	//trace_value(_ipc->host_msg);

	status = ipc_cmd();
	_ipc->host_pending = 0;

	/* clear BUSY bit and set DONE bit - accept new messages */
	ipcxh = shim_read(SHIM_IPCXH);
	ipcxh &= ~SHIM_IPCXH_BUSY;
	ipcxh |= SHIM_IPCXH_DONE | status;
	shim_write(SHIM_IPCXH, ipcxh);

	/* unmask busy interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_BUSY);

	/* are we about to enter D3 ? */
	if (iipc->pm_prepare_D3)
		wait_for_interrupt(0);
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

/* Send stream command */
int ipc_stream_send_notification(int stream_id)
{
	uint32_t header;
	struct ipc_intel_ipc_stream_get_position msg;

	msg.position = 100;/* this position looks not used in driver, it only care the pos registers */
	msg.fw_cycle_count = 0;
	mailbox_outbox_write(0, &msg, sizeof(msg));

	header = IPC_INTEL_GLB_TYPE(IPC_INTEL_GLB_STREAM_MESSAGE) |
		IPC_INTEL_STR_TYPE(IPC_INTEL_STR_NOTIFICATION) |
		IPC_INTEL_STG_TYPE(IPC_POSITION_CHANGED) |
		IPC_INTEL_STR_ID(stream_id);

	/* now interrupt host to tell it we have message sent */
	shim_write(SHIM_IPCDL, header);
	shim_write(SHIM_IPCDH, SHIM_IPCDH_BUSY);

	return 0;
}

int ipc_send_msg(struct ipc_msg *msg)
{

	return 0;
}

int platform_ipc_init(struct ipc *ipc)
{
	struct intel_ipc_data *iipc;
	uint32_t imrd;

	_ipc = ipc;

	/* init ipc data */
	iipc = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(struct intel_ipc_data));
	ipc_set_drvdata(_ipc, iipc);
	iipc->dai[0] = 2;
	iipc->dai[1] = 3;

	/* allocate page table buffer */
	iipc->page_table = rballoc(RZONE_DEV, RMOD_SYS,
		IPC_INTEL_PAGE_TABLE_SIZE);
	if (iipc->page_table)
		bzero(iipc->page_table, IPC_INTEL_PAGE_TABLE_SIZE);

	/* dma */
	iipc->dmac0 = dma_get(DMA_ID_DMAC0);

	/* PM */
	iipc->pm_prepare_D3 = 0;

	/* configure interrupt */
	interrupt_register(IRQ_NUM_EXT_IA, irq_handler, NULL);
	interrupt_enable(IRQ_NUM_EXT_IA);

	/* Unmask Busy and Done interrupts */
	imrd = shim_read(SHIM_IMRD);
	imrd &= ~(SHIM_IMRD_BUSY | SHIM_IMRD_DONE);
	shim_write(SHIM_IMRD, imrd);

	return 0;
}
