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
 * Author(s): Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *	   Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *	   Keyon Jie <yang.jie@linux.intel.com>
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */
#ifndef _COMMON_TEST_H
#define _COMMON_TEST_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include <sof/sof.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>

#define DEBUG_MSG_LEN		256
#define MAX_LIB_NAME_LEN	256

/* number of widgets types supported in testbench */
#define NUM_WIDGETS_SUPPORTED	3

struct testbench_prm {
	char *tplg_file; /* topology file to use */
	char *input_file; /* input file name */
	char *output_file; /* output file name */
	char *bits_in; /* input bit format */
	/*
	 * input and output sample rate parameters
	 * By default, these are calculated from pipeline frames_per_sched
	 * and deadline but they can also be overridden via input arguments
	 * to the testbench.
	 */
	uint32_t fs_in;
	uint32_t fs_out;
};

struct shared_lib_table {
	char *comp_name;
	char library_name[MAX_LIB_NAME_LEN];
	uint32_t widget_type;
	const char *comp_init;
	int register_drv;
	void *handle;
};

extern int debug;

int edf_scheduler_init(void);

void sys_comp_file_init(void);

void sys_comp_filewrite_init(void);

int tb_pipeline_setup(struct sof *sof);

int tb_pipeline_start(struct ipc *ipc, int nch,
		      struct sof_ipc_pipe_new *ipc_pipe,
		      struct testbench_prm *tp);

int tb_pipeline_params(struct ipc *ipc, int nch,
		       struct sof_ipc_pipe_new *ipc_pipe,
		       struct testbench_prm *tp);

void debug_print(char *message);

int get_index_by_name(char *comp_name,
		      struct shared_lib_table *lib_table);

int get_index_by_type(uint32_t comp_type,
		      struct shared_lib_table *lib_table);
#endif
