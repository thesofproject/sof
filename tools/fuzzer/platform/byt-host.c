// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Core IA host SHIM support for Baytrail audio DSP. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include "shim.h"
#include <ipc/trace.h>
#include <ipc/info.h>
#include "../fuzzer.h"
#include "../qemu-bridge.h"

#define MBOX_OFFSET		0x144000

/* Baytrail, Cherrytrail and Braswell - taken from qemu */
#define ADSP_PCI_SIZE				0x00001000
#define ADSP_BYT_PCI_BASE           0xF1200000
#define ADSP_BYT_MMIO_BASE          0xF1400000
#define ADSP_BYT_HOST_IRAM_OFFSET   0x000c0000
#define ADSP_BYT_HOST_DRAM_OFFSET   0x00100000
#define ADSP_BYT_HOST_IRAM_BASE     \
	(ADSP_BYT_MMIO_BASE + ADSP_BYT_HOST_IRAM_OFFSET)
#define ADSP_BYT_HOST_DRAM_BASE     \
	(ADSP_BYT_MMIO_BASE + ADSP_BYT_HOST_DRAM_OFFSET)
#define ADSP_BYT_HOST_SHIM_BASE     \
	(ADSP_BYT_MMIO_BASE + 0x00140000)
#define ADSP_BYT_HOST_MAILBOX_BASE  \
	(ADSP_BYT_MMIO_BASE + 0x00144000)

#define ADSP_CHT_PCI_BASE           0xF1600000
#define ADSP_CHT_MMIO_BASE          0xF1800000
#define ADSP_CHT_HOST_IRAM_BASE     \
	(ADSP_CHT_MMIO_BASE + ADSP_BYT_HOST_IRAM_OFFSET)
#define ADSP_CHT_HOST_DRAM_BASE     \
	(ADSP_CHT_MMIO_BASE + ADSP_BYT_HOST_DRAM_OFFSET)
#define ADSP_CHT_HOST_SHIM_BASE     \
	(ADSP_CHT_MMIO_BASE + 0x00140000)
#define ADSP_CHT_HOST_MAILBOX_BASE  \
	(ADSP_CHT_MMIO_BASE + 0x00144000)

#define ADSP_BYT_IRAM_SIZE          0x14000
#define ADSP_BYT_DRAM_SIZE          0x28000
#define ADSP_BYT_SHIM_SIZE          0x1000
#define ADSP_MAILBOX_SIZE			0x1000

/* TODO get from driver. */
#define BYT_PANIC_OFFSET(x)	(x)

struct byt_data {
	void *bar[MAX_BAR_COUNT];
	struct mailbox host_box;
	struct mailbox dsp_box;
	int boot_complete;
	pthread_mutex_t mutex;
};

/* Platform host description taken from Qemu  - mapped to BAR 0, 1*/
static struct fuzzer_mem_desc byt_mem[] = {
	{.name = "iram", .base = ADSP_BYT_HOST_IRAM_BASE,
	 .size = ADSP_BYT_IRAM_SIZE},
	{.name = "dram", .base = ADSP_BYT_HOST_DRAM_BASE,
	 .size = ADSP_BYT_DRAM_SIZE},
};

/* mapped to BAR 2, 3 */
static struct fuzzer_reg_space byt_io[] = {
	{ .name = "shim",
	  .desc = {.base = ADSP_BYT_HOST_SHIM_BASE,
		   .size = ADSP_BYT_SHIM_SIZE},},
	{ .name = "mbox",
	  .desc = {.base = ADSP_BYT_HOST_MAILBOX_BASE,
		   .size = ADSP_MAILBOX_SIZE},},
};

#define BYT_DSP_BAR	2
#define BYT_MBOX_BAR	3

/*
 * Platform support for BYT/CHT.
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

static uint64_t dsp_read64(struct fuzz *fuzzer, unsigned int bar,
			   unsigned int reg)
{
	struct byt_data *data = fuzzer->platform_data;

	return *((uint64_t *)(data->bar[bar] + reg));
}

static void dsp_write64(struct fuzz *fuzzer, unsigned int bar,
			unsigned int reg, uint64_t value)
{
	struct byt_data *data = fuzzer->platform_data;
	struct qemu_io_msg_irq irq;
	uint32_t active, isrd;

	/* write value to SHM */
	*((uint64_t *)(data->bar[bar] + reg)) = value;

	/* most IO is handled by SHM, but there are some exceptions */
	switch (reg) {
	case SHIM_IPCX:

		/* now set/clear status bit */
		isrd = dsp_read64(fuzzer, bar, SHIM_ISRD) &
			~(SHIM_ISRD_DONE | SHIM_ISRD_BUSY);
		isrd |= value & SHIM_IPCX_BUSY ? SHIM_ISRD_BUSY : 0;
		isrd |= value & SHIM_IPCX_DONE ? SHIM_ISRD_DONE : 0;
		dsp_write64(fuzzer, bar, SHIM_ISRD, isrd);

		/* do we need to send an IRQ ? */
		if (value & SHIM_IPCX_BUSY) {

			fprintf(stdout, "irq: send busy interrupt 0x%8.8lx\n",
				value);

			/* send IRQ to child */
			irq.hdr.type = QEMU_IO_TYPE_IRQ;
			irq.hdr.msg = QEMU_IO_MSG_IRQ;
			irq.hdr.size = sizeof(irq);
			irq.irq = 0;

			qemu_io_send_msg(&irq.hdr);
		}
		break;
	case SHIM_IPCD:

		/* set/clear status bit */
		isrd = dsp_read64(fuzzer, bar, SHIM_ISRD) &
			~(SHIM_ISRD_DONE | SHIM_ISRD_BUSY);
		isrd |= value & SHIM_IPCD_BUSY ? SHIM_ISRD_BUSY : 0;
		isrd |= value & SHIM_IPCD_DONE ? SHIM_ISRD_DONE : 0;
		dsp_write64(fuzzer, bar, SHIM_ISRD, isrd);

		/* do we need to send an IRQ ? */
		if (value & SHIM_IPCD_DONE) {

			fprintf(stdout, "irq: send done interrupt 0x%8.8lx\n",
				value);

			/* send IRQ to child */
			irq.hdr.type = QEMU_IO_TYPE_IRQ;
			irq.hdr.msg = QEMU_IO_MSG_IRQ;
			irq.hdr.size = sizeof(irq);
			irq.irq = 0;

			qemu_io_send_msg(&irq.hdr);
		}
		break;
	case SHIM_IMRX:

		active = dsp_read64(fuzzer, bar, SHIM_ISRX) &
			~(dsp_read64(fuzzer, bar, SHIM_IMRX));

		fprintf(stdout, "irq: masking %lx mask %lx active %x\n",
			dsp_read64(fuzzer, bar, SHIM_ISRD),
			dsp_read64(fuzzer, bar, SHIM_IMRD), active);
		break;
	default:
		break;
	}
}

static uint64_t dsp_update_bits64_unlocked(struct fuzz *fuzzer,
					   unsigned int bar, uint32_t offset,
					   uint64_t mask, uint64_t value)
{
	uint64_t old, new;
	uint64_t ret;

	ret = dsp_read64(fuzzer, bar, offset);
	old = ret;

	new = (old & ~mask) | (value & mask);

	if (old == new)
		return 0;

	dsp_write64(fuzzer, bar, offset, new);
	return 1;
}

static void mailbox_read(struct fuzz *fuzzer, unsigned int offset,
			 void *mbox_data, unsigned int size)
{
	struct byt_data *data = fuzzer->platform_data;

	memcpy(mbox_data, (void *)(data->bar[BYT_MBOX_BAR] + offset), size);
}

static void mailbox_write(struct fuzz *fuzzer, unsigned int offset,
			  void *mbox_data, unsigned int size)
{
	struct byt_data *data = fuzzer->platform_data;

	memcpy((void *)(data->bar[BYT_MBOX_BAR] + offset), mbox_data, size);
}

static int byt_cmd_done(struct fuzz *fuzzer, int dir)
{
	if (dir == SOF_IPC_HOST_REPLY) {
		/* clear BUSY bit and set DONE bit - accept new messages */
		dsp_update_bits64_unlocked(fuzzer, BYT_DSP_BAR, SHIM_IPCD,
					   SHIM_BYT_IPCD_BUSY |
					   SHIM_BYT_IPCD_DONE,
					   SHIM_BYT_IPCD_DONE);

		/* unmask busy interrupt */
		dsp_update_bits64_unlocked(fuzzer, BYT_DSP_BAR, SHIM_IMRX,
					   SHIM_IMRX_BUSY, 0);
	} else {
		/* clear DONE bit - tell DSP we have completed */
		dsp_update_bits64_unlocked(fuzzer, BYT_DSP_BAR, SHIM_IPCX,
					   SHIM_BYT_IPCX_DONE, 0);

		/* unmask Done interrupt */
		dsp_update_bits64_unlocked(fuzzer, BYT_DSP_BAR, SHIM_IMRX,
					   SHIM_IMRX_DONE, 0);
	}

	return 0;
}

/*
 * IPC Doorbell IRQ handler and thread.
 */

static int byt_irq_handler(int irq, void *context)
{
	struct fuzz *fuzzer = (struct fuzz *)context;
	uint64_t isr;
	int ret = IRQ_NONE;

	/* Interrupt arrived, check src */
	isr = dsp_read64(fuzzer, BYT_DSP_BAR, SHIM_ISRX);
	if (isr & (SHIM_ISRX_DONE | SHIM_ISRX_BUSY))
		ret = IRQ_WAKE_THREAD;

	return ret;
}

static int byt_irq_thread(int irq, void *context)
{
	struct fuzz *fuzzer = (struct fuzz *)context;
	struct byt_data *data = fuzzer->platform_data;
	uint64_t ipcx, ipcd;
	uint64_t imrx;

	imrx = dsp_read64(fuzzer, BYT_DSP_BAR, SHIM_IMRX);
	ipcx = dsp_read64(fuzzer, BYT_DSP_BAR, SHIM_IPCX);

	/* reply message from DSP */
	if ((ipcx & SHIM_BYT_IPCX_DONE) &&
	    !(imrx & SHIM_IMRX_DONE)) {
		/* Mask Done interrupt before first */
		dsp_update_bits64_unlocked(fuzzer, BYT_DSP_BAR,
					   SHIM_IMRX,
					   SHIM_IMRX_DONE,
					   SHIM_IMRX_DONE);

		fprintf(stdout, "ipc: reply msg from DSP\n");
		/*
		 * handle immediate reply from DSP core. If the msg is
		 * found, set done bit in cmd_done which is called at the
		 * end of message processing function, else set it here
		 * because the done bit can't be set in cmd_done function
		 * which is triggered by msg
		 */
		fuzzer_ipc_msg_reply(fuzzer, &data->host_box);
		byt_cmd_done(fuzzer, SOF_IPC_DSP_REPLY);
		return IRQ_HANDLED;
	}

	/* new message from DSP */
	ipcd = dsp_read64(fuzzer, BYT_DSP_BAR, SHIM_IPCD);
	if ((ipcd & SHIM_BYT_IPCD_BUSY) &&
	    !(imrx & SHIM_IMRX_BUSY)) {
		/* Mask Busy interrupt before return */
		dsp_update_bits64_unlocked(fuzzer, BYT_DSP_BAR,
					   SHIM_IMRX,
					   SHIM_IMRX_BUSY,
					   SHIM_IMRX_BUSY);

		/* read mailbox */
		if ((ipcd & SOF_IPC_PANIC_MAGIC_MASK) == SOF_IPC_PANIC_MAGIC) {
			fuzzer_ipc_crash(fuzzer, &data->dsp_box,
					BYT_PANIC_OFFSET(ipcd) + MBOX_OFFSET);
		} else {
			fuzzer_ipc_msg_rx(fuzzer, &data->dsp_box);
		}

		if (!data->boot_complete && fuzzer->boot_complete) {
			data->boot_complete = 1;
			byt_cmd_done(fuzzer, SOF_IPC_HOST_REPLY);
			pthread_cond_signal(&cond);
			return IRQ_HANDLED;
		}
	}

	return IRQ_HANDLED;
}

static int byt_send_msg(struct fuzz *fuzzer, struct ipc_msg *msg)
{
	struct byt_data *data = fuzzer->platform_data;
	uint64_t cmd = msg->header;

	/* send the message */
	fuzzer_mailbox_write(fuzzer, &data->host_box, 0, msg->msg_data,
		      msg->msg_size);
	dsp_write64(fuzzer, BYT_DSP_BAR, SHIM_IPCX,
		    cmd | SHIM_BYT_IPCX_BUSY);

	return 0;
}

static int byt_get_reply(struct fuzz *fuzzer, struct ipc_msg *msg)
{
	struct byt_data *data = fuzzer->platform_data;
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
			       msg->reply_size, reply.hdr.size);
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
	struct fuzz *fuzzer = (struct fuzz *)data;

	fprintf(stdout, "msg: id %d msg %d size %d type %d\n",
		msg->id, msg->msg, msg->size, msg->type);

	switch (msg->type) {
	case QEMU_IO_TYPE_IRQ:
		/* IRQ from DSP */
		if (byt_irq_handler(0, fuzzer) != IRQ_NONE)
			byt_irq_thread(0, fuzzer);
		break;
	default:
		break;
	}

	return 0;
}

static int byt_platform_init(struct fuzz *fuzzer,
			     struct fuzz_platform *platform)
{
	struct timespec timeout;
	struct byt_data *data;
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
			fprintf(stderr, "error: failed to create mem region %s\n",
				platform->mem_region[i].name);
			return -ENOMEM;
		}
	}

	for (i = 0; i < platform->num_reg_regions; i++, bar++) {
		data->bar[bar] = fuzzer_create_io_region(fuzzer, bar, i);
		if (!data->bar[bar]) {
			fprintf(stderr, "error: failed to create mem region %s\n",
				platform->reg_region[i].name);
			return -ENOMEM;
		}
	}

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

static void byt_platform_free(struct fuzz *fuzzer)
{
	struct byt_data *data = fuzzer->platform_data;

	fuzzer_free_regions(fuzzer);
	free(data);
}

static void byt_fw_ready(struct fuzz *fuzzer)
{
	struct byt_data *data = fuzzer->platform_data;
	struct sof_ipc_fw_ready fw_ready;
	struct sof_ipc_fw_version version;

	/* read fw_ready data from mailbox */
	fuzzer_mailbox_read(fuzzer, &data->dsp_box, 0,
			    &fw_ready, sizeof(fw_ready));

	/*
	 * Hardcode offsets.
	 * TODO: read init host_box and dsp_box from fw_ready message
	 */
	data->host_box.offset = 0x400;
	data->host_box.size = 0x400;
	data->dsp_box.offset = 0;
	data->dsp_box.size = 0x400;

	fprintf(stdout,
		"ipc: host box 0x%x size 0x%x\n", data->host_box.offset,
		data->host_box.size);
	fprintf(stdout, "ipc: dsp box 0x%x size 0x%x\n", data->dsp_box.offset,
		data->dsp_box.size);

	version = fw_ready.version;
	fprintf(stdout, "ipc: FW version major: %d minor: %d tag: %s\n",
		version.major, version.minor, version.tag);
}

struct fuzz_platform byt_platform = {
	.name = "byt",
	.send_msg = byt_send_msg,
	.get_reply = byt_get_reply,
	.init = byt_platform_init,
	.free = byt_platform_free,
	.mailbox_read = mailbox_read,
	.mailbox_write = mailbox_write,
	.fw_ready = byt_fw_ready,
	.num_mem_regions = ARRAY_SIZE(byt_mem),
	.mem_region = byt_mem,
	.num_reg_regions = ARRAY_SIZE(byt_io),
	.reg_region = byt_io,
};

struct fuzz_platform cht_platform = {
	.name = "cht",
	.send_msg = byt_send_msg,
	.get_reply = byt_get_reply,
	.init = byt_platform_init,
	.free = byt_platform_free,
	.mailbox_read = mailbox_read,
	.mailbox_write = mailbox_write,
	.fw_ready = byt_fw_ready,
	.num_mem_regions = ARRAY_SIZE(byt_mem),
	.mem_region = byt_mem,
	.num_reg_regions = ARRAY_SIZE(byt_io),
	.reg_region = byt_io,
};

struct fuzz_platform bsw_platform = {
	.name = "bsw",
	.send_msg = byt_send_msg,
	.get_reply = byt_get_reply,
	.init = byt_platform_init,
	.free = byt_platform_free,
	.mailbox_read = mailbox_read,
	.mailbox_write = mailbox_write,
	.fw_ready = byt_fw_ready,
	.num_mem_regions = ARRAY_SIZE(byt_mem),
	.mem_region = byt_mem,
	.num_reg_regions = ARRAY_SIZE(byt_io),
	.reg_region = byt_io,
};
