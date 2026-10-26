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
#include "testbench/remote_server.h"
#include "testbench/topology_ipc4.h"
#include <platform/lib/telemetry_sched.h>
#include <ipc4/module.h>

#include <ctype.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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

	for (index = 0; index < TB_MAX_PIPELINES_NUM && token; index++) {
		/* get pipeline id with current index */
		tp->pipelines[index] = atoi(token);

		/* next pipeline */
		token = strtok_r(NULL, ",", &output_token);
	}

	if (index == TB_MAX_PIPELINES_NUM && token) {
		fprintf(stderr, "error: max pipeline number is %d\n",
			TB_MAX_PIPELINES_NUM);
		return -EINVAL;
	}

	/* set total pipeline number */
	tp->pipeline_num = index;
	return 0;
}

/* print usage for testbench */
static void print_usage(char *executable)
{
	printf("Usage: %s <options> -i <input_file1,input_file2,...> ", executable);
	printf("-o <output_file1,output_file2,...>\n\n");
	printf("Options for processing:\n");
	printf("  -t <topology file>\n");
	printf("  -i <input_file1,input_file2,...> (up to %d comma-separated input files)\n",
	       TB_MAX_INPUT_FILE_NUM);
	printf("  -o <output_file1,output_file2,...> (up to %d comma-separated output files)\n\n",
	       TB_MAX_OUTPUT_FILE_NUM);
	printf("Options to control test:\n");
	printf("  -d <level> Sets the traces print level:\n");
	printf("     0 all traces are suppressed\n");
	printf("     1 shows error traces\n");
	printf("     2 shows warning traces and previous\n");
	printf("     3 shows info traces and previous\n");
	printf("     4 shows debug traces and previous, plus other testbench debug messages\n");
	printf("  -p <pipeline1,pipeline2,...> (up to %d comma-separated pipeline IDs)\n",
	       TB_MAX_PIPELINES_NUM);
	printf("  -C <number of copy() iterations>\n");
	printf("  -P <number of dynamic pipeline iterations>\n");
	printf("  -s <script file to set controls, with amixer and sleep commands>\n");
	printf("  -S <port> Start sof-remote TCP server on <port> (default port: 9998)\n");
	printf("  -W Keep testbench running continuously for remote network control\n");
	printf("  -I Run testbench in interactive network server mode (default port: 9998)\n");
	printf("  -A <platform|list> Apply hardware platform preset (tgl,tgl-h,mtl,arl,lnl,ptl,wcl,nvl,nvl-s)\n");
	printf("  -N <num_cores> Set DSP core count (1..%d)\n", TB_TELEM_MAX_CORES);
	printf("  -m <hpsram[,lpsram[,dram]]> Set platform memory sizes (e.g. 2816K,64K,16M)\n");
	printf("  -w <0|1|2|off|virtual|realtime> Enable CPC workload execution time simulation\n");
	printf("  -F <freq_mhz | core:freq_mhz,...> Set target DSP core clock frequency in MHz/kHz/Hz\n");
	printf("  -M <sram|dram[:penalty_x100[:cycles_per_word]]> Set workload memory domain\n");
	printf("  -T <max_events> Print per-iteration LL/DP module scheduling timeline (start/complete/deadline)\n\n");
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

static void print_platform_presets(void)
{
	size_t count = 0, i;
	const struct host_platform_preset *presets = host_platform_get_presets(&count);

	printf("Available Hardware Platform Presets (-A <platform>):\n");
	printf("  %-8s %-6s %-13s %-10s %-10s %-10s %s\n",
	       "Name", "Cores", "Clock (MHz)", "HPSRAM", "LPSRAM", "DRAM/IMR", "Description");
	printf("  ---------------------------------------------------------------------------------------------\n");
	for (i = 0; i < count; i++) {
		printf("  %-8s %-6u %7u.%03u   %6zu KB  %6zu KB  %6zu KB  %s (aliases: %s)\n",
		       presets[i].name,
		       presets[i].num_cores,
		       presets[i].freq_hz / 1000000U,
		       (presets[i].freq_hz % 1000000U) / 1000U,
		       presets[i].hpsram_size_bytes / 1024UL,
		       presets[i].lpsram_size_bytes / 1024UL,
		       presets[i].dram_size_bytes / 1024UL,
		       presets[i].desc,
		       presets[i].aliases);
	}
}

static uint32_t parse_freq_token_hz(const char *str)
{
	char *endp = NULL;
	double val;

	if (!str || !*str)
		return (uint32_t)TB_DSP_NOMINAL_HZ;
	val = strtod(str, &endp);
	if (val <= 0.0)
		return (uint32_t)TB_DSP_NOMINAL_HZ;

	if (endp && (*endp == 'k' || *endp == 'K'))
		return (uint32_t)(val * 1000.0 + 0.5);
	if (endp && (*endp == 'm' || *endp == 'M'))
		return (uint32_t)(val * 1000000.0 + 0.5);
	if (val < 100000.0)
		return (uint32_t)(val * 1000000.0 + 0.5);
	return (uint32_t)(val + 0.5);
}

static void parse_core_freq_arg(const char *arg)
{
	char *copy = strdup(arg);
	char *saveptr = NULL;
	char *tok;

	if (!copy)
		return;

	tok = strtok_r(copy, ",", &saveptr);
	while (tok) {
		char *colon = strchr(tok, ':');

		if (colon) {
			int core = atoi(tok);
			uint32_t hz = parse_freq_token_hz(colon + 1);

			host_workload_sim_set_core_freq(core, hz);
		} else {
			uint32_t hz = parse_freq_token_hz(tok);

			host_workload_sim_set_core_freq(-1, hz);
		}
		tok = strtok_r(NULL, ",", &saveptr);
	}
	free(copy);
}

static size_t parse_mem_size_token(const char *str)
{
	char *endp = NULL;
	double val;

	if (!str || !*str)
		return 0;
	val = strtod(str, &endp);
	if (val <= 0.0)
		return 0;

	if (endp && (*endp == 'm' || *endp == 'M'))
		return (size_t)(val * 1024.0 * 1024.0 + 0.5);
	if (endp && (*endp == 'k' || *endp == 'K'))
		return (size_t)(val * 1024.0 + 0.5);
	if (endp && (*endp == 'b' || *endp == 'B'))
		return (size_t)(val + 0.5);
	if (val <= 65536.0)
		return (size_t)(val * 1024.0 + 0.5); /* Default unit: KB */
	return (size_t)(val + 0.5);
}

static int parse_mem_cfg_arg(const char *arg)
{
	size_t hpsram = 0, lpsram = 0, dram = 0;
	bool enforce = true;
	char *copy, *saveptr = NULL, *tok;
	int pos = 0;

	if (!arg || !*arg)
		return -EINVAL;

	copy = strdup(arg);
	if (!copy)
		return -ENOMEM;

	tok = strtok_r(copy, ",", &saveptr);
	while (tok) {
		char *eq = strchr(tok, '=');

		if (eq) {
			*eq = '\0';
			const char *val = eq + 1;

			if (strcasecmp(tok, "hpsram") == 0 || strcasecmp(tok, "sram") == 0 ||
			    strcasecmp(tok, "hp") == 0) {
				hpsram = parse_mem_size_token(val);
			} else if (strcasecmp(tok, "lpsram") == 0 || strcasecmp(tok, "lp") == 0) {
				lpsram = parse_mem_size_token(val);
			} else if (strcasecmp(tok, "dram") == 0 || strcasecmp(tok, "imr") == 0 ||
				   strcasecmp(tok, "l3") == 0) {
				dram = parse_mem_size_token(val);
			} else if (strcasecmp(tok, "strict") == 0 ||
				   strcasecmp(tok, "enforce") == 0) {
				enforce = (atoi(val) != 0 && strcasecmp(val, "off") != 0 &&
					   strcasecmp(val, "false") != 0);
			}
		} else if (strcasecmp(tok, "nostrict") == 0) {
			enforce = false;
		} else if (strcasecmp(tok, "strict") == 0) {
			enforce = true;
		} else {
			size_t sz = parse_mem_size_token(tok);

			if (pos == 0)
				hpsram = sz;
			else if (pos == 1)
				lpsram = sz;
			else if (pos == 2)
				dram = sz;
			pos++;
		}
		tok = strtok_r(NULL, ",", &saveptr);
	}
	free(copy);

	if (hpsram == 0 && lpsram == 0 && dram == 0) {
		fprintf(stderr, "Error: invalid memory configuration '%s'\n", arg);
		return -EINVAL;
	}

	host_platform_set_memory(hpsram, lpsram, dram, enforce);
	return 0;
}

static void parse_mem_domain_arg(const char *arg)
{
	uint32_t domain = HOST_MEM_DOMAIN_SRAM;
	uint32_t factor = 0;
	uint32_t c_per_word = 0;
	const char *colon;

	if (strncasecmp(arg, "dram", 4) == 0 || strcmp(arg, "1") == 0)
		domain = HOST_MEM_DOMAIN_DRAM;

	colon = strchr(arg, ':');
	if (colon) {
		const char *colon2;

		factor = (uint32_t)strtoul(colon + 1, NULL, 10);
		colon2 = strchr(colon + 1, ':');
		if (colon2)
			c_per_word = (uint32_t)strtoul(colon2 + 1, NULL, 10);
	}
	host_workload_sim_set_mem_domain(domain, factor, c_per_word);
}

static int parse_input_args(int argc, char **argv, struct testbench_prm *tp)
{
	int option = 0;
	int ret = 0;
	bool w_explicit_off = false;
	const char *opt_preset = NULL;
	const char *opt_cores = NULL;
	const char *opt_mem = NULL;
	const char *opt_freq = NULL;

	while ((option = getopt(argc, argv, "hd:i:o:t:b:r:R:c:n:C:P:p:s:S:WIA:N:m:w:F:M:T:")) != -1) {
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

		/* control script file name */
		case 's':
			tp->control_file = strdup(optarg);
			break;

		/* start sof-remote TCP server on port */
		case 'S':
			tp->server_port = atoi(optarg);
			if (tp->server_port <= 0)
				tp->server_port = 9998;
			break;

		/* keep testbench running continuously for remote control */
		case 'W':
		case 'I':
			tp->server_wait = true;
			if (tp->server_port <= 0)
				tp->server_port = 9998;
			break;

		/* hardware platform preset */
		case 'A':
			if (strcasecmp(optarg, "list") == 0 || strcasecmp(optarg, "help") == 0) {
				print_platform_presets();
				exit(EXIT_SUCCESS);
			}
			opt_preset = optarg;
			break;

		/* DSP core count */
		case 'N':
			opt_cores = optarg;
			break;

		/* DSP memory capacities (HPSRAM, LPSRAM, DRAM) */
		case 'm':
			opt_mem = optarg;
			break;

		/* workload execution time simulation mode */
		case 'w':
			if (strcmp(optarg, "0") == 0 || strcasecmp(optarg, "off") == 0 ||
			    strcasecmp(optarg, "disable") == 0) {
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_OFF);
				w_explicit_off = true;
			} else if (strcmp(optarg, "2") == 0 ||
				   strcasecmp(optarg, "realtime") == 0 ||
				   strcasecmp(optarg, "pace") == 0) {
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_REALTIME);
				w_explicit_off = false;
			} else {
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
				w_explicit_off = false;
			}
			break;

		/* target DSP core clock frequency in MHz */
		case 'F':
			opt_freq = optarg;
			break;

		/* workload execution memory domain (sram vs dram) */
		case 'M':
			parse_mem_domain_arg(optarg);
			if (!w_explicit_off &&
			    host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
			break;

		/* print per-iteration LL/DP module scheduling timeline */
		case 'T':
			tp->sched_trace_limit = atoi(optarg);
			if (tp->sched_trace_limit <= 0)
				tp->sched_trace_limit = 64;
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

	/* Apply platform preset first, then explicit -N / -m / -F overrides regardless of order */
	if (opt_preset) {
		if (host_platform_apply_preset(opt_preset) < 0) {
			fprintf(stderr, "Error: unknown platform preset '%s' (use -A list)\n",
				opt_preset);
			return -EINVAL;
		}
		if (!w_explicit_off && host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
			host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
	}

	if (opt_cores) {
		int nc = atoi(opt_cores);

		if (nc < 1 || nc > TB_TELEM_MAX_CORES ||
		    host_platform_set_core_count((uint32_t)nc) < 0) {
			fprintf(stderr, "Error: core count (-N) must be between 1 and %d\n",
				TB_TELEM_MAX_CORES);
			return -EINVAL;
		}
	}

	if (opt_mem) {
		ret = parse_mem_cfg_arg(opt_mem);
		if (ret < 0)
			return ret;
	}

	if (opt_freq) {
		parse_core_freq_arg(opt_freq);
		if (!w_explicit_off && host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
			host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
	}

	return ret;
}

static void test_pipeline_stats(struct testbench_prm *tp, long long delta_t,
				struct tb_heap_usage_record *heap_records,
				int heap_records_count)
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
	if (heap_records_count > 0) {
		for (i = 0; i < heap_records_count; i++)
			printf("Heap usage for module %s: %u bytes\n",
			       heap_records[i].module_name, (uint32_t)heap_records[i].heap_max);
	}

	printf("\n");
	if (tp->total_cycles) {
		pipeline_cycles = tp->total_cycles - file_cycles;
		pipeline_mcps = frames_out > 0 ?
				((float)pipeline_cycles * tp->fs_out / frames_out / 1e6) : 0.0f;
		if (tb_check_trace(LOG_LEVEL_DEBUG))
			printf("Warning: Use -d 3 or smaller value to avoid traces to increase MCPS.\n");

		printf("Total execution cycles: %lld\n", tp->total_cycles);
		printf("File component cycles: %lld\n", file_cycles);
		printf("Pipeline cycles: %lld\n", pipeline_cycles);
		printf("Pipeline MCPS: %6.2f\n\n", pipeline_mcps);
	}

	if (delta_t)
		printf("Total execution time: %lld us, %.2f x realtime\n\n", delta_t,
		       (float)frames_out / tp->fs_out * 1000000 / delta_t);

	/* Print Per-Core, Per-Pipeline, and Per-Module (LL + DP + Buffer) Telemetry */
	{
		struct host_sched_telemetry *st = host_telem_get();
		uint32_t u;

		if (st && (st->num_pipes > 0 || st->num_mods > 0)) {
			const char *mode_str =
				(st->sim_cfg.mode == HOST_WORKLOAD_SIM_REALTIME) ? "REALTIME-PACED" :
				(st->sim_cfg.mode == HOST_WORKLOAD_SIM_VIRTUAL)  ? "VIRTUAL-SIM" :
										   "HOST-MEASURED";
			const char *mem_str =
				(st->sim_cfg.default_mem_domain == HOST_MEM_DOMAIN_DRAM) ? "DRAM" :
											   "SRAM";

			printf("--- Per-Core LL & DP Scheduler Telemetry [Platform=%s Cores=%u HPSRAM=%zu/%zuKB LPSRAM=%zu/%zuKB Mode=%s Mem=%s DRAM_Factor=%u.%02ux] ---\n",
			       st->hw_cfg.platform_name,
			       st->hw_cfg.num_cores,
			       st->hw_cfg.hpsram_peak_bytes / 1024UL,
			       st->hw_cfg.hpsram_size_bytes / 1024UL,
			       st->hw_cfg.lpsram_peak_bytes / 1024UL,
			       st->hw_cfg.lpsram_size_bytes / 1024UL,
			       mode_str, mem_str,
			       st->sim_cfg.dram_factor_x100 / 100,
			       st->sim_cfg.dram_factor_x100 % 100);
			for (u = 0; u < st->hw_cfg.num_cores && u < TB_TELEM_MAX_CORES; u++) {
				struct host_core_telem *ct = &st->cores[u];

				if (!ct->active)
					continue;
				printf("  Core %u (%u.%03u MHz): util=%u.%02u%% overruns=%u | LL: ticks=%u tasks=%u avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus | DP: runs=%u thr=%u avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus\n",
				       u, ct->freq_hz / 1000000U,
				       (ct->freq_hz % 1000000U) / 1000U,
				       ct->total_util_x100 / 100, ct->total_util_x100 % 100,
				       ct->ll_deadline_violations,
				       ct->ll_tick_count, ct->ll_active_tasks,
				       (unsigned long long)ct->ll_avg_cycles,
				       (unsigned long long)ct->ll_peak_cycles,
				       (unsigned long long)ct->ll_total_cycles,
				       ct->ll_avg_kcps, ct->ll_peak_kcps,
				       (unsigned long long)(ct->ll_avg_exec_ns / 1000ULL),
				       (unsigned long long)(ct->ll_avg_exec_ns % 1000ULL),
				       (unsigned long long)(ct->ll_peak_exec_ns / 1000ULL),
				       (unsigned long long)(ct->ll_peak_exec_ns % 1000ULL),
				       ct->dp_run_count, ct->dp_active_threads,
				       (unsigned long long)ct->dp_avg_cycles,
				       (unsigned long long)ct->dp_peak_cycles,
				       (unsigned long long)ct->dp_total_cycles,
				       ct->dp_avg_kcps, ct->dp_peak_kcps,
				       (unsigned long long)(ct->dp_avg_exec_ns / 1000ULL),
				       (unsigned long long)(ct->dp_avg_exec_ns % 1000ULL),
				       (unsigned long long)(ct->dp_peak_exec_ns / 1000ULL),
				       (unsigned long long)(ct->dp_peak_exec_ns % 1000ULL));
			}

			printf("\n--- Per-Pipeline Scheduling Telemetry (%u active) ---\n",
			       st->num_pipes);
			for (u = 0; u < st->num_pipes; u++) {
				struct host_pipe_telem *pt = &st->pipes[u];

				if (!pt->active)
					continue;
				printf("  Pipe %2u: core=%u prio=%u per=%uus mem=%s mods=%uLL/%uDP copies=%llu avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus xruns=%u\n",
				       pt->pipeline_id, pt->core_id, pt->priority, pt->period_us,
				       pt->mem_domain == HOST_MEM_DOMAIN_DRAM ? "DRAM" : "SRAM",
				       pt->num_ll_modules, pt->num_dp_modules,
				       (unsigned long long)pt->copy_iterations,
				       (unsigned long long)pt->avg_cycles,
				       (unsigned long long)pt->peak_cycles,
				       (unsigned long long)pt->total_cycles,
				       pt->avg_kcps, pt->peak_kcps,
				       (unsigned long long)(pt->avg_exec_ns / 1000ULL),
				       (unsigned long long)(pt->avg_exec_ns % 1000ULL),
				       (unsigned long long)(pt->peak_exec_ns / 1000ULL),
				       (unsigned long long)(pt->peak_exec_ns % 1000ULL),
				       pt->xrun_count);
			}

			printf("\n--- Per-Module LL & DP Scheduling + Entry/Exit Buffer Telemetry (%u active) ---\n",
			       st->num_mods);
			for (u = 0; u < st->num_mods; u++) {
				struct host_mod_telem *mt = &st->mods[u];
				const char *wname = "comp";

				if (!mt->active)
					continue;

				if (tp->widget_list.next) {
					struct list_item *it;

					list_for_item(it, &tp->widget_list) {
						struct tplg_comp_info *ci =
							container_of(it, struct tplg_comp_info, item);
						uint32_t cid = (uint32_t)IPC4_COMP_ID(ci->module_id,
										      ci->instance_id);
						if (cid == mt->comp_id) {
							if (ci->name)
								wname = ci->name;
							break;
						}
					}
				} else if (tp->info) {
					int k;

					for (k = 0; k < tp->info_elems; k++) {
						uint32_t cid = (uint32_t)IPC4_COMP_ID(
							tp->info[k].module_id,
							tp->info[k].instance_id);
						if (cid == mt->comp_id) {
							if (tp->info[k].name)
								wname = tp->info[k].name;
							break;
						}
					}
				}

				printf("  [0x%06x %-24s] pipe=%u core=%u %s mem=%s iters=%llu avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus last=%llu.%03llu->%llu.%03lluus(dl=%llu.%03lluus) ibs=%u obs=%u cpc=%u(viol=%u)",
				       mt->comp_id, wname, mt->pipeline_id, mt->core_id,
				       mt->proc_domain == COMP_PROCESSING_DOMAIN_DP ? "DP" : "LL",
				       mt->mem_domain == HOST_MEM_DOMAIN_DRAM ? "DRAM" : "SRAM",
				       (unsigned long long)mt->iterations,
				       (unsigned long long)mt->avg_cycles,
				       (unsigned long long)mt->peak_cycles,
				       (unsigned long long)mt->total_cycles,
				       mt->avg_kcps, mt->peak_kcps,
				       (unsigned long long)(mt->avg_exec_ns / 1000ULL),
				       (unsigned long long)(mt->avg_exec_ns % 1000ULL),
				       (unsigned long long)(mt->peak_exec_ns / 1000ULL),
				       (unsigned long long)(mt->peak_exec_ns % 1000ULL),
				       (unsigned long long)(mt->last_start_ns / 1000ULL),
				       (unsigned long long)(mt->last_start_ns % 1000ULL),
				       (unsigned long long)(mt->last_complete_ns / 1000ULL),
				       (unsigned long long)(mt->last_complete_ns % 1000ULL),
				       (unsigned long long)(mt->last_deadline_ns / 1000ULL),
				       (unsigned long long)(mt->last_deadline_ns % 1000ULL),
				       mt->ibs, mt->obs, mt->effective_cpc, mt->cpc_violations);
				if (mt->proc_domain == COMP_PROCESSING_DOMAIN_DP) {
					printf(" ll_sync=%llu(%llu cyc)",
					       (unsigned long long)mt->ll_ring_sync_iterations,
					       (unsigned long long)mt->ll_ring_sync_total_cycles);
				}
				if (mt->num_in_pins > 0 && mt->in_pins[0].active) {
					struct host_mod_pin_telem *pin = &mt->in_pins[0];

					printf(" | IN0: %u->%uB xfer=%uB(tot=%lluB) rms=%d.%02ddBFS",
					       pin->dp_ring_size ? pin->dp_ring_entry_avail :
								   pin->entry_avail_bytes,
					       pin->dp_ring_size ? pin->dp_ring_exit_avail :
								   pin->exit_avail_bytes,
					       pin->last_bytes_transferred,
					       (unsigned long long)pin->total_bytes_transferred,
					       pin->entry_rms_dbfs_x100 / 100,
					       abs(pin->entry_rms_dbfs_x100 % 100));
				}
				if (mt->num_out_pins > 0 && mt->out_pins[0].active) {
					struct host_mod_pin_telem *pin = &mt->out_pins[0];

					printf(" | OUT0: %u->%uB xfer=%uB(tot=%lluB) rms=%d.%02ddBFS",
					       pin->dp_ring_size ? pin->dp_ring_entry_avail :
								   pin->entry_avail_bytes,
					       pin->dp_ring_size ? pin->dp_ring_exit_avail :
								   pin->exit_avail_bytes,
					       pin->last_bytes_transferred,
					       (unsigned long long)pin->total_bytes_transferred,
					       pin->exit_rms_dbfs_x100 / 100,
					       abs(pin->exit_rms_dbfs_x100 % 100));
				}
				printf("\n");
			}
			printf("\n");

			if (tp->sched_trace_limit > 0 && st->trace_count > 0) {
				uint32_t max_ev = (uint32_t)tp->sched_trace_limit;
				struct host_sched_trace_event *ev_buf;
				uint64_t total_ev = st->trace_total_events;
				uint32_t avail_ev = st->trace_count;
				uint32_t n_ev, e;

				if (max_ev > TB_TELEM_TRACE_CAPACITY)
					max_ev = TB_TELEM_TRACE_CAPACITY;
				ev_buf = calloc(max_ev, sizeof(*ev_buf));
				if (ev_buf) {
					n_ev = host_telem_trace_read(ev_buf, max_ev, 0,
								     -1, -1, -1, -1);
					printf("--- Per-Iteration LL & DP Module Scheduling Timeline (showing %u of %llu events, ring=%u) ---\n",
					       n_ev, (unsigned long long)total_ev, avail_ev);
					printf("  %-5s %-5s %-5s %-4s %-4s %-7s %-32s %12s %12s %10s %12s %21s %13s %11s\n",
					       "Seq", "Tick", "Iter", "Core", "Pipe", "Domain",
					       "Module", "Start(us)", "Complete(us)", "Dur(us)",
					       "Deadline(us)", "Start->End(cyc)", "Cyc/Budget",
					       "In->Out(B)");
					for (e = 0; e < n_ev; e++) {
						struct host_sched_trace_event *ev = &ev_buf[e];
						const char *ename = "comp";
						const char *dom =
							(ev->event_type == HOST_SCHED_EVT_DP_RUN) ? "DP-RUN" :
							(ev->event_type == HOST_SCHED_EVT_DP_SYNC) ? "DP-SYNC" :
												     "LL-COPY";
						uint64_t dur_ns = (ev->complete_ns >= ev->start_ns) ?
								  (ev->complete_ns - ev->start_ns) : 0;
						char mod_col[48];
						char cyc_col[48];
						char bud_col[32];
						char io_col[32];

						if (tp->widget_list.next) {
							struct list_item *it;

							list_for_item(it, &tp->widget_list) {
								struct tplg_comp_info *ci =
									container_of(it, struct tplg_comp_info, item);
								uint32_t cid = (uint32_t)IPC4_COMP_ID(
									ci->module_id, ci->instance_id);
								if (cid == ev->comp_id) {
									if (ci->name)
										ename = ci->name;
									break;
								}
							}
						} else if (tp->info) {
							int k;

							for (k = 0; k < tp->info_elems; k++) {
								uint32_t cid = (uint32_t)IPC4_COMP_ID(
									tp->info[k].module_id,
									tp->info[k].instance_id);
								if (cid == ev->comp_id) {
									if (tp->info[k].name)
										ename = tp->info[k].name;
									break;
								}
							}
						}

						snprintf(mod_col, sizeof(mod_col), "0x%06x %.22s",
							 ev->comp_id, ename);
						snprintf(cyc_col, sizeof(cyc_col), "%llu->%llu",
							 (unsigned long long)ev->start_cycles,
							 (unsigned long long)ev->complete_cycles);
						snprintf(bud_col, sizeof(bud_col), "%u/%u%s",
							 ev->duration_cycles, ev->cpc_budget,
							 ev->cpc_violation ? "!" : "");
						snprintf(io_col, sizeof(io_col), "%u->%u",
							 ev->bytes_in, ev->bytes_out);

						printf("  %-5u %-5u %-5u C%-3u P%-3u %-7s %-32s %8llu.%03llu %8llu.%03llu %6llu.%03llu %8llu.%03llu %21s %13s %11s\n",
						       ev->seq_id, ev->ll_tick_nr, ev->mod_iter_nr,
						       ev->core_id, ev->pipeline_id, dom, mod_col,
						       (unsigned long long)(ev->start_ns / 1000ULL),
						       (unsigned long long)(ev->start_ns % 1000ULL),
						       (unsigned long long)(ev->complete_ns / 1000ULL),
						       (unsigned long long)(ev->complete_ns % 1000ULL),
						       (unsigned long long)(dur_ns / 1000ULL),
						       (unsigned long long)(dur_ns % 1000ULL),
						       (unsigned long long)(ev->deadline_ns / 1000ULL),
						       (unsigned long long)(ev->deadline_ns % 1000ULL),
						       cyc_col, bud_col, io_col);
					}
					printf("\n");
					free(ev_buf);
				}
			}
		}
	}
}

/*
 * Tester thread, one for each virtual core. This is NOT the thread that will
 * execute the virtual core.
 */
static int pipline_test(struct testbench_prm *tp)
{
	struct tb_heap_usage_record heap_usage_records[TB_NUM_WIDGETS_SUPPORTED];
	struct timespec td0 = {0}, td1 = {0};
	long long delta_t = 0;
	int64_t next_control_ns;
	int64_t time_ns;
	int err = 0;
	int heap_usage_records_count = 0;
	int dp_count = 0;

	/* build, run and teardown pipelines */
	while (dp_count < tp->dynamic_pipeline_iterations) {
		fprintf(stdout, "pipeline run %d/%d\n", dp_count,
			tp->dynamic_pipeline_iterations);

		/* print test summary */
		printf("==========================================================\n");
		printf("		           Test Start %d\n", dp_count);
		printf("==========================================================\n");

		host_telem_reset();

		if (tp->tplg_file) {
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
		}

		if (tp->server_port > 0) {
			err = tb_remote_server_start(tp);
			if (err < 0) {
				fprintf(stderr, "error: remote server start failed %d\n", err);
				break;
			}
		}

		if (!tp->fw[0].state && !tp->server_wait) {
			fprintf(stderr, "error: no active output file component found\n");
			tb_remote_server_stop();
			break;
		}
		tp->has_output_file = (tp->fw[0].state != NULL);
		tp->last_output_time_ns = 0;

		/* Apply initial controls time to call again controls handler */
		err = tb_read_controls(tp, &next_control_ns);
		if (err) {
			fprintf(stderr, "error: failed to read control commands.\n");
			goto out;
		}

		tb_gettime(&td0);

		while (true) {
			if (tp->copy_check) {
				if (tp->copy_iterations-- <= 0)
					break;
			}

			if (tb_schedule_pipeline_check_state(tp))
				break;

			if (next_control_ns && tp->has_output_file) {
				time_ns = tp->last_output_time_ns;
				if (time_ns >= next_control_ns) {
					err = tb_read_controls(tp, &next_control_ns);
					if (err) {
						fprintf(stderr,
							"error: failed to read control commands.\n");
						goto out;
					}

					if (next_control_ns)
						next_control_ns += time_ns;
				}
			}
		}

		if (!tp->server_wait)
			tb_schedule_pipeline_check_state(tp); /* Once more to flush out remaining data */
		tb_gettime(&td1);
		delta_t = (td1.tv_sec - td0.tv_sec) * 1000000;
		delta_t += (td1.tv_nsec - td0.tv_nsec) / 1000;
		tb_collect_heap_usage(tp, heap_usage_records, &heap_usage_records_count);

out:
		if (tp->server_port > 0)
			tb_remote_server_stop();

		if (tp->tplg_file) {
			test_pipeline_stats(tp, delta_t, heap_usage_records,
					    heap_usage_records_count);

			err = tb_set_reset_state(tp);
			if (err < 0) {
				fprintf(stderr, "error: pipeline reset %d failed %d\n",
					dp_count, err);
				break;
			}

			err = tb_free_all_pipelines(tp);
			if (err < 0) {
				fprintf(stderr, "error: free pipelines %d failed %d\n",
					dp_count, err);
				break;
			}

			tb_free_topology(tp);
		}
		dp_count++;
	}

	return err < 0 ? err : 0;
}

int main(int argc, char **argv)
{
	struct testbench_prm *tp;
	int i, ret;

	tp = calloc(1, sizeof(struct testbench_prm));
	if (!tp)
		return EXIT_FAILURE;

	list_init(&tp->widget_list);
	list_init(&tp->route_list);
	list_init(&tp->pcm_list);
	list_init(&tp->pipeline_list);
	for (i = 0; i < TB_MAX_INPUT_FILE_NUM; i++)
		tp->fr[i].id = -1;
	for (i = 0; i < TB_MAX_OUTPUT_FILE_NUM; i++)
		tp->fw[i].id = -1;

	/* initialize input and output sample rates, files, etc. */
	tp->channels_in = TESTBENCH_NCH;
	tp->copy_check = false;
	tp->dynamic_pipeline_iterations = 1;
	tp->pipeline_string = calloc(1, TB_DEBUG_MSG_LEN);
	tp->pipelines[0] = 1;
	tp->pipeline_num = 1;
	tp->copy_iterations = 1;
	tp->trace_level = LOG_LEVEL_INFO;

	/* command line arguments*/
	ret = parse_input_args(argc, argv, tp);
	if (ret < 0)
		goto out;

	if (!tp->channels_out)
		tp->channels_out = tp->channels_in;

	if (tp->server_port > 0) {
#if !defined(__XTENSA__) && !defined(CONFIG_XT_SIMULATOR)
		snprintf(tp->server_tmp_dir, sizeof(tp->server_tmp_dir), "/tmp/sof-tb-XXXXXX");
		if (!mkdtemp(tp->server_tmp_dir)) {
			perror("mkdtemp");
			ret = EXIT_FAILURE;
			goto out;
		}
#endif
		if (!tp->fs_in)
			tp->fs_in = 48000;
		if (!tp->bits_in) {
			tp->bits_in = strdup("S16_LE");
			tp->frame_fmt = tplg_find_format(tp->bits_in);
		}
		if (!tp->input_file_num) {
			char in_path[128];
			FILE *sfp;

			snprintf(in_path, sizeof(in_path), "%s/srv_in.raw",
				 tp->server_tmp_dir[0] ? tp->server_tmp_dir : "/tmp");
			sfp = fopen(in_path, "wb");
			if (sfp) {
				int16_t wave[1920];
				int s;

				for (s = 0; s < 1920; s += 2) {
					/* 1 kHz sine wave at 48 kHz (48-sample period) */
					static const int16_t lut48[12] = {
						0, 2129, 4186, 6103, 7812, 9256,
						10386, 11163, 11560, 11560, 11163, 10386
					};
					int idx = (s / 2) % 48;
					int q = idx / 12;
					int pos = idx % 12;
					int16_t v = (q == 0) ? lut48[pos] :
						    (q == 1) ? lut48[11 - pos] :
						    (q == 2) ? -lut48[pos] : -lut48[11 - pos];

					wave[s] = v;
					wave[s + 1] = v;
				}
				for (i = 0; i < 20; i++)
					fwrite(wave, 1, sizeof(wave), sfp);
				fclose(sfp);
			}
			for (i = 0; i < TB_MAX_INPUT_FILE_NUM; i++)
				tp->input_file[i] = strdup(in_path);
			tp->input_file_num = TB_MAX_INPUT_FILE_NUM;
		}
		if (!tp->output_file_num) {
			for (i = 0; i < TB_MAX_OUTPUT_FILE_NUM; i++) {
				char path[128];

				snprintf(path, sizeof(path), "%s/srv_out_%d.raw",
					 tp->server_tmp_dir[0] ? tp->server_tmp_dir : "/tmp", i);
				tp->output_file[i] = strdup(path);
			}
			tp->output_file_num = TB_MAX_OUTPUT_FILE_NUM;
		}
	}

	if (!tp->fs_out)
		tp->fs_out = tp->fs_in;

	/* check mandatory args */
	if (!tp->tplg_file && tp->server_port <= 0) {
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

	if (tp->control_file) {
		tp->control_fh = fopen(tp->control_file, "r");
		if (!tp->control_fh) {
			fprintf(stderr, "error: opening script %s (%s).\n",
				tp->control_file, strerror(errno));
			ret = -errno;
			goto out;
		}
	}

	/* build, run and teardown pipelines */
	ret = pipline_test(tp) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;

	/* free other core FW services */
	tb_free(sof_get());

out:
	/* free all other data */
	free(tp->bits_in);
	free(tp->tplg_file);
	free(tp->control_file);
	if (tp->control_fh)
		fclose(tp->control_fh);

	for (i = 0; i < tp->output_file_num; i++)
		free(tp->output_file[i]);

	for (i = 0; i < tp->input_file_num; i++)
		free(tp->input_file[i]);

#if !defined(__XTENSA__) && !defined(CONFIG_XT_SIMULATOR)
	if (tp->server_tmp_dir[0]) {
		char path[128];

		snprintf(path, sizeof(path), "%s/srv_in.raw", tp->server_tmp_dir);
		unlink(path);
		for (i = 0; i < TB_MAX_OUTPUT_FILE_NUM; i++) {
			snprintf(path, sizeof(path), "%s/srv_out_%d.raw", tp->server_tmp_dir, i);
			unlink(path);
		}
		for (i = 0; i <= 16; i++) {
			snprintf(path, sizeof(path), "%s/llext_%d.so", tp->server_tmp_dir, i);
			unlink(path);
		}
		rmdir(tp->server_tmp_dir);
	}
#endif

	free(tp->pipeline_string);
	free(tp);
	return ret;
}
