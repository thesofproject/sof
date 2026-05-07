This directory contains a tool to create configuration blob for SOF
MFCC component. It's simply run in Matlab or Octave with command
"setup_mfcc". The MFCC configuration parameters can be edited from the
script.

The configuration can be test run with testbench. First the test topologies
need to be created with "scripts/build-tools.sh -t". Next the testbench
is build with "scripts/rebuild-testbench.sh".

Once the previous steps are done, a sample wav file can be processed
with script run_mfcc.sh. The script converts the input to raw 16 kHz
stereo format and runs the testbench for S16, S24, and S32 bit depths,
producing both cepstral coefficient (MFCC) and Mel spectrogram outputs.

./run_mfcc.sh /usr/share/sounds/alsa/Front_Center.wav

Output files from host testbench:
  mfcc_s16.raw, mfcc_s24.raw, mfcc_s32.raw   - cepstral coefficients
  mel_s16.raw, mel_s24.raw, mel_s32.raw       - Mel spectrogram

If the XTENSA_PATH environment variable is set, the script also runs
the Xtensa build of the testbench (via xt-run) and produces additional
output files prefixed with "xt_":
  xt_mfcc_s16.raw, xt_mfcc_s24.raw, xt_mfcc_s32.raw
  xt_mel_s16.raw, xt_mel_s24.raw, xt_mel_s32.raw

All output files can be decoded and plotted at once in Matlab or Octave
with the decode_all.m script:

decode_all

This calls decode_ceps for each MFCC file (13 cepstral coefficients) and
decode_mel for each Mel file (80 Mel bins), plotting spectrograms for all
files that exist including the Xtensa variants.

Individual files can also be decoded manually:

[ceps, t, n] = decode_ceps('mfcc_s16.raw', 13);

In the above it's known from configuration script that MFCC was set up to
output 13 cepstral coefficients from each FFT -> Mel -> DCT -> Cepstral
coefficients computation run.

The 80 bands Mel output can be visualized with command:

[mel, t, n] = decode_mel('mel_s16.raw', 80);

Other kind of signals have quite big visual difference in audio features. Try
e.g. other sound files found in computer.

./run_mfcc.sh /usr/share/sounds/gnome/default/alerts/bark.ogg
./run_mfcc.sh /usr/share/sounds/gnome/default/alerts/sonar.ogg
