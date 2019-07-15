/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef IO_BRIDGE_H
#define IO_BRIDGE_H

#include <stdint.h>

#define QEMU_IO_DEBUG           0

/* IO type */
#define QEMU_IO_TYPE_QEMU       0
#define QEMU_IO_TYPE_REG        1
#define QEMU_IO_TYPE_IRQ        2
#define QEMU_IO_TYPE_GDB        3
#define QEMU_IO_TYPE_PM         4
#define QEMU_IO_TYPE_DMA        5
#define QEMU_IO_TYPE_MEM        6

/* Global Message Reply */
#define QEMU_IO_MSG_REPLY       0

/* Register Messages */
#define QEMU_IO_MSG_REG32W      32
#define QEMU_IO_MSG_REG64W      33
#define QEMU_IO_MSG_REG32R      34
#define QEMU_IO_MSG_REG64R      35

/* IRQ Messages */
#define QEMU_IO_MSG_IRQ         64

/* DMA Messages */
#define QEMU_IO_DMA_REQ_NEW     96
#define QEMU_IO_DMA_REQ_READY   97
#define QEMU_IO_DMA_REQ_COMPLETE    98

/* DMA Direction - relative to msg sender */
#define QEMU_IO_DMA_DIR_READ    256
#define QEMU_IO_DMA_DIR_WRITE   257

/* GDB Messages */
#define QEMU_IO_GDB_STALL       128
#define QEMU_IO_GDB_CONT        129
#define QEMU_IO_GDB_STALL_RPLY  130 /* stall after reply */

/* PM Messages */
#define QEMU_IO_PM_S0           192
#define QEMU_IO_PM_S1           193
#define QEMU_IO_PM_S2           194
#define QEMU_IO_PM_S3           195
#define QEMU_IO_PM_D0           196
#define QEMU_IO_PM_D1           197
#define QEMU_IO_PM_D2           198
#define QEMU_IO_PM_D3           199

/* Common message header */
struct qemu_io_msg {
	uint16_t type;
	uint16_t msg;
	uint32_t size;
	uint32_t id;
};

/* Generic message reply */
struct qemu_io_msg_reply {
	struct qemu_io_msg hdr;
	uint32_t reply;
};

/* Register messages */
struct qemu_io_msg_reg32 {
	struct qemu_io_msg hdr;
	uint32_t reg;
	uint32_t val;
};

struct qemu_io_msg_reg64 {
	struct qemu_io_msg hdr;
	uint64_t reg;
	uint64_t val;
};

/* IRQ Messages */
struct qemu_io_msg_irq {
	struct qemu_io_msg hdr;
	uint32_t irq;
};

/* PM Messages */
struct qemu_io_msg_pm_state {
	struct qemu_io_msg hdr;
};

/* DMA Messages - same message used as reply */
struct qemu_io_msg_dma32 {
	struct qemu_io_msg hdr;
	uint32_t direction;	/*  QEMU_IO_DMA_DIR_ */
	uint32_t reply;		/* 0 or errno */
	uint32_t src;
	uint32_t dest;
	uint32_t size;
	uint32_t dmac_id;
	uint32_t chan_id;
	uint64_t host_data;
	uint64_t client_data;
};

struct qemu_io_msg_dma64 {
	struct qemu_io_msg hdr;
	uint32_t direction;	/*  QEMU_IO_DMA_DIR_ */
	uint32_t reply;		/* 0 or errno */
	uint64_t src;
	uint64_t dest;
	uint64_t size;
	uint32_t dmac_id;
	uint32_t chan_id;
	uint64_t host_data;
	uint64_t client_data;
};

/* API calls for parent and child */
int qemu_io_register_parent(const char *name,
			    int (*cb)(void *, struct qemu_io_msg *msg),
			    void *data);
int qemu_io_register_child(const char *name,
			   int (*cb)(void *, struct qemu_io_msg *msg),
			   void *data);

int qemu_io_send_msg(struct qemu_io_msg *msg);
int qemu_io_send_msg_reply(struct qemu_io_msg *msg);

int qemu_io_register_shm(const char *name, int region, size_t size,
			 void **addr);
int qemu_io_sync(int region, unsigned int offset, size_t length);

void qemu_io_free(void);
void qemu_io_free_shm(int region);

#endif
