This directory contains a tool to create configuration blob for SOF
MFCC component. It's simply run in Matlab or Octave with command
"setup_mfcc". The MFCC configuration parameters can be edited from the
script.

The configuration can be test run with testbench. First the test topologies
need to be created with "scripts/build-tools.sh -t". Next the testbench
is build with "scripts/rebuild-testbench.sh".

Once the previous steps are done, a sample wav file can be processed
into stream of cepstral coefficients with script run_mfcc.sh. E.g.
next command processes an ALSA test file with speech clip "front center".
The output file is hard-coded to mfcc.raw.

./run_mfcc.sh /usr/share/sounds/alsa/Front_Center.wav

The output can be plotted and retrieved with Matlab or Octave command:

[ceps, t, n] = decode_ceps('mfcc.raw', 13);

In the above it's known from configuration script that MFCC was set up to
output 13 cepstral coefficients from each FFT -> Mel -> DCT -> Cepstral
coefficients computation run.

Other kind of signals have quite big visual difference in audio features. Try
e.g. other sound files found in computer.

./run_mfcc.sh /usr/share/sounds/gnome/default/alerts/bark.ogg
./run_mfcc.sh /usr/share/sounds/gnome/default/alerts/sonar.ogg
