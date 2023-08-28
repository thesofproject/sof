// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <platform/lib/ll_schedule.h>
#include <sof/list.h>
#include <getopt.h>
#include "testbench/common_test.h"
#include <tplg_parser/topology.h>
#include "testbench/trace.h"
#include "testbench/file.h"
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>

#define TESTBENCH_NCH	2

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
				printf("file %s: id %d: type %d: samples %d copies %d total time %lu uS avg time %lu uS\n",
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
			tp->frame_fmt = tplg_find_format(tp->bits_in);
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
			tp->channels_in = atoi(optarg);
			break;

		/* output channels */
		case 'n':
			tp->channels_out = atoi(optarg);
			break;

		/* set debug log level */
		case 'd':
			host_trace_level = atoi(optarg);
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
			/* fallthrough */
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

static int test_pipeline_stop(struct testbench_prm *tp)
{
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

static int test_pipeline_reset(struct testbench_prm *tp)
{
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

static void test_pipeline_free(struct testbench_prm *tp)
{
	int i;

	for (i = 0; i < tp->pipeline_num; i++)
		test_pipeline_free_comps(tp->pipelines[i]);
}

static int test_pipeline_params(struct testbench_prm *tp, struct tplg_context *ctx)
{
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
		if (!tp->fs_in)
			tp->fs_in = p->period * p->frames_per_sched;

		if (!tp->fs_out)
			tp->fs_out = p->period * p->frames_per_sched;

		ret = tb_pipeline_params(tp, ipc, p, ctx);
		if (ret < 0) {
			fprintf(stderr, "error: pipeline params failed: %s\n",
				strerror(ret));
			return ret;
		}
	}


	return 0;
}

static int test_pipeline_start(struct testbench_prm *tp)
{
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

static bool test_pipeline_check_state(struct testbench_prm *tp, int state)
{
	struct pipeline *p;
	uint64_t cycles0, cycles1;
	int i;

	tb_getcycles(&cycles0);

	schedule_ll_run_tasks();

	tb_getcycles(&cycles1);
	tp->total_cycles += cycles1 - cycles0;

	/* Run pipeline until EOF from fileread */
	for (i = 0; i < tp->pipeline_num; i++) {
		p = get_pipeline_by_id(tp->pipelines[i]);
		if (p->pipe_task->state	== state)
			return true;
	}

	return false;
}

static int test_pipeline_load(struct testbench_prm *tp, struct tplg_context *ctx)
{
	int ret;

	/* setup the thread virtual core config */
	memset(ctx, 0, sizeof(*ctx));
	ctx->comp_id = 1;
	ctx->core_id = 0;
	ctx->sof = sof_get();
	ctx->tplg_file = tp->tplg_file;
	ctx->ipc_major = 3;

	/* parse topology file and create pipeline */
	ret = tb_parse_topology(tp, ctx);
	if (ret < 0)
		fprintf(stderr, "error: parsing topology\n");

	return ret;
}

static void test_pipeline_stats(struct testbench_prm *tp,
				struct tplg_context *ctx, long long delta_t)
{
	int count = 1;
	struct ipc_comp_dev *icd;
	struct comp_dev *cd;
	struct dai_data *dd;
	struct pipeline *p;
	struct file_comp_data *frcd, *fwcd;
	long long file_cycles, pipeline_cycles;
	float pipeline_mcps;
	int n_in, n_out, frames_out;
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
	if (!tp->fs_in)
		tp->fs_in = p->period * p->frames_per_sched;

	if (!tp->fs_out)
		tp->fs_out = p->period * p->frames_per_sched;

	n_in = frcd->fs.n;
	n_out = fwcd->fs.n;
	file_cycles = frcd->fs.cycles_count + fwcd->fs.cycles_count;

	/* print test summary */
	printf("==========================================================\n");
	printf("		           Test Summary %d\n", count);
	printf("==========================================================\n");
	printf("Test Pipeline:\n");
	printf("%s\n", tp->pipeline_string);
	test_pipeline_get_file_stats(ctx->pipeline_id);

	printf("Input bit format: %s\n", tp->bits_in);
	printf("Input sample rate: %d\n", tp->fs_in);
	printf("Output sample rate: %d\n", tp->fs_out);
	for (i = 0; i < tp->input_file_num; i++) {
		printf("Input[%d] read from file: \"%s\"\n",
		       i, tp->input_file[i]);
	}
	for (i = 0; i < tp->output_file_num; i++) {
		printf("Output[%d] written to file: \"%s\"\n",
		       i, tp->output_file[i]);
	}
	frames_out = n_out / tp->channels_out;
	printf("Input sample (frame) count: %d (%d)\n", n_in, n_in / tp->channels_in);
	printf("Output sample (frame) count: %d (%d)\n", n_out, frames_out);
	if (tp->total_cycles) {
		pipeline_cycles = tp->total_cycles - file_cycles;
		pipeline_mcps = (float)pipeline_cycles * tp->fs_out / frames_out / 1e6;
		printf("Total execution cycles: %lld\n", tp->total_cycles);
		printf("File component cycles: %lld\n", file_cycles);
		printf("Pipeline cycles: %lld\n", pipeline_cycles);
		printf("Pipeline MCPS: %6.2f\n", pipeline_mcps);
		if (!tp->quiet)
			printf("Warning: Use -q to avoid printing to increase MCPS.\n");
	}

	if (delta_t)
		printf("Total execution time: %lld us, %.2f x realtime\n",
		       delta_t, (float)frames_out / tp->fs_out * 1000000 / delta_t);

	printf("\n");
}

/*
 * Tester thread, one for each virtual core. This is NOT the thread that will
 * execute the virtual core.
 */
static int pipline_test(struct testbench_prm *tp)
{
	int dp_count = 0;
	struct tplg_context ctx;
	struct timespec ts;
	struct timespec td0, td1;
	long long delta_t;
	int err;
	int nsleep_time;
	int nsleep_limit;

	/* build, run and teardown pipelines */
	while (dp_count < tp->dynamic_pipeline_iterations) {
		fprintf(stdout, "pipeline run %d/%d\n", dp_count,
			tp->dynamic_pipeline_iterations);

		/* print test summary */
		printf("==========================================================\n");
		printf("		           Test Start %d\n", dp_count);
		printf("==========================================================\n");

		err = test_pipeline_load(tp, &ctx);
		if (err < 0) {
			fprintf(stderr, "error: pipeline load %d failed %d\n",
				dp_count, err);
			break;
		}

		err = test_pipeline_params(tp, &ctx);
		if (err < 0) {
			fprintf(stderr, "error: pipeline params %d failed %d\n",
				dp_count, err);
			break;
		}

		err = test_pipeline_start(tp);
		if (err < 0) {
			fprintf(stderr, "error: pipeline run %d failed %d\n",
				dp_count, err);
			break;
		}

		tb_gettime(&td0);

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
#if defined __XCC__
			err = 0;
#else
			/* wait for next tick */
			err = nanosleep(&ts, &ts);
#endif
			if (err == 0) {
				nsleep_time += tp->tick_period_us; /* sleep fully completed */
				if (test_pipeline_check_state(tp, SOF_TASK_STATE_CANCEL)) {
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

		tb_gettime(&td1);

		err = test_pipeline_stop(tp);
		if (err < 0) {
			fprintf(stderr, "error: pipeline stop %d failed %d\n",
				dp_count, err);
			break;
		}

		delta_t = (td1.tv_sec - td0.tv_sec) * 1000000;
		delta_t += (td1.tv_nsec - td0.tv_nsec) / 1000;
		test_pipeline_stats(tp, &ctx, delta_t);

		err = test_pipeline_reset(tp);
		if (err < 0) {
			fprintf(stderr, "error: pipeline stop %d failed %d\n",
				dp_count, err);
			break;
		}

		test_pipeline_free(tp);

		dp_count++;
	}

	return 0;
}

static struct testbench_prm tp;

int main(int argc, char **argv)
{
	int i, err;

	/* initialize input and output sample rates, files, etc. */
	tp.total_cycles = 0;
	tp.fs_in = 0;
	tp.fs_out = 0;
	tp.bits_in = 0;
	tp.tplg_file = NULL;
	tp.input_file_num = 0;
	tp.output_file_num = 0;
	for (i = 0; i < MAX_OUTPUT_FILE_NUM; i++)
		tp.output_file[i] = NULL;

	for (i = 0; i < MAX_INPUT_FILE_NUM; i++)
		tp.input_file[i] = NULL;

	tp.channels_in = TESTBENCH_NCH;
	tp.channels_out = 0;
	tp.max_pipeline_id = 0;
	tp.copy_check = false;
	tp.quiet = 0;
	tp.dynamic_pipeline_iterations = 1;
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

	if (!tp.channels_out)
		tp.channels_out = tp.channels_in;

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

	if (tp.quiet)
		tb_enable_trace(0); /* reduce trace output */
	else
		tb_enable_trace(1);


	/* initialize ipc and scheduler */
	if (tb_setup(sof_get(), &tp) < 0) {
		fprintf(stderr, "error: pipeline init\n");
		exit(EXIT_FAILURE);
	}

	/* build, run and teardown pipelines */
	pipline_test(&tp);

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

	return EXIT_SUCCESS;
}
