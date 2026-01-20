#!/usr/bin/python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation. All rights reserved.

"""
This is wrapper script to process a wav file with Pytorch's Kaldi
MFCC function into ASCII CSV file with audio features vectors

WAV file nane is provided in the command line. Output file name
is kaldi.csv.
"""

import sys
import numpy
import torchaudio
import torchaudio.compliance.kaldi as K

AUDIO_SAMPLE = sys.argv[1]
#AUDIO_SAMPLE = "test_sine.wav"
#AUDIO_SAMPLE = "test_chirp.wav"
MFCC_CSV = "kaldi.csv"

def export_mfcc_to_csv(mfcc):
    """
    Convert tensor data to numpy and export to CSV

    Parameters
    ----------
    mfcc: Pytorch tensor
    """

    mfcc_np = mfcc.numpy()
    cols = numpy.size(mfcc_np, 1)
    rows = numpy.size(mfcc_np, 0)
    print(f"MFCC data size is {cols} x {rows}\n")
    numpy.savetxt(MFCC_CSV, mfcc_np, delimiter=",")
    print(f"Exported MFCC to {MFCC_CSV}\n" )

def main():
    """
    Extract audio features
    """

    metadata = torchaudio.info(AUDIO_SAMPLE)
    print(metadata)

    waveform, sample_rate = torchaudio.load(AUDIO_SAMPLE)
    if sample_rate != 16000:
        raise Exception('Only 16 kHz audio files can be used.')

    # Use defaults for all parameters, except
    #   preemphasis_coefficient = 0
    #   remove_dc_offset False
    #   window_type "hamming"

    mfcc = K.mfcc(waveform, preemphasis_coefficient = 0, remove_dc_offset = False,
                  window_type = "hamming")

    # With defaults
    #mfcc = K.mfcc(waveform)

    print(mfcc)

    export_mfcc_to_csv(mfcc)

if __name__ == '__main__':
    main()
