#!/usr/bin/python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

"""Split recordings of a repeated keyword into one WAV file per utterance.

Each input WAV is mixed to mono and resampled to 48 kHz. Speech is detected
from the 100 - 7000 Hz band energy of 20 ms frames: a frame is active when its
energy exceeds the noise floor (a low percentile of the frame energies) by
--threshold-db and is at most --peak-range-db below the loudest frame. Active
regions closer than --min-gap are merged into one
utterance, and utterances shorter than --min-len are dropped.

Every utterance is written with the pauses around it, from the end of the
previous utterance to the start of the next one (minus --guard), so that the
clips have as much ambient noise as possible before and after the speech.
The pauses are shared by neighboring clips. --max-pad limits the length of
the ambient part before the speech and --max-pad-after the part after it.
For training clips keep the part after the speech short, since the trainer
places the keyword label at the last detected speech in the clip.

The clip starts and ends are faded with raised cosine ramps of --fade-in and
--fade-out length to avoid clicks from cutting the ambient noise. The ramps
are limited to the ambient part and do not reach the speech.

Output: <out-dir>/<input name>/<input name>_NNN.wav, mono 48 kHz 16-bit.

Example:
    sof_mww_split_utterances.py -o ~/Music/local_keywords/split \\
        ~/Music/local_keywords/raw/*.wav
"""

import argparse
import os
import sys
import wave

import numpy as np
from scipy import signal

FS = 48000


def read_wav(path: str) -> tuple[np.ndarray, int]:
    with wave.open(path, "rb") as w:
        ch, width, fs, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3)
        x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) |
             (b[:, 2].astype(np.int8).astype(np.int32) << 16)).astype(np.float64) / 2 ** 23
    elif width in (2, 4):
        x = np.frombuffer(raw, f"<i{width}").astype(np.float64) / 2 ** (8 * width - 1)
    else:
        raise ValueError(f"{path}: unsupported sample width {width}")
    return x.reshape(-1, ch).mean(axis=1), fs


def write_wav(path: str, x: np.ndarray) -> None:
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(pcm.tobytes())


def fade(x: np.ndarray, n_in: int, n_out: int) -> np.ndarray:
    """Apply raised cosine fade-in and fade-out of n_in and n_out samples."""
    y = x.copy()
    if n_in > 0:
        y[:n_in] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(n_in) / n_in)
    if n_out > 0:
        y[len(y) - n_out:] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(1, n_out + 1) / n_out)
    return y


def find_utterances(x: np.ndarray, args: argparse.Namespace) -> list[tuple[int, int]]:
    """Return (start, end) sample indices of the detected utterances."""
    sos = signal.butter(4, [100, 7000], btype="bandpass", fs=FS, output="sos")
    y = signal.sosfiltfilt(sos, x)
    hop = int(FS * args.frame_ms / 1000)
    n = len(y) // hop
    e_db = 10 * np.log10(np.mean(y[: n * hop].reshape(n, hop) ** 2, axis=1) + 1e-12)
    # Digital silence (zero padding) would pull the floor far below the room noise
    live = e_db[e_db > -100]
    floor = np.percentile(live if live.size else e_db, args.floor_percentile)
    # In very clean recordings breaths and hiss are far above the floor
    threshold = max(floor + args.threshold_db, e_db.max() - args.peak_range_db)
    active = e_db > threshold

    segs = []
    start = None
    for i, a in enumerate(np.append(active, False)):
        if a and start is None:
            start = i
        elif not a and start is not None:
            segs.append([start, i])
            start = None

    min_gap = int(args.min_gap * 1000 / args.frame_ms)
    merged = []
    for s, e in segs:
        if merged and s - merged[-1][1] < min_gap:
            merged[-1][1] = e
        else:
            merged.append([s, e])
    min_len = int(args.min_len * 1000 / args.frame_ms)
    print(f"  noise floor {floor:.1f} dB, threshold {threshold:.1f} dB")
    return [(s * hop, e * hop) for s, e in merged if e - s >= min_len]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="Input WAV files")
    ap.add_argument("-o", "--out-dir", required=True, help="Output directory")
    ap.add_argument("--frame-ms", type=float, default=20.0, help="Energy frame length (default 20 ms)")
    ap.add_argument("--threshold-db", type=float, default=15.0,
                    help="Active when band energy exceeds the noise floor by this (default 15 dB)")
    ap.add_argument("--floor-percentile", type=float, default=10.0,
                    help="Percentile of frame energies used as noise floor (default 10)")
    ap.add_argument("--peak-range-db", type=float, default=40.0,
                    help="Active frames are also at most this far below the loudest frame (default 40 dB)")
    ap.add_argument("--min-gap", type=float, default=0.3,
                    help="Merge active regions closer than this, in s (default 0.3)")
    ap.add_argument("--min-len", type=float, default=0.15,
                    help="Drop utterances shorter than this, in s (default 0.15)")
    ap.add_argument("--guard", type=float, default=0.1,
                    help="Distance kept from the neighboring utterances, in s (default 0.1)")
    ap.add_argument("--max-pad", type=float, default=2.0,
                    help="Max ambient length before the speech, in s (default 2.0)")
    ap.add_argument("--max-pad-after", type=float,
                    help="Max ambient length after the speech, in s (default: --max-pad)")
    ap.add_argument("--fade-in", type=float, default=0.1,
                    help="Fade-in length at the clip start, in s (default 0.1)")
    ap.add_argument("--fade-out", type=float, default=0.05,
                    help="Fade-out length at the clip end, in s (default 0.05)")
    args = ap.parse_args()

    guard = int(args.guard * FS)
    max_pad = int(args.max_pad * FS)
    max_pad_after = int((args.max_pad if args.max_pad_after is None else args.max_pad_after) * FS)
    for path in args.inputs:
        x, fs = read_wav(path)
        if fs != FS:
            g = np.gcd(fs, FS)
            x = signal.resample_poly(x, FS // g, fs // g)
        name = os.path.splitext(os.path.basename(path))[0]
        print(f"{path}: {len(x) / FS:.2f} s")
        utts = find_utterances(x, args)
        out = os.path.join(args.out_dir, name)
        os.makedirs(out, exist_ok=True)
        for k, (s, e) in enumerate(utts):
            prev_end = utts[k - 1][1] + guard if k > 0 else 0
            next_start = utts[k + 1][0] - guard if k + 1 < len(utts) else len(x)
            a = max(prev_end, s - max_pad, 0)
            b = min(next_start, e + max_pad_after, len(x))
            dst = os.path.join(out, f"{name}_{k + 1:03d}.wav")
            n_in = min(int(args.fade_in * FS), s - a)
            n_out = min(int(args.fade_out * FS), b - e)
            write_wav(dst, fade(x[a:b], n_in, n_out))
            print(f"  {os.path.basename(dst)}: speech {s / FS:6.2f}-{e / FS:6.2f} s ({(e - s) / FS:.2f} s), "
                  f"clip {(b - a) / FS:.2f} s, peak {20 * np.log10(np.abs(x[s:e]).max() + 1e-12):.1f} dBFS")
        print(f"  {len(utts)} utterances to {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
