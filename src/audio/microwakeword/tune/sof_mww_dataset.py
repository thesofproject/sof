# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
SOF mel40 feature loader for microWakeWord (MWW) streaming wake-on-voice training.

Consumes the raw hop records emitted by ``sof_mfcc_extract_features.sh`` (one
file per input WAV, produced by the SOF host testbench):

    <feat_root>/<label>/<basename>.raw

Each hop starts with the on-device wire format:

    struct mfcc_data_header  (24 bytes)  magic, frame_number, reserved,
                                         energy, noise_energy, vad_flag
    int32_t values[40]                   Q9.23 mel-log bins

The loader locates hops by scanning for ``MFCC_MAGIC`` (0x6d666363). At each
magic offset it reads the 24-byte header and the following 40 int32 Q9.23 mel
bins, converts to float32 (scaled by 2^-23), and slices recordings into
fixed-size 100-hop windows (1.0 second at 10ms hop stride) for training.

Standalone CLI mode prints dataset shape and class counts:

    python3 sof_mww_dataset.py <feat_root> silence unknown <keyword>
"""

from __future__ import annotations

import glob
import os
import sys

import numpy as np

HOP_HEADER_BYTES = 24
HOP_BINS = 40
HOP_BYTES = HOP_HEADER_BYTES + HOP_BINS * 4
WINDOW_HOPS = 99  # 33 streaming steps of 3 hops (0.99 second at 10 ms hop stride)
MFCC_MAGIC = 0x6D666363  # 'ccfm' on disk, matches struct mfcc_data_header.magic

# Soft mel-log AGC applied to log10(mel_power) features. Units mirror the Q9.23
# runtime AGC in mww.c: gain target +0.25 (=+2.5 dB), floor -2.0 (=-20 dB),
# instant attack against MEL_CLIP_MAX=+1.0 (+10 dB).
# Dual-rate release recovery:
#   - Normal release during silence (vad == 0): 0.5 dB/sec
#   - Super-slow leak during speech (vad == 1): 0.05 dB/sec (1/10th speed)
# Features are clamped to [AGC_CLIP_MIN, AGC_CLIP_MAX] ([-1.0, +1.0]) to match
# the Q1.7 dynamic range of mww.c and maximize int8 quantization precision.
AGC_GAIN_TARGET = 0.25
AGC_GAIN_FLOOR = -2.0
AGC_CLIP_MIN = -1.0
AGC_CLIP_MAX = 1.0
AGC_RELEASE_STEP_PER_HOP = 0.0005         # 0.5 dB/s / (100 hops/s * 10 dB/unit)
AGC_RELEASE_STEP_SPEECH_PER_HOP = 0.00005 # 0.05 dB/s (1/10th rate)


def apply_soft_agc(X: np.ndarray, vad: np.ndarray | None = None) -> np.ndarray:
    """Apply the mel-log AGC to each window or recording independently.

    Clamps all output feature values to ``[AGC_CLIP_MIN, AGC_CLIP_MAX]`` (``[-1.0, +1.0]``).
    If ``vad`` is supplied (shape matching time frames), release step recovers at normal
    speed during silence (vad == 0) and super-slow leak during speech (vad == 1).

    ``X`` can have shape:
      - ``(T, F)``: single recording / sequence of hops
      - ``(N, WINDOW_HOPS, HOP_BINS, 1)`` or ``(N, WINDOW_HOPS, HOP_BINS)``: batch of windows
    Each window or recording gets its own AGC state initialized to ``AGC_GAIN_TARGET``; state
    is not carried across windows.
    """
    if X.size == 0:
        return X

    orig_shape = X.shape
    if X.ndim == 2:
        arr = X[np.newaxis, :, :, np.newaxis]
    elif X.ndim == 3:
        arr = X[:, :, :, np.newaxis]
    elif X.ndim == 4:
        arr = X
    else:
        raise ValueError(f"Expected array with 2, 3, or 4 dimensions, got shape {orig_shape}")

    N, T, F, C = arr.shape
    out = np.empty_like(arr)
    for n in range(N):
        gain = AGC_GAIN_TARGET
        for t in range(T):
            hop = arr[n, t, :, 0]
            headroom = AGC_CLIP_MAX - float(hop.max())
            if gain > headroom:
                gain = headroom
            if gain < AGC_GAIN_FLOOR:
                gain = AGC_GAIN_FLOOR
            out[n, t, :, 0] = np.clip(hop + gain, AGC_CLIP_MIN, AGC_CLIP_MAX)
            is_speech = bool(vad[t]) if (vad is not None and t < len(vad)) else False
            step = AGC_RELEASE_STEP_SPEECH_PER_HOP if is_speech else AGC_RELEASE_STEP_PER_HOP
            if gain < AGC_GAIN_TARGET:
                gain = min(gain + step, AGC_GAIN_TARGET)

    return out.reshape(orig_shape)


def load_raw_hops(path: str) -> tuple[np.ndarray, np.ndarray]:
    """Read a testbench-emitted .raw file, return (N_hops, 40) float32 mel bins and (N_hops,) vad flags.

    Scans the file for MFCC magic markers and decodes the 184-byte hop payloads.
    """
    data = np.fromfile(path, dtype=np.uint8)
    if data.size < HOP_BYTES:
        return np.zeros((0, HOP_BINS), dtype=np.float32), np.zeros((0,), dtype=np.uint32)
    n_u32 = data.size // 4
    u32 = data[: n_u32 * 4].view(np.uint32)
    magic_positions = np.flatnonzero(u32 == MFCC_MAGIC) * 4
    if magic_positions.size == 0:
        return np.zeros((0, HOP_BINS), dtype=np.float32), np.zeros((0,), dtype=np.uint32)

    valid_pos = [p for p in magic_positions if p + HOP_BYTES <= data.size]
    if not valid_pos:
        return np.zeros((0, HOP_BINS), dtype=np.float32), np.zeros((0,), dtype=np.uint32)

    n_hops = len(valid_pos)
    mel = np.empty((n_hops, HOP_BINS), dtype=np.float32)
    vad = np.empty((n_hops,), dtype=np.uint32)

    for i, p in enumerate(valid_pos):
        header_bytes = data[p : p + HOP_HEADER_BYTES]
        vad[i] = np.frombuffer(header_bytes[20:24], dtype=np.uint32)[0]
        mel_bytes = data[p + HOP_HEADER_BYTES : p + HOP_BYTES]
        mel_q23 = np.frombuffer(mel_bytes, dtype=np.int32)
        mel[i] = mel_q23.astype(np.float32) / (1 << 23)

    return mel, vad


def build_silence_pool(feat_root: str) -> np.ndarray:
    """Load and concatenate all silence/noise raw feature files into a single background pool."""
    silence_dirs = ["silence", "noise", "background"]
    all_silence: list[np.ndarray] = []
    for sdir in silence_dirs:
        pattern = os.path.join(feat_root, sdir, "*.raw")
        for f in sorted(glob.glob(pattern)):
            mel, _ = load_raw_hops(f)
            if mel.shape[0] > 0:
                all_silence.append(mel)

    if all_silence:
        return np.concatenate(all_silence, axis=0)
    # Fallback to pure baseline silence if no silence files exist
    return np.full((1000, HOP_BINS), AGC_CLIP_MIN, dtype=np.float32)


def build_ambient_pool(feat_root: str) -> np.ndarray:
    """Build a pool of low-level ambient room background (excluding loud dishwashing/tap noise)."""
    silence_pool = build_silence_pool(feat_root)
    mask = silence_pool.mean(axis=1) < -0.1
    if np.any(mask):
        return silence_pool[mask]
    return np.full((1000, HOP_BINS), AGC_CLIP_MIN, dtype=np.float32)


def slice_into_windows(
    mel: np.ndarray,
    window_hops: int = WINDOW_HOPS,
    hop_step: int = 10,
    pad_value: float = AGC_CLIP_MIN,
) -> np.ndarray:
    """Slice (N_hops, 40) into (M, window_hops, 40) overlapping windows."""
    n_hops = mel.shape[0]
    if n_hops < window_hops:
        # Pad short recordings at the end with silence pad_value
        pad = np.full((window_hops - n_hops, HOP_BINS), pad_value, dtype=mel.dtype)
        return np.concatenate([mel, pad], axis=0)[np.newaxis, ...]

    starts = list(range(0, n_hops - window_hops + 1, hop_step))
    if not starts or starts[-1] != (n_hops - window_hops):
        starts.append(n_hops - window_hops)
    windows = [mel[s : s + window_hops] for s in starts]
    return np.stack(windows, axis=0)


def load_dataset(
    feat_root: str,
    labels: list[str],
    window_hops: int = WINDOW_HOPS,
    jitter_pos_count: int = 6,
    jitter_neg_count: int = 4,
    hop_step_long: int = 5,
    gain_aug_db_min: float = -8.0,
    gain_aug_db_max: float = 3.0,
    seed: int = 0,
) -> tuple[np.ndarray, np.ndarray]:
    """Load and augment feature recordings from <feat_root>/<label>/*.raw.

    Applies streaming-aware temporal augmentations:
      - Stratified multi-tier gain augmentation (quiet/far-field, moderate, nominal)
      - Random time-shift jitter inside a continuous ambient silence slice for keywords
      - Hard negative mining with partial-keyword onset/offset fragments (y=0)
      - Dense temporal slicing and background mixing for non-target speech (y=0)
    """
    rng = np.random.default_rng(seed)
    ambient_pool = build_ambient_pool(feat_root)

    def sample_silence_window(length: int) -> np.ndarray:
        if ambient_pool.shape[0] <= length:
            return np.full((length, HOP_BINS), AGC_CLIP_MIN, dtype=np.float32)
        idx = rng.integers(0, ambient_pool.shape[0] - length)
        return ambient_pool[idx : idx + length].copy()

    def sample_stratified_gain(idx: int, total: int) -> float:
        """Sample gain with balanced coverage across quiet, moderate, and nominal tiers."""
        if gain_aug_db_min is None or gain_aug_db_max is None:
            return 0.0
        g_min = gain_aug_db_min
        g_max = gain_aug_db_max
        span = g_max - g_min
        # Split into 3 equal tiers: Tier 0 (quiet/far-field), Tier 1 (moderate), Tier 2 (nominal)
        tier_size = span / 3.0
        tier = idx % 3
        tier_min = g_min + tier * tier_size
        tier_max = tier_min + tier_size
        gain_db = rng.uniform(tier_min, tier_max)
        return gain_db * 0.1

    all_X: list[np.ndarray] = []
    all_y: list[int] = []

    for label in labels:
        label_dir = os.path.join(feat_root, label)
        raw_files = sorted(glob.glob(os.path.join(label_dir, "*.raw")))
        if not raw_files:
            print(f"Warning: no .raw feature files found in {label_dir}", file=sys.stderr)
            continue

        is_keyword = (label not in ("silence", "unknown", "noise", "background"))
        is_silence = (label in ("silence", "noise", "background"))

        for f in raw_files:
            mel, _ = load_raw_hops(f)
            if mel.shape[0] == 0:
                continue

            T = mel.shape[0]

            if is_keyword:
                # 1. Positive class: Complete keyword with random start offsets & stratified gain
                for j in range(jitter_pos_count):
                    win = sample_silence_window(window_hops)
                    gain_lin = sample_stratified_gain(j, jitter_pos_count)

                    if T <= window_hops:
                        offset = rng.integers(0, max(1, window_hops - T + 1))
                        win[offset : offset + T] = mel + gain_lin
                    else:
                        offset = rng.integers(0, T - window_hops + 1)
                        win = mel[offset : offset + window_hops] + gain_lin

                    all_X.append(win)
                    all_y.append(1)

                # 2. Hard Negative Mining: Partial keyword fragments (onset only, offset only)
                if T < window_hops:
                    gain_p = sample_stratified_gain(0, 3)
                    # Leading partial (first 30-50% of utterance at window end)
                    win_p = sample_silence_window(window_hops)
                    prefix_len = max(5, int(T * rng.uniform(0.3, 0.6)))
                    win_p[window_hops - prefix_len :] = mel[:prefix_len] + gain_p
                    all_X.append(win_p)
                    all_y.append(0)

                    # Trailing partial (last 30-50% of utterance at window start)
                    gain_s = sample_stratified_gain(1, 3)
                    win_s = sample_silence_window(window_hops)
                    suffix_len = max(5, int(T * rng.uniform(0.3, 0.6)))
                    win_s[:suffix_len] = mel[T - suffix_len :] + gain_s
                    all_X.append(win_s)
                    all_y.append(0)

            elif is_silence:
                # Silence: slice across recording
                if T >= window_hops:
                    for s in range(0, T - window_hops + 1, 10):
                        all_X.append(mel[s : s + window_hops])
                        all_y.append(0)
                else:
                    all_X.append(sample_silence_window(window_hops))
                    all_y.append(0)

            else:
                # Non-target speech (unknown): temporal jitter & stratified gain
                for j in range(jitter_neg_count):
                    win = sample_silence_window(window_hops)
                    gain_lin = sample_stratified_gain(j, jitter_neg_count)

                    if T <= window_hops:
                        offset = rng.integers(0, max(1, window_hops - T + 1))
                        win[offset : offset + T] = mel + gain_lin
                    else:
                        offset = rng.integers(0, T - window_hops + 1)
                        win = mel[offset : offset + window_hops] + gain_lin

                    all_X.append(win)
                    all_y.append(0)

                # If long recording, add intermediate overlapping slices
                if T > window_hops + hop_step_long:
                    for s in range(0, T - window_hops + 1, hop_step_long):
                        all_X.append(mel[s : s + window_hops])
                        all_y.append(0)

    # Add pure baseline silence windows to teach network that quiescent background is y=0
    for _ in range(1000):
        all_X.append(np.full((window_hops, HOP_BINS), AGC_CLIP_MIN, dtype=np.float32))
        all_y.append(0)

    if not all_X:
        return np.zeros((0, window_hops, HOP_BINS, 1), dtype=np.float32), np.zeros((0,), dtype=np.int32)

    X = np.stack(all_X, axis=0)[..., np.newaxis]
    y = np.array(all_y, dtype=np.int32)
    return X, y


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <feat_root> <label1> [<label2> ...]")
        sys.exit(1)

    feat_root = sys.argv[1]
    labels = sys.argv[2:]
    X, y = load_dataset(feat_root, labels)
    print(f"Loaded dataset: X shape = {X.shape}, y shape = {y.shape}")
    print(f"Positive samples (y=1): {np.sum(y == 1)}, Negative samples (y=0): {np.sum(y == 0)}")
