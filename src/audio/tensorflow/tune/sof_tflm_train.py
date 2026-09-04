# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
Train a tflite-micro ``tiny_conv`` DS-CNN wake-word model against SOF mel40
features and emit a drop-in replacement for
``src/audio/tensorflow/sof_tflm_quantized_model_data.{cc,h}``.

Architecture matches ``MicroMutableOpResolver<4>`` in
``src/audio/tensorflow/speech.cc``:

    Reshape -> DepthwiseConv2D -> FullyConnected -> Softmax

Consumes features produced by ``sof_mfcc_extract_features.sh`` via
``sof_tflm_dataset.py``. Writes ``<out_dir>/<name>_quantized_model.tflite``
and ``<out_dir>/<name>_labels.txt`` as archive artifacts, plus the fixed
``<out_dir>/sof_tflm_quantized_model_data.{cc,h}`` pair with the C symbol
``g_sof_tflm_quantized_model_data`` that ``speech.cc`` #includes, and
``<out_dir>/sof_tflm_labels.h`` (``TFLM_CATEGORY_COUNT`` / ``_DATA``
macros) that ``speech.h`` #includes. Both C symbol and header names are
fixed so retraining with any keyword list is a drop-in replacement.

Constraints preserved for the tflmcly KPB rule:
    label index 0 = silence
    label index 1 = unknown
    label index >= 2 = keyword(s) that trigger KPB_EVENT_BEGIN_DRAINING

Requires TensorFlow >= 2.10 in an isolated venv (see the module README).
"""

from __future__ import annotations

import argparse
import datetime
import os
import struct
import sys
from pathlib import Path

import numpy as np

import sof_tflm_dataset as mel40_dataset

try:
    import tensorflow as tf
except ImportError as exc:  # pragma: no cover - only hit outside the venv
    print(
        "tensorflow not importable; activate the tflm-train venv first",
        file=sys.stderr,
    )
    raise SystemExit(1) from exc


def build_tiny_conv(num_classes: int) -> tf.keras.Model:
    """Match tflite-micro's ``create_tiny_conv_model`` topology."""
    inputs = tf.keras.Input(
        shape=(mel40_dataset.WINDOW_HOPS, mel40_dataset.HOP_BINS, 1),
        name="mel40",
    )
    x = tf.keras.layers.DepthwiseConv2D(
        depth_multiplier=8,
        kernel_size=(10, 8),
        strides=(2, 2),
        padding="same",
        activation="relu",
        name="dwconv",
    )(inputs)
    x = tf.keras.layers.Dropout(0.5)(x)
    x = tf.keras.layers.Flatten()(x)
    logits = tf.keras.layers.Dense(num_classes, name="logits")(x)
    outputs = tf.keras.layers.Softmax(name="prob")(logits)
    return tf.keras.Model(inputs=inputs, outputs=outputs, name="tiny_conv")


def split_train_val(
    X: np.ndarray,
    y: np.ndarray,
    val_frac: float,
    seed: int,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Stratified split with a fixed RNG so runs are reproducible."""
    rng = np.random.default_rng(seed)
    idx = np.arange(y.size)
    train_mask = np.zeros(y.size, dtype=bool)
    for c in np.unique(y):
        cls_idx = idx[y == c]
        rng.shuffle(cls_idx)
        cut = int(round(cls_idx.size * (1.0 - val_frac)))
        train_mask[cls_idx[:cut]] = True
    return X[train_mask], y[train_mask], X[~train_mask], y[~train_mask]


def representative_dataset(
    X: np.ndarray,
    y: np.ndarray,
    n_samples: int,
):
    """Yield calibration inputs stratified across classes.

    Uniform random picks under-represent minority classes (e.g. a positive
    keyword class with ~5% of the data), which biases int8 activation
    scales toward the majority class and destroys keyword recall after
    quantization. Pick roughly ``n_samples / n_classes`` per class.
    """
    rng = np.random.default_rng(0)
    classes = np.unique(y)
    per_class = max(1, n_samples // classes.size)
    picks: list[int] = []
    for c in classes:
        cls_idx = np.where(y == c)[0]
        take = min(per_class, cls_idx.size)
        picks.extend(rng.choice(cls_idx, size=take, replace=False).tolist())
    rng.shuffle(picks)
    for i in picks:
        yield [X[i : i + 1].astype(np.float32)]


def convert_to_tflite_int8(
    model: tf.keras.Model,
    rep_X: np.ndarray,
    rep_y: np.ndarray,
    n_rep: int,
) -> bytes:
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = lambda: representative_dataset(
        rep_X, rep_y, n_rep
    )
    converter.target_spec.supported_ops = [
        tf.lite.OpsSet.TFLITE_BUILTINS_INT8
    ]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    return converter.convert()


def set_tflite_model_description(tflite_bytes: bytes, description: str) -> bytes:
    """Embed comma-separated labels into TFLite FlatBuffer Model.description."""
    try:
        from tensorflow.lite.python import schema_py_generated as schema_fb
        import flatbuffers

        model_obj = schema_fb.ModelT.InitFromObj(
            schema_fb.Model.GetRootAsModel(tflite_bytes, 0)
        )
        model_obj.description = description
        builder = flatbuffers.Builder(len(tflite_bytes) + len(description) + 256)
        builder.Finish(model_obj.Pack(builder), "TFL3")
        return bytes(builder.Output())
    except Exception as exc:
        print(f"    [WARN] could not embed description in tflite FlatBuffer: {exc}")
        return tflite_bytes


def emit_c_array(
    tflite_bytes: bytes,
    out_cc: Path,
    out_h: Path,
    symbol: str,
) -> None:
    """Emit the same layout tflite-micro's shipped model headers use."""
    guard = symbol.upper() + "_H_"
    with open(out_h, "w") as f:
        f.write(f"// SPDX-License-Identifier: BSD-3-Clause\n")
        f.write(f"// Auto-generated by sof_tflm_train.py — do not edit.\n\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <cstddef>\n#include <cstdint>\n\n")
        f.write(f"extern const unsigned char {symbol}[];\n")
        f.write(f"extern const size_t {symbol}_size;\n\n")
        f.write(f"#endif  // {guard}\n")

    with open(out_cc, "w") as f:
        f.write("// SPDX-License-Identifier: BSD-3-Clause\n")
        f.write("// Auto-generated by sof_tflm_train.py — do not edit.\n\n")
        f.write(f'#include "{out_h.name}"\n\n')
        # Force 8-byte alignment to match tflite-micro flatbuffer expectations.
        f.write(
            f"alignas(8) const unsigned char {symbol}[] = {{\n"
        )
        line = "    "
        for i, b in enumerate(tflite_bytes):
            line += f"0x{b:02x}, "
            if (i + 1) % 12 == 0:
                f.write(line.rstrip() + "\n")
                line = "    "
        if line.strip():
            f.write(line.rstrip() + "\n")
        f.write("};\n\n")
        f.write(f"const size_t {symbol}_size = {len(tflite_bytes)};\n")


def emit_labels_header(labels: list[str], out_h: Path) -> None:
    """Emit sof_tflm_labels.h with TFLM_CATEGORY_COUNT / _DATA macros so
    speech.h picks the retrained label set up on rebuild without any
    manual edit."""
    guard = "SOF_TFLM_LABELS_H_"
    data = ", ".join(f'"{lbl}"' for lbl in labels)
    with open(out_h, "w") as f:
        f.write("// SPDX-License-Identifier: BSD-3-Clause\n")
        f.write("// Auto-generated by sof_tflm_train.py — do not edit.\n\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write(f"#define TFLM_CATEGORY_COUNT  {len(labels)}\n")
        f.write(f"#define TFLM_CATEGORY_DATA   {{ {data}, }}\n\n")
        f.write(f"#endif  // {guard}\n")


# SOF IPC4 ABI definitions for binary control blobs
SOF_IPC4_ABI_MAGIC = 0x34464F53  # 'SOF4' in little-endian
SOF_ABI_VERSION = (3 << 24) | (29 << 12) | 1  # 3.29.1 (0x0301d001)
SOF_CTRL_CMD_BINARY = 3


def build_ipc4_abi_blob(tflite_bytes: bytes, param_id: int = 0) -> bytes:
    """Pack tflite model bytes behind a standard 32-byte struct sof_abi_hdr."""
    size = len(tflite_bytes)
    abi_header = struct.pack("<IIIIIIII", SOF_IPC4_ABI_MAGIC, param_id, size, SOF_ABI_VERSION, 0, 0, 0, 0)
    pad = (4 - (size % 4)) % 4
    return abi_header + tflite_bytes + b"\x00" * pad


def emit_topology2_conf(
    tflite_bytes: bytes,
    out_conf: Path,
    model_name: str,
    labels: list[str],
) -> None:
    """Emit Topology2 configuration blob in .conf format for inclusion to topology."""
    blob8 = build_ipc4_abi_blob(tflite_bytes)
    words = struct.unpack(f"<{len(blob8) // 4}I", blob8)
    out_conf.parent.mkdir(parents=True, exist_ok=True)
    today = datetime.date.today().strftime("%d-%b-%Y")
    labels_str = ", ".join(labels)
    data_name = f"tflm_config_{model_name}"
    with open(out_conf, "w") as f:
        f.write(f"# Exported TFLM Model Control Words {today}\n")
        f.write(f"# Model: {model_name} (classes: {labels_str})\n")
        f.write(f"# Auto-generated by sof_tflm_train.py — do not edit.\n")
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
    words = [tlv_cmd, tlv_size, SOF_IPC4_ABI_MAGIC, 0, size, SOF_ABI_VERSION, 0, 0, 0, 0]
    payload_words = list(struct.unpack(f"<{len(payload_padded) // 4}I", payload_padded))
    all_words = words + payload_words
    out_txt.parent.mkdir(parents=True, exist_ok=True)
    with open(out_txt, "w") as f:
        f.write(",".join(str(w) for w in all_words) + "\n")


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--feat-root", required=True)
    ap.add_argument(
        "--labels",
        nargs="+",
        required=True,
        help="Class labels in index order; put silence first, unknown second.",
    )
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--name", default=None,
                    help="Base name for the archive .tflite and _labels.txt "
                         "artifacts (the C symbol and header names are fixed "
                         "at g_sof_tflm_quantized_model_data and "
                         "sof_tflm_quantized_model_data.{cc,h}). Defaults "
                         "to the first non-{silence,unknown} label, or "
                         "'wov_model' if none is present.")
    ap.add_argument("--epochs", type=int, default=25)
    ap.add_argument("--batch-size", type=int, default=128)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--val-frac", type=float, default=0.15)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--rep-samples", type=int, default=1500,
                    help="Total calibration samples for int8 (stratified per class).")
    ap.add_argument("--window-hop-step", type=int, default=None,
                    help="Windowing stride in hops (default: non-overlapping).")
    ap.add_argument("--feature-clip-min", type=float, default=-1.0,
                    help="Lower bound applied to features before training/calibration.")
    ap.add_argument("--feature-clip-max", type=float, default=1.0,
                    help="Upper bound applied to features. The symmetric "
                         "[-1.0, +1.0] log10 window matches the runtime C clip; "
                         "features map directly to [-1, +1] before int8 quantization.")
    ap.add_argument("--gain-aug-db-min", type=float, default=-8.0,
                    help="Min gain jitter in dB (default -8.0). Set equal to --gain-aug-db-max to disable.")
    ap.add_argument("--gain-aug-db-max", type=float, default=3.0,
                    help="Max gain jitter in dB (default 3.0).")
    ap.add_argument("--soft-agc", action=argparse.BooleanOptionalAction, default=True,
                    help="Apply the soft mel-log AGC (target +2.5 dB, floor -20 dB, "
                         "instant attack against the +1.0 log10 clip, dual-rate "
                         "release: 0.5 dB/s silence, 0.05 dB/s speech) to training "
                         "features so they match what the runtime C AGC produces. "
                         "Use --no-soft-agc to disable.")
    return ap.parse_args()


def main() -> int:
    args = parse_args()
    reserved = {"silence", "unknown"}
    keywords = [lbl for lbl in args.labels if lbl not in reserved]

    if not args.name:
        # Default name contains all trained keywords joined with underscore
        args.name = "_".join(keywords) if keywords else "wov_model"
        print(f">>> --name not set, using {args.name!r}")
    tf.keras.utils.set_random_seed(args.seed)

    print(f">>> Loading features from {args.feat_root}")
    X, y = mel40_dataset.load_dataset(
        args.feat_root,
        args.labels,
        gain_aug_db_min=args.gain_aug_db_min,
        gain_aug_db_max=args.gain_aug_db_max,
        seed=args.seed,
        hop_step=args.window_hop_step,
    )
    print(f"    X={X.shape} y={y.shape} counts={np.bincount(y).tolist()}")

    if args.soft_agc:
        agc_pre_min, agc_pre_max = float(X.min()), float(X.max())
        X = mel40_dataset.apply_soft_agc(X)
        print(f"    soft AGC applied (target=+2.5 dB, floor=-20 dB, release=0.5 dB/s): "
              f"range {agc_pre_min:.3f}..{agc_pre_max:.3f} -> "
              f"{float(X.min()):.3f}..{float(X.max()):.3f}")

    if args.feature_clip_min is not None or args.feature_clip_max is not None:
        pre_min, pre_max = float(X.min()), float(X.max())
        X = np.clip(X, args.feature_clip_min, args.feature_clip_max)
        print(f"    clip [{args.feature_clip_min}, {args.feature_clip_max}]: "
              f"range {pre_min:.3f}..{pre_max:.3f} -> {float(X.min()):.3f}..{float(X.max()):.3f}")

    X_tr, y_tr, X_va, y_va = split_train_val(X, y, args.val_frac, args.seed)
    print(f"    train={X_tr.shape[0]} val={X_va.shape[0]}")

    n_classes = len(args.labels)
    model = build_tiny_conv(n_classes)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(args.lr),
        loss=tf.keras.losses.SparseCategoricalCrossentropy(),
        metrics=["accuracy"],
    )
    model.summary(print_fn=lambda s: print("    " + s))

    model.fit(
        X_tr, y_tr,
        validation_data=(X_va, y_va),
        batch_size=args.batch_size,
        epochs=args.epochs,
        verbose=2,
    )

    print(">>> Converting to int8 tflite")
    tflite_bytes = convert_to_tflite_int8(model, X_tr, y_tr, args.rep_samples)
    tflite_bytes = set_tflite_model_description(tflite_bytes, ",".join(args.labels))

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    tflite_path = out_dir / f"{args.name}_quantized_model.tflite"
    tflite_path.write_bytes(tflite_bytes)
    print(f"    wrote {tflite_path} ({len(tflite_bytes)} bytes)")

    # C symbol + header names are fixed so speech.cc never changes when the
    # keyword list changes. --name only picks the archive basename for the
    # standalone .tflite and _labels.txt artifacts.
    symbol = "g_sof_tflm_quantized_model_data"
    out_cc = out_dir / "sof_tflm_quantized_model_data.cc"
    out_h = out_dir / "sof_tflm_quantized_model_data.h"
    emit_c_array(tflite_bytes, out_cc, out_h, symbol)
    print(f"    wrote {out_cc}")
    print(f"    wrote {out_h}")

    labels_h = out_dir / "sof_tflm_labels.h"
    emit_labels_header(args.labels, labels_h)
    print(f"    wrote {labels_h}")

    labels_path = out_dir / f"{args.name}_labels.txt"
    labels_path.write_text("\n".join(args.labels) + "\n")
    print(f"    wrote {labels_path} (archive copy of the label list)")

    # Export Topology2 config blob (.conf) and sof-ctl IPC4 text blob (.txt)
    out_conf = out_dir / f"{args.name}.conf"
    emit_topology2_conf(tflite_bytes, out_conf, args.name, args.labels)
    print(f"    wrote {out_conf}")

    out_txt = out_dir / f"{args.name}.txt"
    emit_sofctl_ipc4_txt(tflite_bytes, out_txt)
    print(f"    wrote {out_txt}")

    # Also export directly to SOF tools directories if present in workspace/repo
    script_dir = Path(__file__).resolve().parent
    repo_root = None
    for parent in script_dir.parents:
        if (parent / "tools/topology/topology2").is_dir():
            repo_root = parent
            break
    if repo_root is None and "SOF_WORKSPACE" in os.environ:
        candidate = Path(os.environ["SOF_WORKSPACE"]) / "sof"
        if (candidate / "tools/topology/topology2").is_dir():
            repo_root = candidate

    if repo_root is not None:
        tplg_tflm_dir = repo_root / "tools/topology/topology2/include/components/tflm"
        emit_topology2_conf(tflite_bytes, tplg_tflm_dir / f"{args.name}.conf", args.name, args.labels)
        print(f"    exported {tplg_tflm_dir / f'{args.name}.conf'}")

        ctl_tflm_dir = repo_root / "tools/ctl/ipc4/tflm"
        emit_sofctl_ipc4_txt(tflite_bytes, ctl_tflm_dir / f"{args.name}.txt")
        print(f"    exported {ctl_tflm_dir / f'{args.name}.txt'}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
