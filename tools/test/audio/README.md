# SOF Audio Processing Components Tests

### Test for processing components

The script process_test.m provides tests to help to check that audio
performance requirements are met with run of SOF testbench with test
topologies. The tests are currently for measured audio objective
quality parameters. The used criteria for performance is only an
initial assumption and need to be adjusted for various applications
needs.

The scripts support currently support the next list of objective
quality parameters. The test scripts need Matlab(R) [2] or GNU Octave
scientific programming language [3].

	- Gain
	- Frequency Response
	- THD+N vs. Frequency
	- Dynamic Range
	- Attenuation of Alias Products
	- Attenuation of Image Products
	- Level Dependent Logarithmic Gain

Note: The metric is an effort to follow AES17 [1] recommendation for
parameters and test procedures. This was created to help developers to
quickly check their work but has not been verified to be
compliant. Professional equipment and formal calibration process is
recommended for professional applications where both accurate absolute
and relative metric is needed.

The process_test.m script is started from Octave or Matlab command shell.

```
octave --gui &
```

Examples of usage for it is shown with command help

```
help process_test
```

Tests for IIR equalizer with S32_LE input and output format with 48
kHz rate, full test, plots shown, can be run with following command

```
process_test('eqiir', 32, 32, 48000, 1, 1);

```

The components those can be tested with process_test are shown in the
next table. For sof-testbench4 use the 2nd column names,for
sof-testbench3 use the 3rd column names.

| Component                       | IPC4 ID       | IPC3 ID       |
|---------------------------------|---------------|---------------|
| ARIA                            | aria          |               |
| ASRC                            | asrc          | asrc          |
| DC blocker                      | dcblock       | dcblock       |
| Dynamic range control           | drc           | drc           |
| FIR equalizer                   | eqfir         | eq-fir        |
| Gain                            | gain          | volume        |
| IIR equalizer                   | eqiir         | eq-iir        |
| Multiband dynamic range control | drc_multiband | multiband-drc |
| SRC                             | src           | src           |
| Time domain fixed beamformer    | tdfb          | tdfb          |


See README.md in tools/testbench provdes more information about using
the testbench.

### Tests for component SRC and ASRC

The Octave or Matlab script to test sample rate conversion is
src_test.m. See the help for src_test to see the usage.

The top level shell script to launch tests is src_test.sh. See script
src_run.sh for assumed install location of SOF host test bench
executable and component libraries. Exit code 0 indicates success and
exit code 1 indicates failed test cases.

The default in/out rates matrix to test is defined in the beginning of
script src_test.m. The matrix can be also passed from calling function
src_test_top.m if need.

The key objective quality parameters requiremements are in the
beginning of script src_test.m as well under comment Generic test
pass/fail criteria.

Test run creates plots into directory "plots". Brief text format
reports are placed to directory "reports".

### Tests for component TDFB

Scripts for testing special features of the multi-microphone beamformer
are in tdfb_test.m and tdfb_direction_test.m. See the help texts for usage.
Running the test needs special input files those have been created in blobs
export. See directory src/audio/tdfb/tune how to rebuild the blobs are
create simulation data files.

References
----------

[1]	AES17-2020 standard, http://www.aes.org/publications/standards/search.cfm?docID=21
[2]	Matlab(R), https://www.mathworks.com/products/matlab.html
[3]	GNU Octave, https://www.gnu.org/software/octave/
[4]	SoX - Sound eXchange, http://sox.sourceforge.net/
