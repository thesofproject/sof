##ALSA Plugin

The SOF ALSA plugin allows SOF topologies to be run on the host. The plugin
is still WIP with many rough edges that need refined before production
deployment, however the plugin is usable today as a rapid development
framework for SOF infrastructure and processing.

#Features
 * aplay & arecord usage working today
 * alsamixer & amixer usage not working today
 * modules are loaded as SO shared libraries.
 * topology is parsed by the plugin and pipelines associated with the requested PCM ID are loaded
 * pipelines run as individual userspace threads
 * pipelines can be pinned to efficency cores
 * pipelines can use realtime priority.
 * alsa sink and alsa source modules available.
 * pipelines can block (non blocking and mmap todo)

#License
Code is a mixture of LGPL and BSD 3c.

#Usage
Please build as cmake project, for IPC4 (functional)
IPC3 is not functional and not supported

```
cd sof
cmake -GNinja -B build-plugin/ -S tools/plugin/
# Build external projects first to avoid build race condition
# Dropping -GNinja is another (very slow) option.
cmake --build    build-plugin/ -- sof_ep parser_ep
cmake --build    build-plugin/
```
then (use default ALSA prefix atm)

```
sudo cmake --install build-plugin/

Make sure to set the LD_LIBRARY_PATH to include directory where the SOF modules are installed
Ex: "~/work/sof/sof/build_plugin/sof_ep/install/lib:~/work/sof/sof/build_plugin/modules/"
And set the environment variable SOF_PLUGIN_TOPOLOGY_PATH to point to the directory containing the topology binary
```

Code can then be run by starting sof-pipe with your desired topology (Ex: sof-plugin.tplg)

```
 ./sof-pipe -T sof-plugin.tplg
```

At this point the sof-pipe daemon is waiting for IPC. Audio applications can now invoke sof-pipe processing via

```
aplay -Dsof:plugin:1:default:default:48k2c16b -f dat some48kHz.wav
```
The command line is parsed as follows:
- "sof" is the plugin name
- "sof-plugin" is the topology name. The "sof-" prefix and ".tplg" suffix will be appended by the plugin
- "1" is the PCM ID for render/capture
- "default": The first default is the card name
- "default": The second default is the device name
- "48k2c16b" is the config name for 48K, stereo, 16bit

This renders audio to the sof-pipe daemon using the sof-plugin topology playback PCM ID 1.
The above example needs to be 48k as example pipe has no SRC/ASRC.

Likewise

```
arecord -Dsof:plugin:1:default:default:48k2c16b -f dat file.wav
```
Will record audio using the plugin topology and PCM ID 1.

Mixer settings can be adjusted for sof-plugin by

```
alsamixer -Dsof:plugin
```
or
```
amixer -Dsof:plugin cset numid=1 20
```
Right now, only volume controls are supported. Support for bytes and enum controls is pending.

# Instructions for testing OpenVino noise suppression model with the SOF plugin:
1. Fetch the model from the Open Model zoo repository ex: noise-suppression-poconetlike-0001.xml

https://docs.openvino.ai/archive/2023.0/omz_demos.html#build-the-demo-applications-on-linux

2. Source OpenVino environment and get OpenCV
https://www.intel.com/content/www/us/en/developer/tools/openvino-toolkit-download.html

3. Worth building and running the demo noise suppression application in the open model zoo
repository to make sure OpenVino and OpenCV are configured properly.

4. Set environment variable NOISE_SUPPRESSION_MODEL_NAME to point to the model file
```
export NOISE_SUPPRESSION_MODEL_NAME="~/open_model_zoo/demos/noise_suppression_demo/cpp/intel/noise-suppression-poconetlike-0001/FP16/noise-suppression-poconetlike-0001.xml"

```
5. Enable noise suppression by setting NOISE_SUPPRESSION=true in the tplg-targets for the sof-plugin topology

6. Start capture using the following command. This uses the 16K capture from the DMIC from
PCM hw device 0,7. Currently, only 16K capture is supported but in the future this will be expanded
to work with 48K capture.
```
arecord -Dsof:plugin:1:0:7:16k2c16b -f dat -r 16000 --period-size=2048 file_ns_16k.wa
```

#TODO Items (and T-shirt size) for single pipeline E2E audio
 * IPC4 support in tplg parser (M)
 * IPC4 support in plugin (pipe/ipc4.c) (M)
 * Fix ALSA -Dhw: device support (S), currently only default ALSA device works
 * Deprecate POSIX message queues for IPC and use UNIX sockets.(S)
 * Make better build system for modules i.e. remove hack-install.sh (S)
 * Need a simpler aplay/arecord cmd line.
