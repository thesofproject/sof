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

### Quick how-to

The simplest way to build and execute testbench is with supplied
scripts. It executes a simple chirp waveform test for a number of
processing components.  Corrupted audio or failure in execution
results to fail of test.

The commands "build-tools.sh -t" and "host-testbench.sh" are currently
valid only for default IPC3 build of testbench.

```
cd $SOF_WORKSPACE/sof
scripts/build-tools.sh -t
scripts/rebuild-testbench.sh
scripts/host-testbench.sh
```

### Manual run of IPC3 testbench

As an example, process a wav file and listen it. The example
processing component is DC block. The wav file is first converted with
sox to raw 32 bit 48 kHz stereo format, then processed by testbench,
converted back to wav.

```
cd $SOF_WORKSPACE/sof
sox --encoding signed-integer /usr/share/sounds/alsa/Front_Left.wav -L -r 48000 -c 2 -b 32 in.raw
tools/testbench/build_testbench/install/bin/testbench -r 48000 -R 48000 -c 2 -n 2 -b S32_LE \
   -t tools/build_tools/test/topology/test-playback-ssp5-mclk-0-I2S-dcblock-s32le-s32le-48k-24576k-codec.tplg \
   -i in.raw -o out.raw
sox --encoding signed-integer -L -r 48000 -c 2 -b 32 out.raw out.wav
aplay out.wav
```

The testbench binary can be debugged and profiled with native
executable tools like gdb, ddd (GUI for gdb), gprof, and valgrind.

### Profiling of testbench run on xtensa

Fist, the testbench is build in this example for Intel MTL
platform. This needs access to Cadence Xplorer toolchain with the core
confiration for the platform.

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
   -q -r 48000 -R 48000 -c 2 -n 2 -b S32_LE \
   -t tools/build_tools/test/topology/test-playback-ssp5-mclk-0-I2S-drc-s32le-s32le-48k-24576k-codec.tplg \
   -i in.raw -o out.raw 2> trace.txt
```

Then convert the profiler data to readable format and check it.

Note: Current version of testbench does not have functional quiet mode
"-q" switch to suppress trace. Due all debug traces print
out. Majority of performance is spent in printing.

```
$XTENSA_PATH/xt-gprof tools/testbench/build_xt_testbench/testbench profile.out > example_profile.txt
less example_profile.txt
```

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

### Manual run of IPC4 testbench

Apply this patch to SOF and rebuild it.

```
diff --git a/src/arch/host/configs/library_defconfig b/src/arch/host/configs/library_defconfig
index d150690aa..6d167dfba 100644
--- a/src/arch/host/configs/library_defconfig
+++ b/src/arch/host/configs/library_defconfig
@@ -21,8 +21,8 @@ CONFIG_COMP_VOLUME=y
 CONFIG_COMP_VOLUME_LINEAR_RAMP=y
 CONFIG_COMP_VOLUME_WINDOWS_FADE=y
 CONFIG_DEBUG_MEMORY_USAGE_SCAN=n
-CONFIG_IPC_MAJOR_3=y
-CONFIG_IPC_MAJOR_4=n
+CONFIG_IPC_MAJOR_3=n
+CONFIG_IPC_MAJOR_4=y
 CONFIG_LIBRARY=y
 CONFIG_LIBRARY_STATIC=y
 CONFIG_MATH_IIR_DF2T=y
```

Then process a wav file and listen. The example processing component
is DC blocker. The wav file is first converted with sox to raw 32 bit
48 kHz stereo format, then processed by testbench, converted back to
wav.

```
cd $SOF_WORKSPACE/sof
sox --encoding signed-integer /usr/share/sounds/alsa/Front_Center.wav -L -r 48000 -c 2 -b 32 in.raw
tools/testbench/build_testbench/install/bin/testbench -r 48000 -R 48000 -c 2 -n 2 -b S32_LE -p 1,2 \
   -t tools/build_tools/topology/topology2/development/sof-hda-benchmark-dcblock32.tplg \
   -i in.raw -o out.raw
sox --encoding signed-integer -L -r 48000 -c 2 -b 32 out.raw out.wav
aplay out.wav
```

The difference in command line is the use of other topology for IPC4
and use of -p command line option to select pipelines 1 and 2 from
this topology for scheduling and connecting to input and output files.
In this topology the host playback pipeline is 1 and the dai playback
side pipeline is 2. Capture side pipelines would be 3 and 4. And
running both playback and capture would need more input and output
files to be added to -i and -o as comma separated. The more advanced
use cases simulations with IPC4 are still under work.

The debugging and profiling of IPC4 testench is similar as with IPC3.
