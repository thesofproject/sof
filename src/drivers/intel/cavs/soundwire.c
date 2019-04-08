/*
 * Copyright (c) 2019, Intel Corporation
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
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#include <errno.h>
#include <stdbool.h>
#include <sof/stream.h>
#include <sof/dai.h>
#include <sof/alloc.h>
#include <sof/interrupt.h>
#include <sof/pm_runtime.h>
#include <sof/math/numbers.h>
#include <config.h>

#define trace_soundwire(__e, ...) \
	trace_event(TRACE_CLASS_SOUNDWIRE, __e, ##__VA_ARGS__)
#define trace_soundwire_error(__e, ...)	\
	trace_error(TRACE_CLASS_SOUNDWIRE, __e, ##__VA_ARGS__)
#define tracev_soundwire(__e, ...) \
	tracev_event(TRACE_CLASS_SOUNDWIRE, __e, ##__VA_ARGS__)

static int soundwire_trigger(struct dai *dai, int cmd, int direction)
{
	trace_soundwire("soundwire_trigger() cmd %d", cmd);

	return 0;
}

static int soundwire_set_config(struct dai *dai,
				struct sof_ipc_dai_config *config)
{
	trace_soundwire("soundwire_set_config() config->format = 0x%4x",
			config->format);

	return 0;
}

static int soundwire_context_store(struct dai *dai)
{
	trace_soundwire("soundwire_context_store()");

	return 0;
}

static int soundwire_context_restore(struct dai *dai)
{
	trace_soundwire("soundwire_context_restore()");

	return 0;
}

static int soundwire_probe(struct dai *dai)
{
	trace_soundwire("soundwire_probe()");

	return 0;
}

static int soundwire_remove(struct dai *dai)
{
	trace_soundwire("soundwire_remove()");

	return 0;
}

const struct dai_ops soundwire_ops = {
	.trigger		= soundwire_trigger,
	.set_config		= soundwire_set_config,
	.pm_context_store	= soundwire_context_store,
	.pm_context_restore	= soundwire_context_restore,
	.probe			= soundwire_probe,
	.remove			= soundwire_remove,
};
