/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file include/rtos/idc_msg.h
 * \brief IDC message format, for code that sends IDC messages
 *
 * User-space code can include this header. The IDC driver and its
 * kernel-only state are in rtos/idc.h.
 */

#ifndef __ZEPHYR_RTOS_IDC_MSG_H__
#define __ZEPHYR_RTOS_IDC_MSG_H__

#include <rtos/bit.h>
#include <stdint.h>

/** \brief IDC send blocking flag. */
#define IDC_BLOCKING		0

/** \brief IDC send non-blocking flag. */
#define IDC_NON_BLOCKING	1

/** \brief IDC send core power up flag. */
#define IDC_POWER_UP		2

/** \brief IDC send core power down flag. */
#define IDC_POWER_DOWN		3

/** \brief IDC message type. */
#define IDC_TYPE_SHIFT		24
#define IDC_TYPE_MASK		0x7f
#define IDC_TYPE(x)		(((x) & IDC_TYPE_MASK) << IDC_TYPE_SHIFT)

/** \brief IDC message header. */
#define IDC_HEADER_MASK		0xffffff
#define IDC_HEADER(x)		((x) & IDC_HEADER_MASK)

/** \brief IDC message extension. */
#define IDC_EXTENSION_MASK	0x3fffffff
#define IDC_EXTENSION(x)	((x) & IDC_EXTENSION_MASK)

/** \brief IDC power down message. */
#define IDC_MSG_POWER_DOWN	IDC_TYPE(0x2)
#define IDC_MSG_POWER_DOWN_EXT	IDC_EXTENSION(0x0)

/** \brief IDC notify message. */
#define IDC_MSG_NOTIFY		IDC_TYPE(0x3)
#define IDC_MSG_NOTIFY_EXT	IDC_EXTENSION(0x0)

/** \brief IDC IPC processing message. */
#define IDC_MSG_IPC		IDC_TYPE(0x4)
#define IDC_MSG_IPC_EXT		IDC_EXTENSION(0x0)

/** \brief IDC component params message. */
#define IDC_MSG_PARAMS		IDC_TYPE(0x5)
#define IDC_MSG_PARAMS_EXT(x)	IDC_EXTENSION(x)

/** \brief IDC component prepare message. */
#define IDC_MSG_PREPARE		IDC_TYPE(0x6)
#define IDC_MSG_PREPARE_EXT(x)	IDC_EXTENSION(x)

/** \brief IDC component trigger message. */
#define IDC_MSG_TRIGGER		IDC_TYPE(0x7)
#define IDC_MSG_TRIGGER_EXT(x)	IDC_EXTENSION(x)

/** \brief IDC component reset message. */
#define IDC_MSG_RESET		IDC_TYPE(0x8)
#define IDC_MSG_RESET_EXT(x)	IDC_EXTENSION(x)

/** \brief IDC prepare D0ix message. */
#define IDC_MSG_PREPARE_D0ix		IDC_TYPE(0x9)
#define IDC_MSG_PREPARE_D0ix_EXT	IDC_EXTENSION(0x0)

/** \brief IDC secondary core crashed notify message. */
#define IDC_MSG_SECONDARY_CORE_CRASHED		IDC_TYPE(0xA)
#define IDC_MSG_SECONDARY_CORE_CRASHED_EXT(x)	IDC_EXTENSION(x)

/** \brief IDC process async msg */
#define IDC_MSG_AMS	IDC_TYPE(0xB)
#define IDC_MSG_AMS_EXT	IDC_EXTENSION(0x0)

#define IDC_HEADER_TO_AMS_SLOT_MASK(x)	(x & 0xFFFF)

#define IDC_MSG_BIND IDC_TYPE(0xD)
#define IDC_MSG_UNBIND IDC_TYPE(0xE)
#define IDC_MSG_GET_ATTRIBUTE IDC_TYPE(0xF)

/** \brief IDC component delete message. */
#define IDC_MSG_FREE		IDC_TYPE(0x10)
#define IDC_MSG_FREE_EXT(x)	IDC_EXTENSION(x)

/** \brief IDC pipeline set state message. */
#define IDC_MSG_PPL_STATE		IDC_TYPE(0xC)
#define IDC_PPL_STATE_PPL_ID_SHIFT	0
#define IDC_PPL_STATE_PPL_ID_MASK	MASK(23, 0)
#define IDC_PPL_STATE_PHASE_SHIFT	24
#define IDC_PPL_STATE_PHASE_MASK	MASK(27, 24)
#define IDC_PPL_STATE_PHASE_SET(x)	(((x) << IDC_PPL_STATE_PHASE_SHIFT) &	\
					 IDC_PPL_STATE_PHASE_MASK)
#define IDC_PPL_STATE_PHASE_GET(x)	(((x) & IDC_PPL_STATE_PHASE_MASK) >>	\
					 IDC_PPL_STATE_PHASE_SHIFT)
#define IDC_PPL_STATE_PHASE_PREPARE	BIT(0)
#define IDC_PPL_STATE_PHASE_TRIGGER	BIT(1)
#define IDC_PPL_STATE_PHASE_ONESHOT	(IDC_PPL_STATE_PHASE_PREPARE |		\
					 IDC_PPL_STATE_PHASE_TRIGGER)

#define IDC_MSG_PPL_STATE_EXT(_ppl_id, _action)						\
					IDC_EXTENSION(((_ppl_id) & IDC_PPL_STATE_PPL_ID_MASK) |	\
					IDC_PPL_STATE_PHASE_SET(_action))

/** \brief IDC_MSG_SECONDARY_CORE_CRASHED header fields. */
#define IDC_SCC_CORE_SHIFT		0
#define IDC_SCC_CORE_MASK		0xff
#define IDC_SCC_CORE(x)			(((x) & IDC_SCC_CORE_MASK) << IDC_SCC_CORE_SHIFT)

#define IDC_SCC_REASON_SHIFT		8
#define IDC_SCC_REASON_MASK		0xff
#define IDC_SCC_REASON(x)		(((x) & IDC_SCC_REASON_MASK) << IDC_SCC_REASON_SHIFT)

/** \brief Secondary core crash reasons. */
#define IDC_SCC_REASON_WATCHDOG		0x00
#define IDC_SCC_REASON_EXCEPTION	0x01

/** \brief Decodes IDC message type. */
#define iTS(x)	(((x) >> IDC_TYPE_SHIFT) & IDC_TYPE_MASK)

/** \brief IDC message. */
struct idc_msg {
	uint32_t header;	/**< header value */
	uint32_t extension;	/**< extension value */
	uint32_t core;		/**< core id */
	uint32_t size;		/**< payload size in bytes */
	const void *payload;	/**< pointer to payload data */
};

/**
 * \brief Sends an IDC message to another core.
 *
 * In user-space LL builds, messages sent from user threads are forwarded
 * to the user IPC thread of the target core.
 */
int idc_send_msg(struct idc_msg *msg, uint32_t mode);

#ifdef CONFIG_SOF_USERSPACE_LL
/* cross-core component messages between the user-space IPC and LL threads */
int idc_cmd_user(const struct idc_msg *msg, void *payload);
int ipc_user_xcore_send(const struct idc_msg *msg);
#endif

#endif /* __ZEPHYR_RTOS_IDC_MSG_H__ */
