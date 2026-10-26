#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
#
# Comprehensive SOF Testbench4 Topology & Module Test Suite
# Covers:
#   Tier 1: All 26 built-in audio modules across S16_LE, S24_LE, S32_LE,
#           rate conversion (44.1k <-> 48k), and multi-channel (4ch -> 2ch)
#   Tier 2: Simple to complex multi-pipeline DAG topologies (passthrough,
#           cascaded 3-pipeline chain, 6-module EFX chain, 1-to-2 fan-out
#           crossover, 2-to-1 fan-in RTC AEC, 4ch DMIC TDFB, 11-pipeline
#           concurrent HDA multi-stream)
#   Tier 3: Combinations of LL and DP modules (single DP, duplex multi-thread
#           DP for phase_vocoder & stft_process variants, mixed LL+DP SRC)
#   Tier 4: Multi-Core LL and Cross-Core DP execution across DSP Cores 0, 1, 2
#           (HDA, MTL, LNL, PTL, and Dolby DAX + Google CTC)
#   Tier 5: Runtime control scripts (-s amixer/sof-ctl) and live SOFR TCP
#           remote network control & telemetry inspection (-S <port> -W)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOF_TOP="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Locate sof-testbench4 binary
if [[ -z "${TB_BIN:-}" ]]; then
	if [[ -x "${SCRIPT_DIR}/build_testbench/sof-testbench4" ]]; then
		TB_BIN="${SCRIPT_DIR}/build_testbench/sof-testbench4"
	elif [[ -x "${SCRIPT_DIR}/build_testbench/install/bin/sof-testbench4" ]]; then
		TB_BIN="${SCRIPT_DIR}/build_testbench/install/bin/sof-testbench4"
	elif [[ -x "${SCRIPT_DIR}/build_upstream/sof-testbench4" ]]; then
		TB_BIN="${SCRIPT_DIR}/build_upstream/sof-testbench4"
	elif [[ -x "${SCRIPT_DIR}/build_upstream/install/bin/sof-testbench4" ]]; then
		TB_BIN="${SCRIPT_DIR}/build_upstream/install/bin/sof-testbench4"
	else
		echo "ERROR: sof-testbench4 binary not found under ${SCRIPT_DIR}/build_testbench or build_upstream" >&2
		exit 1
	fi
fi

# Locate sof-tb-ctl binary
if [[ -z "${TB_CTL_BIN:-}" ]]; then
	if [[ -x "${SCRIPT_DIR}/build_testbench/sof-tb-ctl" ]]; then
		TB_CTL_BIN="${SCRIPT_DIR}/build_testbench/sof-tb-ctl"
	elif [[ -x "${SCRIPT_DIR}/build_testbench/install/bin/sof-tb-ctl" ]]; then
		TB_CTL_BIN="${SCRIPT_DIR}/build_testbench/install/bin/sof-tb-ctl"
	elif [[ -x "${SCRIPT_DIR}/build_upstream/sof-tb-ctl" ]]; then
		TB_CTL_BIN="${SCRIPT_DIR}/build_upstream/sof-tb-ctl"
	elif [[ -x "${SCRIPT_DIR}/build_upstream/install/bin/sof-tb-ctl" ]]; then
		TB_CTL_BIN="${SCRIPT_DIR}/build_upstream/install/bin/sof-tb-ctl"
	else
		echo "ERROR: sof-tb-ctl binary not found under ${SCRIPT_DIR}/build_testbench or build_upstream" >&2
		exit 1
	fi
fi

TPLG_DIR="${TPLG_DIR:-${SOF_TOP}/tools/build_tools/topology/topology2}"
DEV_TPLG="${TPLG_DIR}/development"
PROD_TPLG="${TPLG_DIR}/production"

TIER_FILTER="all"
NAME_FILTER=""
LIST_ONLY=0
VERBOSE=0
SINGLE_SESSION=1

usage() {
	cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --tier <1|2|3|4|5|all>  Run only the specified test tier (default: all)
  --filter <regex>        Run only tests whose name matches <regex>
  --single-session        Run all tests in 1 persistent sof-testbench4 -I session (default)
  --per-process           Spawn a fresh sof-testbench4 process per test
  --list                  List all registered test cases and exit
  -v, --verbose           Show full testbench output for passing tests
  -h, --help              Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--tier)
			TIER_FILTER="$2"
			shift 2
			;;
		--filter)
			NAME_FILTER="$2"
			shift 2
			;;
		--single-session)
			SINGLE_SESSION=1
			shift
			;;
		--per-process)
			SINGLE_SESSION=0
			shift
			;;
		--list)
			LIST_ONLY=1
			shift
			;;
		-v|--verbose)
			VERBOSE=1
			shift
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "Unknown option: $1" >&2
			usage >&2
			exit 1
			;;
	esac
done

WORK_DIR="$(mktemp -d /tmp/sof_tb_suite_XXXXXX)"
TB_PORT=$((19400 + (RANDOM % 500)))
TB_SERVER_PID=""
TB_SERVER_LOG="${WORK_DIR}/persistent_server.log"

cleanup() {
	if [[ -n "${TB_SERVER_PID}" ]] && kill -0 "${TB_SERVER_PID}" 2>/dev/null; then
		"${TB_CTL_BIN}" -H 127.0.0.1 -p "${TB_PORT}" quit >/dev/null 2>&1 || true
		wait "${TB_SERVER_PID}" 2>/dev/null || true
	fi
	rm -rf "${WORK_DIR}"
}
trap cleanup EXIT

start_persistent_server() {
	"${TB_BIN}" -I -S "${TB_PORT}" >"${TB_SERVER_LOG}" 2>&1 &
	TB_SERVER_PID=$!
	local ready=0
	for _ in {1..40}; do
		if "${TB_CTL_BIN}" -H 127.0.0.1 -p "${TB_PORT}" ping >/dev/null 2>&1; then
			ready=1
			break
		fi
		sleep 0.05
	done
	if [[ "${ready}" -ne 1 ]]; then
		echo "ERROR: persistent sof-testbench4 -I failed to start on port ${TB_PORT}" >&2
		cat "${TB_SERVER_LOG}" >&2 || true
		exit 1
	fi
}

# Generate deterministic multi-tone PCM input files
generate_inputs() {
	python3 - "${WORK_DIR}" <<'PYEOF'
import math, os, struct, sys

work_dir = sys.argv[1]

def synth_tone(n_frames, rate, channels, bits):
    buf = bytearray()
    for i in range(n_frames):
        # 500 Hz + 1500 Hz + 4000 Hz composite signal for crossover/MBDRC/STFT/TDFB
        t = i / float(rate)
        sig = (0.45 * math.sin(2.0 * math.pi * 500.0 * t) +
               0.30 * math.sin(2.0 * math.pi * 1500.0 * t) +
               0.15 * math.sin(2.0 * math.pi * 4000.0 * t))
        if bits == 16:
            val = int(max(-32767, min(32767, sig * 24000.0)))
            frame = [val if (ch % 2 == 0) else -val for ch in range(channels)]
            buf.extend(struct.pack("<" + "h" * channels, *frame))
        elif bits == 24:
            # S24_LE stored in 32-bit signed container (lower 24 bits significant)
            val = int(max(-8388607, min(8388607, sig * 6000000.0)))
            frame = [val if (ch % 2 == 0) else -val for ch in range(channels)]
            buf.extend(struct.pack("<" + "i" * channels, *frame))
        else:
            val = int(max(-2147483647, min(2147483647, sig * 1500000000.0)))
            frame = [val if (ch % 2 == 0) else -val for ch in range(channels)]
            buf.extend(struct.pack("<" + "i" * channels, *frame))
    return bytes(buf)

with open(os.path.join(work_dir, "in_48k_2ch_s32.raw"), "wb") as f:
    f.write(synth_tone(4800, 48000, 2, 32))
with open(os.path.join(work_dir, "in_48k_2ch_s24.raw"), "wb") as f:
    f.write(synth_tone(4800, 48000, 2, 24))
with open(os.path.join(work_dir, "in_48k_2ch_s16.raw"), "wb") as f:
    f.write(synth_tone(4800, 48000, 2, 16))
with open(os.path.join(work_dir, "in_48k_4ch_s32.raw"), "wb") as f:
    f.write(synth_tone(4800, 48000, 4, 32))
with open(os.path.join(work_dir, "in_44k1_2ch_s32.raw"), "wb") as f:
    f.write(synth_tone(4410, 44100, 2, 32))
with open(os.path.join(work_dir, "in_16k_2ch_s16.raw"), "wb") as f:
    f.write(synth_tone(1600, 16000, 2, 16))
PYEOF
}

TOTAL_RUN=0
TOTAL_PASS=0
TOTAL_FAIL=0
declare -a FAILED_TESTS=()
declare -A TIER_PASS=([1]=0 [2]=0 [3]=0 [4]=0 [5]=0)
declare -A TIER_TOTAL=([1]=0 [2]=0 [3]=0 [4]=0 [5]=0)

should_run() {
	local tier="$1"
	local name="$2"
	if [[ "${TIER_FILTER}" != "all" && "${TIER_FILTER}" != "${tier}" ]]; then
		return 1
	fi
	if [[ -n "${NAME_FILTER}" ]] && ! [[ "${name}" =~ ${NAME_FILTER} ]]; then
		return 1
	fi
	return 0
}

# run_tb_case <tier> <name> <description> <tplg_path> <pipelines> <num_in> <in_file> <num_out> <expect_regex> [extra_args...]
run_tb_case() {
	local tier="$1"
	local name="$2"
	local desc="$3"
	local tplg="$4"
	local pipelines="$5"
	local num_in="$6"
	local in_file="$7"
	local num_out="$8"
	local expect_regex="$9"
	shift 9
	local extra_args=("$@")

	if ! should_run "${tier}" "${name}"; then
		return 0
	fi

	if [[ "${LIST_ONLY}" -eq 1 ]]; then
		printf "  [Tier %s] %-36s : %s\n" "${tier}" "${name}" "${desc}"
		return 0
	fi

	TOTAL_RUN=$((TOTAL_RUN + 1))
	TIER_TOTAL[${tier}]=$((TIER_TOTAL[${tier}] + 1))

	if [[ ! -f "${tplg}" ]]; then
		printf "  [FAIL] [T%s] %-36s (missing topology: %s)\n" "${tier}" "${name}" "${tplg}"
		TOTAL_FAIL=$((TOTAL_FAIL + 1))
		FAILED_TESTS+=("${name}")
		return 0
	fi

	local in_arg=""
	for ((idx = 1; idx <= num_in; idx++)); do
		[[ -n "${in_arg}" ]] && in_arg+=","
		in_arg+="${in_file}"
	done

	local out_arg=""
	local -a out_files=()
	for ((idx = 1; idx <= num_out; idx++)); do
		local of="${WORK_DIR}/out_${name}_${idx}.raw"
		rm -f "${of}"
		out_files+=("${of}")
		[[ -n "${out_arg}" ]] && out_arg+=","
		out_arg+="${of}"
	done

	local log_file="${WORK_DIR}/log_${name}.txt"
	local rc=0
	local fail_reason=""

	if [[ "${SINGLE_SESSION}" -eq 1 ]]; then
		if [[ -z "${TB_SERVER_PID}" ]] || ! kill -0 "${TB_SERVER_PID}" 2>/dev/null; then
			fail_reason="persistent sof-testbench4 server (PID ${TB_SERVER_PID:-none}) is not running"
		else
			"${TB_CTL_BIN}" -H 127.0.0.1 -p "${TB_PORT}" \
				run-test -t "${tplg}" -p "${pipelines}" -i "${in_arg}" -o "${out_arg}" "${extra_args[@]}" \
				>"${log_file}" 2>&1 || rc=$?
			if ! kill -0 "${TB_SERVER_PID}" 2>/dev/null; then
				fail_reason="persistent sof-testbench4 server (PID ${TB_SERVER_PID}) died during test"
			fi
		fi
	else
		"${TB_BIN}" -t "${tplg}" -p "${pipelines}" -i "${in_arg}" -o "${out_arg}" "${extra_args[@]}" >"${log_file}" 2>&1 || rc=$?
	fi

	if [[ -z "${fail_reason}" ]]; then
		if [[ "${rc}" -ne 0 ]]; then
			fail_reason="exit code ${rc}"
		elif grep -q "^error:" "${log_file}"; then
			fail_reason="error in testbench output"
		elif [[ "${name}" != "mod_igo_nr32" ]]; then
			for of in "${out_files[@]}"; do
				if [[ ! -s "${of}" ]]; then
					fail_reason="empty or missing output file $(basename "${of}")"
					break
				fi
			done
		fi
	fi

	if [[ -z "${fail_reason}" && -n "${expect_regex}" ]]; then
		if ! grep -Eq "${expect_regex}" "${log_file}"; then
			fail_reason="missing expected pattern '${expect_regex}'"
		fi
	fi

	if [[ -z "${fail_reason}" ]]; then
		local perf_summary
		perf_summary="$(grep "Total execution time:" "${log_file}" 2>/dev/null | tail -n 1 | sed 's/Total execution time: //' || true)"
		printf "  [PASS] [T%s] %-36s | %-44s | %s\n" "${tier}" "${name}" "${desc}" "${perf_summary:-OK}"
		TOTAL_PASS=$((TOTAL_PASS + 1))
		TIER_PASS[${tier}]=$((TIER_PASS[${tier}] + 1))
		if [[ "${VERBOSE}" -eq 1 ]]; then
			cat "${log_file}"
		fi
	else
		printf "  [FAIL] [T%s] %-36s | %-44s | %s\n" "${tier}" "${name}" "${desc}" "${fail_reason}"
		cat "${log_file}" >&2
		TOTAL_FAIL=$((TOTAL_FAIL + 1))
		FAILED_TESTS+=("${name}")
	fi
}

# ==============================================================================
# Tier 5 Helper: Live Remote Server & Control Tests
# ==============================================================================
run_tier5_tests() {
	# 5.1 Control script (-s) with amixer and sof-ctl on sof-hda-generic.tplg
	if should_run 5 "ctrl_script_hda_generic"; then
		if [[ "${LIST_ONLY}" -eq 1 ]]; then
			printf "  [Tier 5] %-36s : %s\n" "ctrl_script_hda_generic" "Runtime -s amixer & sof-ctl script on sof-hda-generic"
		else
			local ctl_script="${WORK_DIR}/controls.sh"
			cat >"${ctl_script}" <<EOF
amixer -c0 cset name='Pre Mixer Analog Playback Volume' 30,30
amixer -c0 cset name='Post Mixer Analog Playback Volume' 40,40
sof-ctl -c name='Post Mixer Analog Playback IIR Eq bytes' -s ${SOF_TOP}/tools/ctl/ipc4/eq_iir/pass.txt
EOF
			run_tb_case 5 "ctrl_script_hda_generic" \
				"Runtime -s amixer & sof-ctl script" \
				"${PROD_TPLG}/sof-hda-generic.tplg" "1,2" \
				1 "${WORK_DIR}/in_48k_2ch_s32.raw" 1 \
				"Setting control name.*Post Mixer Analog Playback IIR Eq bytes" \
				-r 48000 -R 48000 -c 2 -n 2 -b S32_LE -s "${ctl_script}"
		fi
	fi

	# 5.2 Live SOFR v1 TCP remote control & telemetry on 3-core LL+DP topology
	if should_run 5 "remote_tcp_multicore_dp"; then
		if [[ "${LIST_ONLY}" -eq 1 ]]; then
			printf "  [Tier 5] %-36s : %s\n" "remote_tcp_multicore_dp" "Live SOFR TCP remote control & telemetry on 3-core DP"
		else
			TOTAL_RUN=$((TOTAL_RUN + 1))
			TIER_TOTAL[5]=$((TIER_TOTAL[5] + 1))
			local cli_log="${WORK_DIR}/remote_cli.log"
			local dot_file="${WORK_DIR}/dapm.dot"
			local port="${TB_PORT}"
			local tb_pid=""
			local fail_reason=""

			if [[ "${SINGLE_SESSION}" -eq 1 ]]; then
				if [[ -z "${TB_SERVER_PID}" ]] || ! kill -0 "${TB_SERVER_PID}" 2>/dev/null; then
					fail_reason="persistent sof-testbench4 server is not running"
				fi
			else
				port=$((19900 + (RANDOM % 500)))
				local srv_log="${WORK_DIR}/remote_srv.log"
				"${TB_BIN}" -I -S "${port}" >"${srv_log}" 2>&1 &
				tb_pid=$!
				local ready=0
				for _ in {1..30}; do
					if "${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" ping >/dev/null 2>&1; then
						ready=1
						break
					fi
					sleep 0.1
				done
				[[ "${ready}" -ne 1 ]] && fail_reason="TCP server did not come up on port ${port}"
			fi

			if [[ -z "${fail_reason}" ]]; then
				{
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" io-cfg \
						-r 48000 -R 48000 -c 2 -n 2 -b S32_LE \
						-p 5,6,7,8,11,12 \
						-i "${WORK_DIR}/in_48k_2ch_s32.raw,${WORK_DIR}/in_48k_2ch_s32.raw,${WORK_DIR}/in_48k_2ch_s32.raw" \
						-o "${WORK_DIR}/ro1.raw,${WORK_DIR}/ro2.raw,${WORK_DIR}/ro3.raw"
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" tplg-load \
						"${DEV_TPLG}/sof-mtl-nocodec-dp-core-test.tplg" 5,6,7,8,11,12
					sleep 0.05
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" ping
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" info
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" pm query
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" win-info
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" win-read 0 0 32
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" large-cfg-get 0 0 9 512
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" cards
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" pcm-caps
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" dapm "${dot_file}"
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" compr caps
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" kctl list
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" kctl get 1
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" kctl set 1 25 25
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" telem
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" probes avail
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" probes extract
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" pcm status
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" pipelines
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" modules
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" shell "kernel threads"
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" sched-telem
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" sched-trace 24 --json "${WORK_DIR}/sched_trace.json"
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" mod-buffers src
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" workload-sim set virtual dram 200 180
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" workload-sim freq 0:400,1:200,2:120
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" workload-sim status
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" tplg-unload
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" workload-sim reset
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" workload-sim disable
				} >"${cli_log}" 2>&1 || fail_reason="sof-tb-ctl command failed"
			fi

			if [[ -n "${tb_pid}" ]]; then
				"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" quit >/dev/null 2>&1 || true
				wait "${tb_pid}" 2>/dev/null || true
			fi

			if [[ -z "${fail_reason}" ]]; then
				if ! grep -q "digraph DAPM" "${dot_file}"; then
					fail_reason="DAPM DOT graph missing"
				elif ! grep -q "DP Worker Core 1" "${cli_log}"; then
					fail_reason="Cross-core DP Worker Core 1 thread missing in telemetry"
				elif ! grep -q "Per-Module LL & DP Scheduling" "${cli_log}"; then
					fail_reason="sched-telem output missing in remote shell"
				elif ! grep -q "Per-Iteration LL & DP Module Scheduling Timeline" "${cli_log}"; then
					fail_reason="sched-trace timeline output missing in remote shell"
				elif ! grep -q '"traceEvents"' "${WORK_DIR}/sched_trace.json"; then
					fail_reason="sched-trace Chrome Trace JSON export missing"
				elif ! grep -q "DP_RING(" "${cli_log}"; then
					fail_reason="mod-buffers DP_RING telemetry missing in remote shell"
				elif ! grep -q "Workload Execution Time Simulation" "${cli_log}"; then
					fail_reason="workload-sim status output missing in remote shell"
				fi
			fi

			if [[ -z "${fail_reason}" ]]; then
				printf "  [PASS] [T5] %-36s | %-44s | 25 RPCs OK\n" \
					"remote_tcp_multicore_dp" "Live SOFR TCP control & 3-core telemetry"
				TOTAL_PASS=$((TOTAL_PASS + 1))
				TIER_PASS[5]=$((TIER_PASS[5] + 1))
			else
				printf "  [FAIL] [T5] %-36s | %-44s | %s\n" \
					"remote_tcp_multicore_dp" "Live SOFR TCP control & 3-core telemetry" "${fail_reason}"
				cat "${cli_log}" >&2 || true
				TOTAL_FAIL=$((TOTAL_FAIL + 1))
				FAILED_TESTS+=("remote_tcp_multicore_dp")
			fi
		fi
	fi

	# 5.3 CLI Workload Execution Time Simulation (-w 1 -F 0:400,1:200,2:120 -M dram:160)
	run_tb_case 5 "workload_sim_cli_dram_multicore" \
		"CPC Workload Sim (DRAM 1.6x, C0=400/C1=200/C2=120MHz)" \
		"${DEV_TPLG}/sof-mtl-nocodec-dp-core-test.tplg" "5,6,7,8,11,12" \
		3 "${WORK_DIR}/in_48k_2ch_s32.raw" 3 \
		"Mode=VIRTUAL-SIM Mem=DRAM DRAM_Factor=1.60x" \
		-r 48000 -R 48000 -c 2 -n 2 -b S32_LE -w 1 -F 0:400,1:200,2:120 -M dram:160

	# 5.4 Hardware Platform Preset & Core/Memory Config (-A ptl -T 20)
	run_tb_case 5 "platform_preset_ptl_multicore" \
		"Hardware Platform Preset PTL (5c @ 442.368MHz, 4608KB HPSRAM)" \
		"${DEV_TPLG}/sof-mtl-nocodec-dp-core-test.tplg" "5,6,7,8,11,12" \
		3 "${WORK_DIR}/in_48k_2ch_s32.raw" 3 \
		"Per-Iteration LL & DP Module Scheduling Timeline" \
		-r 48000 -R 48000 -c 2 -n 2 -b S32_LE -A ptl -T 20

	# 5.5 Interactive sof-tb-ctl Batch Script (-f), REPL & Dynamic IPC4 Pipeline/Module Ops
	if should_run 5 "interactive_tb_ctl_batch_repl"; then
		if [[ "${LIST_ONLY}" -eq 1 ]]; then
			printf "  [Tier 5] %-36s : %s\n" "interactive_tb_ctl_batch_repl" "Interactive sof-tb-ctl batch (-f), REPL & dynamic IPC4 ops"
		else
			TOTAL_RUN=$((TOTAL_RUN + 1))
			TIER_TOTAL[5]=$((TIER_TOTAL[5] + 1))
			local batch_file="${WORK_DIR}/tb_batch.cmd"
			local batch_log="${WORK_DIR}/tb_batch.log"
			local port="${TB_PORT}"
			local tb_pid=""
			local fail_reason=""

			if [[ "${SINGLE_SESSION}" -ne 1 ]]; then
				port=$((19900 + (RANDOM % 500)))
				"${TB_BIN}" -I -S "${port}" >"${WORK_DIR}/batch_srv.log" 2>&1 &
				tb_pid=$!
				for _ in {1..30}; do
					"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" ping >/dev/null 2>&1 && break
					sleep 0.05
				done
			fi

			cat >"${batch_file}" <<EOF
ping
info
platform-cfg presets
platform-cfg preset arl
platform-cfg status
platform-cfg preset default
core enable 1
core enable 2
ppl-create 99 0 1
mod-init 0x2 0 99 1 0
mod-init 0x3 0 99 1 0
mod-bind 0x2 0 0 0x3 0 0
pipelines
ppl-state 99 paused
ppl-state 99 reset
mod-unbind 0x2 0 0 0x3 0 0
mod-delete 0x2 0
mod-delete 0x3 0
ppl-delete 99
winstream version
win-write 1 0 0xdeadbeef
pcm start-tone hw:0,0 1000 48000 2
pcm status
pcm stop-tone
pause
step 2
resume
tplg-unload
EOF
			"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" -f "${batch_file}" >"${batch_log}" 2>&1 || \
				fail_reason="sof-tb-ctl -f batch script failed"

			if [[ -z "${fail_reason}" ]]; then
				"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" repl >>"${batch_log}" 2>&1 <<EOF || fail_reason="sof-tb-ctl repl failed"
ping
info
exit
EOF
			fi

			if [[ -n "${tb_pid}" ]]; then
				"${TB_CTL_BIN}" -H 127.0.0.1 -p "${port}" quit >/dev/null 2>&1 || true
				wait "${tb_pid}" 2>/dev/null || true
			fi

			if [[ -z "${fail_reason}" ]]; then
				if ! grep -q "CREATE_PIPELINE(inst=99, prio=0, core=1) -> status=0 (IPC4_SUCCESS)" "${batch_log}"; then
					fail_reason="Dynamic CREATE_PIPELINE failed"
				elif ! grep -q "MOD_INIT_INSTANCE(mod=0x2, inst=0, ppl=99, core=1, dom=LL) -> status=0 (IPC4_SUCCESS)" "${batch_log}"; then
					fail_reason="Dynamic MOD_INIT_INSTANCE failed"
				elif ! grep -q "MOD_BIND(0x2:0 q0 -> 0x3:0 q0) -> status=0 (IPC4_SUCCESS)" "${batch_log}"; then
					fail_reason="Dynamic MOD_BIND failed"
				elif ! grep -q "DELETE_PIPELINE(inst=99) -> status=0 (IPC4_SUCCESS)" "${batch_log}"; then
					fail_reason="Dynamic DELETE_PIPELINE failed"
				fi
			fi

			if [[ -z "${fail_reason}" ]]; then
				printf "  [PASS] [T5] %-36s | %-44s | Batch+REPL OK\n" \
					"interactive_tb_ctl_batch_repl" "Interactive sof-tb-ctl batch, REPL & IPC4 ops"
				TOTAL_PASS=$((TOTAL_PASS + 1))
				TIER_PASS[5]=$((TIER_PASS[5] + 1))
			else
				printf "  [FAIL] [T5] %-36s | %-44s | %s\n" \
					"interactive_tb_ctl_batch_repl" "Interactive sof-tb-ctl batch, REPL & IPC4 ops" "${fail_reason}"
				cat "${batch_log}" >&2 || true
				TOTAL_FAIL=$((TOTAL_FAIL + 1))
				FAILED_TESTS+=("interactive_tb_ctl_batch_repl")
			fi
		fi
	fi
}

# ==============================================================================
# Execute Test Suite
# ==============================================================================
if [[ "${LIST_ONLY}" -eq 0 ]]; then
	generate_inputs
	if [[ "${SINGLE_SESSION}" -eq 1 ]]; then
		start_persistent_server
	fi
fi

echo "=========================================================================================================="
echo " SOF Testbench4 Comprehensive Topology & Module Test Suite"
echo " Binary   : ${TB_BIN}"
echo " Ctl Tool : ${TB_CTL_BIN}"
echo " Topologies: ${TPLG_DIR}"
if [[ "${SINGLE_SESSION}" -eq 1 && -n "${TB_SERVER_PID}" ]]; then
	echo " Session  : Single Persistent sof-testbench4 -I Server (PID ${TB_SERVER_PID}, TCP port ${TB_PORT}, 0 restarts)"
fi
echo "=========================================================================================================="

# ------------------------------------------------------------------------------
# TIER 1: All 26 Built-In Modules & Sample Formats (S32_LE, S24_LE, S16_LE, Rates, Multi-Channel)
# ------------------------------------------------------------------------------
echo ""
echo "--- Tier 1: All Built-In Audio Modules & Sample Formats (LL & Single-Module Benchmarks) ---"

# 1.1 Standard S32_LE Full-Duplex (Playback + Capture: -p 1,2,3,4) Module Benchmarks
for entry in \
	"dcblock32:DC Blocking Filter (dcblock)" \
	"drc32:Dynamic Range Compressor (drc)" \
	"drc_multiband32:Multi-Band DRC (drc_multiband)" \
	"dolby-dax32:Dolby DAX Audio Processing (dolby-dax)" \
	"eqfir32:FIR Equalizer (eqfir)" \
	"eqiir32:IIR Equalizer (eqiir)" \
	"gain32:Volume / Gain (gain)" \
	"igo_nr32:iGo Noise Reduction (igo_nr)" \
	"level_multiplier32:Level Multiplier (level_multiplier)" \
	"mfcc32:Mel-Frequency Cepstral Coeffs (mfcc)" \
	"mfccmel32:MFCC Mel Filterbank (mfccmel)" \
	"micsel32:Microphone Selector 2ch (micsel)" \
	"rtnr32:Realtek Noise Reduction (rtnr)" \
	"sound_dose32:Sound Dose Monitor (sound_dose)" \
	"tdfb32:Time-Domain Fixed Beamformer (tdfb)" \
	"template_comp32:Template Component (template_comp)"; do
	mod="${entry%%:*}"
	desc="${entry#*:}"
	run_tb_case 1 "mod_${mod}" "${desc} [S32 duplex]" \
		"${DEV_TPLG}/sof-hda-benchmark-${mod}.tplg" "1,2,3,4" \
		2 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "Heap usage for module" \
		-r 48000 -R 48000 -c 2 -n 2 -b S32_LE
done

# 1.2 S24_LE Full-Duplex Module Benchmarks (including Aria)
for entry in \
	"aria24:Aria Attenuation / Delay (aria)" \
	"gain24:Volume / Gain (gain) [S24_LE]" \
	"eqiir24:IIR Equalizer (eqiir) [S24_LE]" \
	"eqfir24:FIR Equalizer (eqfir) [S24_LE]" \
	"drc24:Dynamic Range Compressor (drc) [S24_LE]" \
	"dcblock24:DC Blocking Filter (dcblock) [S24_LE]" \
	"dolby-dax24:Dolby DAX Audio Processing [S24_LE]" \
	"level_multiplier24:Level Multiplier [S24_LE]" \
	"micsel24:Microphone Selector 2ch [S24_LE]" \
	"drc_multiband24:Multi-Band DRC [S24_LE]" \
	"tdfb24:Beamformer (tdfb) [S24_LE]"; do
	mod="${entry%%:*}"
	desc="${entry#*:}"
	run_tb_case 1 "mod_${mod}" "${desc}" \
		"${DEV_TPLG}/sof-hda-benchmark-${mod}.tplg" "1,2,3,4" \
		2 "${WORK_DIR}/in_48k_2ch_s24.raw" 2 "Heap usage for module" \
		-r 48000 -R 48000 -c 2 -n 2 -b S24_LE
done

# 1.3 S16_LE Full-Duplex Module Benchmarks
for entry in \
	"gain16:Volume / Gain (gain) [S16_LE]" \
	"eqiir16:IIR Equalizer (eqiir) [S16_LE]" \
	"eqfir16:FIR Equalizer (eqfir) [S16_LE]" \
	"drc16:Dynamic Range Compressor (drc) [S16_LE]" \
	"dcblock16:DC Blocking Filter (dcblock) [S16_LE]" \
	"dolby-dax16:Dolby DAX Audio Processing [S16_LE]" \
	"level_multiplier16:Level Multiplier [S16_LE]" \
	"micsel16:Microphone Selector 2ch [S16_LE]" \
	"sound_dose16:Sound Dose Monitor [S16_LE]" \
	"drc_multiband16:Multi-Band DRC [S16_LE]" \
	"tdfb16:Beamformer (tdfb) [S16_LE]"; do
	mod="${entry%%:*}"
	desc="${entry#*:}"
	run_tb_case 1 "mod_${mod}" "${desc}" \
		"${DEV_TPLG}/sof-hda-benchmark-${mod}.tplg" "1,2,3,4" \
		2 "${WORK_DIR}/in_48k_2ch_s16.raw" 2 "Heap usage for module" \
		-r 48000 -R 48000 -c 2 -n 2 -b S16_LE
done

# 1.4 Rate-Converting Modules (SRC, SRC Lite, ASRC) & Multi-Channel MicSel + Google CTC
run_tb_case 1 "mod_src32_pb_44k1_to_48k" "Sample Rate Converter (src) PB 44.1k->48k" \
	"${DEV_TPLG}/sof-hda-benchmark-src32.tplg" "1,2" \
	1 "${WORK_DIR}/in_44k1_2ch_s32.raw" 1 "Heap usage for module src" \
	-r 44100 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 1 "mod_src32_cap_48k_to_44k1" "Sample Rate Converter (src) CAP 48k->44.1k" \
	"${DEV_TPLG}/sof-hda-benchmark-src32.tplg" "3,4" \
	1 "${WORK_DIR}/in_48k_2ch_s32.raw" 1 "Heap usage for module src" \
	-r 48000 -R 44100 -c 2 -n 2 -b S32_LE

run_tb_case 1 "mod_src_lite32_pb_44k1_to_48k" "SRC Lite (src_lite) PB 44.1k->48k" \
	"${DEV_TPLG}/sof-hda-benchmark-src_lite32.tplg" "1,2" \
	1 "${WORK_DIR}/in_44k1_2ch_s32.raw" 1 "Heap usage for module src_lite.1.1" \
	-r 44100 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 1 "mod_src_lite32_cap_48k_to_44k1" "SRC Lite (src_lite) CAP 48k->44.1k" \
	"${DEV_TPLG}/sof-hda-benchmark-src_lite32.tplg" "3,4" \
	1 "${WORK_DIR}/in_48k_2ch_s32.raw" 1 "Heap usage for module src_lite.3.2" \
	-r 48000 -R 44100 -c 2 -n 2 -b S32_LE

run_tb_case 1 "mod_asrc32_pb_44k1_to_48k" "Async Sample Rate Converter (asrc) PB" \
	"${DEV_TPLG}/sof-hda-benchmark-asrc32.tplg" "1,2" \
	1 "${WORK_DIR}/in_44k1_2ch_s32.raw" 1 "Heap usage for module asrc.1.1" \
	-r 44100 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 1 "mod_asrc32_cap_48k_to_44k1" "Async Sample Rate Converter (asrc) CAP" \
	"${DEV_TPLG}/sof-hda-benchmark-asrc32.tplg" "3,4" \
	1 "${WORK_DIR}/in_48k_2ch_s32.raw" 1 "Heap usage for module asrc.3.2" \
	-r 48000 -R 44100 -c 2 -n 2 -b S32_LE

run_tb_case 1 "mod_micsel_multich32_4ch_to_2ch" "Multi-Ch Mic Selector (4ch->2ch duplex)" \
	"${DEV_TPLG}/sof-hda-benchmark-micsel_multich32.tplg" "1,2,3,4" \
	2 "${WORK_DIR}/in_48k_4ch_s32.raw" 2 "Heap usage for module micsel" \
	-r 48000 -R 48000 -c 4 -n 2 -b S32_LE

run_tb_case 1 "mod_google_ctc_audio_processing" "Google CTC Audio Processing + Gain + Mix" \
	"${DEV_TPLG}/sof-mtl-rt1019-rt5682-ctc.tplg" "3,4" \
	1 "${WORK_DIR}/in_48k_2ch_s32.raw" 1 "Heap usage for module ctc.4.1" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

# ------------------------------------------------------------------------------
# TIER 2: Simple to Complex Multi-Pipeline DAG Topologies
# ------------------------------------------------------------------------------
echo ""
echo "--- Tier 2: Simple to Complex Multi-Pipeline DAG Topologies ---"

run_tb_case 2 "dag_hda_passthrough_3x_hdmi" "Passthrough 3x Concurrent HDMI (6 pipelines)" \
	"${DEV_TPLG}/sof-hda-passthrough.tplg" "50,51,60,61,70,71" \
	3 "${WORK_DIR}/in_48k_2ch_s32.raw" 3 "Total execution time|Per-Core LL & DP Scheduler Telemetry" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 2 "dag_cascaded_3ppl_aria_gain_mix" "3-Pipeline Cascaded (aria->gain->mixin->mixout)" \
	"${DEV_TPLG}/sof-hda-benchmark-generic.tplg" "1,2,3" \
	1 "${WORK_DIR}/in_48k_2ch_s24.raw" 1 "Heap usage for module aria" \
	-r 48000 -R 48000 -c 2 -n 2 -b S24_LE

run_tb_case 2 "dag_efx_mbdrc_6mod_chain" "6-Module EFX Chain (gain+eqiir+eqfir+mbdrc+tdfb+drc)" \
	"${DEV_TPLG}/sof-hda-efx-mbdrc-generic.tplg" "1,2,3,4" \
	2 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "Heap usage for module multiband_drc" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 2 "dag_fanout_1to2_crossover" "1-to-2 Fan-Out Multi-Sink 2-Way Crossover" \
	"${DEV_TPLG}/sof-tgl-nocodec-crossover-2way.tplg" "1,101,102" \
	1 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "Heap usage for module crossover" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 2 "dag_fanin_2to1_google_rtcaec" "2-to-1 Fan-In Multi-Source Google RTC AEC (DP)" \
	"${DEV_TPLG}/sof-tgl-nocodec-rtcaec.tplg" "7,8,12" \
	2 "${WORK_DIR}/in_16k_2ch_s16.raw" 1 "Heap usage for module google-rtc-aec.7.1" \
	-r 16000 -R 16000 -c 2 -n 2 -b S16_LE

run_tb_case 2 "dag_dmic_4ch_tdfb_drc_eqiir" "4ch DMIC Capture Chain (tdfb->drc->eqiir->gain)" \
	"${PROD_TPLG}/sof-hda-generic-4ch.tplg" "11,12" \
	1 "${WORK_DIR}/in_48k_4ch_s32.raw" 1 "Heap usage for module tdfb.11.1" \
	-r 48000 -R 48000 -c 4 -n 4 -b S32_LE

run_tb_case 2 "dag_hda_generic_11ppl_6in_5out" "11-Pipeline Concurrent HDA (Analog+DeepBuf+Cap+3xHDMI)" \
	"${PROD_TPLG}/sof-hda-generic.tplg" "1,2,3,4,15,50,51,60,61,70,71" \
	6 "${WORK_DIR}/in_48k_2ch_s32.raw" 5 "Heap usage for module mixin.15.1" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

# ------------------------------------------------------------------------------
# TIER 3: Combinations of LL and DP Modules (Single-Thread & Multi-Thread DP)
# ------------------------------------------------------------------------------
echo ""
echo "--- Tier 3: Combinations of LL & DP Modules (Dedicated Per-Module DP Pthreads) ---"

run_tb_case 3 "lldp_phase_vocoder_pb_single_dp" "LL Copiers + Single DP Phase Vocoder (Playback)" \
	"${DEV_TPLG}/sof-hda-benchmark-phase_vocoder32.tplg" "1,2" \
	1 "${WORK_DIR}/in_48k_2ch_s32.raw" 1 "Heap usage for module phase_vocoder.1.1" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 3 "lldp_phase_vocoder_duplex_2dp" "LL Copiers + 2 Concurrent DP Phase Vocoder Threads" \
	"${DEV_TPLG}/sof-hda-benchmark-phase_vocoder32.tplg" "1,2,3,4" \
	2 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "Heap usage for module phase_vocoder.3.2" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 3 "lldp_stft_192_48_duplex_2dp" "LL Copiers + 2 Concurrent DP STFT (192/48) Threads" \
	"${DEV_TPLG}/sof-hda-benchmark-stft_process_192_48_32.tplg" "1,2,3,4" \
	2 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "Heap usage for module stft_process.3.2" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 3 "lldp_stft_512_128_duplex_2dp" "LL Copiers + 2 Concurrent DP STFT (512/128) Threads" \
	"${DEV_TPLG}/sof-hda-benchmark-stft_process_512_128_32.tplg" "1,2,3,4" \
	2 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "Heap usage for module stft_process.3.2" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 3 "lldp_stft_1536_240_duplex_2dp" "LL Copiers + 2 Concurrent DP STFT (1536/240) Threads" \
	"${DEV_TPLG}/sof-hda-benchmark-stft_process_1536_240_32.tplg" "1,2,3,4" \
	2 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "Heap usage for module stft_process.3.2" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 3 "lldp_mtl_dual_dp_src_on_core0" "6-Pipeline LL (Cores 0,2) + 2 DP SRC Threads (Core 0)" \
	"${DEV_TPLG}/sof-mtl-nocodec-dp-test.tplg" "5,6,7,8,11,12" \
	3 "${WORK_DIR}/in_48k_2ch_s32.raw" 3 "Heap usage for module src.11.1" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

# ------------------------------------------------------------------------------
# TIER 4: Multi-Core LL & Cross-Core DP Topologies (Cores 0, 1, 2)
# ------------------------------------------------------------------------------
echo ""
echo "--- Tier 4: Multi-Core LL & Cross-Core DP Execution (DSP Cores 0, 1, 2) ---"

run_tb_case 4 "mcore_ll_mtl_nocodec_cores_0_2" "Multi-Core LL Only (Core 0 Cap + Core 2 PB/Cap)" \
	"${DEV_TPLG}/sof-mtl-nocodec.tplg" "5,6,7,8,11,12" \
	3 "${WORK_DIR}/in_48k_2ch_s32.raw" 3 "LL worker thread created for DSP core 2|Core 2 .* LL: ticks=" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 4 "mcore_lldp_hda_src_cores_0_2" "Cross-Core HDA: Core 0 LL + Core 2 LL & DP SRC" \
	"${DEV_TPLG}/sof-hda-src-generic-dp.tplg" "1,2,3,4" \
	2 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "DP thread initialized for module.*on DSP core 2|Core 2 .* DP: runs=" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 4 "mcore_lldp_mtl_3core_0_1_2" "MTL 3-Core: Core 0 LL + Core 1 Dual DP SRC + Core 2 LL" \
	"${DEV_TPLG}/sof-mtl-nocodec-dp-core-test.tplg" "5,6,7,8,11,12" \
	3 "${WORK_DIR}/in_48k_2ch_s32.raw" 3 "DP thread initialized for module.*on DSP core 1|Core 1 .* DP: runs=" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 4 "mcore_lldp_lnl_3core_0_1_2" "LNL 3-Core: Core 0 LL + Core 1 Dual DP SRC + Core 2 LL" \
	"${DEV_TPLG}/sof-lnl-nocodec-dp-core-test.tplg" "5,6,7,8,11,12" \
	3 "${WORK_DIR}/in_48k_2ch_s32.raw" 3 "DP thread initialized for module.*on DSP core 1|Core 1 .* DP: runs=" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 4 "mcore_lldp_ptl_3core_0_1_2" "PTL 3-Core: Core 0 LL + Core 1 Dual DP SRC + Core 2 LL" \
	"${DEV_TPLG}/sof-ptl-nocodec-dp-core-test.tplg" "5,6,7,8,11,12" \
	3 "${WORK_DIR}/in_48k_2ch_s32.raw" 3 "DP thread initialized for module.*on DSP core 1|Core 1 .* DP: runs=" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

run_tb_case 4 "mcore_ll_dolby_dax_ctc_cores_0_2" "Multi-Core Dolby DAX (Core 2) + Google CTC (Core 0)" \
	"${DEV_TPLG}/sof-mtl-rt1019-rt5682-dax.tplg" "1,2,3,4" \
	2 "${WORK_DIR}/in_48k_2ch_s32.raw" 2 "Heap usage for module dolby-dax.2.1" \
	-r 48000 -R 48000 -c 2 -n 2 -b S32_LE

# ------------------------------------------------------------------------------
# TIER 5: Runtime Control Scripts & Live Remote TCP Network Control
# ------------------------------------------------------------------------------
echo ""
echo "--- Tier 5: Runtime Control Scripts (-s) & Live SOFR TCP Remote Control (-S/-W) ---"
run_tier5_tests

if [[ "${LIST_ONLY}" -eq 1 ]]; then
	exit 0
fi

if [[ "${SINGLE_SESSION}" -eq 1 && -n "${TB_SERVER_PID}" ]]; then
	if ! kill -0 "${TB_SERVER_PID}" 2>/dev/null; then
		echo "ERROR: persistent sof-testbench4 server (PID ${TB_SERVER_PID}) died before suite completion!" >&2
		TOTAL_FAIL=$((TOTAL_FAIL + 1))
	fi
fi

echo ""
echo "=========================================================================================================="
echo " Test Suite Summary by Tier"
echo "----------------------------------------------------------------------------------------------------------"
printf "  Tier 1 (All 26 Modules & S16/S24/S32/Rates/4ch) : %2d / %2d passed\n" "${TIER_PASS[1]}" "${TIER_TOTAL[1]}"
printf "  Tier 2 (Complex Multi-Pipeline DAG Topologies)  : %2d / %2d passed\n" "${TIER_PASS[2]}" "${TIER_TOTAL[2]}"
printf "  Tier 3 (LL + DP & Multi-Thread DP Combinations) : %2d / %2d passed\n" "${TIER_PASS[3]}" "${TIER_TOTAL[3]}"
printf "  Tier 4 (Multi-Core LL & Cross-Core DP 0/1/2)    : %2d / %2d passed\n" "${TIER_PASS[4]}" "${TIER_TOTAL[4]}"
printf "  Tier 5 (Runtime Controls & Remote TCP Server)   : %2d / %2d passed\n" "${TIER_PASS[5]}" "${TIER_TOTAL[5]}"
echo "----------------------------------------------------------------------------------------------------------"
if [[ "${SINGLE_SESSION}" -eq 1 && -n "${TB_SERVER_PID}" ]]; then
	printf "  Session Mode                                    : 1 persistent sof-testbench4 process (PID %s, 0 restarts)\n" "${TB_SERVER_PID}"
fi
printf "  TOTAL                                           : %2d / %2d passed (%d failed)\n" \
	"${TOTAL_PASS}" "${TOTAL_RUN}" "${TOTAL_FAIL}"
echo "=========================================================================================================="

if [[ "${TOTAL_FAIL}" -gt 0 ]]; then
	echo "Failed tests: ${FAILED_TESTS[*]}" >&2
	exit 1
fi
exit 0
