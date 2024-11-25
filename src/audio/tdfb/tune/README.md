# Settings blobs generators for TDFB

This directory contains the scripts to generate settings blobs for
the time-domain fixed beamformer (TDFB) for various microphone array
geometries and beamformer features.

The requirement is Octave or Matlab with signal processing package.

The most straightforward way to generate all blobs for topologies,
sof-ctl and UCM is to run command

```
./sof_example_all.sh
```

It creates the blobs for passthrough mode, single beam blobs for
line and circular arrays, and dual beam blobs for stereo audio
capture. Running it can take about 30 minutes.

All the topology ASCII text format blobs contain instructions
how to generate them from command line shell. E.g. these
commands do a more fine grained build.

```
cd $SOF_WORKSPACE/sof

# Generate pass-through blobs
cd tools/tune/tdfb; octave --no-window-system example_pass_config.m

# Generate blobs for line array and single beam
cd tools/tune/tdfb; octave --no-window-system example_line_array.m

# Generate blobs for stereo capture for arrays with known mm-spacing
cd tools/tune/tdfb; matlab -nodisplay -nosplash -nodesktop -r example_two_beams

# Generate bobs for stereo capture for generic arrays with known microphones count
cd tools/tune/tdfb; matlab -nodisplay -nosplash -nodesktop -r example_two_beams_default
```

Further information about TDFB component is available in SOF Docs, see
https://thesofproject.github.io/latest/algos/tdfb/time_domain_fixed_beamformer.html
