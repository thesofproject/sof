/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_AUDIO_PIPELINE_TRACE_H__
#define __SOF_AUDIO_PIPELINE_TRACE_H__

#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/* pipeline tracing */
extern struct tr_ctx pipe_tr;

#define trace_pipe_get_tr_ctx(pipe_p) (&(pipe_p)->tctx)
#define trace_pipe_get_id(pipe_p) ((pipe_p)->pipeline_id)
#define trace_pipe_get_subid(pipe_p) ((pipe_p)->comp_id)

/* class (driver) level (no device object) tracing */

#define pipe_cl_err(__e, ...)						\
	tr_err(&pipe_tr, __e, ##__VA_ARGS__)

#define pipe_cl_warn(__e, ...)						\
	tr_warn(&pipe_tr, __e, ##__VA_ARGS__)

#define pipe_cl_info(__e, ...)						\
	tr_info(&pipe_tr, __e, ##__VA_ARGS__)

#define pipe_cl_dbg(__e, ...)						\
	tr_dbg(&pipe_tr, __e, ##__VA_ARGS__)

/* device tracing */
#define pipe_err(pipe_p, __e, ...)					\
	trace_dev_err(trace_pipe_get_tr_ctx, trace_pipe_get_id,		\
		      trace_pipe_get_subid, pipe_p, __e, ##__VA_ARGS__)

#define pipe_warn(pipe_p, __e, ...)					\
	trace_dev_warn(trace_pipe_get_tr_ctx, trace_pipe_get_id,	\
		       trace_pipe_get_subid, pipe_p, __e, ##__VA_ARGS__)

#define pipe_info(pipe_p, __e, ...)					\
	trace_dev_info(trace_pipe_get_tr_ctx, trace_pipe_get_id,	\
		       trace_pipe_get_subid, pipe_p, __e, ##__VA_ARGS__)

#define pipe_dbg(pipe_p, __e, ...)					\
	trace_dev_dbg(trace_pipe_get_tr_ctx, trace_pipe_get_id,		\
		      trace_pipe_get_subid, pipe_p, __e, ##__VA_ARGS__)

#endif
