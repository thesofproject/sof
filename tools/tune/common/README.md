Common Scripts to Generate Byte Control Files
=======================================

The tools need GNU Octave version 4.0.0 or later with octave-signal
package.

alsactl_write(fn, blob8)
---------------

Converts blob8 to a CSV text file.
The output can be used to configure an audio component using sof-ctl.
Example:
```
sof-ctl -n 22 -s output.csv # Apply config to control numid=22
```

blob_write(fn, blob8)
---------------

Converts blob8 to a binary file.
The output can be used to configure an audio component using sof-ctl.
Example:
```
sof-ctl -n 22 -b set output.bin # Apply config to control numid=22
```

tplg_write(fn, blob8, name, comment)
---------------

Converts blob8 to a topology config file. The control bytes generated will
be called {name}_priv.
