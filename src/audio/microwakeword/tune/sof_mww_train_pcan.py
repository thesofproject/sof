# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
All-in-one Python reference training pipeline for microWakeWord (MWW) using PCAN.

Uses TensorFlow's audio_microfrontend (Per-Channel Auto-Gain Normalization / PCAN)
to extract 40-bin mel features directly from WAV datasets (e.g. ~/wov/wavs/{silence,unknown,<keyword>}),
trains the streaming MixConv neural network, int8 quantizes to TFLite, and runs a streaming
evaluation to print a full confusion matrix and detection performance report.

Used to benchmark and compare PCAN frontend performance against SOF MFCC testbench features.

Example:
    python3 sof_mww_train_pcan.py \
        --wav-root ~/wov/wavs \
        --keyword hey_jarvis \
        --out-dir ~/wov/model_pcan \
        --epochs 30
"""

from __future__ import annotations

import argparse
import glob
import os
import sys
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
WINDOW_HOPS = 100  # 1.0 second at 10 ms hop stride
SLICE_HOPS = 3     # 30 ms streaming chunk size (MWW_FEATURE_SLICE_COUNT)


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


def extract_pcan_features(
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


# -----------------------------------------------------------------------------
# 2. Window Slicing and Dataset Loading
# -----------------------------------------------------------------------------

def slice_into_windows(
    mel: np.ndarray,
    window_hops: int = WINDOW_HOPS,
    hop_step: int = 10,
) -> np.ndarray:
    """Slice (N_hops, 40) into (M, window_hops, 40) overlapping windows."""
    n_hops = mel.shape[0]
    if n_hops < window_hops:
        pad = np.zeros((window_hops - n_hops, HOP_BINS), dtype=mel.dtype)
        return np.concatenate([mel, pad], axis=0)[np.newaxis, ...]

    starts = list(range(0, n_hops - window_hops + 1, hop_step))
    if not starts or starts[-1] != (n_hops - window_hops):
        starts.append(n_hops - window_hops)
    windows = [mel[s : s + window_hops] for s in starts]
    return np.stack(windows, axis=0)


def load_pcan_dataset(
    wav_root: str,
    labels: list[str],
    window_hops: int = WINDOW_HOPS,
    hop_step_keyword: int = 5,
    hop_step_negative: int = 20,
    gain_aug_db_min: float = -30.0,
    gain_aug_db_max: float = 6.0,
    seed: int = 0,
) -> tuple[np.ndarray, np.ndarray, dict[str, list[str]]]:
    """Extract PCAN features for all WAVs and slice into dataset windows.

    Returns:
      X: (N, window_hops, 40, 1) float32 tensor
      y: (N,) binary labels (0 for negative, 1 for positive keyword)
      file_map: Dict mapping label -> list of WAV paths
    """
    rng = np.random.default_rng(seed)
    all_X: list[np.ndarray] = []
    all_y: list[int] = []
    file_map: dict[str, list[str]] = {}

    for label in labels:
        label_dir = os.path.join(wav_root, label)
        wav_files = sorted(glob.glob(os.path.join(label_dir, "*.wav")))
        file_map[label] = wav_files

        if not wav_files:
            print(f"Warning: no .wav files found in {label_dir}", file=sys.stderr)
            continue

        is_keyword = (label not in ("silence", "unknown", "noise", "background"))
        step = hop_step_keyword if is_keyword else hop_step_negative
        y_val = 1 if is_keyword else 0

        for f in wav_files:
            pcm = load_wav_pcm16(f)
            mel = extract_pcan_features(pcm)
            if mel.shape[0] == 0:
                continue

            windows = slice_into_windows(mel, window_hops=window_hops, hop_step=step)

            # Random gain augmentation
            if gain_aug_db_min is not None and gain_aug_db_max is not None:
                gains_db = rng.uniform(gain_aug_db_min, gain_aug_db_max, size=(windows.shape[0], 1, 1))
                gains_lin = gains_db * 0.1
                windows = windows + gains_lin

            for w in windows:
                all_X.append(w)
                all_y.append(y_val)

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

    # Layer 4: MixConv DW(4) + PW(60) + BN + ReLU with causal left padding of 3 steps
    x = tf.keras.layers.ZeroPadding2D(padding=((3, 0), (0, 0)), name="pad4")(x)
    x = tf.keras.layers.DepthwiseConv2D(
        kernel_size=(4, 1),
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
                kernel_size=(4, 1), strides=(1, 1), padding="valid", activation=None, name="dw4"
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
            self.state4 = tf.Variable(tf.zeros((1, 3, 1, 60)), trainable=False, name="stream_4")
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
# 5. Streaming Evaluation and Confusion Matrix
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
    slice_hops: int = 3,
) -> dict:
    """Run streaming inference on all WAV files using PCAN features and compute confusion matrix."""
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
            pcm = load_wav_pcm16(f)
            mel = extract_pcan_features(pcm)
            if mel.shape[0] < slice_hops:
                continue

            # Reset streaming internal states
            interp.allocate_tensors()

            max_prob = 0.0
            n_hops = mel.shape[0]

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

                if prob > max_prob:
                    max_prob = prob

            peak_probs.append(max_prob)
            fired = (max_prob >= threshold)
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


def print_confusion_matrix_report(cm: dict, threshold: float) -> None:
    """Print formatted evaluation report with confusion matrix."""
    print("\n" + "=" * 70)
    print(f"microWakeWord PCAN Streaming Evaluation Report (Threshold: {threshold:.2f})")
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
    parser.add_argument("--class-weight-neg", type=float, default=5.0, help="Loss multiplier for negative samples (default 5.0)")
    parser.add_argument("--threshold", type=float, default=0.5, help="Streaming detection threshold (default 0.5)")
    parser.add_argument("--seed", type=int, default=0, help="Random seed (default 0)")

    args = parser.parse_args()
    labels = ["silence", "unknown"] + args.keyword

    print(f">>> [PCAN Pipeline] Loading WAVs from {args.wav_root} for classes: {labels}")
    X, y, file_map = load_pcan_dataset(args.wav_root, labels=labels, seed=args.seed)

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

    print(">>> Training base model with PCAN features...")
    model.fit(
        X_train,
        y_train,
        validation_data=(X_val, y_val),
        epochs=args.epochs,
        batch_size=args.batch_size,
        verbose=2,
    )

    print(">>> Building fused layer-by-layer streaming inference model...")
    streaming_model = build_streaming_inference_model(model, slice_hops=SLICE_HOPS, num_mels=HOP_BINS)

    print(">>> Quantizing to int8 with representative dataset...")
    tflite_bytes = convert_to_tflite_int8(streaming_model, rep_X=X_train)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    tflite_path = out_dir / f"{args.name}_quantized_model.tflite"

    with open(tflite_path, "wb") as f:
        f.write(tflite_bytes)
    print(f">>> Saved int8 quantized model: {tflite_path} ({len(tflite_bytes)} bytes)")

    print(">>> Running streaming evaluation and generating Confusion Matrix...")
    cm = evaluate_streaming_confusion_matrix(
        str(tflite_path),
        file_map=file_map,
        keywords=args.keyword,
        threshold=args.threshold,
        slice_hops=SLICE_HOPS,
    )
    print_confusion_matrix_report(cm, threshold=args.threshold)

    return 0


if __name__ == "__main__":
    sys.exit(main())
