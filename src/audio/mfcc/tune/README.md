# SOF MFCC Tuning Tools

This directory contains a tool to create configuration blob for SOF
MFCC component. It's simply run in Matlab or Octave with command
`setup_mfcc`. The MFCC configuration parameters can be edited from the
script.

## Testbench run

The configuration can be test run with testbench. First the test topologies
need to be created with `scripts/build-tools.sh -t`. Next the testbench
is built with `scripts/rebuild-testbench.sh`.

Once the previous steps are done, a sample wav file can be processed
with script `run_mfcc.sh`. The script converts the input to raw 16 kHz
stereo format and runs the testbench for S16, S24, and S32 bit depths,
producing both cepstral coefficient (MFCC) and Mel spectrogram outputs.

```
./run_mfcc.sh /usr/share/sounds/alsa/Front_Center.wav
```

Output files from host testbench:

| File | Content |
|------|---------|
| `mfcc_s16.raw`, `mfcc_s24.raw`, `mfcc_s32.raw` | Cepstral coefficients |
| `mel_s16.raw`, `mel_s24.raw`, `mel_s32.raw` | Mel spectrogram |

If the `XTENSA_PATH` environment variable is set, the script also runs
the Xtensa build of the testbench (via `xt-run`) and produces additional
output files prefixed with `xt_`:

| File | Content |
|------|---------|
| `xt_mfcc_s16.raw`, `xt_mfcc_s24.raw`, `xt_mfcc_s32.raw` | Cepstral coefficients |
| `xt_mel_s16.raw`, `xt_mel_s24.raw`, `xt_mel_s32.raw` | Mel spectrogram |

## Decoding and Plotting

All output files can be decoded and plotted at once in Matlab or Octave
with the `decode_all.m` script:

```matlab
decode_all
```

This calls `decode_ceps` for each MFCC file (13 cepstral coefficients) and
`decode_mel` for each Mel file (80 Mel bins), plotting spectrograms for all
files that exist including the Xtensa variants.

Individual files can also be decoded manually:

```matlab
[ceps, t, n] = decode_ceps('mfcc_s16.raw', 13);
```

In the above it's known from configuration script that MFCC was set up to
output 13 cepstral coefficients from each FFT → Mel → DCT → Cepstral
coefficients computation run.

The 80 bands Mel output can be visualized with command:

```matlab
[mel, t, n] = decode_mel('mel_s16.raw', 80);
```

## Live Whisper Transcription with DSP VAD

The directory contains a Python script `sof_mel_to_text_live_dsp_vad.py`.
It can be used with development topologies
`sof-arl-cs42l43-l0-cs35l56-l23-mfcc-mel-normal.tplg` and
`sof-mtl-rt713-l0-rt1316-l12-mfcc-mel-normal.tplg`.

It captures from default audio device `hw:0,47` (headset microphone PCM).
The Mel audio features and VAD flags are packed to be compatible with a
normal PCM stream. The captured frames with detected speech are sent to the
Whisper speech recognizer model for conversion to text.

The more efficient method for audio features capture uses the compress PCM without
continuous redundant data. Such MFCC development topologies are:

- `sof-mtl-rt713-l0-rt1316-l12-mfcc-mel-compr.tplg`
- `sof-mtl-rt713-l0-rt1316-l12-mfcc-ceps-compr.tplg`
- `sof-arl-cs42l43-l0-cs35l56-l23-mfcc-mel-compr.tplg`
- `sof-arl-cs42l43-l0-cs35l56-l23-mfcc-ceps-compr.tplg`

E.g. the script `sof_mel_spectrogram_compress.py` uses the mfcc-mel-compr topology version.

### Prerequisites

The script needs OpenVINO. Please follow the install procedure from
<https://docs.openvino.ai/2025/get-started/install-openvino.html>.

The following Python pip installs are needed into the same OpenVINO venv.

```bash
pip install openvino openvino-tokenizers openvino-genai
pip install optimum[intel]
pip install transformers
pip install huggingface_hub
```

The real-time spectrogram viewers in this directory use GTK 4; their setup is described
under [Live Spectrogram Viewers](#live-spectrogram-viewers) below.

### NPU / GPU Support

The script by default runs the Whisper encoder model in the NPU. To
use the NPU, install the driver from
<https://github.com/intel/linux-npu-driver/releases>. If the NPU is not
available, change the encoder to CPU with run option `--encoder-device CPU`.
With a GPU both `--encoder-device GPU` and `--decoder-device GPU` can be set.

### Example run

Check which capture devices are available.

```bash
arecord -l
```

In this example the devices hw:0,47 and hw:0,48 support the audio
features stream.

```bash
**** List of CAPTURE Hardware Devices ****
card 0: sofsoundwire [sof-soundwire], device 1: Jack In (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: sofsoundwire [sof-soundwire], device 4: Microphone (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: sofsoundwire [sof-soundwire], device 47: Jack In Audio Features (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: sofsoundwire [sof-soundwire], device 48: Microphone Audio Features (*) []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
```

With Whisper model run the CPU and with internal microphones of laptop
the run command is:

```bash
python3 sof_mel_to_text_live_dsp_vad.py --encoder-device CPU --device hw:0,48
```

The script run output is shown below

```bash
=== Live SOF Mel → Whisper Transcription (DSP VAD) ===

Starting capture: arecord -D hw:0,48 -f S32_LE -c 2 -r 16000 -t raw --buffer-size 8192
VAD source: DSP (embedded in stream)
Silence trigger: 100ms (10 frames)
Whisper model: whisper-medium-int4-ov (encoder: CPU, decoder: CPU)

  [   0.01s] SILENCE
  [   1.39s] SPEECH
  [   2.57s] SILENCE
  [   2.66s] Transcribing 118 frames (1.2s)...
  [Whisper] encoder: 1.30s
  [Whisper] decoder: 0.59s (3 tokens)

  >> "Hello computer"
```
## Live Whisper Transcription with Compress PCM

The `sof_mel_to_text_live_compress.py` script captures Mel spectrogram
frames from a SOF compress PCM device and performs live Whisper
transcription using OpenVINO. Unlike `sof_mel_to_text_live_dsp_vad.py`
which uses `arecord`, this script reads directly from the compress PCM
device with discontinuous frames handling.

The same OpenVINO prerequisites and pip packages apply as described above
for `sof_mel_to_text_live_dsp_vad.py`.

```bash
# Microphone compress audio features
python3 sof_mel_to_text_live_compress.py --card 0 --device 54 --model whisper-medium-int4-ov

# Jack In compress audio features
python3 sof_mel_to_text_live_compress.py --card 0 --device 53 --model whisper-medium-int4-ov
```

### Compress PCM device IDs

The compress audio-features capture PCMs in the topology2 platform
configs use the following IDs (see
`tools/topology/topology2/platform/intel/sdw-*-audio-feature-compress.conf`):

| Device | Name                              | PCM ID |
|--------|-----------------------------------|--------|
| Jack   | "Jack In Compress Audio Features" | 53     |
| Mic    | "Microphone Compress Audio Features" | 54  |

The non-compress PCMs (`47` jack, `48` mic, "Audio Features") are PCM
streams intended for `arecord`/`hw:0,N` (used by
`sof_mel_to_text_live_dsp_vad.py` above) and will not work with the
compress scripts below.

## Live Spectrogram Viewers

These viewers are helpful for interactively checking VAD operation and
developing improvements, or for educational visualization of Mel and
cepstral-coefficient spectrograms.

#### Additional dependencies to install

GTK 4 and the introspection bindings are shipped by the distribution.
On Debian / Ubuntu (24.04 or newer):

```bash
sudo apt install python3-gi gir1.2-gtk-4.0 python3-numpy
sudo apt install libgirepository-2.0-dev libcairo2-dev pkg-config \
                 python3-dev gir1.2-gtk-4.0 build-essential
pip install PyGObject pycairo numpy
```

#### Renderer selection

GSK auto-selects the renderer; to force the GL or Vulkan back-ends
(usually the smoothest):

```bash
GSK_RENDERER=ngl    python3 sof_mel_spectrogram_compress.py ...
GSK_RENDERER=vulkan python3 sof_mel_spectrogram_compress.py ...
```

### Mel Spectrogram

The `sof_mel_spectrogram_compress.py` script captures Mel spectrogram
frames from a SOF compress PCM device and displays them as a live
scrolling spectrogram with VAD status in a GTK 4 window. This is a
lightweight viewer that does not run Whisper inference.

```bash
# Microphone compress audio features
python3 sof_mel_spectrogram_compress.py --card 0 --device 54 --width 300

# Jack In compress audio features
python3 sof_mel_spectrogram_compress.py --card 0 --device 53 --width 300
```

### Cepstral Spectrogram

The `sof_ceps_spectrogram_compress.py` script is the counterpart
that displays cepstral coefficients (MFCC) instead of Mel bands. It
imports the GPU-accelerated widgets from `sof_mel_spectrogram_compress.py`,
so both files must remain in the same directory.

```bash
# Microphone compress audio features
python3 sof_ceps_spectrogram_compress.py --card 0 --device 54 --num-ceps 13 --width 300

# Jack In compress audio features
python3 sof_ceps_spectrogram_compress.py --card 0 --device 53 --num-ceps 13 --width 300
```
