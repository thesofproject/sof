# SOF testbench

### Features

 * Simulate IPC3 and IPC4 SOF versions with run of set of processing
   components based on desired topology.
 * Supports arbitrary multi-pipeline DAG topologies, including linear chains,
   1-to-N fan-out (e.g., crossover), N-to-1 fan-in (e.g., AEC reference + mic,
   deepbuffer + main playback mixing), diamond graphs, `module-copier`
   (`SND_SOC_TPLG_DAPM_BUFFER`), and concurrent multi-PCM playback and capture.
 * Multi-core and multi-threaded execution on host CPUs (`pthread` +
   `pthread_setaffinity_np`) honoring topology `core` / `core_id` assignments
   for both Low Latency (LL) pipelines and Data Processing (DP) modules.
 * Replaces host and dai components with file I/O with raw binary
   S16_LE/S24_LE/S32_LE or text format files for audio waveforms.
 * Much faster than real-time execution in native build, e.g. x86 on
   Linux for efficient validation usage.
 * With xtensa DSP build offers cycles accurate simulated environment
   execution. And also with options for simulation speed vs. model
   accuracy.
 * Allows easy use of conventional debugger, profiler, leak and memory check
   tools usage for DSP firmware code.

### Architecture

#### Multi-Pipeline Graphs & Multi-Pin Binding

`sof-testbench4` parses the full topology widget, route, and PCM lists and constructs the active subgraph selected via `-p <pipeline1,pipeline2,...>`:

 * **Multi-Source / Multi-Sink Discovery**: Automatically discovers all active `AIF_IN` (playback host), `AIF_OUT` (capture host), `DAI_IN` (playback DAI), and `DAI_OUT` (capture DAI) endpoints across enabled pipelines and binds them in order to the comma-separated `-i` input files and `-o` output files.
 * **Pin Binding & Sink Formats**: Parses `SOF_TKN_COMP_INPUT_PIN_BINDING_WNAME` and `SOF_TKN_COMP_OUTPUT_PIN_BINDING_WNAME` tuples to assign exact source and destination queue IDs (`src_queue_id`, `dst_queue_id`) for multi-pin modules (such as `crossover`, `mixin`/`mixout`, `google-rtc-audio-processing`, and multi-sink `copier`s), and sends `SOF_IPC4_COPIER_MODULE_CFG_PARAM_SET_SINK_FORMAT` IPCs for non-zero output pins.
 * **DAG Traversal**: Prepares, sets up, binds, triggers, and tears down branching and merging pipeline graphs without duplicate widget initialization or double unbinding.

#### Multi-Core LL & DP Thread Scheduling

`sof-testbench4` supports up to `CONFIG_CORE_COUNT=8` virtual DSP cores (`CONFIG_CROSS_CORE_STREAM=y`) mapped onto host CPU cores via `sched_getaffinity` and `pthread_setaffinity_np`:

 * **Topology Core Assignment**:
   * Pipelines (`SND_SOC_TPLG_DAPM_SCHEDULER`) use `SOF_TKN_SCHED_CORE` (`core`) and `SOF_TKN_SCHED_PRIORITY` (`priority`), propagated via `SOF_IPC4_GLB_CREATE_PIPELINE`.
   * Widgets use `SOF_TKN_COMP_CORE_ID` (`core_id`) and `SOF_TKN_COMP_SCHED_DOMAIN` (`LL` vs. `DP`), propagated via `SOF_IPC4_MOD_INIT_INSTANCE`. LL widgets inherit their parent pipeline's `core_id`, while DP widgets can reside on any DSP core independent of their parent LL pipeline.
 * **Per-Core LL Worker Threads (`ll_schedule_pthread.c`)**:
   * Each active DSP core `c` (`0 .. CONFIG_CORE_COUNT - 1`) spawns a dedicated persistent `pthread` worker pinned to host CPU `c % num_allowed_host_cpus` with thread-local `cpu_get_id() == c`.
   * On each 1 ms tick (`schedule_ll_run_tasks()`), all active LL core worker threads run their core's queued LL pipeline tasks concurrently in parallel across host CPU cores and synchronize at the end of the LL cycle before triggering the DP scheduler.
 * **Per-Module DP Worker Threads (`dp_schedule_pthread.c`)**:
   * Each Data Processing (DP) module (`COMP_PROCESSING_DOMAIN_DP`) runs in its own dedicated `pthread` pinned to the host CPU core corresponding to the module's topology `core_id`.
   * Cross-core and same-core LL-to-DP and DP-to-LL connections communicate through lock-free single-producer single-consumer (SPSC) shared `ring_buffer` instances.
 * **Xtensa Simulator & Single-Threaded C Library Partitioning (`ll_schedule.c` / `dp_schedule.c`)**:
   * When built for the Xtensa simulator (`xt-run` via `scripts/rebuild-testbench.sh -p <platform>`, or manually with `-DTESTBENCH_XTENSA_SIM=ON`), CMake automatically selects the pure standard-C cooperative schedulers (`ll_schedule.c` and `dp_schedule.c`) instead of `ll_schedule_pthread.c` and `dp_schedule_pthread.c`.
   * This eliminates dependencies on `<pthread.h>`, `-lpthread`, `<sched.h>`, and `clock_gettime` that are unavailable in the Xtensa newlib (`-mlsp=sim`) runtime, while executing DP modules synchronously inside `scheduler_dp_ll_tick()` so their instruction cycles are included in `XT_RSR_CCOUNT()` profiling.

#### Per-Core, Per-Pipeline, and Per-Module Scheduling & Buffer Entry/Exit Telemetry (`telemetry_sched.c`)

`sof-testbench4` instruments every DSP core's LL and DP schedulers, every pipeline walk (`pipeline_copy()`), and every LL (`comp_copy()`) and DP (`dp_task_run()`) module execution:

 * **Per-Core LL & DP Scheduler Telemetry**: Tracks LL tick counts, active LL tasks, LL average/peak/total DSP cycles and KCPS, DP thread run counts, active DP threads, DP average/peak/total DSP cycles and KCPS, and combined core utilization (`total_util_x100`) on every DSP core (`0 .. CONFIG_CORE_COUNT - 1`).
 * **Per-Pipeline Telemetry**: Tracks copy iterations, priority, period, core assignment, LL and DP module counts, average/peak/total cycles, KCPS, and XRUN counts for each active pipeline.
 * **Per-Module Scheduling & Buffer Entry/Exit Telemetry**:
   * Tracks `iterations`, `last/avg/peak/total_cycles`, `avg/peak_kcps`, `ibs`, `obs`, `cpc` budget and `cpc_violations`, plus LL ring-buffer sync iterations and cycles for DP modules.
   * On **entry** (before processing) and **exit** (after processing) of every module's `copy()` / `dp_task_run()` call, captures for each input (producer) and output (consumer) pin:
     * Primary LL `comp_buffer` fill levels (`entry_avail_bytes`, `entry_free_bytes`, `exit_avail_bytes`, `exit_free_bytes`) and secondary DP `ring_buffer` fill levels (`dp_ring_size`, `dp_ring_entry_avail`, `dp_ring_exit_avail`).
     * Exact bytes consumed/produced (`last_bytes_transferred`, `total_bytes_transferred`).
     * Signal levels in `0.01 dBFS` (`entry_rms_dbfs_x100`, `entry_peak_dbfs_x100`, `exit_rms_dbfs_x100`, `exit_peak_dbfs_x100`) and rolling S16-normalized sample taps (`entry_samples[64]`, `exit_samples[64]`).
 * **CLI & Remote Exposure**: Printed automatically at the end of every testbench run, exposed in SRAM Windows 2 & 3 and `SOF_NET_OP_TELEM_SNAPSHOT`, and queryable live via `sof-remote shell "sched-telem"` and `sof-remote shell "mod-buffers [comp_id|name]"`.
 * **Accurate CPC, Core Frequency & SRAM/DRAM Workload Execution Time Simulation**:
   * Enabled via `-w <0|1|2|off|virtual|realtime>`, `-F <freq_mhz | core:freq_mhz,...>`, and `-M <sram|dram[:penalty_x100[:cycles_per_word]]>` or dynamically over TCP via `SOF_NET_OP_WORKLOAD_SIM_CFG` (`0x0029`) and `sof-remote shell "workload-sim ..."`.
   * Computes module cycle consumption from each module's topology `SOF_TKN_COMP_CPC` (`base_cfg.cpc`, per-module override, or calibrated module-type CPC), scales cycles by memory residency (`SRAM` 1.00x vs `DRAM` cache/bus stall multiplier + per-word transfer penalty), and computes execution time ($T_{\text{exec}} = \text{cycles} / f_{\text{core}}$), core utilization %, and 1 ms LL tick deadline overruns against each core's target clock frequency.
   * Supports both fast virtual cycle/time accounting (`-w 1` / `virtual`) and real-time wall-clock pacing (`-w 2` / `realtime`).

#### Network Remote Control & Telemetry Interface (`remote_server.c`)

`sof-testbench4` exposes the `sof-remote` TCP binary framing protocol (`SOF_NET_MAGIC = 0x534F4652` `'SOFR'`, `SOF_NET_VERSION = 1`, default TCP port `9998`) via `-S <port>` and `-W`:

 * **Supported Opcodes**:
   * `SOF_NET_OP_PING`, `SOF_NET_OP_GET_INFO`, `SOF_NET_OP_PM_CTL` (`query`, `release`/D3 pause, `wake`/D0 resume)
   * `SOF_NET_OP_IPC_TX` (direct IPC4 message dispatch) and `SOF_NET_OP_IPC_LARGE_CFG` (BaseFW params 9, 12, 14, 15, 17, 18, 30 and live module `LARGE_CONFIG_GET`/`SET`)
   * `SOF_NET_OP_WIN_INFO`, `SOF_NET_OP_WIN_READ`, `SOF_NET_OP_WIN_WRITE`, `SOF_NET_OP_WINSTREAM_IO` (emulated SRAM Windows 0–3 and debug slots)
   * `SOF_NET_OP_TELEM_SNAPSHOT` (single-RPC composite telemetry snapshot consumed by `tools/softop/softop.py` and `sof-remote telem`)
   * `SOF_NET_OP_WORKLOAD_SIM_CFG` (`0x0029`, query/configure CPC workload execution simulation mode, SRAM/DRAM residency, DRAM stall multiplier, per-core target clock frequencies, and per-module CPC/memory overrides via `sof-remote workload-sim ...`)
   * `SOF_NET_OP_SHELL_EXEC` (`info`, `pipelines`, `modules`, `tplg-load <file> [p1,p2,...]`, `ppl-create`, `ppl-state`, `ppl-delete`, `mod-bind`, `mod-unbind`, `mod-delete`, `controls`, live `amixer cset ...`, live `sof-ctl ...`, `stats`, `heap`, `sched-telem`, `sched-telem-reset`, `workload-sim [status|enable|disable|freq|mem|mod|reset]`, `mod-buffers [id|name]`, `kernel cycles`, `kernel threads`, `pause`, `resume`, `step [N]`, `loglevel <0..4>`, `quit`)
   * `SOF_NET_OP_LLEXT_LOAD` (upload and load dynamic `.so` / `.llext` module images via `dlopen` / LLEXT table registration)
   * `SOF_NET_OP_PROBE_CTL` (enumerate and read inter-component `struct comp_buffer` audio streams as probe points)
   * `SOF_NET_OP_PCM_CTL` (`start-tone`, `stop-tone`, `status`, and `capture` from output streams)
 * **Tick-Boundary Synchronization**: All network RPCs that inspect or mutate IPC/module/topology state acquire `g_tb_lock` between 1 ms LL ticks (`tb_remote_tick_begin()` / `tb_remote_tick_end()`), guaranteeing thread safety across multi-core LL and DP worker threads.

### Command-Line Options

```
Usage: sof-testbench4 <options> -i <input_file1,input_file2,...> -o <output_file1,output_file2,...>

Options for processing:
  -t <topology file>
  -i <input_file1,input_file2,...>  (up to 16 comma-separated input files)
  -o <output_file1,output_file2,...> (up to 16 comma-separated output files)

Options to control test:
  -d <level> Sets the traces print level:
     0 all traces are suppressed
     1 shows error traces
     2 shows warning traces and previous
     3 shows info traces and previous
     4 shows debug traces and previous, plus other testbench debug messages
  -p <pipeline1,pipeline2,...> (up to 16 comma-separated pipeline IDs, default: 1)
  -C <number of copy() iterations>
  -P <number of dynamic pipeline iterations>
  -s <script file to set controls, with amixer and sleep commands>
  -S <port> Start sof-remote TCP server on <port> (default port: 9998)
  -W Keep testbench running continuously for remote network control
  -w <0|1|2|off|virtual|realtime> Enable CPC workload execution time simulation
  -F <freq_mhz | core:freq_mhz,...> Set target DSP core clock frequency in MHz
  -M <sram|dram[:penalty_x100[:cycles_per_word]]> Set workload memory domain

Options for input and output format override:
  -b <input_format>, S16_LE, S24_LE, or S32_LE
  -c <input channels>
  -n <output channels>
  -r <input rate>
  -R <output rate>

Help:
  -h
```

### Prerequisites

Before running the SOF testbench, install the required dependencies:

```
sudo apt install valgrind octave-signal octave # For Ubuntu/Debian
sudo dnf install valgrind octave-signal octave # For Fedora
```

### Quick how-to

The simplest way to build and execute testbench is with supplied
scripts. It executes a simple chirp waveform test for a number of
processing components.  Corrupted audio or failure in execution
or a memory violation results to fail of test.

```
cd $SOF_WORKSPACE/sof
scripts/build-tools.sh
scripts/rebuild-testbench.sh
scripts/host-testbench.sh
```

### Manual run of IPC4 testbench

As an example, process a wav file and listen it. The example
processing component is IIR equalizer. The wav file is first converted
with sox to raw 32 bit 48 kHz stereo format, then processed by
testbench, converted back to wav.

The example processes the playback direction of topology
sof-hda-benchmark-eqiir32.tplg with IIR component. The playback
direction in the component test topologies consists of host pipeline 1
and dai pipeline 2. Therefore the command line has option -p 1,2.

```
cd $SOF_WORKSPACE/sof
sox --encoding signed-integer /usr/share/sounds/alsa/Front_Left.wav -L -r 48000 -c 2 -b 32 in.raw
tools/testbench/build_testbench/install/bin/sof-testbench4 -r 48000 -c 2 -b S32_LE \
  -p 1,2 -t tools/build_tools/topology/topology2/development/sof-hda-benchmark-eqiir32.tplg \
  -i in.raw -o out.raw
sox --encoding signed-integer -L -r 48000 -c 2 -b 32 out.raw out.wav
aplay out.wav
```

To test capture use -p 3,4 with same topologies. To run both
directions, use -p 1,2,3,4 and provide multiple input and output
files separated with comma. Use e.g. -i i1.raw,i2.raw
-o o1.raw,o2.raw.

### Multi-Pipeline, DP, and Multi-Core Examples

```
# 1-to-2 Fan-Out (2-way Crossover across pipelines 1, 101, 102)
tools/testbench/build_testbench/install/bin/sof-testbench4 -r 48000 -R 48000 -c 2 -n 2 -b S32_LE \
  -t tools/build_tools/topology/topology2/development/sof-tgl-nocodec-crossover-2way.tplg \
  -p 1,101,102 -i in.raw -o out_sink1.raw,out_sink2.raw

# 2-to-1 Fan-In (Google RTC AEC capture + playback reference across pipelines 7, 8, 12)
tools/testbench/build_testbench/install/bin/sof-testbench4 -r 16000 -R 16000 -c 2 -n 2 -b S16_LE \
  -t tools/build_tools/topology/topology2/development/sof-tgl-nocodec-rtcaec.tplg \
  -p 7,8,12 -i mic_in.raw,ref_in.raw -o aec_out.raw

# Multiple DP Modules in Dedicated Threads (Playback + Capture Phase Vocoder)
tools/testbench/build_testbench/install/bin/sof-testbench4 -r 48000 -R 48000 -c 2 -n 2 -b S32_LE \
  -t tools/build_tools/topology/topology2/development/sof-hda-benchmark-phase_vocoder32.tplg \
  -p 1,2,3,4 -i pb_in.raw,cap_in.raw -o pb_out.raw,cap_out.raw

# Multi-Core LL & Cross-Core DP Execution (Core 0 Capture LL, Core 1 DP SRC threads, Core 2 Playback/Capture LL)
tools/testbench/build_testbench/install/bin/sof-testbench4 -r 48000 -R 48000 -c 2 -n 2 -b S32_LE \
  -t tools/build_tools/topology/topology2/development/sof-mtl-nocodec-dp-core-test.tplg \
  -p 5,6,7,8,11,12 -i pb_in.raw,cap1_in.raw,cap2_in.raw -o pb_out.raw,cap1_out.raw,cap2_out.raw

# Interactive Remote Server Mode (connect with sof-remote or tools/softop/softop.py)
tools/testbench/build_testbench/sof-testbench4 \
  -t tools/build_tools/topology/topology2/target/sof-ipc4-tplg/sof-hda-generic.tplg \
  -p 1,2 -S 9998 -W

# In another terminal, query and control testbench over TCP:
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 info
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 telem
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 cards
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 pcm-caps
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 dapm /tmp/tb_dapm.dot
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 compr caps
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 kctl list
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 kctl get 1
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 kctl set 1 28 28
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 shell "pipelines"
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 shell "sched-telem"
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 shell "mod-buffers"
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 shell "controls"
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 shell "amixer cset name=\"Pre Mixer Analog Playback Volume\" 20,20"
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 pcm start-tone hw:0,0 1000
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 pcm capture hw:0,0 100
python3 tools/softop/softop.py --remote 127.0.0.1:9998
tools/sof-remote-server/sof-remote -H 127.0.0.1 -p 9998 shell "quit"
```

### Comprehensive Topology & Module Test Suite

`tools/testbench/run_test_suite.sh` executes 67 automated test cases across 5 progressive tiers:

 * **Tier 1 (46 tests)**: All 26 built-in SOF audio processing modules (`dcblock`, `drc`, `drc_multiband`, `dolby-dax`, `eqfir`, `eqiir`, `gain`, `igo_nr`, `level_multiplier`, `mfcc`, `mfccmel`, `micsel`, `micsel_multich`, `rtnr`, `sound_dose`, `tdfb`, `template_comp`, `aria`, `src`, `src_lite`, `asrc`, `crossover`, `google_ctc_audio_processing`, `google_rtc_audio_processing`, `phase_vocoder`, `stft_process`, `mixin`/`mixout`, `host-copier`/`dai-copier`/`module-copier`) across `S32_LE`, `S24_LE`, `S16_LE`, rate conversion (`44.1 kHz <-> 48 kHz`), and multi-channel (`4ch -> 2ch`).
 * **Tier 2 (7 tests)**: Simple to complex multi-pipeline DAG topologies (3x HDMI passthrough, 3-pipeline cascaded `aria->gain->mixin->mixout`, 6-module EFX chain, 1-to-2 fan-out crossover, 2-to-1 fan-in Google RTC AEC, 4ch DMIC `tdfb->drc->eqiir->gain`, and 11-pipeline 6-input/5-output concurrent `sof-hda-generic.tplg`).
 * **Tier 3 (6 tests)**: Combinations of Low-Latency (`LL`) and Data Processing (`DP`) modules (single-thread DP `phase_vocoder`, full-duplex dual-thread DP `phase_vocoder` and `stft_process` `192/48`, `512/128`, `1536/240`, and 6-pipeline LL + 2 DP `src` threads).
 * **Tier 4 (6 tests)**: Multi-Core LL and Cross-Core DP execution across DSP Cores 0, 1, and 2 (`sof-mtl-nocodec.tplg`, `sof-hda-src-generic-dp.tplg`, `sof-mtl-nocodec-dp-core-test.tplg`, `sof-lnl-nocodec-dp-core-test.tplg`, `sof-ptl-nocodec-dp-core-test.tplg`, and `sof-mtl-rt1019-rt5682-dax.tplg`).
 * **Tier 5 (2 tests)**: Runtime control scripts (`-s` with `amixer` and `sof-ctl`) and live `SOFR` v1 TCP remote control & 3-core telemetry inspection (`-S <port> -W` with `sof-remote`).

```
# Run the complete 67-test suite
tools/testbench/run_test_suite.sh

# Run a single tier (1..5) or filter by regex
tools/testbench/run_test_suite.sh --tier 4
tools/testbench/run_test_suite.sh --filter "mcore|fanout|fanin"

# Or run via CTest from the testbench build directory
ctest --test-dir tools/testbench/build_testbench --output-on-failure
```

### Apply controls to simulation

The testbench supports shell script like amixer and sleep commands for
the controls. Create e.g. this script file as controls.sh:

```
#!/bin/sh

# Example test sequence for DRC and volume components

amixer -c0 cset name='Post Mixer Analog Playback DRC switch' off
amixer -c0 cset name='Post Mixer Analog Playback Volume' 40,30
sleep 1
amixer -c0 cset name='Post Mixer Analog Playback Volume' 0
sleep 1
amixer -c0 cset name='Post Mixer Analog Playback Volume' 45
```

Then generarate sine wave (997 Hz, -3 dB level, 3 seconds) with sox
and check the impact to processed signal with next commands:

```
sox -n --encoding signed-integer -L -r 48000 -c 2 -b 32 in.raw synth 3 sine 997 norm -3

tools/testbench/build_testbench/install/bin/sof-testbench4 -r 48000 -c 2 -b S32_LE   -p 1,2 \
 -t tools/build_tools/topology/topology2/production/sof-hda-generic.tplg \
-i in.raw -o out.raw -s controls.sh

sox --encoding signed-integer -L -r 48000 -c 2 -b 32 out.raw out.wav
```

As second example apply in one second intervals bytes control blobs
for IIR equalizer. The impact is easiest to hear with pink noise
signal. Create the control script below and run the following commands
to create the input, run testbench, and convert to wav for examining
the output.

```
#!/bin/sh

# Example test sequence for IIR equalizer, switch other processing off

amixer -c0 cset name='Post Mixer Analog Playback DRC switch' off
amixer -c0 cset name='Post Mixer Analog Playback Volume' 45
sof-ctl -c name='Post Mixer Analog Playback FIR Eq bytes' -s tools/ctl/ipc4/eq_fir/pass.txt
sof-ctl -c name='Post Mixer Analog Playback IIR Eq bytes' -s tools/ctl/ipc4/eq_iir/pass.txt
sleep 1
sof-ctl -c name='Post Mixer Analog Playback IIR Eq bytes' -s tools/ctl/ipc4/eq_iir/loudness.txt
sleep 1
sof-ctl -c name='Post Mixer Analog Playback IIR Eq bytes' -s tools/ctl/ipc4/eq_iir/bandpass.txt
sleep 1
sof-ctl -c name='Post Mixer Analog Playback IIR Eq bytes' -s tools/ctl/ipc4/eq_iir/bassboost.txt
sleep 1
sof-ctl -c name='Post Mixer Analog Playback IIR Eq bytes' -s tools/ctl/ipc4/eq_iir/highpass_50hz_0db_48khz.txt
```

```
sox -n --encoding signed-integer -L -r 48000 -c 2 -b 32 in.raw synth 5 pinknoise norm -20

tools/testbench/build_testbench/install/bin/sof-testbench4 -r 48000 -c 2 -b S32_LE -p 1,2 \
 -t tools/build_tools/topology/topology2/production/sof-hda-generic.tplg \
 -i in.raw -o out.raw -s controls.sh

sox --encoding signed-integer -L -r 48000 -c 2 -b 32 out.raw out.wav
```

### Run testbench with helper script

The scripts/sof-testbench-helper.sh simplifies the task. See the help
text shown with option -h.

```
Usage: scripts/sof-testbench-helper.sh <options>
Options
  -b <bits>, default 32
  -c <channels>, default 2
  -h shows this text
  -i <input wav>, default /usr/share/sounds/alsa/Front_Center.wav
  -k keep temporary files in /tmp
  -m <module>, default gain
  -n <pipelines>, default 1,2
  -o <output wav>, default none
  -p <profiling result text>, use with -x, default none
  -r <rate>, default 48000
  -t <force topology>, default none, e.g. production/sof-hda-generic.tplg
  -v runs with valgrind, not available with -x
  -x runs testbench with xt-run simulator

Example: run DRC with xt-run with profiling (slow)
scripts/sof-testbench-helper.sh -x -m drc -p profile-drc32.txt

Example: process with native build DRC file Front_Center.wav (fast)
scripts/sof-testbench-helper.sh -m drc -i /usr/share/sounds/alsa/Front_Center.wav \
  -o ~/tmp/Front_Center_with_DRC.wav

Example: check component eqiir with valgrind
scripts/sof-testbench-helper.sh -v -m eqiir
```

The above manual run can be executed with single command. The -i could
be omitted as well since the ALSA clip is the default input wav.


```
scripts/sof-testbench-helper.sh -m eqiir -i /usr/share/sounds/alsa/Front_Center.wav -o out.wav
```

To run the same example with Xtensa simulator (MTL platform), set up
the build environment, build, and execute with additional -x
option. An MCPS estimate for pipeline execution is printed in the end
of run than can be useful when checking impact of code
optimizations. Note that the cycles of testbench file component that
replaces host-copier and dai-copier are excluded.

```
export XTENSA_TOOLS_ROOT=~/xtensa/XtDevTools
export ZEPHYR_TOOLCHAIN_VARIANT=xt-clang
scripts/rebuild-testbench.sh -p mtl
scripts/sof-testbench-helper.sh -x -m eqiir -i /usr/share/sounds/alsa/Front_Center.wav -o out.wav
```

### Run Xtensa profiler with helper script

When profiling add to above run script option -p, e.g. (can omit output wav conversion).


```
scripts/sof-testbench-helper.sh -x -m eqiir -p profile-eqiir.txt
less profile-eqiir.txt
```

The output of xt-gprof looks like

```
Flat profile:
                                           self      total
       cumulative       self             cycles     cycles
  %        cycles     cycles    calls     /call      /call  name
             (K)        (K)                (K)        (K)
56.73     10144.51   10144.51   137088      0.07       0.07  iir_df1
12.91     12453.23    2308.72     1428      1.62       8.72  eq_iir_s32_default
 3.23     13031.59     578.36     4290      0.13       3.61  module_adapter_copy
 2.22     13429.03     397.44     2860      0.14       0.67  file_process
 2.14     13811.37     382.34     3178      0.12       0.13  memmove

```

To profile a set of processing components in SOF there is example
script sof-testbench-build-profile.sh. It profiles also two
hda-generic topologies with multiple SOF components in it. The help
for the script is shown with option -h:

```
Usage: scripts/sof-testbench-build-profile.sh <options>
  -d <directory> directory to place code profiling reports
  -h shows this text
  -p <platform> sets platform for scripts/rebuild-testbench.sh
```

It can be run for platform mtl with command
```
scripts/sof-testbench-build-profile.sh -p mtl
```

The profiling reports and component run logs (with MCPS) can be found
from default directory tools/testbench/profile.

### Perform audio processing quality checks

This step needs Matlab or Octave tool with signal processing
package. A number of tests is performed for IIR equalizer component as
example. The script opens plot windows and outputs a text report in
the end. A simple pass/fail criteria is used to report a verdict of
the test run.

```
cd $SOF_WORKSPACE/sof/tools/test/audio
octave -q --eval "pkg load signal io; [n_fail]=process_test('eq-iir', 32, 32, 48000, 1, 1); input('Press ENTER'); exit(n_fail)"
```

See from interactive Octave shell command "help process_test" the
explanation for the test run parameters.

### Manual run of IPC3 testbench

The testbench build is by default IPC4. The IPC version can be changed by applying next
patch:

```
diff --git a/src/arch/host/configs/library_defconfig b/src/arch/host/configs/library_defconfig
index 1b9483ffe..5c944d292 100644
--- a/src/arch/host/configs/library_defconfig
+++ b/src/arch/host/configs/library_defconfig
@@ -22,8 +22,8 @@ CONFIG_COMP_VOLUME=y
 CONFIG_COMP_VOLUME_LINEAR_RAMP=y
 CONFIG_COMP_VOLUME_WINDOWS_FADE=y
 CONFIG_DEBUG_MEMORY_USAGE_SCAN=n
-CONFIG_IPC_MAJOR_3=n
-CONFIG_IPC_MAJOR_4=y
+CONFIG_IPC_MAJOR_3=y
+CONFIG_IPC_MAJOR_4=n
 CONFIG_LIBRARY=y
 CONFIG_MATH_IIR_DF2T=y
```

As an example, process a wav file and listen it. The example
processing component is DC block. The wav file is first converted with
sox to raw 32 bit 48 kHz stereo format, then processed by testbench,
converted back to wav.

```
cd $SOF_WORKSPACE/sof
scripts/rebuild-testbench.sh
scripts/build-tools.sh -t
sox --encoding signed-integer /usr/share/sounds/alsa/Front_Left.wav -L -r 48000 -c 2 -b 32 in.raw
tools/testbench/build_testbench/install/bin/sof-testbench3 -r 48000 -R 48000 -c 2 -n 2 -b S32_LE \
   -t tools/build_tools/test/topology/test-playback-ssp5-mclk-0-I2S-dcblock-s32le-s32le-48k-24576k-codec.tplg \
   -i in.raw -o out.raw
sox --encoding signed-integer -L -r 48000 -c 2 -b 32 out.raw out.wav
aplay out.wav
```

The testbench binary can be debugged and profiled with native
executable tools like gdb, ddd (GUI for gdb), gprof, and valgrind.

### Manual profiling of testbench run on xtensa

The example is with IPC3 build for Intel MTL platform. The similar commands would work with IPC4
build also, with changed topology for IPC4. This needs access to Cadence Xplorer toolchain with
the core confiration for the platform.

```
cd $SOF_WORKSPACE/sof
export XTENSA_TOOLS_ROOT=~/xtensa/XtDevTools
export ZEPHYR_TOOLCHAIN_VARIANT=xt-clang
scripts/rebuild-testbench.sh -p mtl
source tools/testbench/build_xt_testbench/xtrun_env.sh
```

Next the testbench is run with xt-run simulator. The example component
is dynamic range processing (DRC). For proling data generate use
option --profile. Note that this is a lot slower than native run. An
1s extract of a pink noise file is used as example.

```
sox --encoding signed-integer /usr/share/sounds/alsa/Noise.wav -L -r 48000 -c 2 -b 32 in.raw trim 0.0 1.0
$XTENSA_PATH/xt-run --profile=profile.out tools/testbench/build_xt_testbench/testbench \
   -r 48000 -R 48000 -c 2 -n 2 -b S32_LE \
   -t tools/build_tools/test/topology/test-playback-ssp5-mclk-0-I2S-drc-s32le-s32le-48k-24576k-codec.tplg \
   -i in.raw -o out.raw 2> trace.txt
```

Then convert the profiler data to readable format and check it.

```
$XTENSA_PATH/xt-gprof tools/testbench/build_xt_testbench/testbench profile.out > example_profile.txt
less example_profile.txt
```
