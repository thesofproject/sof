// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/drivers/ipc.h>
#include <sof/list.h>
#include <getopt.h>
#include <dlfcn.h>
#include "testbench/common_test.h"
#include <tplg_parser/topology.h>
#include "testbench/trace.h"
#include "testbench/file.h"

#define DECLARE_SOF_TB_UUID(entity_name, uuid_name,			\
			 va, vb, vc,					\
			 vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7)	\
	struct sof_uuid uuid_name = {					\
		.a = va, .b = vb, .c = vc,				\
		.d = {vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7}		\
	}

#define SOF_TB_UUID(uuid_name) (&(uuid_name))

DECLARE_SOF_TB_UUID("crossover", crossover_uuid, 0x948c9ad1, 0x806a, 0x4131,
		    0xad, 0x6c, 0xb2, 0xbd, 0xa9, 0xe3, 0x5a, 0x9f);

DECLARE_SOF_TB_UUID("tdfb", tdfb_uuid,  0xdd511749, 0xd9fa, 0x455c,
		    0xb3, 0xa7, 0x13, 0x58, 0x56, 0x93, 0xf1, 0xaf);

DECLARE_SOF_TB_UUID("drc", drc_uuid, 0xb36ee4da, 0x006f, 0x47f9,
		    0xa0, 0x6d, 0xfe, 0xcb, 0xe2, 0xd8, 0xb6, 0xce);

DECLARE_SOF_TB_UUID("multiband_drc", multiband_drc_uuid, 0x0d9f2256, 0x8e4f, 0x47b3,
		    0x84, 0x48, 0x23, 0x9a, 0x33, 0x4f, 0x11, 0x91);

#define TESTBENCH_NCH 2 /* Stereo */

/* shared library look up table */
struct shared_lib_table lib_table[NUM_WIDGETS_SUPPORTED] = {
	{"file", "", SOF_COMP_HOST, NULL, 0, NULL}, /* File must be first */
	{"volume", "libsof_volume.so", SOF_COMP_VOLUME, NULL, 0, NULL},
	{"src", "libsof_src.so", SOF_COMP_SRC, NULL, 0, NULL},
	{"asrc", "libsof_asrc.so", SOF_COMP_ASRC, NULL, 0, NULL},
	{"eq-fir", "libsof_eq-fir.so", SOF_COMP_EQ_FIR, NULL, 0, NULL},
	{"eq-iir", "libsof_eq-iir.so", SOF_COMP_EQ_IIR, NULL, 0, NULL},
	{"dcblock", "libsof_dcblock.so", SOF_COMP_DCBLOCK, NULL, 0, NULL},
	{"crossover", "libsof_crossover.so", SOF_COMP_NONE, SOF_TB_UUID(crossover_uuid), 0, NULL},
	{"tdfb", "libsof_tdfb.so", SOF_COMP_NONE, SOF_TB_UUID(tdfb_uuid), 0, NULL},
	{"drc", "libsof_drc.so", SOF_COMP_NONE, SOF_TB_UUID(drc_uuid), 0, NULL},
	{"multiband_drc", "libsof_multiband_drc.so", SOF_COMP_NONE, SOF_TB_UUID(multiband_drc_uuid), 0, NULL},
};

/* compatible variables, not used */
intptr_t _comp_init_start, _comp_init_end;

/*
 * Parse output filenames from user input
 * This function takes in the output filenames as an input in the format:
 * "output_file1,output_file2,..."
 * The max supported output filename number is 4, min is 1.
 */
static int parse_output_files(char *outputs, struct testbench_prm *tp)
{
	char *output_token = NULL;
	char *token = strtok_r(outputs, ",", &output_token);
	int index;

	for (index = 0; index < MAX_OUTPUT_FILE_NUM && token; index++) {
		/* get output file name with current index */
		tp->output_file[index] = strdup(token);

		/* next output */
		token = strtok_r(NULL, ",", &output_token);
	}

	if (index == MAX_OUTPUT_FILE_NUM && token) {
		fprintf(stderr, "error: max output file number is %d\n",
			MAX_OUTPUT_FILE_NUM);
		for (index = 0; index < MAX_OUTPUT_FILE_NUM; index++)
			free(tp->output_file[index]);
		return -EINVAL;
	}

	/* set total output file number */
	tp->output_file_num = index;
	return 0;
}

/*
 * Parse shared library from user input
 * Currently only handles volume and src comp
 * This function takes in the libraries to be used as an input in the format:
 * "vol=libsof_volume.so,src=libsof_src.so,..."
 * The function parses the above string to identify the following:
 * component type and the library name and sets up the library handle
 * for the component and stores it in the shared library table
 */
static int parse_libraries(char *libs)
{
	char *lib_token = NULL;
	char *comp_token = NULL;
	char *token = strtok_r(libs, ",", &lib_token);
	int index;

	while (token) {

		/* get component type */
		char *token1 = strtok_r(token, "=", &comp_token);

		/* get shared library index from library table */
		index = get_index_by_name(token1, lib_table);

		if (index < 0) {
			fprintf(stderr, "error: unsupported comp type\n");
			return -EINVAL;
		}

		/* get shared library name */
		token1 = strtok_r(NULL, "=", &comp_token);
		if (!token1)
			break;

		/* set to new name that may be used while loading */
		strncpy(lib_table[index].library_name, token1,
			MAX_LIB_NAME_LEN - 1);

		/* next library */
		token = strtok_r(NULL, ",", &lib_token);
	}
	return 0;
}

/* print usage for testbench */
static void print_usage(char *executable)
{
	printf("Usage: %s -i <input_file> ", executable);
	printf("-o <output_file1,output_file2,...> ");
	printf("-t <tplg_file> -b <input_format> -c <channels>");
	printf("-a <comp1=comp1_library,comp2=comp2_library>\n");
	printf("input_format should be S16_LE, S32_LE, S24_LE or FLOAT_LE\n");
	printf("Example Usage:\n");
	printf("%s -i in.txt -o out.txt -t test.tplg ", executable);
	printf("-r 48000 -R 96000 -c 2");
	printf("-b S16_LE -a vol=libsof_volume.so\n");
}

/* free components */
static void free_comps(void)
{
	struct list_item *clist;
	struct list_item *temp;
	struct ipc_comp_dev *icd = NULL;

	list_for_item_safe(clist, temp, &sof_get()->ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		switch (icd->type) {
		case COMP_TYPE_COMPONENT:
			comp_free(icd->cd);
			list_item_del(&icd->list);
			rfree(icd);
			break;
		case COMP_TYPE_BUFFER:
			rfree(icd->cb->stream.addr);
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

static void parse_input_args(int argc, char **argv, struct testbench_prm *tp)
{
	int option = 0;
	int ret = 0;

	while ((option = getopt(argc, argv, "hdi:o:t:b:a:r:R:c:")) != -1) {
		switch (option) {
		/* input sample file */
		case 'i':
			tp->input_file = strdup(optarg);
			break;

		/* output sample files */
		case 'o':
			ret = parse_output_files(optarg, tp);
			break;

		/* topology file */
		case 't':
			tp->tplg_file = strdup(optarg);
			break;

		/* input samples bit format */
		case 'b':
			tp->bits_in = strdup(optarg);
			tp->frame_fmt = find_format(tp->bits_in);
			break;

		/* override default libraries */
		case 'a':
			ret = parse_libraries(optarg);
			break;

		/* input sample rate */
		case 'r':
			tp->fs_in = atoi(optarg);
			break;

		/* output sample rate */
		case 'R':
			tp->fs_out = atoi(optarg);
			break;

		/* input/output channels */
		case 'c':
			tp->channels = atoi(optarg);
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

		if (ret < 0)
			exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	struct testbench_prm tp;
	struct ipc_comp_dev *pcm_dev;
	struct pipeline *p;
	struct pipeline *curr_p;
	struct sof_ipc_pipe_new *ipc_pipe;
	struct comp_dev *cd;
	struct file_comp_data *frcd, *fwcd;
	char pipeline[DEBUG_MSG_LEN];
	clock_t tic, toc;
	double c_realtime, t_exec;
	int n_in, n_out, ret;
	int i;

	/* initialize input and output sample rates, files, etc. */
	tp.fs_in = 0;
	tp.fs_out = 0;
	tp.bits_in = 0;
	tp.input_file = NULL;
	tp.tplg_file = NULL;
	for (i = 0; i < MAX_OUTPUT_FILE_NUM; i++)
		tp.output_file[i] = NULL;
	tp.output_file_num = 0;
	tp.channels = TESTBENCH_NCH;
	tp.max_pipeline_id = 0;

	/* command line arguments*/
	parse_input_args(argc, argv, &tp);

	/* check args */
	if (!tp.tplg_file || !tp.input_file || !tp.output_file_num ||
	    !tp.bits_in) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* initialize ipc and scheduler */
	if (tb_pipeline_setup(sof_get()) < 0) {
		fprintf(stderr, "error: pipeline init\n");
		exit(EXIT_FAILURE);
	}

	/* parse topology file and create pipeline */
	if (parse_topology(sof_get(), &tp, pipeline) < 0) {
		fprintf(stderr, "error: parsing topology\n");
		exit(EXIT_FAILURE);
	}

	/* Get pointer to filewrite */
	pcm_dev = ipc_get_comp_by_id(sof_get()->ipc, tp.fw_id);
	if (!pcm_dev) {
		fprintf(stderr, "error: failed to get pointers to filewrite\n");
		exit(EXIT_FAILURE);
	}
	fwcd = comp_get_drvdata(pcm_dev->cd);

	/* Get pointer to fileread */
	pcm_dev = ipc_get_comp_by_id(sof_get()->ipc, tp.fr_id);
	if (!pcm_dev) {
		fprintf(stderr, "error: failed to get pointers to fileread\n");
		exit(EXIT_FAILURE);
	}
	frcd = comp_get_drvdata(pcm_dev->cd);

	/* Run pipeline until EOF from fileread */
	pcm_dev = ipc_get_comp_by_id(sof_get()->ipc, tp.sched_id);
	p = pcm_dev->cd->pipeline;
	ipc_pipe = &p->ipc_pipe;

	/* input and output sample rate */
	if (!tp.fs_in)
		tp.fs_in = ipc_pipe->period * ipc_pipe->frames_per_sched;

	if (!tp.fs_out)
		tp.fs_out = ipc_pipe->period * ipc_pipe->frames_per_sched;

	/* set pipeline params and trigger start */
	if (tb_pipeline_start(sof_get()->ipc, ipc_pipe, &tp) < 0) {
		fprintf(stderr, "error: pipeline params\n");
		exit(EXIT_FAILURE);
	}

	cd = pcm_dev->cd;
	tb_enable_trace(false); /* reduce trace output */
	tic = clock();

	while (frcd->fs.reached_eof == 0) {
		/*
		 * Schedule copy for all pipelines which have the same schedule
		 * component as the working one.
		 *
		 * In common convention pipelines are added with monotonic
		 * increasing IDs started from 1, we could take care of it in
		 * test topologies so this for-loop will walk all pipelines.
		 */
		for (i = 1; i <= tp.max_pipeline_id; i++) {
			pcm_dev = ipc_get_comp_by_ppl_id(sof_get()->ipc,
							 COMP_TYPE_PIPELINE, i);
			if (pcm_dev) {
				curr_p = pcm_dev->pipeline;
				if (pipeline_is_same_sched_comp(p, curr_p))
					pipeline_schedule_copy(curr_p, 0);
			}
		}
	}

	if (!frcd->fs.reached_eof)
		printf("warning: possible pipeline xrun\n");

	/* reset and free pipeline */
	toc = clock();
	tb_enable_trace(true);
	pipeline_trigger(p, cd, COMP_TRIGGER_STOP);
	ret = pipeline_reset(p, cd);
	if (ret < 0) {
		fprintf(stderr, "error: pipeline reset\n");
		exit(EXIT_FAILURE);
	}

	n_in = frcd->fs.n;
	n_out = fwcd->fs.n;
	t_exec = (double)(toc - tic) / CLOCKS_PER_SEC;
	c_realtime = (double)n_out / tp.channels / tp.fs_out / t_exec;

	/* free all components/buffers in pipeline */
	free_comps();

	/* print test summary */
	printf("==========================================================\n");
	printf("		           Test Summary\n");
	printf("==========================================================\n");
	printf("Test Pipeline:\n");
	printf("%s\n", pipeline);
	printf("Input bit format: %s\n", tp.bits_in);
	printf("Input sample rate: %d\n", tp.fs_in);
	printf("Output sample rate: %d\n", tp.fs_out);
	for (i = 0; i < tp.output_file_num; i++) {
		printf("Output[%d] written to file: \"%s\"\n",
		       i, tp.output_file[i]);
	}
	printf("Input sample count: %d\n", n_in);
	printf("Output sample count: %d\n", n_out);
	printf("Total execution time: %.2f us, %.2f x realtime\n",
	       1e3 * t_exec, c_realtime);

	/* free all other data */
	free(tp.bits_in);
	free(tp.input_file);
	free(tp.tplg_file);
	for (i = 0; i < tp.output_file_num; i++)
		free(tp.output_file[i]);

	/* close shared library objects */
	for (i = 0; i < NUM_WIDGETS_SUPPORTED; i++) {
		if (lib_table[i].handle)
			dlclose(lib_table[i].handle);
	}

	return EXIT_SUCCESS;
}
