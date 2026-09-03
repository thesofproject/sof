DC Blocking Filter Control Bytes Generator
=======================================

This is a tool to generate the topology control bytes file (.m4) and configuration
files used by sof-ctl. See example_dcblock.m for reference on how to use it.

The tools need GNU Octave version 4.0.0 or later with octave-signal
package.

dcblock_build_blob.m
---------------

This script takes an array of floating point coefficients and the endianness.
Returns a blob used to configure the binary controls of the DC Blocking Filter
component.

The blob can be passed to alsactl_write(), blob_write(), tplg_write() to generate
a CSV text, binary and topology file respectively.

dcblock_plot_transferfn.m
---------------

This script takes the R coefficient and the sampling frequency to plot the
Frequency Response of the DCB filter H(z) = (1-1/z)/(1-R/z).

dcblock_plot_stepfn.m
--------------

This script takes the R coefficient and the sampling frequency to plot the
Step Response of the DCB filter. It is useful to visualize how the DC component
of a signal reacts to the filter.

