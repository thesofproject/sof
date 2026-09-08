# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
Off-device streaming verifier for microWakeWord (MWW) quantized models.

Evaluates an int8 streaming TFLite model on testbench-emitted MFCC feature
files (.raw). Feeds 3 hops (MWW_FEATURE_SLICE_COUNT = 3) per Invoke() step,
tracking streaming internal ring buffers, and checks whether detection
(prob >= threshold) fires appropriately on positive and negative recordings.

Example:
    python3 sof_mww_verify.py \\
        --tflite ~/wov/model/mww_model_quantized_model.tflite \\
        --feat-root ~/wov/feats \\
        --keyword hey_intel
"""

from __future__ import annotations

import argparse
import glob
import os
import sys

import numpy as np

import sof_mww_dataset as mww_dataset

try:
    import tflite_runtime.interpreter as tflite  # type: ignore
except ImportError:
    try:
        from tensorflow.lite.python.interpreter import Interpreter as _Interp

        class tflite:  # type: ignore
            Interpreter = _Interp
    except ImportError:
        import tensorflow as tf
        tflite = tf.lite


def _quantize(x: np.ndarray, scale: float, zero_point: int, dtype) -> np.ndarray:
    q = np.round(x / scale) + zero_point
    info = np.iinfo(dtype)
    return np.clip(q, info.min, info.max).astype(dtype)


def _dequantize(q: np.ndarray, scale: float, zero_point: int) -> np.ndarray:
    return (q.astype(np.float32) - zero_point) * scale


def run_streaming_verification(
    model_path: str,
    feat_root: str,
    keywords: list[str],
    threshold: float = 0.65,
    consecutive: int = 2,
    slice_hops: int = 3,
) -> dict:
    """Run streaming inference on all .raw files in feat_root."""
    interp = tflite.Interpreter(model_path=model_path)
    interp.allocate_tensors()

    in_det = interp.get_input_details()[0]
    out_det = interp.get_output_details()[0]

    in_scale, in_zp = in_det.get("quantization", (0.0, 0))
    out_scale, out_zp = out_det.get("quantization", (0.0, 0))
    in_dtype = in_det["dtype"]
    out_dtype = out_det["dtype"]

    results = {}
    all_labels = ["silence", "unknown"] + keywords

    for label in all_labels:
        label_dir = os.path.join(feat_root, label)
        raw_files = sorted(glob.glob(os.path.join(label_dir, "*.raw")))
        is_pos = (label in keywords)

        n_files = len(raw_files)
        n_detected = 0
        peak_probs = []

        for f in raw_files:
            mel, vad = mww_dataset.load_raw_hops(f)
            if mel.shape[0] < slice_hops:
                continue

            # Apply soft AGC across the whole file, freezing release during speech (vad == 1)
            mel_agc = mww_dataset.apply_soft_agc(mel, vad=vad)

            # Reset interpreter state variables before each audio stream
            if hasattr(interp, "reset_all_variables"):
                interp.reset_all_variables()
            else:
                interp.allocate_tensors()

            max_prob = 0.0
            consec = 0
            detected = False
            n_hops = mel_agc.shape[0]

            for s in range(0, n_hops - slice_hops + 1, slice_hops):
                chunk = mel_agc[s : s + slice_hops][np.newaxis, ...]  # shape (1, 3, 40)
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

                if prob >= threshold:
                    consec += 1
                    if consec >= consecutive:
                        detected = True
                else:
                    consec = 0

            peak_probs.append(max_prob)
            if detected:
                n_detected += 1

        results[label] = {
            "is_positive": is_pos,
            "total_files": n_files,
            "detected_files": n_detected,
            "detection_rate": (n_detected / n_files) if n_files > 0 else 0.0,
            "mean_peak_prob": float(np.mean(peak_probs)) if peak_probs else 0.0,
        }

    return results


def print_report(results: dict, threshold: float) -> None:
    print("\n" + "=" * 65)
    print(f"microWakeWord Streaming Verification Report (Threshold: {threshold:.2f})")
    print("=" * 65)
    print(f"  {'Class':<18} {'Role':<10} {'Files':>7} {'Detected':>10} {'Rate':>8} {'Mean Peak':>10}")
    print("  " + "-" * 61)

    total_pos = 0
    detected_pos = 0
    total_neg = 0
    detected_neg = 0

    for label, stat in results.items():
        role = "Positive" if stat["is_positive"] else "Negative"
        rate_pct = stat["detection_rate"] * 100.0
        print(
            f"  {label:<18} {role:<10} {stat['total_files']:>7d} "
            f"{stat['detected_files']:>10d} {rate_pct:>7.1f}% {stat['mean_peak_prob']:>9.3f}"
        )
        if stat["is_positive"]:
            total_pos += stat["total_files"]
            detected_pos += stat["detected_files"]
        else:
            total_neg += stat["total_files"]
            detected_neg += stat["detected_files"]

    print("  " + "-" * 61)
    recall = (detected_pos / total_pos) if total_pos > 0 else 0.0
    far = (detected_neg / total_neg) if total_neg > 0 else 0.0
    prec = (detected_pos / (detected_pos + detected_neg)) if (detected_pos + detected_neg) > 0 else 0.0

    print(f"  Overall Wake Word Recall (True Positive Rate) : {recall * 100.0:6.2f}% ({detected_pos}/{total_pos})")
    print(f"  Overall False Alarm Rate (False Positive Rate): {far * 100.0:6.2f}% ({detected_neg}/{total_neg})")
    print(f"  Precision                                     : {prec * 100.0:6.2f}%")
    print("=" * 65 + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tflite", required=True, help="Path to int8 .tflite model")
    parser.add_argument("--feat-root", required=True, help="Directory containing <label>/*.raw features")
    parser.add_argument("--keyword", required=True, action="append", help="Target keyword label(s)")
    parser.add_argument("--threshold", type=float, default=0.65, help="Detection threshold (default 0.65)")
    parser.add_argument("--consecutive", type=int, default=2, help="Consecutive detections required (default 2)")

    args = parser.parse_args()
    results = run_streaming_verification(
        model_path=args.tflite,
        feat_root=args.feat_root,
        keywords=args.keyword,
        threshold=args.threshold,
        consecutive=args.consecutive,
    )
    print_report(results, threshold=args.threshold)
    return 0


if __name__ == "__main__":
    sys.exit(main())
