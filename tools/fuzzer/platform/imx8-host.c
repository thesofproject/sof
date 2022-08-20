// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
// Copyright 2020 NXP
//
// Author: Diana Cretu <diana.cretu@nxp.com>

/* Host MU support for i.MX8 audio DSP. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include "mu.h"
#include <ipc/trace.h>
#include <ipc/info.h>
#include "../fuzzer.h"
#include "../qemu-bridge.h"

#define MBOX_OFFSET		0x144000

#define ADSP_IMX8_DSP_MAILBOX_BASE      0x92C00000
#define ADSP_IMX8_HOST_IRAM_OFFSET      0x10000
#define ADSP_IMX8_HOST_IRAM_BASE        0x596f8000
#define ADSP_IMX8_HOST_DRAM_BASE        0x596e8000
#define SDRAM0_BASE     0x92400000
#define SDRAM0_SIZE     0x800000
#define SDRAM1_BASE     0x92C00000
#define SDRAM1_SIZE     0x800000

/* Mailbox configuration */
#define ADSP_SRAM_OUTBOX_BASE       ADSP_IMX8_DSP_MAILBOX_BASE
#define ADSP_SRAM_OUTBOX_SIZE       0x1000
#define ADSP_SRAM_OUTBOX_OFFSET     0

#define ADSP_SRAM_INBOX_BASE        (ADSP_SRAM_OUTBOX_BASE \
					+ ADSP_SRAM_OUTBOX_SIZE)
#define ADSP_SRAM_INBOX_SIZE        0x1000
#define ADSP_SRAM_INBOX_OFFSET      ADSP_SRAM_OUTBOX_SIZE

#define ADSP_SRAM_DEBUG_BASE        (ADSP_SRAM_INBOX_BASE \
				     + ADSP_SRAM_INBOX_SIZE)
#define ADSP_SRAM_DEBUG_SIZE        0x800
#define ADSP_SRAM_DEBUG_OFFSET      (ADSP_SRAM_INBOX_OFFSET \
				     + ADSP_SRAM_INBOX_SIZE)

#define ADSP_SRAM_EXCEPT_BASE       (ADSP_SRAM_DEBUG_BASE \
				     + ADSP_SRAM_DEBUG_SIZE)
#define ADSP_SRAM_EXCEPT_SIZE       0x800
#define ADSP_SRAM_EXCEPT_OFFSET     (ADSP_SRAM_DEBUG_OFFSET \
				     + ADSP_SRAM_DEBUG_SIZE)

#define ADSP_SRAM_STREAM_BASE       (ADSP_SRAM_EXCEPT_BASE \
				     + ADSP_SRAM_EXCEPT_SIZE)
#define ADSP_SRAM_STREAM_SIZE       0x1000
#define ADSP_SRAM_STREAM_OFFSET     (ADSP_SRAM_EXCEPT_OFFSET \
				     + ADSP_SRAM_EXCEPT_SIZE)

#define ADSP_SRAM_TRACE_BASE        (ADSP_SRAM_STREAM_BASE \
				     + ADSP_SRAM_STREAM_SIZE)
#define ADSP_SRAM_TRACE_SIZE        0x1000
#define ADSP_SRAM_TRACE_OFFSET      (ADSP_SRAM_STREAM_OFFSET \
				     + ADSP_SRAM_STREAM_SIZE)

#define ADSP_IMX8_DSP_MAILBOX_SIZE  (ADSP_SRAM_INBOX_SIZE \
				     + ADSP_SRAM_OUTBOX_SIZE \
				     + ADSP_SRAM_DEBUG_SIZE \
				     + ADSP_SRAM_EXCEPT_SIZE \
				     + ADSP_SRAM_STREAM_SIZE \
				     + ADSP_SRAM_TRACE_SIZE)
#define ADSP_IMX8_IRAM_SIZE          0x8000
#define ADSP_IMX8_DRAM_SIZE          0x8000

#define ADSP_IMX8_DSP_MU_SIZE		0x10000
#define ADSP_IMX8_DSP_MU13B_BASE	0x5D310000
#define ADSP_IMX8_DSP_MU13A_BASE	0x5D280000

#define ADSP_MAILBOX_SIZE		0x1000

/* TODO get from driver. */
#define IMX8_PANIC_OFFSET(x)	(x)

struct imx8_data {
	void *bar[MAX_BAR_COUNT];
	struct mailbox host_box;
	struct mailbox dsp_box;
	int boot_complete;
	pthread_mutex_t mutex;
};

/* Platform host description taken from Qemu
 * - mapped to BAR 0, 1, 2, 3
 */
static struct fuzzer_mem_desc imx8_mem[] = {
	{.name = "iram", .base = ADSP_IMX8_HOST_IRAM_BASE,
	 .size = ADSP_IMX8_IRAM_SIZE},
	{.name = "dram", .base = ADSP_IMX8_HOST_DRAM_BASE,
	 .size = ADSP_IMX8_DRAM_SIZE},
	{.name = "sdram0", .base = SDRAM0_BASE,
	 .size = SDRAM1_SIZE},
	{.name = "sdram1", .base = SDRAM1_BASE,
	 .size = SDRAM1_SIZE},
};

/* mapped to BAR 4, 5, 6 */
static struct fuzzer_reg_space imx8_io[] = {
	{ .name = "mu_13a",
	  .desc = {.base = ADSP_IMX8_DSP_MU13A_BASE,
		   .size = ADSP_IMX8_DSP_MU_SIZE},},
	{ .name = "mu_13b",
	  .desc = {.base = ADSP_IMX8_DSP_MU13B_BASE,
		   .size = ADSP_IMX8_DSP_MU_SIZE},},
	{ .name = "mbox",
	  .desc = {.base = ADSP_IMX8_DSP_MAILBOX_BASE,
		   .size = ADSP_IMX8_DSP_MAILBOX_SIZE},},
};

#define IMX8_MU13_A_BAR		4
#define IMX8_MU13_B_BAR		5
#define IMX8_MBOX_BAR		6

/*
 * Platform support for i.MX8.
 *
 * The IPC portions below are copied and pasted from the SOF driver with some
 * modification for data structure and printing.
 *
 * The "driver" code below no longer writes directly to the HW but writes
 * to the virtual HW as exported by qemu as Posix SHM and message queues.
 *
 * Register IO and mailbox IO is performed using shared memory regions between
 * fuzzer and qemu.
 *
 * IRQs are send using message queues between fuzzer and qemu.
 *
 * SHM and message queues can be inspected from the cmd line by using
 * "less -C" on /dev/shm/name and /dev/mqueue/name
 */

static uint64_t imx8_fixup_side_b(struct fuzz *fuzzer, unsigned int bar,
				  unsigned int reg, uint64_t value)
{
	uint64_t status_side_b;

	status_side_b = imx_mu_read(fuzzer, IMX8_MU13_B_BAR, IMX_MU_xCR);

	switch (reg) {
	case IMX_MU_xCR:
		/* new message sent to DSP */
		if (value & IMX_MU_xCR_GIRn(0)) {
			if (status_side_b & IMX_MU_xCR_GIEn(0)) {
				imx_mu_xsr_rmw(fuzzer, IMX8_MU13_B_BAR,
					       IMX_MU_xSR_GIPn(0), 0);
			}
		}

		/* reply message sent to DSP */
		if (value & IMX_MU_xCR_GIRn(1))
			if (status_side_b & IMX_MU_xCR_GIEn(1)) {
				imx_mu_xsr_rmw(fuzzer, IMX8_MU13_B_BAR,
					       IMX_MU_xSR_GIPn(1), 0);
			}
		break;
	default:
		break;
	}

	return status_side_b;
}

static uint64_t imx_mu_read(struct fuzz *fuzzer, unsigned int bar,
			    unsigned int reg)
{
	struct imx8_data *data = fuzzer->platform_data;

	return *((uint64_t *)(data->bar[bar] + reg));
}

static void imx_mu_write(struct fuzz *fuzzer, unsigned int bar,
			 unsigned int reg, uint64_t value)
{
	struct imx8_data *data = fuzzer->platform_data;

	/* write value to SHM */
	*((uint64_t *)(data->bar[bar] + reg)) = value;

	imx8_fixup_side_b(fuzzer, IMX8_MU13_B_BAR, reg, value);
}

static uint64_t imx_mu_xcr_rmw(struct fuzz *fuzzer,
			       unsigned int bar,
			       uint64_t set, uint64_t clr)
{
	uint64_t val;

	val = imx_mu_read(fuzzer, bar, IMX_MU_xCR);
	val &= ~clr;
	val |= set;
	imx_mu_write(fuzzer, bar, IMX_MU_xCR, val);

	return val;
}

static uint64_t imx_mu_xsr_rmw(struct fuzz *fuzzer,
			       unsigned int bar,
			       uint64_t set, uint64_t clr)
{
	uint64_t val;

	val = imx_mu_read(fuzzer, bar, IMX_MU_xSR);
	val &= ~clr;
	val |= set;
	imx_mu_write(fuzzer, bar, IMX_MU_xSR, val);

	return val;
}

static void mailbox_read(struct fuzz *fuzzer, unsigned int offset,
			 void *mbox_data, unsigned int size)
{
	struct imx8_data *data = fuzzer->platform_data;

	memcpy(mbox_data, (void *)(data->bar[IMX8_MBOX_BAR] + offset), size);
}

static void mailbox_write(struct fuzz *fuzzer, unsigned int offset,
			  void *mbox_data, unsigned int size)
{
	struct imx8_data *data = fuzzer->platform_data;

	memcpy((void *)(data->bar[IMX8_MBOX_BAR] + offset), mbox_data, size);
}

static int imx8_cmd_done(struct fuzz *fuzzer, int dir)
{
	if (dir == SOF_IPC_HOST_REPLY) {
		/* activate GIRn(1) - notify host that reply is ready */
		imx_mu_xcr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       IMX_MU_xCR_GIRn(1), 0);

		/* enable GP interrupt #1 - accept new messages */
		imx_mu_xcr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       IMX_MU_xCR_GIEn(1), 0);
	} else {
		/* Clear GP pending interrupt #0 */
		imx_mu_xsr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       0, IMX_MU_xSR_GIPn(0));
	}
	return 0;
}

/*
 * IPC Doorbell IRQ handler.
 */
static int imx8_irq_handler(int irq, void *context)
{
	struct fuzz *fuzzer = (struct fuzz *)context;
	struct imx8_data *data = fuzzer->platform_data;
	uint64_t status;

	/* Interrupt arrived, check src */
	status = imx_mu_read(fuzzer, IMX8_MU13_A_BAR, IMX_MU_xSR);

	/* reply message from DSP */
	if (status & IMX_MU_xSR_GIPn(0)) {
		/* Disable GP interrupt #0 */
		imx_mu_xcr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       0, IMX_MU_xCR_GIEn(0));

		/* Clear GP pending interrupt #0 */
		imx_mu_xsr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       0, IMX_MU_xSR_GIPn(0));

		/*
		 * handle immediate reply from DSP core. If the msg is
		 * found, set done bit in cmd_done which is called at the
		 * end of message processing function, else set it here
		 * because the done bit can't be set in cmd_done function
		 * which is triggered by msg
		 */
		fuzzer_ipc_msg_reply(fuzzer, &data->host_box);
		imx8_cmd_done(fuzzer, SOF_IPC_DSP_REPLY);

		/* unmask GP interrupt #1 */
		imx_mu_xcr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       IMX_MU_xCR_GIEn(0), 0);

		return IRQ_HANDLED;
	}

	/* new message from DSP */
	if (status & IMX_MU_xSR_GIPn(1)) {
		/* Disable GP interrupt #0 */
		imx_mu_xcr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       0, IMX_MU_xCR_GIEn(1));

		/* Clear GP pending interrupt #1 */
		imx_mu_xsr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       0, IMX_MU_xSR_GIPn(1));

		/* read mailbox */
		if ((status & SOF_IPC_PANIC_MAGIC_MASK) ==
		    SOF_IPC_PANIC_MAGIC) {
			fuzzer_ipc_crash(fuzzer, &data->dsp_box,
					 IMX8_PANIC_OFFSET(status)
					 + MBOX_OFFSET);
		} else {
			fuzzer_ipc_msg_rx(fuzzer, &data->dsp_box);
		}

		if (!data->boot_complete && fuzzer->boot_complete) {
			data->boot_complete = 1;
			imx8_cmd_done(fuzzer, SOF_IPC_HOST_REPLY);
			pthread_cond_signal(&cond);
			return IRQ_HANDLED;
		}

		/* Enable GIEn(1) */
		imx_mu_xcr_rmw(fuzzer, IMX8_MU13_A_BAR,
			       IMX_MU_xCR_GIEn(1), 0);
	}

	return IRQ_HANDLED;
}

static int imx8_send_msg(struct fuzz *fuzzer, struct ipc_msg *msg)
{
	struct imx8_data *data = fuzzer->platform_data;
	struct qemu_io_msg_irq irq;

	/* send the message */
	fuzzer_mailbox_write(fuzzer, &data->host_box, 0, msg->msg_data,
			     msg->msg_size);

	/* now interrupt DSP to tell it we have sent a message */
	imx_mu_xcr_rmw(fuzzer, IMX8_MU13_A_BAR,
		       IMX_MU_xCR_GIRn(0), 0);

	irq.hdr.type = QEMU_IO_TYPE_IRQ;
	irq.hdr.msg = QEMU_IO_MSG_IRQ;
	irq.hdr.size = sizeof(irq);
	irq.irq = 0;

	qemu_io_send_msg(&irq.hdr);

	return 0;
}

static int imx8_get_reply(struct fuzz *fuzzer, struct ipc_msg *msg)
{
	struct imx8_data *data = fuzzer->platform_data;
	struct sof_ipc_reply reply;
	int ret = 0;
	uint32_t size;

	/* get reply */
	fuzzer_mailbox_read(fuzzer, &data->host_box, 0, &reply, sizeof(reply));

	if (reply.error < 0) {
		size = sizeof(reply);
		ret = reply.error;
	} else {
		/* reply correct size ? */
		if (reply.hdr.size != msg->reply_size) {
			fprintf(stderr,
				"error: reply expected 0x%x got 0x%x bytes\n",
				msg->reply_size, reply.hdr.size
				);
			size = msg->reply_size;
			ret = -EINVAL;
		} else {
			size = reply.hdr.size;
		}
	}

	/* read the message */
	if (msg->msg_data && size > 0)
		fuzzer_mailbox_read(fuzzer, &data->host_box, 0,
				    msg->reply_data, size);

	return ret;
}

/* called when we receive a message from qemu */
static int bridge_cb(void *data, struct qemu_io_msg *msg)
{
	uint64_t status, status_sideA;
	struct fuzz *fuzzer = (struct fuzz *)data;

	fprintf(stdout, "msg: id %d msg %d size %d type %d\n",
		msg->id, msg->msg, msg->size, msg->type);

	switch (msg->type) {
	case QEMU_IO_TYPE_IRQ:
		/* IRQ from DSP */
		status = imx_mu_read(fuzzer, IMX8_MU13_B_BAR, IMX_MU_xCR);
		status_sideA = imx_mu_read(fuzzer, IMX8_MU13_A_BAR, IMX_MU_xCR);

		/* check GIRn bit from processor side B */
		if (status & IMX_MU_xCR_GIRn(1)) {
			/* if GIRn(1) is activated on processor side B,
			 * activate GIPn(1) on processor side A
			 */
			if (status_sideA & IMX_MU_xCR_GIEn(1)) {
				imx_mu_xsr_rmw(fuzzer, IMX8_MU13_A_BAR,
					       IMX_MU_xSR_GIPn(1), 0);
			}
		}

		if (status & IMX_MU_xCR_GIRn(0)) {
			/* if GIRn(0) is activated on processor side B,
			 * activate GIPn(0) on processor side A
			 */
			if (status_sideA & IMX_MU_xCR_GIEn(0)) {
				imx_mu_xsr_rmw(fuzzer, IMX8_MU13_A_BAR,
					       IMX_MU_xSR_GIPn(0), 0);
			}
		}

		imx8_irq_handler(0, fuzzer);
		break;
	default:
		break;
	}

	return 0;
}

static int imx8_platform_init(struct fuzz *fuzzer,
			      struct fuzz_platform *platform)
{
	struct timespec timeout;
	struct imx8_data *data;
	struct timeval tp;
	int i, bar;
	int ret = 0;

	/* init private data */
	data = calloc(sizeof(*data), 1);
	if (!data)
		return -ENOMEM;
	fuzzer->platform_data = data;
	fuzzer->platform = platform;

	/* create SHM for memories and register regions */
	for (i = 0, bar = 0; i < platform->num_mem_regions; i++, bar++) {
		data->bar[bar] = fuzzer_create_memory_region(fuzzer, bar, i);
		if (!data->bar[bar]) {
			fprintf(stderr,
				"error: failed to create mem region %s\n",
				platform->mem_region[i].name);
			return -ENOMEM;
		}
	}

	for (i = 0; i < platform->num_reg_regions; i++, bar++) {
		data->bar[bar] = fuzzer_create_io_region(fuzzer, bar, i);
		if (!data->bar[bar]) {
			fprintf(stderr,
				"error: failed to create mem region %s\n",
				platform->reg_region[i].name);
			return -ENOMEM;
		}
	}

	/* enable GIE #0 for DSP -> Fuzzer message notification
	 * enable GIE #1 for Fuzzer -> DSP message notification
	 */
	imx_mu_xcr_rmw(fuzzer, IMX8_MU13_A_BAR,
		       IMX_MU_xCR_GIEn(0) | IMX_MU_xCR_GIEn(1), 0);

	/* initialise bridge to qemu */
	qemu_io_register_parent(platform->name, &bridge_cb, (void *)fuzzer);

	/* set boot wait timeout */
	gettimeofday(&tp, NULL);
	timeout.tv_sec  = tp.tv_sec;
	timeout.tv_nsec = tp.tv_usec * 1000;
	timeout.tv_sec += 5;

	/* first lock the boot wait mutex */
	pthread_mutex_lock(&data->mutex);

	/* now wait for mutex to be unlocked by boot ready message */
	while (!ret && !data->boot_complete)
		ret = pthread_cond_timedwait(&cond, &data->mutex, &timeout);

	if (ret == ETIMEDOUT && !data->boot_complete)
		fprintf(stderr, "error: DSP boot timeout\n");

	pthread_mutex_unlock(&data->mutex);

	return ret;
}

static void imx8_platform_free(struct fuzz *fuzzer)
{
	struct imx8_data *data = fuzzer->platform_data;

	fuzzer_free_regions(fuzzer);
	free(data);
}

static void imx8_fw_ready(struct fuzz *fuzzer)
{
	struct imx8_data *data = fuzzer->platform_data;
	struct sof_ipc_fw_ready fw_ready;
	struct sof_ipc_fw_version version;

	/* read fw_ready data from mailbox */
	fuzzer_mailbox_read(fuzzer, &data->dsp_box, 0,
			    &fw_ready, sizeof(fw_ready));

	/*
	 * Hardcode offsets.
	 * TODO: read init host_box and dsp_box from
	 *  fw_ready message
	 */
	data->host_box.offset = 0x1000;
	data->host_box.size = 0x1000;
	data->dsp_box.offset = 0;
	data->dsp_box.size = 0x1000;

	fprintf(stdout,
		"ipc: host box 0x%x size 0x%x\n",
		data->host_box.offset, data->host_box.size);
	fprintf(stdout, "ipc: dsp box 0x%x size 0x%x\n",
		data->dsp_box.offset, data->dsp_box.size);

	version = fw_ready.version;
	fprintf(stdout, "ipc: FW version major: %d minor: %d tag: %s\n",
		version.major, version.minor, version.tag);
}

struct fuzz_platform imx8_platform = {
	.name = "i.MX8",
	.send_msg = imx8_send_msg,
	.get_reply = imx8_get_reply,
	.init = imx8_platform_init,
	.free = imx8_platform_free,
	.mailbox_read = mailbox_read,
	.mailbox_write = mailbox_write,
	.fw_ready = imx8_fw_ready,
	.num_mem_regions = ARRAY_SIZE(imx8_mem),
	.mem_region = imx8_mem,
	.num_reg_regions = ARRAY_SIZE(imx8_io),
	.reg_region = imx8_io,
};
