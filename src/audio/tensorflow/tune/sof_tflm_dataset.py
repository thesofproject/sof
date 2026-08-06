# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
SOF mel40 feature loader for TFLM wake-on-voice training.

Consumes the raw hop records that ``sof_mfcc_extract_features.sh`` emits (one
file per input WAV, produced by the SOF testbench):

    <feat_root>/<label>/<basename>.raw

Each hop starts with the on-device wire format:

    struct mfcc_data_header  (24 bytes)  magic, frame_number, reserved,
                                         energy, noise_energy, vad_flag
    int32_t values[40]                   Q9.23 mel-log bins

The loader locates hops by scanning for ``MFCC_MAGIC`` (0x6d666363) so it
works for both the packed on-wire compress layout (184 bytes/hop, back to
back) and the legacy PCM-sink layout the testbench emits when the MFCC
config has ``compress_output=false`` (24-byte header + 160 mel bytes +
zero-padding out to ``sink_frame_bytes * frame_shift`` per hop; e.g. 2560
bytes for a stereo S32 16 kHz sink at 20 ms hop). At each magic offset it
reads the 24-byte header and the following 40 int32 Q9.23 mel bins,
converts to float32, and slices each recording into fixed-size 49-hop
windows to match the tflmcly sliding window (49 hops * 40 channels =
1960-value input tensor).

Standalone CLI mode prints dataset shape and per-label counts for a quick
sanity check::

    python3 sof_tflm_dataset.py <feat_root> silence unknown <keyword>
"""

from __future__ import annotations

import glob
import os
import sys

import numpy as np

HOP_HEADER_BYTES = 24
HOP_BINS = 40
HOP_BYTES = HOP_HEADER_BYTES + HOP_BINS * 4
WINDOW_HOPS = 49
MFCC_MAGIC = 0x6D666363  # 'ccfm' on disk, matches struct mfcc_data_header.magic

# Soft mel-log AGC applied to log10(mel_power) features. One decade of
# log10-power equals +10 dB, so the units below mirror the Q9.23 runtime
# AGC in tflm-classify.c: gain target 0.0 (=0 dB), floor -2.0 (=-20 dB
# attenuation), instant attack against MEL_CLIP_MAX=+1.0 (+10 dB), additive
# release step per 20 ms hop giving ~0.5 dB/sec recovery (50 hops/sec).
AGC_GAIN_TARGET = 0.0
AGC_GAIN_FLOOR = -2.0
AGC_CLIP_MAX = 1.0
AGC_RELEASE_STEP_PER_HOP = 0.0010


def apply_soft_agc(X: np.ndarray) -> np.ndarray:
    """Apply the mel-log AGC to each 49-hop window independently.

    ``X`` has shape ``(N, WINDOW_HOPS, HOP_BINS, 1)`` (as returned by
    :func:`load_dataset`). Each window gets its own AGC state initialized
    to ``AGC_GAIN_TARGET``; state is not carried across windows. The same
    scalar gain is applied to all HOP_BINS bins within a single hop, so
    the spectral shape is preserved.
    """
    if X.size == 0:
        return X
    N, T, F, C = X.shape
    out = np.empty_like(X)
    for n in range(N):
        gain = AGC_GAIN_TARGET
        for t in range(T):
            hop = X[n, t, :, 0]
            headroom = AGC_CLIP_MAX - float(hop.max())
            if gain > headroom:
                gain = headroom
            if gain < AGC_GAIN_FLOOR:
                gain = AGC_GAIN_FLOOR
            out[n, t, :, 0] = hop + gain
            if gain < AGC_GAIN_TARGET:
                gain = min(gain + AGC_RELEASE_STEP_PER_HOP, AGC_GAIN_TARGET)
    return out


def load_raw_hops(path: str) -> np.ndarray:
    """Read a testbench-emitted .raw file, return (N_hops, 40) float32.

    Scans the file for MFCC magic markers and decodes the fixed 184-byte
    hop payload at each hit, so the same loader works for compress-packed
    (184 B/hop) and legacy PCM-padded (e.g. 2560 B/hop) layouts.
    """
    data = np.fromfile(path, dtype=np.uint8)
    if data.size < HOP_BYTES:
        return np.zeros((0, HOP_BINS), dtype=np.float32)
    n_u32 = data.size // 4
    u32 = data[: n_u32 * 4].view(np.uint32)
    magic_positions = np.flatnonzero(u32 == MFCC_MAGIC) * 4
    if magic_positions.size == 0:
        return np.zeros((0, HOP_BINS), dtype=np.float32)
    # Drop any tail magic without a full 184-byte payload behind it.
    magic_positions = magic_positions[
        magic_positions + HOP_BYTES <= data.size
    ]
    if magic_positions.size == 0:
        return np.zeros((0, HOP_BINS), dtype=np.float32)
    offsets = magic_positions[:, None] + (
        HOP_HEADER_BYTES + np.arange(HOP_BINS * 4)
    )
    mel_bytes = data[offsets].reshape(-1, HOP_BINS * 4)
    q = mel_bytes.view(np.int32).reshape(-1, HOP_BINS)
    return q.astype(np.float32) / float(1 << 23)


def window_hops(
    feat: np.ndarray,
    n: int = WINDOW_HOPS,
    hop_step: int | None = None,
) -> np.ndarray:
    """Slice a (N_hops, 40) feature stream into (K, n, 40) windows.

    When ``feat`` is shorter than ``n`` and ``hop_step`` is given (smaller than ``n``),
    generate time-shifted windows by sliding the short utterance across different
    offsets inside the ``n``-hop window (padding with the utterance's edge silence frames).
    """
    if feat.shape[0] == 0:
        return np.zeros((1, n, HOP_BINS), dtype=np.float32)
    L = feat.shape[0]
    if L < n:
        if hop_step is None or hop_step >= n:
            pad = n - L
            padded = np.concatenate(
                [feat, np.repeat(feat[-1:], pad, axis=0)], axis=0
            )
            return padded[np.newaxis, ...]

        # Generate multiple time shifts: place feat at leading offsets from 0 to (n - L).
        windows = []
        for offset in range(0, n - L + 1, hop_step):
            pad_left = offset
            pad_right = n - L - offset
            left = np.repeat(feat[:1], pad_left, axis=0) if pad_left > 0 else np.zeros((0, HOP_BINS), dtype=np.float32)
            right = np.repeat(feat[-1:], pad_right, axis=0) if pad_right > 0 else np.zeros((0, HOP_BINS), dtype=np.float32)
            w = np.concatenate([left, feat, right], axis=0)
            windows.append(w)
        if (n - L) % hop_step != 0:
            offset = n - L
            pad_left = offset
            left = np.repeat(feat[:1], pad_left, axis=0)
            w = np.concatenate([left, feat], axis=0)
            windows.append(w)
        return np.stack(windows, axis=0)

    if hop_step is None:
        hop_step = n
    starts = list(range(0, feat.shape[0] - n + 1, hop_step))
    if not starts:
        starts = [0]
    return np.stack([feat[s : s + n] for s in starts], axis=0)


def load_dataset(
    feat_root: str,
    labels: list[str],
    hop_step: int | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    """Return X (N, 49, 40, 1) float32 and y (N,) int32 for the given labels.

    Label index in ``labels`` becomes the integer class id — keep the caller's
    order stable across training and inference. For the SOF tflmcly KPB rule
    to fire, put silence at index 0 and unknown at index 1; any keyword class
    must sit at index >= 2.
    """
    X_parts: list[np.ndarray] = []
    y_parts: list[np.ndarray] = []
    for label_idx, label in enumerate(labels):
        pattern = os.path.join(feat_root, label, "*.raw")
        files = sorted(glob.glob(pattern))
        if not files:
            print(f"warning: no .raw files under {pattern}", file=sys.stderr)
            continue
        for path in files:
            feat = load_raw_hops(path)
            wins = window_hops(feat, hop_step=hop_step)
            X_parts.append(wins)
            y_parts.append(
                np.full(wins.shape[0], label_idx, dtype=np.int32)
            )
    if not X_parts:
        raise RuntimeError(f"no .raw features found under {feat_root}")
    X = np.concatenate(X_parts, axis=0).astype(np.float32)
    y = np.concatenate(y_parts, axis=0)
    return X[..., np.newaxis], y


def _cli() -> int:
    if len(sys.argv) < 3:
        print(
            "usage: sof_tflm_dataset.py <feat_root> <label1> [label2 ...]",
            file=sys.stderr,
        )
        return 1
    X, y = load_dataset(sys.argv[1], sys.argv[2:])
    print(f"X: {X.shape} {X.dtype} min={X.min():.3f} max={X.max():.3f}")
    print(f"y: {y.shape} {y.dtype} counts={np.bincount(y).tolist()}")
    return 0


if __name__ == "__main__":
    sys.exit(_cli())
