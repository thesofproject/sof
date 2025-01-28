// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <sof/list.h>
#include <tplg_parser/topology.h>

#include "testbench/trace.h"
#include "testbench/file.h"
#include "testbench/utils.h"

#include <ctype.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

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

	for (index = 0; index < TB_MAX_OUTPUT_FILE_NUM && token; index++) {
		/* get output file name with current index */
		tp->output_file[index] = strdup(token);

		/* next output */
		token = strtok_r(NULL, ",", &output_token);
	}

	if (index == TB_MAX_OUTPUT_FILE_NUM && token) {
		fprintf(stderr, "error: max output file number is %d\n",
			TB_MAX_OUTPUT_FILE_NUM);
		for (index = 0; index < TB_MAX_OUTPUT_FILE_NUM; index++)
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

	for (index = 0; index < TB_MAX_INPUT_FILE_NUM && token; index++) {
		/* get input file name with current index */
		tp->input_file[index] = strdup(token);

		/* next input */
		token = strtok_r(NULL, ",", &input_token);
	}

	if (index == TB_MAX_INPUT_FILE_NUM && token) {
		fprintf(stderr, "error: max input file number is %d\n",
			TB_MAX_INPUT_FILE_NUM);
		for (index = 0; index < TB_MAX_INPUT_FILE_NUM; index++)
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

	for (index = 0; index < TB_MAX_OUTPUT_FILE_NUM && token; index++) {
		/* get output file name with current index */
		tp->pipelines[index] = atoi(token);

		/* next output */
		token = strtok_r(NULL, ",", &output_token);
	}

	if (index == TB_MAX_OUTPUT_FILE_NUM && token) {
		fprintf(stderr, "error: max output file number is %d\n",
			TB_MAX_OUTPUT_FILE_NUM);
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
	printf("  -t <topology file>\n\n");
	printf("Options to control test:\n");
	printf("  -d <level> Sets the traces print level:\n");
	printf("     0 all traces are suppressed\n");
	printf("     1 shows error traces\n");
	printf("     2 shows warning traces and previous\n");
	printf("     3 shows info traces and previous\n");
	printf("     4 shows debug traces and previous, plus other testbench  debug messages\n");
	printf("  -p <pipeline1,pipeline2,...>\n");
	printf("  -C <number of copy() iterations>\n");
	printf("  -D <pipeline duration in ms>\n");
	printf("  -P <number of dynamic pipeline iterations>\n");
	printf("  -T <microseconds for tick, 0 for batch mode>\n\n");
	printf("Options for input and output format override:\n");
	printf("  -b <input_format>, S16_LE, S24_LE, or S32_LE\n");
	printf("  -c <input channels>\n");
	printf("  -n <output channels>\n");
	printf("  -r <input rate>\n");
	printf("  -R <output rate>\n\n");
	printf("Help:\n");
	printf("  -h\n\n");
	printf("Example Usage:\n");
	printf("%s -r 48000 -c 2 -b S16_LE -i in.raw -o out.raw -t <test.tplg>\n\n", executable);
}

static int parse_input_args(int argc, char **argv, struct testbench_prm *tp)
{
	int option = 0;
	int ret = 0;

	while ((option = getopt(argc, argv, "hd:i:o:t:b:r:R:c:n:C:P:p:T:D:")) != -1) {
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
			if (isdigit((int)*optarg)) {
				tp->trace_level = atoi(optarg);
			} else {
				fprintf(stderr, "Error: Debug level must be a digit.\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;

		/* number of pipeline copy() iterations */
		case 'C':
			tp->copy_iterations = atoi(optarg);
			tp->copy_check = true;
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
		case 'h':
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);

		default:
			fprintf(stderr, "unknown option %c\n", option);
			print_usage(argv[0]);
			ret = -EINVAL;
		}

		if (ret < 0)
			return ret;
	}

	return ret;
}

static void test_pipeline_stats(struct testbench_prm *tp, long long delta_t)
{
	long long file_cycles, pipeline_cycles;
	float pipeline_mcps;
	int n_in, n_out, frames_out;
	int i;
	int count = 1;

	n_out = 0;
	n_in = 0;
	file_cycles = 0;
	for (i = 0; i < tp->input_file_num; i++) {
		if (tp->fr[i].id < 0)
			continue;

		n_in += tp->fr[i].state->n;
		file_cycles += tp->fr[i].state->cycles_count;
	}

	for (i = 0; i < tp->output_file_num; i++) {
		if (tp->fw[i].id < 0)
			continue;

		n_out += tp->fw[i].state->n;
		file_cycles += tp->fw[i].state->cycles_count;
	}

	/* print test summary */
	printf("==========================================================\n");
	printf("		           Test Summary %d\n", count);
	printf("==========================================================\n");

	for (i = 0; i < tp->pipeline_num; i++) {
		printf("pipeline %d\n", tp->pipelines[i]);
		tb_show_file_stats(tp, tp->pipelines[i]);
	}

	printf("Input bit format: %s\n", tp->bits_in);
	printf("Input sample rate: %d\n", tp->fs_in);
	printf("Output sample rate: %d\n", tp->fs_out);

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
		if (tb_check_trace(LOG_LEVEL_DEBUG))
			printf("Warning: Use -d 3 or smaller value to avoid traces to increase MCPS.\n");
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

		err = tb_load_topology(tp);
		if (err < 0) {
			fprintf(stderr, "error: topology load %d failed %d\n", dp_count, err);
			break;
		}

		err = tb_set_up_all_pipelines(tp);
		if (err < 0) {
			fprintf(stderr, "error: pipelines set up %d failed %d\n", dp_count, err);
			break;
		}

		err = tb_set_running_state(tp);
		if (err < 0) {
			fprintf(stderr, "error: pipelines state set %d failed %d\n", dp_count, err);
			break;
		}

		err = tb_find_file_components(tp); /* Track file comp status during copying */
		if (err < 0) {
			fprintf(stderr, "error: file component find failed %d\n", err);
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
				if (tb_schedule_pipeline_check_state(tp))
					break;
			} else {
				if (err == EINTR) {
					continue; /* interrupted - keep going */
				} else {
					printf("error: sleep failed: %s\n", strerror(err));
					break;
				}
			}
		}

		tb_schedule_pipeline_check_state(tp); /* Once more to flush out remaining data */

		tb_gettime(&td1);

		err = tb_set_reset_state(tp);
		if (err < 0) {
			fprintf(stderr, "error: pipeline reset %d failed %d\n",
				dp_count, err);
			break;
		}

		/* TODO: This should be printed after reset and free to get cleaner output
		 * but the file internal status would be lost there.
		 */
		delta_t = (td1.tv_sec - td0.tv_sec) * 1000000;
		delta_t += (td1.tv_nsec - td0.tv_nsec) / 1000;
		test_pipeline_stats(tp, delta_t);

		err = tb_free_all_pipelines(tp);
		if (err < 0) {
			fprintf(stderr, "error: free pipelines %d failed %d\n", dp_count, err);
			break;
		}

		tb_free_topology(tp);
		dp_count++;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct testbench_prm *tp;
	int i, ret;

	tp = calloc(1, sizeof(struct testbench_prm));
	if (!tp)
		return EXIT_FAILURE;

	/* initialize input and output sample rates, files, etc. */
	tp->channels_in = TESTBENCH_NCH;
	tp->copy_check = false;
	tp->dynamic_pipeline_iterations = 1;
	tp->pipeline_string = calloc(1, TB_DEBUG_MSG_LEN);
	tp->pipelines[0] = 1;
	tp->pipeline_num = 1;
	tp->pipeline_duration_ms = 5000;
	tp->copy_iterations = 1;
	tp->trace_level = LOG_LEVEL_INFO;

	/* command line arguments*/
	ret = parse_input_args(argc, argv, tp);
	if (ret < 0)
		goto out;

	if (!tp->channels_out)
		tp->channels_out = tp->channels_in;

	if (!tp->fs_out)
		tp->fs_out = tp->fs_in;

	/* check mandatory args */
	if (!tp->tplg_file) {
		fprintf(stderr, "topology file not specified, use -t file.tplg\n");
		print_usage(argv[0]);
		ret = EXIT_FAILURE;
		goto out;
	}

	if (!tp->input_file_num) {
		fprintf(stderr, "input files not specified, use -i file1,file2\n");
		print_usage(argv[0]);
		ret = EXIT_FAILURE;
		goto out;
	}

	if (!tp->output_file_num) {
		fprintf(stderr, "output files not specified, use -o file1,file2\n");
		print_usage(argv[0]);
		ret = EXIT_FAILURE;
		goto out;
	}

	if (!tp->bits_in) {
		fprintf(stderr, "input format not specified, use -b format\n");
		print_usage(argv[0]);
		ret = EXIT_FAILURE;
		goto out;
	}

	/* initialize ipc and scheduler */
	if (tb_setup(sof_get(), tp) < 0) {
		fprintf(stderr, "error: pipeline init\n");
		ret = EXIT_FAILURE;
		goto out;
	}

	/* build, run and teardown pipelines */
	pipline_test(tp);

	/* free other core FW services */
	tb_free(sof_get());
	ret = EXIT_SUCCESS;

out:
	/* free all other data */
	free(tp->bits_in);
	free(tp->tplg_file);
	for (i = 0; i < tp->output_file_num; i++)
		free(tp->output_file[i]);

	for (i = 0; i < tp->input_file_num; i++)
		free(tp->input_file[i]);

	free(tp->pipeline_string);
	free(tp);
	return ret;
}
