/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 *	   Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __FUZZER_H__
#define __FUZZER_H__

#include <stdint.h>

#define DEBUG_MSG_LEN 512

/* SOF Panic */
#define SOF_IPC_PANIC_MAGIC			0x0dead000
#define SOF_IPC_PANIC_MAGIC_MASK		0x0ffff000

/* SOF driver max BARs */
#define MAX_BAR_COUNT	8

/* SOF driver IPC reply types */
#define SOF_IPC_DSP_REPLY		0
#define SOF_IPC_HOST_REPLY		1

/* kernel IRQ retrun values */
#define IRQ_NONE	0
#define IRQ_WAKE_THREAD	1
#define IRQ_HANDLED	2

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

struct fuzz;
struct fuzz_platform;

/* platform memory regions */
struct fuzzer_mem_desc {
	const char *name;
	unsigned long base;
	size_t size;
	unsigned long alias;
	void *ptr;
};

/* Register descriptor */
struct fuzzer_reg_desc {
	const char *name;	/* register name */
	uint32_t offset;	/* register offset */
	size_t size;		/* register/area size */
};

/* Device register space */
struct fuzzer_reg_space {
	const char *name;	/* device name */
	int irq;
	struct fuzzer_mem_desc desc;
};

struct mailbox {
	unsigned int offset;
	unsigned int size;
};

/* IPC message */
struct ipc_msg {
	uint32_t header;
	void *msg_data;
	unsigned int msg_size;
	void *reply_data;
	unsigned int reply_size;
};

/* platform description */
struct fuzz_platform {
	const char *name;

	/* all ops mandatory */
	int (*send_msg)(struct fuzz *f, struct ipc_msg *msg);
	int (*get_reply)(struct fuzz *f, struct ipc_msg *msg);
	int (*init)(struct fuzz *f, struct fuzz_platform *platform);
	void (*free)(struct fuzz *f);
	void (*mailbox_read)(struct fuzz *fuzzer, unsigned int offset,
			     void *mbox_data, unsigned int size);
	void (*mailbox_write)(struct fuzz *fuzzer, unsigned int offset,
			      void *mbox_data, unsigned int size);
	void (*fw_ready)(struct fuzz *fuzzer);

	/* registers */
	struct fuzzer_reg_space *reg_region;
	int num_reg_regions;

	/* memories */
	struct fuzzer_mem_desc *mem_region;
	int num_mem_regions;
};

/* runtime context */
struct fuzz {
	struct fuzz_platform *platform;
	int boot_complete;

	/* ipc */
	struct ipc_msg msg;

	/* ipc mutex */
	pthread_mutex_t ipc_mutex;

	FILE *tplg_file;

	void *platform_data; /* core does not touch this */
};

/* called by platform when it receives IPC message */
void fuzzer_ipc_msg_rx(struct fuzz *fuzzer, struct mailbox *mailbox);

/* called by platform when it receives IPC message reply */
void fuzzer_ipc_msg_reply(struct fuzz *fuzzer, struct mailbox *mailbox);

/* called by platform when FW crashses */
void fuzzer_ipc_crash(struct fuzz *fuzzer, struct mailbox *mailbox,
		      unsigned int offset);

/* called by platforms to allocate memory/register regions */
void *fuzzer_create_memory_region(struct fuzz *fuzzer, int id, int idx);
void *fuzzer_create_io_region(struct fuzz *fuzzer, int id, int idx);
void fuzzer_free_regions(struct fuzz *fuzzer);

/* ipc */
int fuzzer_send_msg(struct fuzz *fuzzer);

/* topology */
int parse_tplg(struct fuzz *fuzzer, char *tplg_filename);

/* Convenience platform ops */
static inline void fuzzer_mailbox_read(struct fuzz *fuzzer,
				       struct mailbox *mailbox, int offset,
				       void *dest, size_t bytes)
{
	fuzzer->platform->mailbox_read(fuzzer, mailbox->offset + offset,
				       dest, bytes);
}

static inline void fuzzer_mailbox_write(struct fuzz *fuzzer,
				       struct mailbox *mailbox, int offset,
				       void *src, size_t bytes)
{
	fuzzer->platform->mailbox_write(fuzzer, mailbox->offset + offset,
				       src, bytes);
}

static inline void fuzzer_fw_ready(struct fuzz *fuzzer)
{
	fuzzer->platform->fw_ready(fuzzer);
}

extern struct fuzz_platform byt_platform;
extern struct fuzz_platform cht_platform;
extern struct fuzz_platform bsw_platform;
extern struct fuzz_platform hsw_platform;
extern struct fuzz_platform bdw_platform;
extern struct fuzz_platform imx8_platform;

extern pthread_cond_t cond;

#endif
