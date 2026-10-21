# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
All-in-one Python reference training pipeline for microWakeWord (MWW) using PCAN.

Extracts 40-bin PCAN mel features from WAV datasets (e.g. ~/wov/wavs/{silence,unknown,<keyword>})
with the SOF MFCC component run in sof-testbench4 (default, bit-exact with firmware) or with
TensorFlow's audio_microfrontend (--frontend tf). Every clip is embedded in continuous
background audio from the ambient classes and the mixture is peak-normalized to a random
level in [--level-min-dbfs, --level-max-dbfs], so training windows hold real context over
the full streaming receptive field as on the device. Trains the streaming MixConv neural
network, int8 quantizes to TFLite, and runs a streaming evaluation to print a confusion
matrix and detection report.

Example:
    python3 sof_mww_train_pcan.py \
        --wav-root ~/wov/wavs \
        --keyword hey_jarvis \
        --out-dir ~/wov/model_pcan \
        --epochs 30
"""

from __future__ import annotations

import argparse
import datetime
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

# Force legacy Keras 2 to ensure compatibility with TFLite int8 converter and resource variables
os.environ["TF_USE_LEGACY_KERAS"] = "1"

import numpy as np

try:
    import tensorflow as tf
    from tensorflow.lite.experimental.microfrontend.python.ops import (
        audio_microfrontend_op as frontend_op,
    )
except ImportError as exc:
    print(
        "Error: tensorflow is required. Please activate your Python training venv:\n"
        "  source ~/venvs/mww-train/bin/activate",
        file=sys.stderr,
    )
    raise SystemExit(1) from exc

HOP_BINS = 40
# Must cover the streaming receptive field: conv0 (5 hops, stride 3) + DW kernels
# 5/9/13/21 + last 5 steps = (4 + 8 + 12 + 20 + 4) * 3 + 5 = 149 hops
WINDOW_HOPS = 160  # 1.6 s at 10 ms hop stride
SLICE_HOPS = 3     # 30 ms streaming chunk size (MWW_FEATURE_SLICE_COUNT)

AMBIENT_LABELS = ("ambient", "silence", "noise", "background", "babble")
PRE_CONTEXT_SAMPLES = 32000  # background before each clip, longer than the receptive field

# SOF MFCC maps PCAN output 0..666 to int8 as ((v * 256 + 333) / 666) - 128
PCAN_INT8_SCALE = 666.0 / 256.0
HOP_SAMPLES = 160
FRAME_HOPS = 3     # 30 ms analysis window = 3 hops
SOF_HOP_BYTES = 64  # 24-byte struct mfcc_data_header + 40 x int8
SOF_HOP_MAGIC = 0x6D666363

_SOF_WORKSPACE = os.environ.get("SOF_WORKSPACE", os.path.expanduser("~/work/current/sof"))
DEFAULT_TESTBENCH = os.path.join(
    _SOF_WORKSPACE, "sof/tools/testbench/build_testbench/install/bin/sof-testbench4"
)
DEFAULT_TPLG = os.path.join(
    _SOF_WORKSPACE,
    "sof/tools/build_tools/topology/topology2/development/sof-hda-benchmark-mfccmel40_10ms_pcan32.tplg",
)


# -----------------------------------------------------------------------------
# 1. WAV Loading & Audio Microfrontend (PCAN) Feature Extraction
# -----------------------------------------------------------------------------

def load_wav_pcm16(path: str) -> np.ndarray:
    """Read a WAV file and return 1D int16 PCM audio samples."""
    with wave.open(path, "rb") as w:
        n_channels = w.getnchannels()
        sampwidth = w.getsampwidth()
        framerate = w.getframerate()
        n_frames = w.getnframes()
        data = w.readframes(n_frames)

    if sampwidth == 2:
        pcm = np.frombuffer(data, dtype=np.int16)
    elif sampwidth == 4:
        # 32-bit integer PCM -> convert to int16
        pcm = (np.frombuffer(data, dtype=np.int32) >> 16).astype(np.int16)
    elif sampwidth == 3:
        # 24-bit PCM
        raw = np.frombuffer(data, dtype=np.uint8)
        raw_reshaped = raw.reshape(-1, 3)
        # Sign-extend 24-bit to 32-bit, then take high 16 bits
        int32 = (
            raw_reshaped[:, 0].astype(np.int32)
            | (raw_reshaped[:, 1].astype(np.int32) << 8)
            | (raw_reshaped[:, 2].astype(np.int8).astype(np.int32) << 16)
        )
        pcm = (int32 >> 8).astype(np.int16)
    else:
        raise ValueError(f"Unsupported sample width: {sampwidth} bytes in {path}")

    # Stereo -> take first channel
    if n_channels > 1:
        pcm = pcm.reshape(-1, n_channels)[:, 0]

    return pcm


def random_level(pcm: np.ndarray, rng: np.random.Generator | None,
                 level_db: tuple[float, float] | None) -> np.ndarray:
    """Peak-normalize a clip to a random level drawn uniformly in dBFS."""
    if rng is None or level_db is None:
        return pcm
    peak = float(np.max(np.abs(pcm.astype(np.int32)))) if pcm.size else 0.0
    if peak < 1.0:
        return pcm
    target = 32767.0 * 10.0 ** (rng.uniform(level_db[0], level_db[1]) / 20.0)
    return np.clip(np.round(pcm.astype(np.float32) * (target / peak)), -32768, 32767).astype(np.int16)


class BackgroundPool:
    """Continuous background audio built from the ambient/silence/noise clips."""

    def __init__(self, wav_root: str):
        self.clips: list[np.ndarray] = []
        for label in AMBIENT_LABELS:
            for f in sorted(glob.glob(os.path.join(wav_root, label, "*.wav"))):
                pcm = load_wav_pcm16(f).astype(np.float32)
                if pcm.size >= HOP_SAMPLES and np.any(pcm):
                    self.clips.append(pcm)
        print(f">>> Background pool: {len(self.clips)} clips, "
              f"{sum(c.size for c in self.clips) / 16000:.0f} s")

    def get(self, n: int, rng: np.random.Generator) -> np.ndarray:
        """Return n samples of unit-RMS background."""
        if not self.clips:
            return rng.standard_normal(n).astype(np.float32)
        parts = []
        left = n
        while left > 0:
            c = self.clips[int(rng.integers(0, len(self.clips)))]
            st = int(rng.integers(0, c.size))
            seg = c[st : st + left]
            seg = seg / max(np.sqrt(np.mean(seg ** 2)), 1e-3)
            parts.append(seg)
            left -= seg.size
        return np.concatenate(parts)[:n]


def embed_in_background(
    fg: np.ndarray,
    pool: BackgroundPool,
    rng: np.random.Generator,
    level_db: tuple[float, float] | None,
    snr_db: tuple[float, float] = (5.0, 35.0),
    post_s: tuple[float, float] = (0.1, 0.8),
) -> tuple[np.ndarray, int]:
    """Place a clip into continuous background like a live capture stream.

    The clip starts after PRE_CONTEXT_SAMPLES plus a random 0..29 ms phase
    offset. Returns the int16 stream and the first hop of the clip.
    """
    pre = PRE_CONTEXT_SAMPLES + int(rng.integers(0, HOP_SAMPLES * SLICE_HOPS))
    post = int(rng.uniform(*post_s) * 16000)
    fg = fg.astype(np.float32)
    fg_rms = max(float(np.sqrt(np.mean(fg ** 2))), 1.0)
    x = pool.get(pre + fg.size + post, rng) * fg_rms * 10.0 ** (-rng.uniform(*snr_db) / 20.0)
    x[pre : pre + fg.size] += fg
    x = np.clip(np.round(x), -32768, 32767).astype(np.int16)
    return random_level(x, rng, level_db), pre // HOP_SAMPLES


class SofTestbenchFrontend:
    """PCAN features computed by the SOF MFCC component in sof-testbench4.

    The clip is repeated back to back for at least WARMUP_SAMPLES and only the
    last copy's hops are kept, so the PCAN noise estimate is warmed up as in a
    continuous capture stream.
    """

    WARMUP_SAMPLES = 32000  # cold-start PCAN state settles in about 1.5 s

    def __init__(self, testbench: str, tplg: str):
        for p in (testbench, tplg):
            if not os.path.isfile(p):
                raise FileNotFoundError(p)
        self.testbench = testbench
        self.tplg = tplg
        self.tmpdir = tempfile.mkdtemp(prefix="sof_mww_fe_")
        self.in_path = os.path.join(self.tmpdir, "in.raw")
        self.out_path = os.path.join(self.tmpdir, "out.raw")

    def close(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def __call__(self, pcm_int16: np.ndarray) -> np.ndarray:
        n = len(pcm_int16) - len(pcm_int16) % HOP_SAMPLES
        n_hops = n // HOP_SAMPLES - (FRAME_HOPS - 1)
        if n_hops <= 0:
            return np.zeros((0, HOP_BINS), dtype=np.float32)

        reps = 1 + -(-self.WARMUP_SAMPLES // n)
        mono = np.tile(pcm_int16[:n], reps).astype(np.int32) << 16
        np.repeat(mono, 2).astype("<i4").tofile(self.in_path)
        cmd = [self.testbench, "-d", "0", "-r", "16000", "-c", "2", "-b", "S32_LE",
               "-p", "3,4", "-t", self.tplg, "-i", self.in_path, "-o", self.out_path]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if res.returncode != 0:
            raise RuntimeError(f"sof-testbench4 failed:\n{res.stdout.decode(errors='replace')}")

        data = np.fromfile(self.out_path, dtype=np.uint8)
        data = data[: data.size - data.size % SOF_HOP_BYTES].reshape(-1, SOF_HOP_BYTES)
        if data.shape[0] < n_hops or np.any(data[:, :4].copy().view("<u4") != SOF_HOP_MAGIC):
            raise RuntimeError(f"unexpected sof-testbench4 output: {data.shape[0]} hops, need {n_hops}")
        q = data[-n_hops:, 24:].view(np.int8).astype(np.float32)
        return (q + 128.0) * PCAN_INT8_SCALE


def extract_pcan_features_tf(
    pcm_int16: np.ndarray,
    sample_rate: int = 16000,
    window_size_ms: int = 30,
    window_step_ms: int = 10,
    num_channels: int = 40,
    pcan_strength: float = 0.95,
    pcan_offset: float = 80.0,
) -> np.ndarray:
    """Extract 40-bin mel spectrogram features using Google PCAN microfrontend.

    Returns float32 array of shape (N_hops, num_channels).
    """
    if len(pcm_int16) < (window_size_ms * sample_rate // 1000):
        return np.zeros((0, num_channels), dtype=np.float32)

    audio_tensor = tf.constant(pcm_int16, dtype=tf.int16)
    feats = frontend_op.audio_microfrontend(
        audio_tensor,
        sample_rate=sample_rate,
        window_size=window_size_ms,
        window_step=window_step_ms,
        num_channels=num_channels,
        upper_band_limit=7500.0,
        lower_band_limit=125.0,
        enable_pcan=True,
        pcan_strength=pcan_strength,
        pcan_offset=pcan_offset,
        gain_bits=21,
        enable_log=True,
        scale_shift=6,
        out_scale=1,
        out_type=tf.float32,
    )
    return feats.numpy()


# Selected in main() with --frontend
extract_pcan_features = extract_pcan_features_tf


# -----------------------------------------------------------------------------
# 2. Window Slicing and Dataset Loading
# -----------------------------------------------------------------------------

def find_speech_bounds(mel: np.ndarray, threshold_ratio: float = 0.15) -> tuple[int, int]:
    """Find start and end hop indices of active speech in the mel spectrogram."""
    frame_energy = np.sum(mel, axis=1)
    min_e, max_e = float(np.min(frame_energy)), float(np.max(frame_energy))
    if max_e - min_e < 1.0:
        return 0, mel.shape[0]
    thresh = min_e + (max_e - min_e) * threshold_ratio
    active = np.where(frame_energy > thresh)[0]
    if len(active) == 0:
        return 0, mel.shape[0]
    start = max(0, int(active[0]) - 5)
    end = min(mel.shape[0], int(active[-1]) + 5)
    return start, end


def generate_synthetic_transients(
    n_samples: int = 2500,
    window_hops: int = WINDOW_HOPS,
    rng: np.random.Generator | None = None,
) -> list[np.ndarray]:
    """Generate synthetic acoustic transients (clicks, taps, pops, step changes).

    These serve as hard negatives (y=0) to ensure the streaming classifier does
    not learn a spurious shortcut where quiet background followed by a sudden
    acoustic onset at the window's end triggers the wake word.
    """
    if rng is None:
        rng = np.random.default_rng(0)

    transients: list[np.ndarray] = []
    for _ in range(n_samples):
        win = np.zeros((window_hops, HOP_BINS), dtype=np.float32)
        # Background: 50% flat silence, 50% low-level ambient noise floor
        if rng.random() > 0.5:
            win += rng.uniform(0.0, 50.0, size=(window_hops, HOP_BINS)).astype(np.float32)

        # Transient width: 1 to 8 hops (10 ms to 80 ms)
        w = int(rng.integers(1, 9))

        # Position: 70% placed near window end (hops 80..window_hops-w), where
        # the temporal Dense layer looks; 30% placed earlier in the window
        if rng.random() > 0.3:
            pos = int(rng.integers(max(0, window_hops - 18), max(1, window_hops - w + 1)))
        else:
            pos = int(rng.integers(0, max(1, window_hops - w)))

        t_type = rng.choice(
            [
                "broadband_click",
                "decaying_impulse",
                "hf_snap",
                "lf_thump",
                "mid_tap",
                "isolated_hop",
                "keyboard_cadence",
            ]
        )

        if t_type == "broadband_click":
            # Sharp loud broadband burst across all 40 mel channels (clicks, desk taps, glitches)
            amp = rng.uniform(150.0, 650.0)
            win[pos : pos + w, :] += amp
        elif t_type == "decaying_impulse":
            # High initial peak decaying exponentially across hops
            init_amp = rng.uniform(250.0, 650.0)
            decay = np.exp(-np.arange(w) * rng.uniform(0.4, 1.2))[:, np.newaxis]
            win[pos : pos + w, :] += (init_amp * decay).astype(np.float32)
        elif t_type == "hf_snap":
            # High frequency snap (mouse click, keyboard key switch): channels 16..39
            amp = rng.uniform(200.0, 600.0)
            win[pos : pos + w, 16:40] += amp
        elif t_type == "lf_thump":
            # Low frequency mechanical thump (desk bump, footstep): channels 0..14
            amp = rng.uniform(200.0, 600.0)
            win[pos : pos + w, 0:15] += amp
        elif t_type == "mid_tap":
            # Mid-frequency acoustic tap: channels 8..26
            amp = rng.uniform(200.0, 600.0)
            win[pos : pos + w, 8:27] += amp
        elif t_type == "isolated_hop":
            # Exactly 1 hop (10 ms) glitch spike
            amp = rng.uniform(250.0, 660.0)
            ch_mask = rng.random(HOP_BINS) > 0.15
            win[pos, ch_mask] += amp
        elif t_type == "keyboard_cadence":
            # Rhythmic sequence of 2 to 5 key clicks spaced 80ms to 220ms apart
            n_clicks = int(rng.integers(2, 6))
            spacing = int(rng.integers(8, 23))  # 80ms to 220ms apart
            end_pos = int(rng.integers(max(0, window_hops - 15), max(1, window_hops - 3)))
            positions = [end_pos - idx * spacing for idx in range(n_clicks)]
            for p in positions:
                if 0 <= p < window_hops:
                    c_len = int(rng.integers(1, 4))
                    amp = rng.uniform(150.0, 500.0)
                    ch_start = int(rng.integers(8, 20))
                    win[p : min(window_hops, p + c_len), ch_start:40] += amp

        win = np.clip(win, 0.0, 666.0)
        transients.append(win)

    return transients


def load_pcan_dataset(
    wav_root: str,
    labels: list[str],
    window_hops: int = WINDOW_HOPS,
    level_db: tuple[float, float] | None = (-45.0, -6.0),
    pos_level_copies: int = 2,
    seed: int = 0,
    pool: BackgroundPool | None = None,
) -> tuple[np.ndarray, np.ndarray, dict[str, list[str]]]:
    """Extract PCAN features for all WAVs and generate balanced training windows.

    Every clip is embedded in continuous background audio (see
    embed_in_background) and the features of the whole stream are computed,
    so each window_hops window holds real context over the full streaming
    receptive field, as in the firmware. The mixture is peak-normalized to a
    random level in level_db; keyword clips are used pos_level_copies times.

    Windows (label):
      - keyword: ending at speech end, one per 3-hop stride phase (1);
        partial keyword, onset and trailing context (0)
      - unknown speech: onsets and ending at word end (0)
      - ambient: sliding windows (0)

    Returns:
      X: (N, window_hops, 40, 1) float32 tensor
      y: (N,) binary labels (0 for negative, 1 for positive keyword)
      file_map: Dict mapping label -> list of WAV paths
    """
    rng = np.random.default_rng(seed)
    if pool is None:
        pool = BackgroundPool(wav_root)
    all_X: list[np.ndarray] = []
    all_y: list[int] = []
    file_map: dict[str, list[str]] = {}

    def add(mel: np.ndarray, end: int, label: int) -> None:
        if window_hops <= end <= mel.shape[0]:
            all_X.append(mel[end - window_hops : end].copy())
            all_y.append(label)

    for label in labels:
        label_dir = os.path.join(wav_root, label)
        wav_files = sorted(glob.glob(os.path.join(label_dir, "*.wav")))
        file_map[label] = wav_files

        if not wav_files:
            print(f"Warning: no .wav files found in {label_dir}", file=sys.stderr)
            continue

        is_keyword = label not in AMBIENT_LABELS + ("unknown",)
        is_ambient = label in AMBIENT_LABELS
        n_copies = max(1, pos_level_copies) if is_keyword else 1
        print(f">>> Extracting features for {label}: {len(wav_files)} WAVs x {n_copies} level(s)")

        for f in wav_files * n_copies:
            fg = load_wav_pcm16(f)
            if fg.size < HOP_SAMPLES * 5:
                continue
            x, fg_start = embed_in_background(fg, pool, rng, level_db)
            mel = extract_pcan_features(x)
            fg_end = min(mel.shape[0], fg_start + fg.size // HOP_SAMPLES)

            if is_keyword:
                s0, s1 = find_speech_bounds(mel[fg_start:fg_end])
                speech_start, speech_end = fg_start + s0, fg_start + s1
                # One window per phase of the 3-hop inference stride
                for k in range(SLICE_HOPS):
                    add(mel, speech_end + k - 1 + 3 * int(rng.integers(-1, 2)), 1)
                mid_pt = max(5, (speech_end - speech_start) // 2)
                add(mel, speech_start + mid_pt, 0)   # first half of the keyword
                add(mel, speech_start + 10, 0)       # first 100 ms
                add(mel, speech_start - 5, 0)        # leading background
                add(mel, speech_end + 40, 0)         # 400 ms after the keyword

            elif is_ambient:
                for end in range(fg_start + 20, mel.shape[0] + 1, 20):
                    add(mel, end, 0)

            else:
                s0, s1 = find_speech_bounds(mel[fg_start:fg_end])
                for k_on in (5, 10, 20):
                    add(mel, fg_start + s0 + k_on, 0)
                add(mel, fg_start + s1 + int(rng.integers(-1, 2)), 0)

    # Add synthetic acoustic transients (clicks, taps, pops, keyboard typing) as hard negatives (y=0)
    print(">>> Generating 4,000 synthetic acoustic transients and typing cadences (clicks, taps, typing)...")
    transients = generate_synthetic_transients(n_samples=4000, window_hops=window_hops, rng=rng)
    for t_win in transients:
        all_X.append(t_win)
        all_y.append(0)

    # Synthesize continuous multi-word conversational speech from unknown words
    unk_files = file_map.get("unknown", [])
    if len(unk_files) >= 10:
        print(f">>> Synthesizing continuous multi-word conversational speech from {len(unk_files)} unknown words...")
        for _ in range(1200):
            k = rng.integers(2, 5)  # 2 to 4 words per sentence
            picks = rng.choice(unk_files, size=k)
            pcm_parts = []
            for p in picks:
                p_pcm = load_wav_pcm16(p)
                if len(p_pcm) > 0:
                    pcm_parts.append(p_pcm)
                    pause = np.zeros(rng.integers(800, 2400), dtype=np.int16)
                    pcm_parts.append(pause)
            if pcm_parts:
                x, fg_start = embed_in_background(np.concatenate(pcm_parts), pool, rng, level_db)
                cat_mel = extract_pcan_features(x)
                for end in range(fg_start + 10, cat_mel.shape[0] + 1, 15):
                    add(cat_mel, end, 0)

    # Add pure baseline silence windows to teach network that quiescent background is y=0
    for _ in range(1000):
        all_X.append(np.zeros((window_hops, HOP_BINS), dtype=np.float32))
        all_y.append(0)

    if not all_X:
        return np.zeros((0, window_hops, HOP_BINS, 1), dtype=np.float32), np.zeros((0,), dtype=np.int32), file_map

    X = np.stack(all_X, axis=0)[..., np.newaxis]
    y = np.array(all_y, dtype=np.int32)
    return X, y, file_map


def split_train_val(
    X: np.ndarray,
    y: np.ndarray,
    val_frac: float = 0.2,
    seed: int = 0,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Stratified train/val split."""
    rng = np.random.default_rng(seed)
    idx = np.arange(y.size)
    train_mask = np.zeros(y.size, dtype=bool)
    for c in np.unique(y):
        cls_idx = idx[y == c]
        rng.shuffle(cls_idx)
        cut = int(round(cls_idx.size * (1.0 - val_frac)))
        train_mask[cls_idx[:cut]] = True
    return X[train_mask], y[train_mask], X[~train_mask], y[~train_mask]


# -----------------------------------------------------------------------------
# 3. Model Architecture & Streaming Model Construction
# -----------------------------------------------------------------------------

def build_mww_model(window_hops: int = 99, num_mels: int = 40) -> tf.keras.Model:
    """Build the base MixConv model for offline training on spectrogram windows with exact causal padding."""
    inputs = tf.keras.Input(shape=(window_hops, num_mels, 1), name="pcan_in")
    x = tf.keras.layers.Reshape((window_hops, 1, num_mels), name="reshape")(inputs)

    # Layer 0: Conv2D + BN + ReLU with causal left padding of 2 hops
    x = tf.keras.layers.ZeroPadding2D(padding=((2, 0), (0, 0)), name="pad0")(x)
    x = tf.keras.layers.Conv2D(
        filters=30,
        kernel_size=(5, 1),
        strides=(3, 1),
        padding="valid",
        use_bias=True,
        activation=None,
        name="conv0",
    )(x)
    x = tf.keras.layers.BatchNormalization(name="bn0")(x)
    x = tf.keras.layers.ReLU(name="relu0")(x)

    # Layer 1: MixConv DW(5) + PW(60) + BN + ReLU with causal left padding of 4 steps
    x = tf.keras.layers.ZeroPadding2D(padding=((4, 0), (0, 0)), name="pad1")(x)
    x = tf.keras.layers.DepthwiseConv2D(
        kernel_size=(5, 1),
        strides=(1, 1),
        padding="valid",
        activation=None,
        name="dw1",
    )(x)
    x = tf.keras.layers.Conv2D(
        filters=60,
        kernel_size=(1, 1),
        strides=(1, 1),
        padding="valid",
        use_bias=True,
        activation=None,
        name="pw1",
    )(x)
    x = tf.keras.layers.BatchNormalization(name="bn1")(x)
    x = tf.keras.layers.ReLU(name="relu1")(x)

    # Layer 2: MixConv DW(9) + PW(60) + BN + ReLU with causal left padding of 8 steps
    x = tf.keras.layers.ZeroPadding2D(padding=((8, 0), (0, 0)), name="pad2")(x)
    x = tf.keras.layers.DepthwiseConv2D(
        kernel_size=(9, 1),
        strides=(1, 1),
        padding="valid",
        activation=None,
        name="dw2",
    )(x)
    x = tf.keras.layers.Conv2D(
        filters=60,
        kernel_size=(1, 1),
        strides=(1, 1),
        padding="valid",
        use_bias=True,
        activation=None,
        name="pw2",
    )(x)
    x = tf.keras.layers.BatchNormalization(name="bn2")(x)
    x = tf.keras.layers.ReLU(name="relu2")(x)

    # Layer 3: MixConv DW(13) + PW(60) + BN + ReLU with causal left padding of 12 steps
    x = tf.keras.layers.ZeroPadding2D(padding=((12, 0), (0, 0)), name="pad3")(x)
    x = tf.keras.layers.DepthwiseConv2D(
        kernel_size=(13, 1),
        strides=(1, 1),
        padding="valid",
        activation=None,
        name="dw3",
    )(x)
    x = tf.keras.layers.Conv2D(
        filters=60,
        kernel_size=(1, 1),
        strides=(1, 1),
        padding="valid",
        use_bias=True,
        activation=None,
        name="pw3",
    )(x)
    x = tf.keras.layers.BatchNormalization(name="bn3")(x)
    x = tf.keras.layers.ReLU(name="relu3")(x)

    # Layer 4: MixConv DW(21) + PW(60) + BN + ReLU with causal left padding of 20 steps
    x = tf.keras.layers.ZeroPadding2D(padding=((20, 0), (0, 0)), name="pad4")(x)
    x = tf.keras.layers.DepthwiseConv2D(
        kernel_size=(21, 1),
        strides=(1, 1),
        padding="valid",
        activation=None,
        name="dw4",
    )(x)
    x = tf.keras.layers.Conv2D(
        filters=60,
        kernel_size=(1, 1),
        strides=(1, 1),
        padding="valid",
        use_bias=True,
        activation=None,
        name="pw4",
    )(x)
    x = tf.keras.layers.BatchNormalization(name="bn4")(x)
    x = tf.keras.layers.ReLU(name="relu4")(x)

    # Temporal feature aggregation (last 5 steps: 5 * 60 = 300)
    x = tf.keras.layers.Lambda(lambda t: t[:, -5:, :, :], name="last5")(x)
    x = tf.keras.layers.Flatten(name="flatten")(x)
    x = tf.keras.layers.Dropout(0.3, name="dropout")(x)
    outputs = tf.keras.layers.Dense(
        1, activation="sigmoid", bias_initializer=tf.keras.initializers.Constant(-2.0), name="dense"
    )(x)

    return tf.keras.Model(inputs=inputs, outputs=outputs, name="mww_pcan_model")


def build_streaming_inference_model(
    trained_model: tf.keras.Model,
    slice_hops: int = 3,
    num_mels: int = 40,
) -> tf.keras.Model:
    """Build true layer-by-layer streaming MixConv inference model with internal ring buffers and fused BatchNorm."""
    def fold_bn(conv_layer, bn_layer):
        gamma, beta, mean, var = bn_layer.get_weights()
        weights = conv_layer.get_weights()
        W = weights[0]
        b = weights[1] if len(weights) > 1 else np.zeros(W.shape[-1], dtype=np.float32)
        scale = gamma / np.sqrt(var + bn_layer.epsilon)
        W_fused = W * scale
        b_fused = (b - mean) * scale + beta
        return [W_fused, b_fused]

    class StreamingMixConvMWW(tf.keras.Model):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self.conv0 = tf.keras.layers.Conv2D(
                30, kernel_size=(5, 1), strides=(3, 1), padding="valid", activation="relu", name="conv0"
            )
            self.dw1 = tf.keras.layers.DepthwiseConv2D(
                kernel_size=(5, 1), strides=(1, 1), padding="valid", activation=None, name="dw1"
            )
            self.pw1 = tf.keras.layers.Conv2D(
                60, kernel_size=(1, 1), strides=(1, 1), padding="valid", activation="relu", name="pw1"
            )
            self.dw2 = tf.keras.layers.DepthwiseConv2D(
                kernel_size=(9, 1), strides=(1, 1), padding="valid", activation=None, name="dw2"
            )
            self.pw2 = tf.keras.layers.Conv2D(
                60, kernel_size=(1, 1), strides=(1, 1), padding="valid", activation="relu", name="pw2"
            )
            self.dw3 = tf.keras.layers.DepthwiseConv2D(
                kernel_size=(13, 1), strides=(1, 1), padding="valid", activation=None, name="dw3"
            )
            self.pw3 = tf.keras.layers.Conv2D(
                60, kernel_size=(1, 1), strides=(1, 1), padding="valid", activation="relu", name="pw3"
            )
            self.dw4 = tf.keras.layers.DepthwiseConv2D(
                kernel_size=(21, 1), strides=(1, 1), padding="valid", activation=None, name="dw4"
            )
            self.pw4 = tf.keras.layers.Conv2D(
                60, kernel_size=(1, 1), strides=(1, 1), padding="valid", activation="relu", name="pw4"
            )
            self.dense = tf.keras.layers.Dense(1, activation="sigmoid", name="dense")

            # Layer-by-layer persistent state ring buffers
            self.state0 = tf.Variable(tf.zeros((1, 2, 1, num_mels)), trainable=False, name="stream")
            self.state1 = tf.Variable(tf.zeros((1, 4, 1, 30)), trainable=False, name="stream_1")
            self.state2 = tf.Variable(tf.zeros((1, 8, 1, 60)), trainable=False, name="stream_2")
            self.state3 = tf.Variable(tf.zeros((1, 12, 1, 60)), trainable=False, name="stream_3")
            self.state4 = tf.Variable(tf.zeros((1, 20, 1, 60)), trainable=False, name="stream_4")
            self.state5 = tf.Variable(tf.zeros((1, 4, 1, 60)), trainable=False, name="stream_5")

        @tf.function(
            input_signature=[
                tf.TensorSpec(shape=(1, slice_hops, num_mels), dtype=tf.float32, name="input_audio")
            ]
        )
        def call(self, inputs):
            x = tf.expand_dims(inputs, axis=2)  # (1, 3, 1, 40)
            c0 = tf.concat([self.state0, x], axis=1)
            self.state0.assign(c0[:, 3:, :, :])
            y0 = self.conv0(c0)

            c1 = tf.concat([self.state1, y0], axis=1)
            self.state1.assign(c1[:, 1:, :, :])
            y1 = self.pw1(self.dw1(c1))

            c2 = tf.concat([self.state2, y1], axis=1)
            self.state2.assign(c2[:, 1:, :, :])
            y2 = self.pw2(self.dw2(c2))

            c3 = tf.concat([self.state3, y2], axis=1)
            self.state3.assign(c3[:, 1:, :, :])
            y3 = self.pw3(self.dw3(c3))

            c4 = tf.concat([self.state4, y3], axis=1)
            self.state4.assign(c4[:, 1:, :, :])
            y4 = self.pw4(self.dw4(c4))

            c5 = tf.concat([self.state5, y4], axis=1)
            self.state5.assign(c5[:, 1:, :, :])

            flat = tf.reshape(c5, (1, 300))
            return self.dense(flat)

    streaming_model = StreamingMixConvMWW(name="mww_pcan_streaming_model")
    # Initialize weights by passing dummy input
    dummy_in = tf.zeros((1, slice_hops, num_mels))
    streaming_model(dummy_in)

    # Set fused weights
    streaming_model.conv0.set_weights(fold_bn(trained_model.get_layer("conv0"), trained_model.get_layer("bn0")))
    streaming_model.dw1.set_weights(trained_model.get_layer("dw1").get_weights())
    streaming_model.pw1.set_weights(fold_bn(trained_model.get_layer("pw1"), trained_model.get_layer("bn1")))
    streaming_model.dw2.set_weights(trained_model.get_layer("dw2").get_weights())
    streaming_model.pw2.set_weights(fold_bn(trained_model.get_layer("pw2"), trained_model.get_layer("bn2")))
    streaming_model.dw3.set_weights(trained_model.get_layer("dw3").get_weights())
    streaming_model.pw3.set_weights(fold_bn(trained_model.get_layer("pw3"), trained_model.get_layer("bn3")))
    streaming_model.dw4.set_weights(trained_model.get_layer("dw4").get_weights())
    streaming_model.pw4.set_weights(fold_bn(trained_model.get_layer("pw4"), trained_model.get_layer("bn4")))
    streaming_model.dense.set_weights(trained_model.get_layer("dense").get_weights())

    return streaming_model


# -----------------------------------------------------------------------------
# 4. Int8 Quantization
# -----------------------------------------------------------------------------

def convert_to_tflite_int8(
    model: tf.keras.Model,
    rep_X: np.ndarray,
    n_rep: int = 200,
) -> bytes:
    """Convert streaming model to full int8 quantized TFLite flatbuffer."""
    rng = np.random.default_rng(0)

    def representative_dataset():
        rng = np.random.default_rng(0)
        # Pin input range to [0, 255 * scale] so input quantization equals the
        # firmware int8 PCAN mapping: scale 666/256, zero point -128.
        pin = np.zeros((1, SLICE_HOPS, HOP_BINS), dtype=np.float32)
        pin[0, 0, 0] = 255.0 * PCAN_INT8_SCALE
        yield [pin]
        samples_yielded = 0
        while samples_yielded < n_rep:
            idx = rng.integers(0, rep_X.shape[0])
            win = rep_X[idx, :, :, 0]
            for h in range(0, win.shape[0] - SLICE_HOPS + 1, SLICE_HOPS):
                slice_3 = win[h : h + SLICE_HOPS, :]
                yield [slice_3[np.newaxis, ...].astype(np.float32)]
                samples_yielded += 1
                if samples_yielded >= n_rep:
                    break

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    return converter.convert()


# -----------------------------------------------------------------------------
# 5. Export Utilities (C-Array, Topology2 Conf, and sof-ctl IPC4 txt)
# -----------------------------------------------------------------------------

# SOF IPC4 ABI definitions
SOF_IPC4_ABI_MAGIC = 0x34464F53  # 'SOF4'
SOF_ABI_VERSION = (3 << 24) | (29 << 12) | 1  # 3.29.1
SOF_CTRL_CMD_BINARY = 3


def build_ipc4_abi_blob(tflite_bytes: bytes, param_id: int = 0) -> bytes:
    """Pack tflite model bytes behind a standard 32-byte struct sof_abi_hdr."""
    size = len(tflite_bytes)
    abi_header = struct.pack("<IIIIIIII", SOF_IPC4_ABI_MAGIC, param_id, size, SOF_ABI_VERSION, 0, 0, 0, 0)
    pad = (4 - (size % 4)) % 4
    return abi_header + tflite_bytes + b"\x00" * pad


def emit_model_header(
    tflite_bytes: bytes,
    out_h: Path,
    model_name: str,
) -> None:
    """Emit a self-contained built-in model header."""
    symbol_name = re.sub(r"[^a-z0-9_]", "_", model_name.lower())
    symbol = f"mww_model_data_{symbol_name}"
    guard = symbol.upper() + "_H_"
    with open(out_h, "w") as f:
        f.write("// SPDX-License-Identifier: BSD-3-Clause\n")
        f.write("// Auto-generated by sof_mww_train_pcan.py — do not edit.\n\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <cstddef>\n#include <cstdint>\n\n")
        f.write(f"alignas(16) static const unsigned char {symbol}[] = {{\n")
        line = "    "
        for i, b in enumerate(tflite_bytes):
            line += f"0x{b:02x}, "
            if (i + 1) % 12 == 0:
                f.write(line.rstrip() + "\n")
                line = "    "
        if line.strip():
            f.write(line.rstrip() + "\n")
        f.write("};\n\n")
        f.write(f"static const size_t {symbol}_size = sizeof({symbol});\n\n")
        f.write(f"#endif  // {guard}\n")


def emit_topology2_conf(
    tflite_bytes: bytes,
    out_conf: Path,
    model_name: str,
) -> None:
    """Emit Topology2 configuration blob in .conf format."""
    blob8 = build_ipc4_abi_blob(tflite_bytes)
    words = struct.unpack(f"<{len(blob8) // 4}I", blob8)
    out_conf.parent.mkdir(parents=True, exist_ok=True)
    today = datetime.date.today().strftime("%d-%b-%Y")
    data_name = f"mww_config_{model_name}"
    with open(out_conf, "w") as f:
        f.write(f"# Exported microWakeWord Model Control Words {today}\n")
        f.write(f"# Model: {model_name}\n")
        f.write(f"# Auto-generated by sof_mww_train_pcan.py — do not edit.\n")
        f.write(f'Object.Base.data."{data_name}" {{\n')
        f.write('\twords "\n')
        lines = []
        for i in range(0, len(words), 8):
            chunk = words[i : i + 8]
            lines.append(",".join(f"0x{w:08x}" for w in chunk))
        for idx, line in enumerate(lines):
            if idx == len(lines) - 1:
                f.write(f"\t\t{line}\"\n")
            else:
                f.write(f"\t\t{line},\n")
        f.write("}\n")


def emit_sofctl_ipc4_txt(
    tflite_bytes: bytes,
    out_txt: Path,
) -> None:
    """Emit sof-ctl IPC4 text blob in comma-separated uint32 CSV format."""
    size = len(tflite_bytes)
    tlv_cmd = SOF_CTRL_CMD_BINARY
    tlv_size = 32 + size
    pad = (4 - (size % 4)) % 4
    payload_padded = tflite_bytes + b"\x00" * pad
    tlv_header = struct.pack("<II", tlv_cmd, tlv_size)
    abi_header = struct.pack("<IIIIIIII", SOF_IPC4_ABI_MAGIC, 0, size, SOF_ABI_VERSION, 0, 0, 0, 0)
    blob = tlv_header + abi_header + payload_padded

    words = struct.unpack(f"<{len(blob) // 4}I", blob)
    out_txt.parent.mkdir(parents=True, exist_ok=True)
    with open(out_txt, "w") as f:
        f.write(",".join(str(w) for w in words) + "\n")


# -----------------------------------------------------------------------------
# 6. Streaming Evaluation and Confusion Matrix
# -----------------------------------------------------------------------------

def _quantize(x: np.ndarray, scale: float, zero_point: int, dtype) -> np.ndarray:
    q = np.round(x / scale) + zero_point
    info = np.iinfo(dtype)
    return np.clip(q, info.min, info.max).astype(dtype)


def _dequantize(q: np.ndarray, scale: float, zero_point: int) -> np.ndarray:
    return (q.astype(np.float32) - zero_point) * scale


def evaluate_streaming_confusion_matrix(
    tflite_path: str,
    file_map: dict[str, list[str]],
    keywords: list[str],
    threshold: float = 0.5,
    avg_len: int = 4,
    slice_hops: int = 3,
    level_db: tuple[float, float] | None = None,
    seed: int = 1,
    pool: BackgroundPool | None = None,
) -> dict:
    """Run streaming inference on all WAV files and compute the confusion matrix.

    Each clip is streamed after continuous background (embed_in_background),
    and detections count from the clip start. A detection fires when the mean
    of the last avg_len probabilities reaches threshold, as in the firmware.
    """
    rng = np.random.default_rng(seed)
    if pool is None:
        pool = BackgroundPool("")
    interp = tf.lite.Interpreter(model_path=tflite_path)
    interp.allocate_tensors()

    in_det = interp.get_input_details()[0]
    out_det = interp.get_output_details()[0]

    in_scale, in_zp = in_det.get("quantization", (0.0, 0))
    out_scale, out_zp = out_det.get("quantization", (0.0, 0))
    in_dtype = in_det["dtype"]
    out_dtype = out_det["dtype"]

    results = {}
    tp, fp, tn, fn = 0, 0, 0, 0

    for label, files in file_map.items():
        is_pos = (label in keywords)
        n_files = len(files)
        n_detected = 0
        peak_probs = []

        for f in files:
            pcm, fg_start = embed_in_background(load_wav_pcm16(f), pool, rng, level_db)
            mel = extract_pcan_features(pcm)
            if mel.shape[0] < slice_hops:
                continue

            # Reset streaming internal states
            interp.allocate_tensors()

            max_prob = 0.0
            n_hops = mel.shape[0]
            hist = np.zeros(avg_len)
            fired = False

            for s in range(0, n_hops - slice_hops + 1, slice_hops):
                chunk = mel[s : s + slice_hops][np.newaxis, ...]  # shape: (1, 3, 40)
                if np.issubdtype(in_dtype, np.integer) and in_scale > 0:
                    chunk = _quantize(chunk, in_scale, in_zp, in_dtype)

                interp.set_tensor(in_det["index"], chunk)
                interp.invoke()
                y = interp.get_tensor(out_det["index"])

                if np.issubdtype(out_dtype, np.integer) and out_scale > 0:
                    prob = float(_dequantize(y, out_scale, out_zp).flatten()[0])
                else:
                    prob = float(y.flatten()[0])

                hist = np.roll(hist, 1)
                hist[0] = prob
                if s < fg_start:
                    continue

                if prob > max_prob:
                    max_prob = prob
                if hist.mean() >= threshold:
                    fired = True

            peak_probs.append(max_prob)
            if fired:
                n_detected += 1

            if is_pos:
                if fired:
                    tp += 1
                else:
                    fn += 1
            else:
                if fired:
                    fp += 1
                else:
                    tn += 1

        results[label] = {
            "is_positive": is_pos,
            "total_files": n_files,
            "detected_files": n_detected,
            "detection_rate": (n_detected / n_files) if n_files > 0 else 0.0,
            "mean_peak_prob": float(np.mean(peak_probs)) if peak_probs else 0.0,
        }

    total_samples = tp + fp + tn + fn
    accuracy = (tp + tn) / total_samples if total_samples > 0 else 0.0
    precision = tp / (tp + fp) if (tp + fp) > 0 else 0.0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0.0
    f1 = 2 * (precision * recall) / (precision + recall) if (precision + recall) > 0 else 0.0
    fpr = fp / (fp + tn) if (fp + tn) > 0 else 0.0

    cm = {
        "tp": tp,
        "fp": fp,
        "tn": tn,
        "fn": fn,
        "accuracy": accuracy,
        "precision": precision,
        "recall": recall,
        "f1": f1,
        "fpr": fpr,
        "class_results": results,
    }
    return cm


def print_confusion_matrix_report(cm: dict, threshold: float, avg_len: int = 4) -> None:
    """Print formatted evaluation report with confusion matrix."""
    print("\n" + "=" * 70)
    print(f"microWakeWord PCAN Streaming Evaluation Report (avg of {avg_len} >= {threshold:.2f})")
    print("=" * 70)
    print(f"  {'Class':<18} {'Role':<10} {'Files':>7} {'Detected':>10} {'Rate':>8} {'Mean Peak':>10}")
    print("  " + "-" * 66)

    for label, stat in cm["class_results"].items():
        role = "Positive" if stat["is_positive"] else "Negative"
        rate_pct = stat["detection_rate"] * 100.0
        print(
            f"  {label:<18} {role:<10} {stat['total_files']:>7d} "
            f"{stat['detected_files']:>10d} {rate_pct:>7.1f}% {stat['mean_peak_prob']:>9.3f}"
        )

    print("  " + "-" * 66)
    print("\n>>> Confusion Matrix:")
    print("                      Actual Positive      Actual Negative")
    print(f"  Predicted Positive:  TP = {cm['tp']:<15d}  FP = {cm['fp']:<15d}")
    print(f"  Predicted Negative:  FN = {cm['fn']:<15d}  TN = {cm['tn']:<15d}")
    print("\n>>> Performance Summary Metrics:")
    print(f"  - Accuracy                     : {cm['accuracy'] * 100.0:6.2f}%")
    print(f"  - Precision                    : {cm['precision'] * 100.0:6.2f}%")
    print(f"  - Recall / True Positive Rate  : {cm['recall'] * 100.0:6.2f}% ({cm['tp']}/{cm['tp'] + cm['fn']})")
    print(f"  - False Positive / Alarm Rate  : {cm['fpr'] * 100.0:6.2f}% ({cm['fp']}/{cm['fp'] + cm['tn']})")
    print(f"  - F1-Score                     : {cm['f1']:.4f}")
    print("=" * 70 + "\n")


# -----------------------------------------------------------------------------
# 6. Main CLI
# -----------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wav-root", required=True, help="Directory containing <label>/*.wav files")
    parser.add_argument("--keyword", required=True, action="append", help="Target positive keyword label(s)")
    parser.add_argument("--name", default="mww_pcan_model", help="Base name for output model")
    parser.add_argument("--out-dir", default=".", help="Directory to save trained model artifacts")
    parser.add_argument("--epochs", type=int, default=30, help="Training epochs (default 30)")
    parser.add_argument("--batch-size", type=int, default=64, help="Batch size (default 64)")
    parser.add_argument("--lr", type=float, default=1e-3, help="Learning rate (default 1e-3)")
    parser.add_argument("--val-frac", type=float, default=0.2, help="Validation fraction (default 0.2)")
    parser.add_argument("--class-weight-neg", type=float, default=2.0, help="Loss multiplier for negative samples (default 2.0)")
    parser.add_argument("--threshold", type=float, default=0.5,
                        help="Detection threshold for the averaged probability (default 0.5)")
    parser.add_argument("--avg-len", type=int, default=4,
                        help="Number of inferences averaged for detection (default 4)")
    parser.add_argument("--seed", type=int, default=0, help="Random seed (default 0)")
    parser.add_argument("--frontend", choices=("sof", "tf"), default="sof",
                        help="PCAN feature frontend: sof = SOF MFCC in sof-testbench4 (default), "
                             "tf = TensorFlow audio_microfrontend")
    parser.add_argument("--testbench", default=DEFAULT_TESTBENCH, help="sof-testbench4 binary")
    parser.add_argument("--tplg", default=DEFAULT_TPLG, help="Testbench MFCC PCAN topology")
    parser.add_argument("--level-min-dbfs", type=float, default=-45.0,
                        help="Min random clip peak level in dBFS (default -45)")
    parser.add_argument("--level-max-dbfs", type=float, default=-6.0,
                        help="Max random clip peak level in dBFS (default -6)")
    parser.add_argument("--pos-level-copies", type=int, default=2,
                        help="Number of random-level copies per keyword clip (default 2)")

    args = parser.parse_args()

    global extract_pcan_features
    sof_frontend = None
    if args.frontend == "sof":
        try:
            sof_frontend = SofTestbenchFrontend(args.testbench, args.tplg)
        except FileNotFoundError as exc:
            print(f"Error: missing {exc}. Build the testbench (scripts/rebuild-testbench.sh) and the "
                  "sof-hda-benchmark-mfccmel40_10ms_pcan32 topology, or use --frontend tf.",
                  file=sys.stderr)
            return 1
        extract_pcan_features = sof_frontend
    print(f">>> PCAN frontend: {args.frontend}")
    level_db = (args.level_min_dbfs, args.level_max_dbfs)

    try:
        return run(args, level_db)
    finally:
        if sof_frontend is not None:
            sof_frontend.close()


def run(args: argparse.Namespace, level_db: tuple[float, float]) -> int:
    labels = []
    # Discover available standard negative categories present in wav_root
    for neg_dir in ("ambient", "silence", "unknown", "babble", "noise"):
        if os.path.isdir(os.path.join(args.wav_root, neg_dir)):
            labels.append(neg_dir)
    if not labels:
        labels = ["silence", "unknown"]
    labels += args.keyword

    print(f">>> [PCAN Pipeline] Loading WAVs from {args.wav_root} for classes: {labels}")
    pool = BackgroundPool(args.wav_root)
    X, y, file_map = load_pcan_dataset(args.wav_root, labels=labels, level_db=level_db,
                                       pos_level_copies=args.pos_level_copies, seed=args.seed,
                                       pool=pool)

    if X.shape[0] == 0:
        print(f"Error: No audio samples found under {args.wav_root}.", file=sys.stderr)
        return 1

    print(f">>> Total extracted window samples: {X.shape[0]} (pos: {np.sum(y == 1)}, neg: {np.sum(y == 0)})")

    X_train, y_train, X_val, y_val = split_train_val(X, y, val_frac=args.val_frac, seed=args.seed)
    print(f">>> Train split: {X_train.shape[0]} samples, Val split: {X_val.shape[0]} samples")

    model = build_mww_model(window_hops=WINDOW_HOPS, num_mels=HOP_BINS)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=args.lr),
        loss=tf.keras.losses.BinaryCrossentropy(),
        metrics=["accuracy", tf.keras.metrics.Precision(name="prec"), tf.keras.metrics.Recall(name="rec")],
    )

    class_weights = {0: float(args.class_weight_neg), 1: 1.0}
    print(f">>> Training base model with PCAN features (class_weight={class_weights})...")
    model.fit(
        X_train,
        y_train,
        validation_data=(X_val, y_val),
        epochs=args.epochs,
        batch_size=args.batch_size,
        class_weight=class_weights,
        verbose=2,
    )

    print(">>> Building fused layer-by-layer streaming inference model...")
    streaming_model = build_streaming_inference_model(model, slice_hops=SLICE_HOPS, num_mels=HOP_BINS)

    print(">>> Quantizing to int8 with representative dataset...")
    tflite_bytes = convert_to_tflite_int8(streaming_model, rep_X=X_train)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. Save .tflite flatbuffer
    tflite_path = out_dir / f"{args.name}_quantized_model.tflite"
    with open(tflite_path, "wb") as f:
        f.write(tflite_bytes)
    print(f">>> Saved int8 quantized model: {tflite_path} ({len(tflite_bytes)} bytes)")

    # 2. Save built-in model header
    header_name = re.sub(r"[^a-z0-9_]", "_", args.name.lower())
    h_path = out_dir / f"mww_model_data_{header_name}.h"
    emit_model_header(tflite_bytes, h_path, args.name)
    print(f">>> Emitted built-in model header to {h_path}")

    # 3. Save Topology2 config
    conf_path = out_dir / f"{args.name}.conf"
    emit_topology2_conf(tflite_bytes, conf_path, model_name=args.name)
    print(f">>> Emitted Topology2 conf to {conf_path}")

    # 4. Save sof-ctl IPC4 text
    txt_path = out_dir / f"{args.name}.txt"
    emit_sofctl_ipc4_txt(tflite_bytes, txt_path)
    print(f">>> Emitted sof-ctl text to {txt_path}")

    print(">>> Running streaming evaluation and generating Confusion Matrix...")
    cm = evaluate_streaming_confusion_matrix(
        str(tflite_path),
        file_map=file_map,
        keywords=args.keyword,
        threshold=args.threshold,
        avg_len=args.avg_len,
        slice_hops=SLICE_HOPS,
        level_db=level_db,
        pool=pool,
    )
    print_confusion_matrix_report(cm, threshold=args.threshold, avg_len=args.avg_len)

    babble_wav = os.path.expanduser("~/.cache/demand_cafeteria/PCAFETER/ch01.wav")
    if os.path.exists(babble_wav):
        print(f"\n>>> Verifying model on 60 seconds of continuous cafeteria babble (with 33 startup warmup steps): {babble_wav}")
        with wave.open(babble_wav, "rb") as bw:
            raw_babble = np.frombuffer(bw.readframes(16000 * 60), dtype=np.int16)
        mel_b = extract_pcan_features(raw_babble)
        interp = tf.lite.Interpreter(model_path=str(tflite_path))
        interp.allocate_tensors()
        in_det = interp.get_input_details()[0]
        out_det = interp.get_output_details()[0]
        in_scale, in_zp = in_det.get("quantization", (0.0, 0))
        out_scale, out_zp = out_det.get("quantization", (0.0, 0))

        # Warm up streaming delay lines (matches firmware MWW_WARMUP_INFERENCES = 33)
        for _ in range(33):
            silence_slice = np.full((1, SLICE_HOPS, HOP_BINS), in_zp if in_scale > 0 else -128.0, dtype=in_det["dtype"])
            interp.set_tensor(in_det["index"], silence_slice)
            interp.invoke()

        b_max_prob = 0.0
        b_hist = np.zeros(args.avg_len)
        b_fired = False
        for s in range(0, mel_b.shape[0] - SLICE_HOPS + 1, SLICE_HOPS):
            chk = mel_b[s : s + SLICE_HOPS][np.newaxis, ...]
            if np.issubdtype(in_det["dtype"], np.integer) and in_scale > 0:
                chk = _quantize(chk, in_scale, in_zp, in_det["dtype"])
            interp.set_tensor(in_det["index"], chk)
            interp.invoke()
            y_raw = interp.get_tensor(out_det["index"])
            if np.issubdtype(out_det["dtype"], np.integer) and out_scale > 0:
                p_val = float(_dequantize(y_raw, out_scale, out_zp).flatten()[0])
            else:
                p_val = float(y_raw.flatten()[0])
            if p_val > b_max_prob:
                b_max_prob = p_val
            b_hist = np.roll(b_hist, 1)
            b_hist[0] = p_val
            if b_hist.mean() >= args.threshold:
                b_fired = True
        print(f">>> Result on continuous cafeteria babble: Peak Prob = {b_max_prob * 100.0:.1f}%, Triggered = {'YES (FAIL)' if b_fired else 'NO (PASS)'}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
