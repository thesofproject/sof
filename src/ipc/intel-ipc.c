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
#include <platform/interrupt.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <uapi/intel-ipc.h>

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
	dbg();

	mailbox_outbox_write(0, &fw_version, sizeof(fw_version));

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_fw_caps(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
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

/* Stream ring info */
struct sst_hsw_ipc_stream_ring {
	u32 ring_pt_address;
	u32 num_pages;
	u32 ring_size;
	u32 ring_offset;
	u32 ring_first_pfn;
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

static uint32_t ipc_stream_alloc(uint32_t header)
{
	struct ipc_intel_ipc_stream_alloc_req req;
	struct ipc_intel_ipc_stream_alloc_reply reply;
	struct stream_params params;
	struct comp_desc host, dai;
	int err;

	dbg();
	mailbox_inbox_read(0, &req, sizeof(req));

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
		/* TODO: */
		break;
	};

	/* read in format to create params */
	params.pcm.channels = req.format.ch_num;
	/* TODO: rest of params */

	/* set host PCM params */
	err = pipeline_params(pipeline_static, &host, &params);	
	if (err < 0)
		goto error;

	/* read in ringbuffer and send to host PCM */
	params.dma_ring.ring_pt_address = req.ringinfo.ring_pt_address;
	/* TODO: read rest of ring buffer */
	err = pipeline_params(pipeline_static, &host, &params);	
	if (err < 0)
		goto error;

	/* Configure SSP DMA and send to DAI*/
	params.dma_dev.src = 0;
	/* TODO: read rest of ring buffer */
	err = pipeline_params(pipeline_static, &dai, &params);	
	if (err < 0)
		goto error;

	/* initialise the pipeline */
	err = pipeline_prepare(pipeline_static, &host);
	if (err < 0)
		goto error;

	/* at this point pipeline is ready for command so send stream reply */
	reply.stream_hw_id = 0;
	reply.mixer_hw_id = 0; // returns rate ????
	reply.read_position_register_address = 0; /* TODO: setup in mailbox */
	reply.presentation_position_register_address = 0; /* TODO: setup in mailbox */
	//reply.peak_meter_register_address[IPC_INTEL_NO_CHANNELS];
	//reply.volume_register_address[IPC_INTEL_NO_CHANNELS];
	mailbox_outbox_write(0, &reply, sizeof(reply));

error:	/* TODO */
	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_free(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_info(uint32_t header)
{
	struct ipc_intel_ipc_stream_info_reply info;

	dbg();

	/* TODO: get data from topology */ 
	info.mixer_hw_id = 1;
	info.peak_meter_register_address[0] = 0;
	info.peak_meter_register_address[1] = 0;
	info.volume_register_address[0] = 0;
	info.volume_register_address[1] = 0;
	mailbox_outbox_write(0, &info, sizeof(info));

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_dump(uint32_t header)
{
	dbg();

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
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_device_set_formats(uint32_t header)
{
	dbg();

	// call
	//int pipeline_params(int pipeline_id, struct pipe_comp_desc *host_desc,
	//struct stream_params *params);

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_save(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_restore(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_set_volume(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_get_volume(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_write_pos(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stage_message(uint32_t header)
{
	uint32_t stg = msg_get_stage_type(header);

	dbg();

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
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_pause(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_resume(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_notification(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_message(uint32_t header)
{
	uint32_t str = msg_get_stream_type(header);

	dbg();

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

	header = shim_read(SHIM_IPCX);
	type = msg_get_global_type(header);
	dbg_val(type);
	dbg();

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
	uint32_t status, ipcx;
	
	dbg();
	status = ipc_cmd();

	/* clear BUSY bit and set DONE bit - accept new messages */
	ipcx = shim_read(SHIM_IPCX);
	ipcx &= ~SHIM_IPCX_BUSY;
	ipcx |= SHIM_IPCX_DONE | status;
	shim_write(SHIM_IPCX, ipcx);

	/* unmask busy interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_BUSY);
}

static void do_notify(void)
{
	dbg();

	/* clear DONE bit - tell Host we have completed */
	shim_write(SHIM_IPCD, shim_read(SHIM_IPCD) & ~SHIM_IPCD_DONE);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DONE);
}

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
	uint32_t isr;

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
		do_cmd();
	}

	interrupt_clear(IRQ_NUM_EXT_IA);
}

int platform_ipc_init(struct ipc *context)
{
	interrupt_register(IRQ_NUM_EXT_IA, irq_handler, context);
	interrupt_enable(IRQ_NUM_EXT_IA);

	return 0;
}
