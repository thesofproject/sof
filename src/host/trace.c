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
void setup_trace_table(void)
{
	char buffer[2048];
	char *trace = "sof/trace.h";
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

/* print trace event */
void _trace_event(uint32_t event)
{
	char a, b, c;
	char *trace_class = NULL;

	if (test_bench_trace > 0) {
		a = event & 0xff;
		b = (event >> 8) & 0xff;
		c = (event >> 16) & 0xff;

		/* look up subsystem from trace class table */
		trace_class = strdup(get_trace_class(event >> 24));

		/* print trace event stderr*/
		if (strcmp(trace_class, "value") == 0)
			fprintf(stderr, "Trace value %d\n", event);
		else
			fprintf(stderr, "Trace %s %c%c%c\n", trace_class,
				c, b, a);
	}

	free(trace_class);
}

void _trace_event_mbox_atomic(uint32_t event)
{
	_trace_event(event);
}

/* enable trace in testbench */
void tb_enable_trace(bool enable)
{
	test_bench_trace = enable;
	if (enable)
		debug_print("trace print enabled\n");
	else
		debug_print("trace print disabled\n");
}
