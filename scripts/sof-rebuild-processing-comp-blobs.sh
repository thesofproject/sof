#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Intel Corporation.

set -e

if [ -z "${SOF_WORKSPACE}" ]; then
    echo "Error: environment variable SOF_WORKSPACE need to be set to top level sof directory"
    exit 1
fi

if ! command -v octave &> /dev/null; then
    echo "Error: this scrip needs GNU Octave, see https://octave.org/"
    exit 1
fi

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
cd "$SOF_WORKSPACE"/sof/tools/tune/mfcc; $OCTAVE setup_mfcc.m
