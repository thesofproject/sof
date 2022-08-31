/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@intel.com>
 */

/**
 * \file include/ipc/input-event.h
 * \brief IPC definitions
 * \author Jyri Sarha <jyri.sarha@intel.com>
 */

#ifndef __IPC_INPUT_EVENT_H__
#define __IPC_INPUT_EVENT_H__

#include <ipc/header.h>
#include <stdint.h>

struct sof_ipc_input_event {
	struct sof_ipc_reply rhdr;
	uint32_t code;		/* 'code' for Linux input_report_key() */
	int32_t value;		/* 'value' for Linux input_report_key() */
} __attribute__((packed, aligned(4)));

/*
 * Values of Linux input report key 'code' used by SOF input device. 
 * Copy the values from Lunux source tree:
 * include/uapi/linux/input-event-codes.h
 */

#define BTN_0			0x100
#define BTN_1			0x101
#define BTN_2			0x102
#define BTN_3			0x103
#define BTN_4			0x104
#define BTN_5			0x105
#define BTN_6			0x106
#define BTN_7			0x107
#define BTN_8			0x108
#define BTN_9			0x109

#endif /* __IPC_TRACE_H__ */
