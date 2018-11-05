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
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#include <sof/preproc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "host/common_test.h"
#include "host/trace.h"

#define MAX_TRACE_CLASSES	255
/* testbench trace definition */

/* enable trace by default in testbench */
static int test_bench_trace = 1;
int num_trace_classes;

/* set up trace class identifier table based on SOF trace header file */
int setup_trace_table(void)
{
	char buffer[2048];
	char *trace = "uapi/logging.h";
	char *trace_filename = malloc(strlen(SOF_INC) + strlen(trace) + 2);
	char *token;
	int ret, i = 0;
	size_t size;
	FILE *fp;

	/* set up trace file name using include directory prefix */
	sprintf(trace_filename, "%s/%s", SOF_INC, trace);

	fp = fopen(trace_filename, "r");
	if (!fp) {
		fprintf(stderr, "error: opening trace include file %s\n",
			trace_filename);
		return -EINVAL;
	}

	/* find number of trace classes defined */
	while (fgets(buffer, sizeof(buffer), fp)) {
		char identifier[1024];
		int value = 0, shift = 0;

		ret = sscanf(buffer, "#define %s (%d << %d)", identifier,
			     &value, &shift);
		if (ret == 3) {
			/* if TRACE_CLASS definition */
			if (strstr(identifier, "TRACE_CLASS"))
				i++;
		}
	}

	num_trace_classes = i;

	/* allocate memory for trace table */
	size = sizeof(struct trace_class_table);
	trace_table = (struct trace_class_table *)malloc(size *
							 num_trace_classes);

	/* rewind file pointer */
	fseek(fp, 0, SEEK_SET);

	i = 0;

	/* read lines from header */
	while (fgets(buffer, sizeof(buffer), fp)) {
		char identifier[1024];
		int value = 0, shift = 0;

		ret = sscanf(buffer, "#define %s (%d << %d)", identifier,
			     &value, &shift);
		if (ret == 3) {
			/* if TRACE_CLASS definition */
			if (strstr(identifier, "TRACE_CLASS")) {
				/* extract subsystem name */
				token = strtok(identifier, "_");
				token = strtok(NULL, "_");
				token = strtok(NULL, "_");

				/* add trace class entry */
				trace_table[i].trace_class = value;
				trace_table[i].class_name = strdup(token);
				i++;
			}
		}
	}
	fclose(fp);
	free(trace_filename);
	return 0;
}

void free_trace_table(void)
{
	int i;

	for (i = 0; i < num_trace_classes; i++)
		free(trace_table[i].class_name);

	free(trace_table);
}

/* look up subsystem class name from table */
static char *get_trace_class(uint32_t trace_class)
{
	int i;

	/* look up trace class table and return subsystem name */
	for (i = 0; i < num_trace_classes; i++) {
		if (trace_table[i].trace_class == trace_class)
			return trace_table[i].class_name;
	}

	return "value";
}

#define META_SEQ_STEP_param_procU(i, _) META_CONCAT(param, i) %u

#define HOST_TRACE_EVENT_NTH_PARAMS(id_count, param_count)		\
	uintptr_t event							\
	META_SEQ_FROM_0_TO(id_count   , META_SEQ_STEP_id_uint32_t)	\
	META_SEQ_FROM_0_TO(param_count, META_SEQ_STEP_param_uint32_t)

#define HOST_TRACE_EVENT_NTH(postfix, param_count)			\
	META_FUNC_WITH_VARARGS(						\
		_trace_event, META_CONCAT(postfix, param_count),	\
		 void, HOST_TRACE_EVENT_NTH_PARAMS(2, param_count)	\
	)

/* print trace event */
#define HOST_TRACE_EVENT_NTH_IMPL(arg_count)				\
HOST_TRACE_EVENT_NTH(, arg_count)					\
{									\
	char a, b, c;							\
									\
	if (test_bench_trace > 0) {					\
		/* look up subsystem from trace class table */		\
		char *trace_class = strdup(get_trace_class(event >> 24));\
									\
		a = event & 0xff;					\
		b = (event >> 8) & 0xff;				\
		c = (event >> 16) & 0xff;				\
									\
		/* print trace event stderr*/				\
		if (!strcmp(trace_class, "value"))			\
			fprintf(stderr,					\
			"Trace value %lu, "META_QUOTE(			\
				META_SEQ_FROM_0_TO(arg_count,		\
					META_SEQ_STEP_param_procU	\
				))"\n"					\
			, event META_SEQ_FROM_0_TO(arg_count,		\
				META_SEQ_STEP_param));			\
		else							\
			fprintf(stderr,					\
			"Trace %s %c%c%c\n"				\
			, trace_class, c, b, a);			\
		if (trace_class)					\
			free(trace_class);				\
	}								\
}									\
HOST_TRACE_EVENT_NTH(_mbox_atomic, arg_count)				\
{									\
	META_CONCAT(_trace_event, arg_count)				\
		(event META_SEQ_FROM_0_TO(2, META_SEQ_STEP_id)		\
		 META_SEQ_FROM_0_TO(arg_count, META_SEQ_STEP_param));	\
}

/* Implementation of
 * void _trace_event0(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic0(uint32_t log_entry, uint32_t params...) {...}
 */
HOST_TRACE_EVENT_NTH_IMPL(0);

/* Implementation of
 * void _trace_event1(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic1(uint32_t log_entry, uint32_t params...) {...}
 */
HOST_TRACE_EVENT_NTH_IMPL(1);

/* Implementation of
 * void _trace_event2(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic2(uint32_t log_entry, uint32_t params...) {...}
 */
HOST_TRACE_EVENT_NTH_IMPL(2);

/* Implementation of
 * void _trace_event3(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic3(uint32_t log_entry, uint32_t params...) {...}
 */
HOST_TRACE_EVENT_NTH_IMPL(3);

/* Implementation of
 * void _trace_event4(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic4(uint32_t log_entry, uint32_t params...) {...}
 */
HOST_TRACE_EVENT_NTH_IMPL(4);

/* Implementation of
 * void _trace_event5(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic5(uint32_t log_entry, uint32_t params...) {...}
 */
HOST_TRACE_EVENT_NTH_IMPL(5);

/* enable trace in testbench */
void tb_enable_trace(bool enable)
{
	test_bench_trace = enable;
	if (enable)
		debug_print("trace print enabled\n");
	else
		debug_print("trace print disabled\n");
}
