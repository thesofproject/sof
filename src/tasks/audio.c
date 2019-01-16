/*
 * Copyright (c) 2016, Intel Corporation
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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Generic audio task.
 */

#include <sof/task.h>
#include <sof/wait.h>
#include <sof/debug.h>
#include <sof/timer.h>
#include <sof/interrupt.h>
#include <sof/ipc.h>
#include <sof/agent.h>
#include <platform/idc.h>
#include <platform/interrupt.h>
#include <platform/shim.h>
#include <sof/audio/pipeline.h>
#include <sof/work.h>
#include <sof/debug.h>
#include <sof/trace.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

struct audio_data {
	struct pipeline *p;
};

int do_task_master_core(struct sof *sof)
{
	int ret;
#ifdef STATIC_PIPE
	struct audio_data pdata;
#endif
	/* init default audio components */
	sys_comp_init();
	sys_comp_dai_init();
	sys_comp_host_init();
	sys_comp_mixer_init();
	sys_comp_mux_init();
	sys_comp_switch_init();
	sys_comp_volume_init();
	sys_comp_src_init();
	sys_comp_tone_init();
	sys_comp_eq_iir_init();
	sys_comp_eq_fir_init();

#if STATIC_PIPE
	/* init static pipeline */
	pdata.p = init_static_pipeline();
	if (pdata.p == NULL)
		panic(SOF_IPC_PANIC_TASK);
#endif
	/* let host know DSP boot is complete */
	ret = platform_boot_complete(0);
	if (ret < 0)
		return ret;

	/* main audio IPC processing loop */
	while (1) {
		/* sleep until next IPC or DMA */
		sa_enter_idle(sof);
		wait_for_interrupt(0);

		/* now process any IPC messages from host */
		ipc_process_msg_queue();

		/* schedule any idle tasks */
		schedule();
	}

	/* something bad happened */
	return -EIO;
}

int do_task_slave_core(struct sof *sof)
{
	/* main audio IDC processing loop */
	while (1) {
		/* sleep until next IDC */
		wait_for_interrupt(0);

		/* schedule any idle tasks */
		schedule();
	}

	/* something bad happened */
	return -EIO;
}
