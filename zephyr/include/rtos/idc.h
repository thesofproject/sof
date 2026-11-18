/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file include/rtos/idc.h
 * \brief IDC header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __ZEPHYR_RTOS_IDC_H__
#define __ZEPHYR_RTOS_IDC_H__

#include <rtos/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>
#include <rtos/cache.h>
#include <rtos/idc_msg.h>

#include <sof/debug/telemetry/performance_monitor.h>

/** \brief IDC task deadline. */
#define IDC_DEADLINE	100

/** \brief ROM wake version parsed by ROM during core wake up. */
#define IDC_ROM_WAKE_VERSION	0x2

/** \brief IDC power up message. */
#define IDC_MSG_POWER_UP	(IDC_TYPE(0x1) | \
					IDC_HEADER(IDC_ROM_WAKE_VERSION))
#define IDC_MSG_POWER_UP_EXT	IDC_EXTENSION(SOF_TEXT_START >> 2)

/** \brief Max IDC message payload size in bytes. */
#define IDC_MAX_PAYLOAD_SIZE	(DCACHE_LINE_SIZE * 2)

/** \brief IDC free function flags */
#define IDC_FREE_IRQ_ONLY	BIT(0)	/**< disable only irqs */

/**
 * \brief IDC message payload.
 *
 * Placed in coherent (SHARED_DATA) memory and shared across cores.
 */
struct idc_payload {
	uint8_t data[IDC_MAX_PAYLOAD_SIZE];
} __aligned(DCACHE_LINE_SIZE);

/** \brief IDC data. */
struct idc {
	uint32_t busy_bit_mask;		/**< busy interrupt mask */
	struct idc_msg received_msg;	/**< received message */
	struct task idc_task;		/**< IDC processing task */
	struct idc_payload *payload;
	int irq;
#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS
	/* io performance measurement */
	struct io_perf_data_item *io_perf_in_msg_count;
	struct io_perf_data_item *io_perf_out_msg_count;
#endif
};

/* idc trace context, used by multiple units */
extern struct tr_ctx idc_tr;

static inline struct idc_payload *idc_payload_get(struct idc *idc,
						  uint32_t core)
{
	return idc->payload + core;
}

void idc_enable_interrupts(int target_core, int source_core);

void idc_free(uint32_t flags);

int platform_idc_init(void);

int platform_idc_restore(void);

enum task_state idc_do_cmd(void *data);

void idc_cmd(struct idc_msg *msg);

int idc_msg_status_get(uint32_t core);

int idc_init(void);

void idc_init_thread(void);

struct idc **idc_get(void);

#endif /* __ZEPHYR_RTOS_IDC_H__ */
