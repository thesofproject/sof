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
mkdir build_plugin
cd build_plugin
cmake ../tools/plugin -DPLUGIN_IPC4=ON
```
then (use default ALSA prefix atm)

```
sudo make install <ALSA prefix>

Make sure to set the LD_LIBRARY_PATH to include directory where the SOF modules are installed
Ex: "~/work/sof/sof/build_plugin/sof_ep/install/lib:~/work/sof/sof/build_plugin/modules/"
And set the environment variable SOF_PLUGIN_TOPOLOGY_PATH to point to the directory containing the topology binary
```

Code can then be run by starting sof-pipe with your desired topology

```
 ./sof-pipe -T sof-tgl-nocodec.tplg
```

At this point the sof-pipe daemon is waiting for IPC. Audio applications can now invoke sof-pipe processing via

```
aplay -Dsof:tgl-nocodec:1:default:default:48k2c16b -f dat some48kHz.wav
```
The command line is parsed as follows:
- "sof" is the plugin name
- "tgl-nocodec" is the topology name. The "sof-" prefix and ".tplg" suffix will be appended by the plugin
- "1" is the PCM ID for render/capture
- "default": The first default is the card name
- "default": The second default is the device name
- "48k2c16b" is the config name for 48K, stereo, 16bit

This renders audio to the sof-pipe daemon using the tgl-nocodec topology playback PCM ID 1.
The above example needs to be 48k as example pipe has no SRC/ASRC.

Likewise

```
arecord -Dsof:tgl-nocodec:1:default:default:48k2c16b -f dat file.wav
```
Will record audio using the tgl-nocodec topology and PCM ID 1.

Mixer settings can be adjusted for bdw-nocodec by (Not functional yet)

```
alsamixer -Dsof:tgl-nocodec:1
```

#TODO Items (and T-shirt size) for single pipeline E2E audio
 * IPC4 support in tplg parser (M)
 * IPC4 support in plugin (pipe/ipc4.c) (M)
 * Fix ALSA -Dhw: device support (S), currently only default ALSA device works
 * Deprecate POSIX message queues for IPC and use UNIX sockets.(S)
 * Make better build system for modules i.e. remove hack-install.sh (S)
 * Need a simpler aplay/arecord cmd line.
