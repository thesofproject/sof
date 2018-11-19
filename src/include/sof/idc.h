/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file include/sof/idc.h
 * \brief IDC header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __INCLUDE_IDC_H__
#define __INCLUDE_IDC_H__

#include <sof/schedule.h>
#include <sof/trace.h>

/** \brief IDC trace function. */
#define trace_idc(__e)	trace_event(TRACE_CLASS_IDC, __e)

/** \brief IDC trace value function. */
#define tracev_idc(__e, ...)	tracev_event(TRACE_CLASS_IDC, __e, ##__VA_ARGS__)

/** \brief IDC trace error function. */
#define trace_idc_error(__e, ...)	trace_error(TRACE_CLASS_IDC, __e, ##__VA_ARGS__)

/** \brief IDC send blocking flag. */
#define IDC_BLOCKING		0

/** \brief IDC send non-blocking flag. */
#define IDC_NON_BLOCKING	1

/** \brief IDC send timeout in cycles. */
#define IDC_TIMEOUT	800000

/** \brief IDC task deadline. */
#define IDC_DEADLINE	100

/** \brief ROM wake version parsed by ROM during core wake up. */
#define IDC_ROM_WAKE_VERSION	0x2

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

/** \brief IDC power up message. */
#define IDC_MSG_POWER_UP	(IDC_TYPE(0x1) | \
					IDC_HEADER(IDC_ROM_WAKE_VERSION))
#define IDC_MSG_POWER_UP_EXT	IDC_EXTENSION(SOF_TEXT_START >> 2)

/** \brief IDC power down message. */
#define IDC_MSG_POWER_DOWN	IDC_TYPE(0x2)
#define IDC_MSG_POWER_DOWN_EXT	IDC_EXTENSION(0x0)

/** \brief IDC trigger pipeline message. */
#define IDC_MSG_PPL_TRIGGER		IDC_TYPE(0x3)
#define IDC_MSG_PPL_TRIGGER_EXT(x)	IDC_EXTENSION(x)

/** \brief IDC component command message. */
#define IDC_MSG_COMP_CMD	IDC_TYPE(0x4)
#define IDC_MSG_COMP_CMD_EXT(x)	IDC_EXTENSION(x)

/** \brief IDC notify message. */
#define IDC_MSG_NOTIFY		IDC_TYPE(0x5)
#define IDC_MSG_NOTIFY_EXT	IDC_EXTENSION(0x0)

/** \brief Decodes IDC message type. */
#define iTS(x)	(((x) >> IDC_TYPE_SHIFT) & IDC_TYPE_MASK)

/** \brief IDC message. */
struct idc_msg {
	uint32_t header;	/**< header value */
	uint32_t extension;	/**< extension value */
	uint32_t core;		/**< core id */
};

/** \brief IDC data. */
struct idc {
	spinlock_t lock;		/**< lock mechanism */
	uint32_t busy_bit_mask;		/**< busy interrupt mask */
	uint32_t done_bit_mask;		/**< done interrupt mask */
	struct idc_msg received_msg;	/**< received message */
	struct task idc_task;		/**< IDC processing task */
};

#endif
