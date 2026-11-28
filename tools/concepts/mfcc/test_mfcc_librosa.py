#!/usr/bin/python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation. All rights reserved.

"""
This is wrapper script to process a wav file with Librosa's
MFCC function into ASCII CSV file with audio features vectors

WAV file nane is provided in the command line. Output file name
is librosa.csv.
"""

import sys
import numpy
import librosa
import torchaudio

AUDIO_SAMPLE = sys.argv[1]
#AUDIO_SAMPLE = "test_sine.wav"
#AUDIO_SAMPLE = "test_chirp.wav"
MFCC_CSV = "librosa.csv"

def export_mfcc_to_csv(mfcc):
    """
    Export numpy array to CSV

    Parameters
    ----------
    mfcc: Numpy array
    """

    cols = numpy.size(mfcc, 1)
    rows = numpy.size(mfcc, 0)
    print(f"MFCC data size is {cols} x {rows}\n")
    numpy.savetxt(MFCC_CSV, mfcc, delimiter=",")
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

    # htk True gives the familiar mel <-> hz conversion
    melspec = librosa.feature.melspectrogram(
        y = waveform.numpy()[0],
        sr = 16000, n_fft = 512, win_length = 400, hop_length = 160,
        window = 'hann', n_mels = 13, htk=True
    )

    mfcc = librosa.feature.mfcc(
        S=librosa.core.spectrum.power_to_db(melspec),
        n_mfcc=13, dct_type=2, norm='ortho')

    print(mfcc)

    export_mfcc_to_csv(mfcc)

if __name__ == '__main__':
    main()
