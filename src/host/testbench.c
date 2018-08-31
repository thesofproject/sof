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
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#include <sof/ipc.h>
#include <sof/list.h>
#include <getopt.h>
#include <dlfcn.h>
#include "host/common_test.h"
#include "host/topology.h"
#include "host/trace.h"
#include "host/file.h"

#define TESTBENCH_NCH 2 /* Stereo */

/* shared library look up table */
struct shared_lib_table lib_table[NUM_WIDGETS_SUPPORTED] = {
{"file", "", SND_SOC_TPLG_DAPM_AIF_IN, "", 0, NULL},
{"vol", "libsof_volume.so", SND_SOC_TPLG_DAPM_PGA, "sys_comp_volume_init", 0,
	NULL},
{"src", "libsof_src.so", SND_SOC_TPLG_DAPM_SRC, "sys_comp_src_init", 0, NULL},
};

/* main firmware context */
static struct sof sof;
static int fr_id; /* comp id for fileread */
static int fw_id; /* comp id for filewrite */
static int sched_id; /* comp id for scheduling comp */

int debug;

/*
 * Parse shared library from user input
 * Currently only handles volume and src comp
 * This function takes in the libraries to be used as an input in the format:
 * "vol=libsof_volume.so,src=libsof_src.so,..."
 * The function parses the above string to identify the following:
 * component type and the library name and sets up the library handle
 * for the component and stores it in the shared library table
 */
static void parse_libraries(char *libs)
{
	char *lib_token, *comp_token;
	char *token = strtok_r(libs, ",", &lib_token);
	char message[DEBUG_MSG_LEN];
	int index;

	while (token) {

		/* get component type */
		char *token1 = strtok_r(token, "=", &comp_token);

		/* get shared library index from library table */
		index = get_index_by_name(token1, lib_table);

		if (index < 0) {
			fprintf(stderr, "error: unsupported comp type\n");
			break;
		}

		/* get shared library name */
		token1 = strtok_r(NULL, "=", &comp_token);
		if (!token1)
			break;

		/* close default shared library object */
		if (lib_table[index].handle)
			dlclose(lib_table[index].handle);

		/* open volume shared library object */
		lib_table[index].handle = dlopen(token1, RTLD_LAZY);
		if (!lib_table[index].handle) {
			fprintf(stderr, "error: %s\n", dlerror());
			exit(EXIT_FAILURE);
		}

		sprintf(message, "opening shared lib %s\n", token1);
		debug_print(message);

		/* next library */
		token = strtok_r(NULL, ",", &lib_token);
	}
}

/* print usage for testbench */
static void print_usage(char *executable)
{
	printf("Usage: %s -i <input_file> -o <output_file> ", executable);
	printf("-t <tplg_file> -b <input_format> ");
	printf("-a <comp1=comp1_library,comp2=comp2_library>\n");
	printf("input_format should be S16_LE, S32_LE, S24_LE or FLOAT_LE\n");
	printf("Example Usage:\n");
	printf("%s -i in.txt -o out.txt -t test.tplg ", executable);
	printf("-r 48000 -R 96000 ");
	printf("-b S16_LE -a vol=libsof_volume.so\n");
}

/* free components */
static void free_comps(void)
{
	struct list_item *clist;
	struct list_item *temp;
	struct ipc_comp_dev *icd = NULL;

	list_for_item_safe(clist, temp, &sof.ipc->shared_ctx->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		switch (icd->type) {
		case COMP_TYPE_COMPONENT:
			comp_free(icd->cd);
			list_item_del(&icd->list);
			rfree(icd);
			break;
		case COMP_TYPE_BUFFER:
			rfree(icd->cb->addr);
			rfree(icd->cb);
			list_item_del(&icd->list);
			rfree(icd);
			break;
		default:
			rfree(icd->pipeline);
			list_item_del(&icd->list);
			rfree(icd);
			break;
		}
	}
}

static int set_up_library_table(void)
{
	int i;

	/* set up default shared libraries */
	for (i = 1; i < NUM_WIDGETS_SUPPORTED; i++) {

		/* open default shared library */
		lib_table[i].handle =
				dlopen(lib_table[i].library_name, RTLD_LAZY);
		if (!lib_table[i].handle) {
			fprintf(stderr, "error: %s\n", dlerror());
			return -EINVAL;
		}
	}

	return 0;
}

static void parse_input_args(int argc, char **argv)
{
	int option = 0;

	while ((option = getopt(argc, argv, "hdi:o:t:b:a:r:R:")) != -1) {
		switch (option) {
		/* input sample file */
		case 'i':
			input_file = strdup(optarg);
			break;

		/* output sample file */
		case 'o':
			output_file = strdup(optarg);
			break;

		/* topology file */
		case 't':
			tplg_file = strdup(optarg);
			break;

		/* input samples bit format */
		case 'b':
			bits_in = strdup(optarg);
			break;

		/* override default libraries */
		case 'a':
			parse_libraries(optarg);
			break;

		/* input sample rate */
		case 'r':
			fs_in = atoi(optarg);
			break;

		/* output sample rate */
		case 'R':
			fs_out = atoi(optarg);
			break;

		/* enable debug prints */
		case 'd':
			debug = 1;
			break;

		/* print usage */
		case 'h':
		default:
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char **argv)
{
	struct ipc_comp_dev *pcm_dev;
	struct pipeline *p;
	struct sof_ipc_pipe_new *ipc_pipe;
	struct comp_dev *cd;
	struct file_comp_data *frcd, *fwcd;
	char pipeline[DEBUG_MSG_LEN];
	clock_t tic, toc;
	double c_realtime, t_exec;
	int n_in, n_out, ret;
	int i;

	/* initialize input and output sample rates */
	fs_in = 0;
	fs_out = 0;

	/* set up shared library look up table */
	ret = set_up_library_table();
	if (ret < 0) {
		fprintf(stderr, "error: setting up shared libraried\n");
		exit(EXIT_FAILURE);
	}

	/* set up trace class definition table from trace header */
	ret = setup_trace_table();
	if (ret < 0) {
		fprintf(stderr, "error: setting up trace header table\n");
		exit(EXIT_FAILURE);
	}
	/* command line arguments*/
	parse_input_args(argc, argv);

	/* check args */
	if (!tplg_file || !input_file || !output_file || !bits_in) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* initialize ipc and scheduler */
	if (tb_pipeline_setup(&sof) < 0) {
		fprintf(stderr, "error: pipeline init\n");
		exit(EXIT_FAILURE);
	}

	/* parse topology file and create pipeline */
	if (parse_topology(tplg_file, &sof, &fr_id, &fw_id, &sched_id, bits_in,
	    input_file, output_file, lib_table, pipeline) < 0) {
		fprintf(stderr, "error: parsing topology\n");
		exit(EXIT_FAILURE);
	}

	/* Get pointers to fileread and filewrite */
	pcm_dev = ipc_get_comp(sof.ipc, fw_id);
	fwcd = comp_get_drvdata(pcm_dev->cd);
	pcm_dev = ipc_get_comp(sof.ipc, fr_id);
	frcd = comp_get_drvdata(pcm_dev->cd);

	/* Run pipeline until EOF from fileread */
	pcm_dev = ipc_get_comp(sof.ipc, sched_id);
	p = pcm_dev->cd->pipeline;
	ipc_pipe = &p->ipc_pipe;

	/* input and output sample rate */
	if (!fs_in)
		fs_in = ipc_pipe->deadline * ipc_pipe->frames_per_sched;

	if (!fs_out)
		fs_out = ipc_pipe->deadline * ipc_pipe->frames_per_sched;

	/* set pipeline params and trigger start */
	if (tb_pipeline_start(sof.ipc, TESTBENCH_NCH, bits_in, ipc_pipe) < 0) {
		fprintf(stderr, "error: pipeline params\n");
		exit(EXIT_FAILURE);
	}

	cd = pcm_dev->cd;
	tb_enable_trace(false); /* reduce trace output */
	tic = clock();

	while (frcd->fs.reached_eof == 0)
		pipeline_schedule_copy(p, 0);

	if (!frcd->fs.reached_eof)
		printf("warning: possible pipeline xrun\n");

	/* reset and free pipeline */
	toc = clock();
	tb_enable_trace(true);
	ret = pipeline_reset(p, cd);
	if (ret < 0) {
		fprintf(stderr, "error: pipeline reset\n");
		exit(EXIT_FAILURE);
	}

	n_in = frcd->fs.n;
	n_out = fwcd->fs.n;
	t_exec = (double)(toc - tic) / CLOCKS_PER_SEC;
	c_realtime = (double)n_out / TESTBENCH_NCH / fs_out / t_exec;

	/* free all components/buffers in pipeline */
	free_comps();

	/* free trace class defs */
	free_trace_table();

	/* print test summary */
	printf("==========================================================\n");
	printf("		           Test Summary\n");
	printf("==========================================================\n");
	printf("Test Pipeline:\n");
	printf("%s\n", pipeline);
	printf("Input bit format: %s\n", bits_in);
	printf("Input sample rate: %d\n", fs_in);
	printf("Output sample rate: %d\n", fs_out);
	printf("Output written to file: \"%s\"\n", output_file);
	printf("Input sample count: %d\n", n_in);
	printf("Output sample count: %d\n", n_out);
	printf("Total execution time: %.2f us, %.2f x realtime\n",
	       1e3 * t_exec, c_realtime);

	/* free all other data */
	free(bits_in);
	free(input_file);
	free(tplg_file);
	free(output_file);

	/* close shared library objects */
	for (i = 0; i < NUM_WIDGETS_SUPPORTED; i++) {
		if (lib_table[i].handle)
			dlclose(lib_table[i].handle);
	}

	return EXIT_SUCCESS;
}
