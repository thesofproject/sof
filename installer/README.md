The GNUmakefile in this directory prepares ``/lib/firmware/intel/sof/`` and
``/lib/firmware/intel/sof-tplg/`` directories.

It extracts what's needed from the output of the scripts
``./scripts/xtensa-build-all.sh`` and ``./scripts/build-tools.sh -T
-l``. It automatically runs these scripts when needed for the platforms
listed in config.mk and performs incremental builds when they have
already been run.

It does not copy anything to ``/lib/firmware/`` directly but to local,
"staging" subdirectory first. The staging area can then be installed with
rsync to a local or remote ``/lib/firmware/intel/`` or to a release
location. This gives an opportunity to inspect the staging area and
avoids running everything as root.

The default target (re-)generates the staging area:

    make -C installer/

Then, to install the staging area:

    sudo make -C installer/ rsync

By default, the "rsync" target installs to the local
``/lib/firmware/intel/`` directory. To install to a different host or
different directory, copy the ``sample-config.mk`` file to ``config.mk``
and follow the instructions inside the file. ``config.mk`` can also be
used to change the list of platforms installed and a number of other
Make variables. As usual with Make, many parameters can also be
overridden on the command line.

To stage and install in one go:

    make -C installer/ stage rsync

"stage" is the default target and it tries to stage everything:
firmware, dictionaries and topologies. As usual with Make, it's possible
to invoke individual targets. Find a list of targets at the top of
GNUMakefile.

You can use `make -jN stage` to build multiple platforms faster but do
*not* `make -jN stage rsync` as this will start deploying before the
builds are all complete. That's because we want the rsync target to be
able to deploy subsets. Instead do: `make -jN somethings && make rsync`.

Sample output:

    staging/sof: symbolic link to sof-v1.6.1
    staging/sof-v1.6.1/
    ├── community/
    │   ├── sof-tgl.ri
    │   ├── sof-cnl.ri
    │   ├── sof-icl.ri
    │   ├── sof-jsl.ri
    │   ├── sof-apl.ri
    │   ├── sof-cfl.ri -> sof-cnl.ri
    │   ├── sof-cml.ri -> sof-cnl.ri
    │   ├── sof-ehl.ri -> sof-tgl.ri
    │   └── sof-glk.ri -> sof-apl.ri
    ├── intel-signed/
    ├── sof-bdw.ri
    ├── sof-cht.ri
    ├── sof-byt.ri
    ├── sof-cnl.ldc
    ├── sof-tgl.ldc
    ├── sof-icl.ldc
    ├── sof-jsl.ldc
    ├── sof-apl.ldc
    ├── sof-bdw.ldc
    ├── sof-byt.ldc
    ├── sof-cht.ldc
    ├── sof-apl.ri -> intel-signed/sof-apl.ri
    ├── sof-cfl.ri -> intel-signed/sof-cfl.ri
    ├── sof-cml.ri -> intel-signed/sof-cml.ri
    ├── sof-cnl.ri -> intel-signed/sof-cnl.ri
    ├── sof-ehl.ri -> intel-signed/sof-ehl.ri
    ├── sof-glk.ri -> intel-signed/sof-glk.ri
    ├── sof-icl.ri -> intel-signed/sof-icl.ri
    ├── sof-jsl.ri -> intel-signed/sof-jsl.ri
    ├── sof-tgl.ri -> intel-signed/sof-tgl.ri
    ├── sof-cfl.ldc -> sof-cnl.ldc
    ├── sof-cml.ldc -> sof-cnl.ldc
    ├── sof-ehl.ldc -> sof-tgl.ldc
    └── sof-glk.ldc -> sof-apl.ldc

