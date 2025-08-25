#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Intel Corporation.

set -e

if [ -z "${SOF_WORKSPACE}" ]; then
    echo "Error: environment variable SOF_WORKSPACE need to be set to top level sof directory"
    exit 1
fi

if ! command -v octave &> /dev/null; then
    echo "Error: this script needs GNU Octave, see https://octave.org/"
    exit 1
fi

"$SOF_WORKSPACE"/sof/scripts/build-tools.sh -c

if ! command -v sof-ctl &> /dev/null; then
    echo "Error: The sof-ctl utility is not found from path for executables. It is needed"
    echo "       to retrieve SOF ABI header It can be added with e.g. symlink to user's binaries:"
    echo "       ln -s $SOF_WORKSPACE/sof/tools/build_tools/ctl/sof-ctl $HOME/bin/sof-ctl"
    exit 1
fi

cmp --quiet "$(which sof-ctl)" "$SOF_WORKSPACE"/sof/tools/build_tools/ctl/sof-ctl || {
    echo "Error: The sof-ctl in user's path is not the same as sof-ctl build from tools."
    exit 1
}

OCTAVE="octave --quiet --no-window-system"
cd "$SOF_WORKSPACE"/sof/src/audio/aria/tune; $OCTAVE sof_aria_blobs.m
cd "$SOF_WORKSPACE"/sof/src/audio/crossover/tune; $OCTAVE sof_example_crossover.m
cd "$SOF_WORKSPACE"/sof/src/audio/dcblock/tune; $OCTAVE sof_example_dcblock.m
cd "$SOF_WORKSPACE"/sof/src/audio/drc/tune; $OCTAVE  sof_example_drc.m
cd "$SOF_WORKSPACE"/sof/src/audio/eq_iir/tune; $OCTAVE sof_example_iir_eq.m
cd "$SOF_WORKSPACE"/sof/src/audio/eq_iir/tune; $OCTAVE sof_example_fir_eq.m
cd "$SOF_WORKSPACE"/sof/src/audio/eq_iir/tune; $OCTAVE sof_example_iir_bandsplit.m
cd "$SOF_WORKSPACE"/sof/src/audio/eq_iir/tune; $OCTAVE sof_example_spk_eq.m
cd "$SOF_WORKSPACE"/sof/src/audio/multiband_drc/tune; $OCTAVE sof_example_multiband_drc.m
cd "$SOF_WORKSPACE"/sof/src/audio/tdfb/tune; ./sof_example_all.sh
cd "$SOF_WORKSPACE"/sof/src/audio/selector/tune; $OCTAVE ./sof_selector_blobs.m
cd "$SOF_WORKSPACE"/sof/tools/tune/mfcc; $OCTAVE setup_mfcc.m
cd "$SOF_WORKSPACE"/sof/src/audio/level_multiplier/tune; $OCTAVE sof_level_multiplier_blobs.m
