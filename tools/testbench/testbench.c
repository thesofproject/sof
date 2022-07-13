// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <pthread.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <sof/list.h>
#include <getopt.h>
#include <dlfcn.h>
#include "testbench/common_test.h"
#include <tplg_parser/topology.h>
#include "testbench/trace.h"
#include "testbench/file.h"
#include <limits.h>
#include <stdlib.h>

#ifdef TESTBENCH_CACHE_CHECK
#include <arch/lib/cache.h>
struct tb_cache_context hc = {0};

/* cache debugger */
struct tb_cache_context *tb_cache = &hc;
int tb_elem_id;
#else

/* host thread context - folded into cachec context when cache debug is enabled */
struct tb_host_context {
	pthread_t thread_id[CONFIG_CORE_COUNT];
};

struct tb_host_context hc = {0};
#endif

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

DECLARE_SOF_TB_UUID("mux", mux_uuid, 0xc607ff4d, 0x9cb6, 0x49dc,
		    0xb6, 0x78, 0x7d, 0xa3, 0xc6, 0x3e, 0xa5, 0x57);

DECLARE_SOF_TB_UUID("demux", demux_uuid, 0xc4b26868, 0x1430, 0x470e,
		    0xa0, 0x89, 0x15, 0xd1, 0xc7, 0x7f, 0x85, 0x1a);

DECLARE_SOF_TB_UUID("google-rtc-audio-processing", google_rtc_audio_processing_uuid,
		    0xb780a0a6, 0x269f, 0x466f, 0xb4, 0x77, 0x23, 0xdf, 0xa0,
		    0x5a, 0xf7, 0x58);

#define TESTBENCH_NCH 2 /* Stereo */

struct pipeline_thread_data {
	struct testbench_prm *tp;
	int count;			/* copy iteration count */
	int core_id;
};

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
	{"multiband_drc", "libsof_multiband_drc.so", SOF_COMP_NONE,
		SOF_TB_UUID(multiband_drc_uuid), 0, NULL},
	{"mixer", "libsof_mixer.so", SOF_COMP_MIXER, NULL, 0, NULL},
	{"mux", "libsof_mux.so", SOF_COMP_MUX, SOF_TB_UUID(mux_uuid), 0, NULL},
	{"demux", "libsof_mux.so", SOF_COMP_DEMUX, SOF_TB_UUID(demux_uuid), 0, NULL},
	{"google-rtc-audio-processing", "libsof_google-rtc-audio-processing.so", SOF_COMP_NONE,
		SOF_TB_UUID(google_rtc_audio_processing_uuid), 0, NULL},
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
 * Parse inputfilenames from user input
 */
static int parse_input_files(char *inputs, struct testbench_prm *tp)
{
	char *input_token = NULL;
	char *token = strtok_r(inputs, ",", &input_token);
	int index;

	for (index = 0; index < MAX_INPUT_FILE_NUM && token; index++) {
		/* get input file name with current index */
		tp->input_file[index] = strdup(token);

		/* next input */
		token = strtok_r(NULL, ",", &input_token);
	}

	if (index == MAX_INPUT_FILE_NUM && token) {
		fprintf(stderr, "error: max input file number is %d\n",
			MAX_INPUT_FILE_NUM);
		for (index = 0; index < MAX_INPUT_FILE_NUM; index++)
			free(tp->input_file[index]);
		return -EINVAL;
	}

	/* set total input file number */
	tp->input_file_num = index;
	return 0;
}

static int parse_pipelines(char *pipelines, struct testbench_prm *tp)
{
	char *output_token = NULL;
	char *token = strtok_r(pipelines, ",", &output_token);
	int index;

	for (index = 0; index < MAX_OUTPUT_FILE_NUM && token; index++) {
		/* get output file name with current index */
		tp->pipelines[index] = atoi(token);

		/* next output */
		token = strtok_r(NULL, ",", &output_token);
	}

	if (index == MAX_OUTPUT_FILE_NUM && token) {
		fprintf(stderr, "error: max output file number is %d\n",
			MAX_OUTPUT_FILE_NUM);
		return -EINVAL;
	}

	/* set total output file number */
	tp->pipeline_num = index;
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
			fprintf(stderr, "error: unsupported comp type %s\n", token1);
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
	printf("Usage: %s <options> -i <input_file> ", executable);
	printf("-o <output_file1,output_file2,...>\n\n");
	printf("Options for processing:\n");
	printf("  -t <topology file>\n");
	printf("  -a <comp1=comp1_library,comp2=comp2_library>, override default library\n\n");
	printf("Options to control test:\n");
	printf("  -d Run in debug mode\n");
	printf("  -q Run in quiet mode, suppress traces output\n");
	printf("  -p <pipeline1,pipeline2,...>\n");
	printf("  -s Use real time priorities for threads (needs sudo)\n");
	printf("  -C <number of copy() iterations>\n");
	printf("  -D <pipeline duration in ms>\n");
	printf("  -P <number of dynamic pipeline iterations>\n");
	printf("  -T <microseconds for tick, 0 for batch mode>\n");
	printf("  -V <number of virtual cores>\n\n");
	printf("Options for input and output format override:\n");
	printf("  -b <input_format>, S16_LE, S24_LE, or S32_LE\n");
	printf("  -c <input channels>\n");
	printf("  -n <output channels>\n");
	printf("  -r <input rate>\n");
	printf("  -R <output rate>\n\n");
	printf("Environment variables\n");
	printf("  SOF_HOST_CORE0=<i> - Map DSP core 0..N to host i..i+N\n");
	printf("Help:\n");
	printf("  -h\n\n");
	printf("Example Usage:\n");
	printf("%s -i in.txt -o out.txt -t test.tplg ", executable);
	printf("-r 48000 -R 96000 -c 2 ");
	printf("-b S16_LE -a volume=libsof_volume.so\n");
}

/* free components */
static void test_pipeline_free_comps(int pipeline_id)
{
	struct list_item *clist;
	struct list_item *temp;
	struct ipc_comp_dev *icd = NULL;
	int err;

	/* remove the components for this pipeline */
	list_for_item_safe(clist, temp, &sof_get()->ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);

		switch (icd->type) {
		case COMP_TYPE_COMPONENT:
			if (icd->cd->pipeline->pipeline_id != pipeline_id)
				break;
			err = ipc_comp_free(sof_get()->ipc, icd->id);
			if (err)
				fprintf(stderr, "failed to free comp %d\n",
					icd->id);
			break;
		case COMP_TYPE_BUFFER:
			if (icd->cb->pipeline_id != pipeline_id)
				break;
			err = ipc_buffer_free(sof_get()->ipc, icd->id);
			if (err)
				fprintf(stderr, "failed to free buffer %d\n",
					icd->id);
			break;
		default:
			if (icd->pipeline->pipeline_id != pipeline_id)
				break;
			err = ipc_pipeline_free(sof_get()->ipc, icd->id);
			if (err)
				fprintf(stderr, "failed to free pipeline %d\n",
					icd->id);
			break;
		}
	}
}

static void test_pipeline_set_test_limits(int pipeline_id, int max_copies,
					  int max_samples)
{
	struct list_item *clist;
	struct list_item *temp;
	struct ipc_comp_dev *icd = NULL;
	struct comp_dev *cd;
	struct dai_data *dd;
	struct file_comp_data *fcd;

	/* set the test limits for this pipeline */
	list_for_item_safe(clist, temp, &sof_get()->ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);

		switch (icd->type) {
		case COMP_TYPE_COMPONENT:
			cd = icd->cd;
			if (cd->pipeline->pipeline_id != pipeline_id)
				break;

			switch (cd->drv->type) {
			case SOF_COMP_HOST:
			case SOF_COMP_DAI:
			case SOF_COMP_FILEREAD:
			case SOF_COMP_FILEWRITE:
				/* only file limits supported today. TODO: add others */
				dd = comp_get_drvdata(cd);
				fcd = comp_get_drvdata(dd->dai);
				fcd->max_samples = max_samples;
				fcd->max_copies = max_copies;
				break;
			default:
				break;
			}
			break;
		case COMP_TYPE_BUFFER:
		default:
			break;
		}
	}
}

static void test_pipeline_get_file_stats(int pipeline_id)
{
	struct list_item *clist;
	struct list_item *temp;
	struct ipc_comp_dev *icd;
	struct comp_dev *cd;
	struct dai_data *dd;
	struct file_comp_data *fcd;
	unsigned long time;

	/* get the file IO status for each file in pipeline */
	list_for_item_safe(clist, temp, &sof_get()->ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);

		switch (icd->type) {
		case COMP_TYPE_COMPONENT:
			cd = icd->cd;
			if (cd->pipeline->pipeline_id != pipeline_id)
				break;
			switch (cd->drv->type) {
			case SOF_COMP_HOST:
			case SOF_COMP_DAI:
			case SOF_COMP_FILEREAD:
			case SOF_COMP_FILEWRITE:
				dd = comp_get_drvdata(cd);
				fcd = comp_get_drvdata(dd->dai);

				time = cd->pipeline->pipe_task->start;
				if (fcd->fs.copy_count == 0)
					fcd->fs.copy_count = 1;
				printf("file %s: id %d: type %d: samples %d copies %d total time %zu uS avg time %zu uS\n",
				       fcd->fs.fn, cd->ipc_config.id, cd->drv->type, fcd->fs.n,
				       fcd->fs.copy_count, time, time / fcd->fs.copy_count);
				break;
			default:
				break;
			}
			break;
		case COMP_TYPE_BUFFER:
		default:
			break;
		}
	}
}

static int parse_input_args(int argc, char **argv, struct testbench_prm *tp)
{
	int option = 0;
	int ret = 0;

	while ((option = getopt(argc, argv, "hdqi:o:t:b:a:r:R:c:n:C:P:Vp:T:D:")) != -1) {
		switch (option) {
		/* input sample file */
		case 'i':
			ret = parse_input_files(optarg, tp);
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
			tp->cmd_frame_fmt = find_format(tp->bits_in);
			break;

		/* override default libraries */
		case 'a':
			ret = parse_libraries(optarg);
			break;

		/* input sample rate */
		case 'r':
			tp->cmd_fs_in = atoi(optarg);
			break;

		/* output sample rate */
		case 'R':
			tp->cmd_fs_out = atoi(optarg);
			break;

		/* input/output channels */
		case 'c':
			tp->cmd_channels_in = atoi(optarg);
			break;

		/* output channels */
		case 'n':
			tp->cmd_channels_out = atoi(optarg);
			break;

			/* enable debug prints */
		case 'd':
			debug = 1;
			break;

		/* number of pipeline copy() iterations */
		case 'C':
			tp->copy_iterations = atoi(optarg);
			tp->copy_check = true;
			break;

		case 'q':
			tp->quiet = true;
			break;

		/* number of dynamic pipeline iterations */
		case 'P':
			tp->dynamic_pipeline_iterations = atoi(optarg);
			break;

		/* number of virtual cores */
		case 'V':
			tp->num_vcores = atoi(optarg);
			break;

		/* output sample files */
		case 'p':
			ret = parse_pipelines(optarg, tp);
			break;

		/* Microseconds for tick, 0 = batch (tickless) */
		case 'T':
			tp->tick_period_us = atoi(optarg);
			break;

		/* pipeline duration in millisec, 0 = realtime (tickless) */
		case 'D':
			tp->pipeline_duration_ms = atoi(optarg);
			break;

		/* print usage */
		default:
			fprintf(stderr, "unknown option %c\n", option);
			ret = -EINVAL;
			__attribute__ ((fallthrough));
		case 'h':
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
		}

		if (ret < 0)
			return ret;
	}

	return ret;
}

static struct pipeline *get_pipeline_by_id(int id)
{
	struct ipc_comp_dev *pcm_dev;
	struct ipc *ipc = sof_get()->ipc;

	pcm_dev = ipc_get_ppl_src_comp(ipc, id);
	return pcm_dev->cd->pipeline;
}

static int test_pipeline_stop(struct pipeline_thread_data *ptdata)
{
	struct testbench_prm *tp = ptdata->tp;
	struct pipeline *p;
	struct ipc *ipc = sof_get()->ipc;
	int ret = 0;
	int i;

	for (i = 0; i < tp->pipeline_num; i++) {
		p = get_pipeline_by_id(tp->pipelines[i]);
		ret = tb_pipeline_stop(ipc, p);
		if (ret < 0)
			break;
	}

	return ret;
}

static int test_pipeline_reset(struct pipeline_thread_data *ptdata)
{
	struct testbench_prm *tp = ptdata->tp;
	struct pipeline *p;
	struct ipc *ipc = sof_get()->ipc;
	int ret = 0;
	int i;

	for (i = 0; i < tp->pipeline_num; i++) {
		p = get_pipeline_by_id(tp->pipelines[i]);
		ret = tb_pipeline_reset(ipc, p);
		if (ret < 0)
			break;
	}

	return ret;
}

static void test_pipeline_free(struct pipeline_thread_data *ptdata)
{
	struct testbench_prm *tp = ptdata->tp;
	int i;

	for (i = 0; i < tp->pipeline_num; i++)
		test_pipeline_free_comps(tp->pipelines[i]);
}

static int test_pipeline_params(struct pipeline_thread_data *ptdata, struct tplg_context *ctx)
{
	struct testbench_prm *tp = ptdata->tp;
	struct ipc_comp_dev *pcm_dev;
	struct pipeline *p;
	struct ipc *ipc = sof_get()->ipc;
	int ret = 0;
	int i;

	/* Run pipeline until EOF from fileread */

	for (i = 0; i < tp->pipeline_num; i++) {
		pcm_dev = ipc_get_ppl_src_comp(ipc, tp->pipelines[i]);
		if (!pcm_dev) {
			fprintf(stderr, "error: pipeline %d has no source component\n",
				tp->pipelines[i]);
			return -EINVAL;
		}

		/* set up pipeline params */
		p = pcm_dev->cd->pipeline;

		/* input and output sample rate */
		if (!ctx->fs_in)
			ctx->fs_in = p->period * p->frames_per_sched;

		if (!ctx->fs_out)
			ctx->fs_out = p->period * p->frames_per_sched;

		ret = tb_pipeline_params(ipc, p, ctx);
		if (ret < 0) {
			fprintf(stderr, "error: pipeline params failed: %s\n",
				strerror(ret));
			return ret;
		}
	}


	return 0;
}

static int test_pipeline_start(struct pipeline_thread_data *ptdata)
{
	struct testbench_prm *tp = ptdata->tp;
	struct pipeline *p;
	struct ipc *ipc = sof_get()->ipc;
	int i;

	/* Run pipeline until EOF from fileread */
	for (i = 0; i < tp->pipeline_num; i++) {
		p = get_pipeline_by_id(tp->pipelines[i]);

		/* do we need to apply copy count limit ? */
		if (tp->copy_check)
			test_pipeline_set_test_limits(tp->pipelines[i], tp->copy_iterations, 0);

		/* set pipeline params and trigger start */
		if (tb_pipeline_start(ipc, p) < 0) {
			fprintf(stderr, "error: pipeline params\n");
			return -EINVAL;
		}
	}

	return 0;
}

static bool test_pipeline_check_state(struct pipeline_thread_data *ptdata, int state)
{
	struct testbench_prm *tp = ptdata->tp;
	struct pipeline *p;
	int i;

	/* Run pipeline until EOF from fileread */
	for (i = 0; i < tp->pipeline_num; i++) {
		p = get_pipeline_by_id(tp->pipelines[i]);
		if (p->pipe_task->state	== state)
			return true;
	}

	return false;
}

static int test_pipeline_load(struct pipeline_thread_data *ptdata, struct tplg_context *ctx)
{
	struct testbench_prm *tp = ptdata->tp;
	int ret;

	/* setup the thread virtual core config */
	memset(ctx, 0, sizeof(*ctx));
	ctx->comp_id = 1000 * ptdata->core_id;
	ctx->core_id = ptdata->core_id;
	ctx->file = tp->file;
	ctx->sof = sof_get();
	ctx->tp = tp;
	ctx->tplg_file = tp->tplg_file;
	ctx->fs_in = tp->cmd_fs_in;
	ctx->fs_out = tp->cmd_fs_out;
	ctx->channels_in = tp->cmd_channels_in;
	ctx->channels_out = tp->cmd_channels_out;
	ctx->frame_fmt = tp->cmd_frame_fmt;

	/* parse topology file and create pipeline */
	ret = parse_topology(ctx);
	if (ret < 0)
		fprintf(stderr, "error: parsing topology\n");

	return ret;
}

static void test_pipeline_stats(struct pipeline_thread_data *ptdata,
				struct tplg_context *ctx, uint64_t delta)
{
	struct testbench_prm *tp = ptdata->tp;
	int count = ptdata->count;
	struct ipc_comp_dev *icd;
	struct comp_dev *cd;
	struct dai_data *dd;
	struct pipeline *p;
	struct file_comp_data *frcd, *fwcd;
	int n_in, n_out;
	int i;

	/* Get pointer to filewrite */
	icd = ipc_get_comp_by_id(sof_get()->ipc, tp->fw_id);
	if (!icd) {
		fprintf(stderr, "error: failed to get pointers to filewrite\n");
		exit(EXIT_FAILURE);
	}
	cd = icd->cd;
	dd = comp_get_drvdata(cd);
	fwcd = comp_get_drvdata(dd->dai);

	/* Get pointer to fileread */
	icd = ipc_get_comp_by_id(sof_get()->ipc, tp->fr_id);
	if (!icd) {
		fprintf(stderr, "error: failed to get pointers to fileread\n");
		exit(EXIT_FAILURE);
	}
	cd = icd->cd;
	dd = comp_get_drvdata(cd);
	frcd = comp_get_drvdata(dd->dai);

	/* Run pipeline until EOF from fileread */
	icd = ipc_get_comp_by_id(sof_get()->ipc, ctx->sched_id);
	p = icd->cd->pipeline;

	/* input and output sample rate */
	if (!ctx->fs_in)
		ctx->fs_in = p->period * p->frames_per_sched;

	if (!ctx->fs_out)
		ctx->fs_out = p->period * p->frames_per_sched;

	n_in = frcd->fs.n;
	n_out = fwcd->fs.n;

	/* print test summary */
	printf("==========================================================\n");
	printf("		           Test Summary %d\n", count);
	printf("==========================================================\n");
	printf("Test Pipeline:\n");
	printf("%s\n", tp->pipeline_string);
	test_pipeline_get_file_stats(ctx->pipeline_id);

	printf("Input bit format: %s\n", tp->bits_in);
	printf("Input sample rate: %d\n", ctx->fs_in);
	printf("Output sample rate: %d\n", ctx->fs_out);
	for (i = 0; i < tp->input_file_num; i++) {
		printf("Input[%d] read from file: \"%s\"\n",
		       i, tp->input_file[i]);
	}
	for (i = 0; i < tp->output_file_num; i++) {
		printf("Output[%d] written to file: \"%s\"\n",
		       i, tp->output_file[i]);
	}
	printf("Input sample (frame) count: %d (%d)\n", n_in, n_in / ctx->channels_in);
	printf("Output sample (frame) count: %d (%d)\n", n_out, n_out / ctx->channels_out);
	printf("Total execution time: %zu us, %.2f x realtime\n\n",
	       delta, (double)((double)n_out / ctx->channels_out / ctx->fs_out) * 1000000 / delta);
}

/*
 * Tester thread, one for each virtual core. This is NOT the thread that will
 * execute the virtual core.
 */
static void *pipline_test(void *data)
{
	struct pipeline_thread_data *ptdata = data;
	struct testbench_prm *tp = ptdata->tp;
	int dp_count = 0;
	struct tplg_context ctx;
	struct timespec ts;
	struct timespec td0, td1;
	int err;
	int nsleep_time;
	int nsleep_limit;
	uint64_t delta;

	/* build, run and teardown pipelines */
	while (dp_count < tp->dynamic_pipeline_iterations) {
		fprintf(stdout, "pipeline run %d/%d\n", dp_count,
			tp->dynamic_pipeline_iterations);

		/* print test summary */
		printf("==========================================================\n");
		printf("		           Test Start %d\n", dp_count);
		printf("==========================================================\n");

		err = test_pipeline_load(ptdata, &ctx);
		if (err < 0) {
			fprintf(stderr, "error: pipeline load %d failed %d\n",
				dp_count, err);
			break;
		}

		err = test_pipeline_params(ptdata, &ctx);
		if (err < 0) {
			fprintf(stderr, "error: pipeline params %d failed %d\n",
				dp_count, err);
			break;
		}

		err = test_pipeline_start(ptdata);
		if (err < 0) {
			fprintf(stderr, "error: pipeline run %d failed %d\n",
				dp_count, err);
			break;
		}
		clock_gettime(CLOCK_MONOTONIC, &td0);

		/* sleep to let the pipeline work - we exit at timeout OR
		 * if copy iterations OR max_samples is reached (whatever first)
		 */
		nsleep_time = 0;
		ts.tv_sec = tp->tick_period_us / 1000000;
		ts.tv_nsec = (tp->tick_period_us % 1000000) * 1000;
		if (!tp->copy_check)
			nsleep_limit = INT_MAX;
		else
			nsleep_limit = tp->copy_iterations *
				       tp->pipeline_duration_ms;

		while (nsleep_time < nsleep_limit) {
			/* wait for next tick */
			err = nanosleep(&ts, &ts);
			if (err == 0) {
				nsleep_time += tp->tick_period_us; /* sleep fully completed */
				if (test_pipeline_check_state(ptdata, SOF_TASK_STATE_CANCEL)) {
					fprintf(stdout, "pipeline cancelled !\n");
					break;
				}
			} else {
				if (err == EINTR) {
					continue; /* interrupted - keep going */
				} else {
					printf("error: sleep failed: %s\n", strerror(err));
					break;
				}
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &td1);
		err = test_pipeline_stop(ptdata);
		if (err < 0) {
			fprintf(stderr, "error: pipeline stop %d failed %d\n",
				dp_count, err);
			break;
		}

		delta = (td1.tv_sec - td0.tv_sec) * 1000000;
		delta += (td1.tv_nsec - td0.tv_nsec) / 1000;
		test_pipeline_stats(ptdata, &ctx, delta);

		err = test_pipeline_reset(ptdata);
		if (err < 0) {
			fprintf(stderr, "error: pipeline stop %d failed %d\n",
				dp_count, err);
			break;
		}

		test_pipeline_free(ptdata);

		ptdata->count++;
		dp_count++;
	}

	return NULL;
}

static struct testbench_prm tp;

int main(int argc, char **argv)
{
	struct pipeline_thread_data ptdata[CONFIG_CORE_COUNT];
	int i, err;

	/* initialize input and output sample rates, files, etc. */
	debug = 0;
	tp.cmd_fs_in = 0;
	tp.cmd_fs_out = 0;
	tp.bits_in = 0;
	tp.tplg_file = NULL;
	tp.input_file_num = 0;
	tp.output_file_num = 0;
	for (i = 0; i < MAX_OUTPUT_FILE_NUM; i++)
		tp.output_file[i] = NULL;

	for (i = 0; i < MAX_INPUT_FILE_NUM; i++)
		tp.input_file[i] = NULL;

	tp.cmd_channels_in = TESTBENCH_NCH;
	tp.cmd_channels_out = 0;
	tp.max_pipeline_id = 0;
	tp.copy_check = false;
	tp.quiet = 0;
	tp.dynamic_pipeline_iterations = 1;
	tp.num_vcores = 0;
	tp.pipeline_string = calloc(1, DEBUG_MSG_LEN);
	tp.pipelines[0] = 1;
	tp.pipeline_num = 1;
	tp.tick_period_us = 0; /* Execute fast non-real time, for 1 ms tick use -T 1000 */
	tp.pipeline_duration_ms = 5000;
	tp.copy_iterations = 1;

	/* command line arguments*/
	err = parse_input_args(argc, argv, &tp);
	if (err < 0)
		goto out;

	if (!tp.cmd_channels_out)
		tp.cmd_channels_out = tp.cmd_channels_in;

	/* check mandatory args */
	if (!tp.tplg_file) {
		fprintf(stderr, "topology file not specified, use -t file.tplg\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (!tp.input_file_num) {
		fprintf(stderr, "input files not specified, use -i file1,file2\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (!tp.output_file_num) {
		fprintf(stderr, "output files not specified, use -o file1,file2\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (!tp.bits_in) {
		fprintf(stderr, "input format not specified, use -b format\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (tp.num_vcores > CONFIG_CORE_COUNT) {
		fprintf(stderr, "virtual core count %d is greater than max %d\n",
			tp.num_vcores, CONFIG_CORE_COUNT);
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	} else {
		if (!tp.num_vcores)
			tp.num_vcores = 1;
	}

	if (tp.quiet)
		tb_enable_trace(false); /* reduce trace output */
	else
		tb_enable_trace(true);

	/* initialize ipc and scheduler */
	if (tb_setup(sof_get(), &tp) < 0) {
		fprintf(stderr, "error: pipeline init\n");
		exit(EXIT_FAILURE);
	}

	/* build, run and teardown pipelines */
	for (i = 0; i < tp.num_vcores; i++) {
		ptdata[i].core_id = i;
		ptdata[i].tp = &tp;
		ptdata[i].count = 0;

		err = pthread_create(&hc.thread_id[i], NULL,
				     pipline_test, &ptdata[i]);
		if (err) {
			printf("error: can't create thread %d %s\n", err, strerror(err));
			goto join;
		}
	}

join:
	for (i = 0; i < tp.num_vcores; i++)
		err = pthread_join(hc.thread_id[i], NULL);

	/* free other core FW services */
	tb_free(sof_get());

out:
	/* free all other data */
	free(tp.bits_in);
	free(tp.tplg_file);
	for (i = 0; i < tp.output_file_num; i++)
		free(tp.output_file[i]);

	for (i = 0; i < tp.input_file_num; i++)
		free(tp.input_file[i]);

	free(tp.pipeline_string);

#ifdef TESTBENCH_CACHE_CHECK
	_cache_free_all();
#endif

	/* close shared library objects */
	for (i = 0; i < NUM_WIDGETS_SUPPORTED; i++) {
		if (lib_table[i].handle)
			dlclose(lib_table[i].handle);
	}

	return EXIT_SUCCESS;
}
