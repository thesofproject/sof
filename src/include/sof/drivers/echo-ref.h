/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_ECHO_REF_H__
#define __SOF_DRIVERS_ECHO_REF_H__

#include <sof/bit.h>
#include <sof/lib/dai.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#define trace_echo(__e, ...) \
	trace_event(TRACE_CLASS_DAI, __e, ##__VA_ARGS__)
#define tracev_echo(__e, ...) \
	tracev_event(TRACE_CLASS_DAI, __e, ##__VA_ARGS__)
#define trace_echo_error(__e, ...) \
	trace_error(TRACE_CLASS_DAI, __e, ##__VA_ARGS__)

extern const struct dai_driver echo_ref_driver;
#endif /* __SOF_DRIVERS_ECHO_REF_H__ */
