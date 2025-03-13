# SOF testbench

### Features

 * Simulate IPC3 and IPC4 SOF versions with run of set of processing
   components based on desired topology.
 * Replaces host and dai components with file I/O with raw binary
   S16_LE/S24_LE/S32_LE or text format files for audio waveforms.
 * Much faster than real-time execution in native build, e.g. x86 on
   Linux for efficient validation usage.
 * With xtensa DSP build offers cycles accurate simulated environment
   execution. And also with options for simulation speed vs. model
   accuracy.
 * Allows easy use of conventional debugger, profiler, leak and memory check
   tools usage for DSP firmware code.

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
 CONFIG_LIBRARY_STATIC=y
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
