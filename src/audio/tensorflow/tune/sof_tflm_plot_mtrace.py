#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2026, Intel Corporation. All rights reserved.

"""
Visualize TensorFlow Lite Micro (TFLM) streaming diagnostics from SOF mtrace logs.

Parses log lines produced by CONFIG_COMP_TENSORFLOW_DEBUG_TRACE:
  [DBG hop <n>] vad=<0|1> E=<energy> Ne=<noise_energy>
                mel_min=<min> mel_max=<max>
                f_min=<min> f_max=<max> agc_q23=<gain>
and model inference / detection lines:
  TFLM top prediction: <class> confidence=<pct> pct (inferences=<n>): ...
  TFLM KEYWORD DETECTED: <class> confidence=<pct> pct
  [DBG raw_output] ret=0 <class1>=<val1> <class2>=<val2> ...
  TFLM keyword trigger -> notifying KPB to begin draining

Generates a multi-panel PNG displaying:
  1. Prediction confidence (%) per class and keyword detection trigger events
  2. Frame log-energy (E), Noise floor estimate (Ne), and SNR (E - Ne)
  3. Mel filterbank log-energy envelope [mel_min, mel_max]
  4. Quantized neural-network input features [f_min, f_max] (int8)
  5. Soft automatic gain control (AGC) gain tracking
  Along with VAD speech active region highlighting and hop counters.
"""

import argparse
import os
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# Timestamp at the start of a Zephyr log line: e.g. "[   91.711085] <inf> ..."
TIME_RE = re.compile(r"^\[\s*([0-9.]+)\]")

# Regex patterns for SOF mtrace log messages
HOP_RE = re.compile(
    r"\[DBG hop\s+(\d+)\]\s+"
    r"vad=(\d+)\s+E=(-?\d+)\s+Ne=(-?\d+)\s+"
    r"mel_min=(-?\d+)\s+mel_max=(-?\d+)\s+"
    r"f_min=(-?\d+)\s+f_max=(-?\d+)\s+agc_q23=(-?\d+)"
)

# Also support hop lines that have the timestamp prefix directly on the line
HOP_WITH_TIME_RE = re.compile(
    r"\[\s*([0-9.]+)\]\s*.*\[DBG hop\s+(\d+)\]\s+"
    r"vad=(\d+)\s+E=(-?\d+)\s+Ne=(-?\d+)\s+"
    r"mel_min=(-?\d+)\s+mel_max=(-?\d+)\s+"
    r"f_min=(-?\d+)\s+f_max=(-?\d+)\s+agc_q23=(-?\d+)"
)

TOP_PRED_RE = re.compile(
    r"TFLM top prediction:\s+(\w+)\s+confidence=(\d+)\s+pct\s*\(inferences=(\d+)\)"
)

RAW_OUTPUT_RE = re.compile(
    r"\[DBG raw_output\]\s+ret=(-?\d+)\s+(.+)"
)

DETECT_RE = re.compile(
    r"TFLM KEYWORD DETECTED:\s+(\w+)\s+confidence=(\d+)\s+pct"
)

KPB_TRIGGER_RE = re.compile(
    r"TFLM keyword trigger -> notifying KPB to begin draining"
)

SUMMARY_RE = re.compile(
    r"\[TFLM STREAM SHUTDOWN SUMMARY\]\s+Total Inferences=(\d+)\s*\|\s*"
    r"Keyword Events:\s*(.+?)\s*\|\s*Total KPB Triggers=(\d+)"
)


def parse_mtrace(file_path):
    """Parse TFLM diagnostic, inference, and detection events from mtrace text log."""
    hops = []
    preds = []
    detects = []
    kpb_triggers = []
    summaries = []

    if file_path == "-":
        f = sys.stdin
    else:
        if not os.path.isfile(file_path):
            print(f"Error: file not found: {file_path}", file=sys.stderr)
            sys.exit(1)
        f = open(file_path, "r", errors="ignore")

    last_t = 0.0
    pending_raw = None

    try:
        for line in f:
            # Track timestamp from log lines
            m_t = TIME_RE.match(line)
            if m_t:
                last_t = float(m_t.group(1))

            # Check for direct timestamped hop line
            m = HOP_WITH_TIME_RE.search(line)
            if m:
                t, hop, vad, e, ne, m_min, m_max, f_min, f_max, agc = m.groups()
                last_t = float(t)
                hops.append({
                    "t": float(t),
                    "hop": int(hop),
                    "vad": int(vad),
                    "e_raw": int(e),
                    "ne_raw": int(ne),
                    "mel_min_raw": int(m_min),
                    "mel_max_raw": int(m_max),
                    "f_min": int(f_min),
                    "f_max": int(f_max),
                    "agc_raw": int(agc),
                })
                continue

            # Check for unadorned DBG hop line (uses last known timestamp)
            m = HOP_RE.search(line)
            if m:
                hop, vad, e, ne, m_min, m_max, f_min, f_max, agc = m.groups()
                hops.append({
                    "t": last_t,
                    "hop": int(hop),
                    "vad": int(vad),
                    "e_raw": int(e),
                    "ne_raw": int(ne),
                    "mel_min_raw": int(m_min),
                    "mel_max_raw": int(m_max),
                    "f_min": int(f_min),
                    "f_max": int(f_max),
                    "agc_raw": int(agc),
                })
                continue

            # Raw logits/output before prediction line
            m = RAW_OUTPUT_RE.search(line)
            if m:
                _, kvs = m.groups()
                parts = dict(re.findall(r"(\w+)=(-?\d+)", kvs))
                pending_raw = {k: int(v) for k, v in parts.items()}
                continue

            # Top prediction line
            m = TOP_PRED_RE.search(line)
            if m:
                cat, conf, inf_idx = m.groups()
                preds.append({
                    "t": last_t,
                    "class": cat,
                    "confidence": int(conf),
                    "inference": int(inf_idx),
                    "raw_output": pending_raw or {},
                })
                pending_raw = None
                continue

            # Detection line
            m = DETECT_RE.search(line)
            if m:
                cat, conf = m.groups()
                detects.append({
                    "t": last_t,
                    "class": cat,
                    "confidence": int(conf),
                })
                continue

            # KPB notification line
            m = KPB_TRIGGER_RE.search(line)
            if m:
                kpb_triggers.append(last_t)
                continue

            # Shutdown summary line
            m = SUMMARY_RE.search(line)
            if m:
                total_inf, kw_events_str, total_kpb = m.groups()
                summaries.append({
                    "inferences": int(total_inf),
                    "events_str": kw_events_str.strip(),
                    "kpb_triggers": int(total_kpb),
                })
    finally:
        if file_path != "-":
            f.close()

    return hops, preds, detects, kpb_triggers, summaries


def plot_tflm_diagnostics(hops, preds, detects, kpb_triggers, summaries,
                          output_path, title=None, threshold=50,
                          raw_units=False, show_vad=True, dpi=150):
    """Plot TFLM diagnostics across 5 synchronized subplots."""
    if not hops:
        print("Error: No 'DBG hop' records found in input.", file=sys.stderr)
        sys.exit(1)

    t0 = hops[0]["t"]
    t_hops = np.array([h["t"] - t0 for h in hops])
    hop_idx = np.array([h["hop"] for h in hops])
    vad = np.array([h["vad"] for h in hops])
    f_min = np.array([h["f_min"] for h in hops])
    f_max = np.array([h["f_max"] for h in hops])

    q23_scale = 1.0 if raw_units else (1.0 / (1 << 23))
    unit_label = "raw" if raw_units else "Q23"

    e = np.array([h["e_raw"] * q23_scale for h in hops])
    ne = np.array([h["ne_raw"] * q23_scale for h in hops])
    mel_min = np.array([h["mel_min_raw"] * q23_scale for h in hops])
    mel_max = np.array([h["mel_max_raw"] * q23_scale for h in hops])
    agc = np.array([h["agc_raw"] * q23_scale for h in hops])

    fig, axes = plt.subplots(
        5, 1, figsize=(14, 12), sharex=True,
        gridspec_kw={"height_ratios": [1.5, 1.2, 1.2, 1.2, 1.0]}
    )
    plt.subplots_adjust(hspace=0.18)

    # 1. Highlight VAD active regions across all subplots
    if show_vad and len(vad) > 1:
        vad_padded = np.pad(vad, (1, 1), "constant")
        vad_diff = np.diff(vad_padded)
        starts = np.where(vad_diff == 1)[0]
        ends = np.where(vad_diff == -1)[0]
        for s_idx, e_idx in zip(starts, ends):
            t_start = t_hops[min(s_idx, len(t_hops) - 1)]
            t_end = t_hops[min(max(0, e_idx - 1), len(t_hops) - 1)]
            for ax in axes:
                ax.axvspan(t_start, t_end, color="#c8e6c9", alpha=0.35, zorder=0)

    # -------------------------------------------------------------
    # Subplot 1: Model Prediction Confidence & Detections
    # -------------------------------------------------------------
    ax1 = axes[0]
    palette = {
        "silence": "#78909c",
        "unknown": "#8d6e63",
        "banana": "#fbc02d",
        "strawberry": "#e91e63",
        "orange": "#ff6f00",
    }
    fallback_colors = ["#26a69a", "#ab47bc", "#5c6bc0", "#42a5f5", "#66bb6a"]

    # Gather all classes observed in predictions
    seen_classes = []
    for p in preds:
        c = p["class"]
        if c not in seen_classes:
            seen_classes.append(c)

    # Plot confidence per class
    for idx, c in enumerate(seen_classes):
        c_preds = [p for p in preds if p["class"] == c]
        if not c_preds:
            continue
        c_t = np.array([p["t"] - t0 for p in c_preds])
        c_conf = np.array([p["confidence"] for p in c_preds])
        col = palette.get(c, fallback_colors[idx % len(fallback_colors)])
        ax1.plot(c_t, c_conf, color=col, lw=1.8, marker="o", markersize=4,
                 label=f"{c} ({len(c_preds)})", zorder=3)

    ax1.axhline(threshold, color="#757575", linestyle="--", lw=1.2,
                label=f"Detection Threshold ({threshold}%)")
    ax1.set_ylim(-5, 105)
    ax1.set_ylabel("Confidence (%)", fontweight="bold", fontsize=10)

    # Annotate keyword detections
    detect_labeled = False
    for d in detects:
        dt = d["t"] - t0
        dp = d["confidence"]
        dclass = d["class"]
        lbl = "Keyword Detected" if not detect_labeled else None
        detect_labeled = True
        ax1.plot(dt, dp, marker="*", markersize=18, color="#ffd700",
                 markeredgecolor="#b71c1c", markeredgewidth=1.5, zorder=5, label=lbl)
        ax1.annotate(
            f"Detected: {dclass} ({dp}%)", xy=(dt, dp),
            xytext=(dt - 0.55, dp - 22 if dp > 50 else dp + 15),
            arrowprops=dict(arrowstyle="->", color="#b71c1c", lw=1.8),
            fontweight="bold", color="#b71c1c", fontsize=9,
            bbox=dict(boxstyle="round,pad=0.3", fc="#ffffff", ec="#b71c1c", alpha=0.9),
            zorder=6
        )

    # Mark KPB triggers
    kpb_labeled = False
    for kt in kpb_triggers:
        lbl = "KPB Drain Trigger" if not kpb_labeled else None
        kpb_labeled = True
        for ax in axes:
            ax.axvline(kt - t0, color="#1565c0", linestyle=":", lw=1.5, alpha=0.8, label=lbl)

    ax1.grid(True, linestyle=":", alpha=0.6)
    ax1.legend(loc="upper left", framealpha=0.9, fontsize=8, ncol=max(1, len(seen_classes) // 2 + 1))

    # Plot title
    if title is None:
        title = "TensorFlow Lite Micro (TFLM) Streaming Diagnostics"
    stats_str = f"Hops: {len(hops)} | Inferences: {len(preds)} | Detections: {len(detects)}"
    ax1.set_title(f"{title}\n({stats_str})", fontsize=12, fontweight="bold", pad=28)

    # Secondary x-axis for Hop counter on top
    ax_top = ax1.twiny()
    ax_top.set_xlim(t_hops[0], t_hops[-1])
    step = max(1, len(hop_idx) // 10)
    tick_indices = list(range(0, len(hop_idx), step))
    if tick_indices[-1] != len(hop_idx) - 1:
        tick_indices.append(len(hop_idx) - 1)
    ax_top.set_xticks([t_hops[i] for i in tick_indices])
    ax_top.set_xticklabels([str(hop_idx[i]) for i in tick_indices])
    ax_top.set_xlabel("Hop Index", fontweight="bold", fontsize=10, labelpad=8)

    # -------------------------------------------------------------
    # Subplot 2: Energy & Noise Estimate
    # -------------------------------------------------------------
    ax2 = axes[1]
    ax2.plot(t_hops, e, color="#1e88e5", lw=1.5, label=f"E (Frame Energy, {unit_label})")
    ax2.plot(t_hops, ne, color="#fb8c00", lw=1.5, linestyle="--", label=f"Ne (Noise Floor, {unit_label})")
    ax2.plot(t_hops, e - ne, color="#43a047", lw=1.2, alpha=0.8, label="SNR (E - Ne)")
    ax2.set_ylabel(f"Energy ({unit_label})", fontweight="bold", fontsize=10)
    ax2.grid(True, linestyle=":", alpha=0.6)
    ax2.legend(loc="upper left", framealpha=0.9)

    # -------------------------------------------------------------
    # Subplot 3: Mel Filterbank Envelope
    # -------------------------------------------------------------
    ax3 = axes[2]
    ax3.fill_between(t_hops, mel_min, mel_max, color="#90caf9", alpha=0.45,
                     label=f"Mel Range [min, max] ({unit_label})")
    ax3.plot(t_hops, mel_max, color="#0d47a1", lw=1.2, label=f"mel_max ({unit_label})")
    ax3.plot(t_hops, mel_min, color="#00838f", lw=1.2, label=f"mel_min ({unit_label})")
    ax3.set_ylabel(f"Mel Log-E ({unit_label})", fontweight="bold", fontsize=10)
    ax3.grid(True, linestyle=":", alpha=0.6)
    ax3.legend(loc="upper left", framealpha=0.9)

    # -------------------------------------------------------------
    # Subplot 4: Quantized Model Input Features (int8)
    # -------------------------------------------------------------
    ax4 = axes[3]
    ax4.plot(t_hops, f_max, color="#6a1b9a", lw=1.2, label="f_max (int8)")
    ax4.plot(t_hops, f_min, color="#ab47bc", lw=1.2, label="f_min (int8)")
    ax4.axhline(0, color="#424242", lw=0.8, linestyle=":")
    ax4.axhline(127, color="#d32f2f", lw=0.9, linestyle="--", alpha=0.6, label="int8 bounds [-128, 127]")
    ax4.axhline(-128, color="#d32f2f", lw=0.9, linestyle="--", alpha=0.6)
    ax4.set_ylim(-138, 138)
    ax4.set_ylabel("Feature (int8)", fontweight="bold", fontsize=10)
    ax4.grid(True, linestyle=":", alpha=0.6)
    ax4.legend(loc="upper left", framealpha=0.9)

    # -------------------------------------------------------------
    # Subplot 5: Automatic Gain Control (AGC) Gain
    # -------------------------------------------------------------
    ax5 = axes[4]
    ax5.plot(t_hops, agc, color="#b26a00", lw=1.6, label=f"AGC Gain ({unit_label})")
    ax5.set_ylabel(f"AGC ({unit_label})", fontweight="bold", fontsize=10)
    ax5.set_xlabel("Time (seconds relative to first hop)", fontweight="bold", fontsize=10)
    ax5.grid(True, linestyle=":", alpha=0.6)
    ax5.legend(loc="upper left", framealpha=0.9)

    # Save figure
    fig.savefig(output_path, dpi=dpi, bbox_inches="tight")
    plt.close(fig)
    print(f"Visualization saved to: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Visualize TensorFlow Lite Micro (TFLM) hop metrics and detections from SOF mtrace logs."
    )
    parser.add_argument(
        "input", nargs="?", default="/home/singalsu/tmp/mtrace.txt",
        help="Path to mtrace log file (default: /home/singalsu/tmp/mtrace.txt, or '-' for stdin)"
    )
    parser.add_argument(
        "-o", "--output", default=None,
        help="Output PNG image path (default: <input_stem>_tflm.png or /home/singalsu/tmp/tflm_mtrace.png)"
    )
    parser.add_argument(
        "-t", "--threshold", type=int, default=50,
        help="Keyword detection probability threshold in percent (default: 50)"
    )
    parser.add_argument(
        "--raw-units", action="store_true",
        help="Plot raw integer values instead of Q23 float representation for E, Ne, mel, and AGC"
    )
    parser.add_argument(
        "--no-vad", action="store_true",
        help="Disable green shading for VAD active regions"
    )
    parser.add_argument(
        "--dpi", type=int, default=150,
        help="DPI resolution for the generated PNG (default: 150)"
    )
    parser.add_argument(
        "--title", default=None,
        help="Custom title for the figure"
    )

    args = parser.parse_args()

    # Determine default output path if not specified
    if args.output is None:
        if args.input == "-":
            args.output = "tflm_mtrace.png"
        else:
            base, _ = os.path.splitext(args.input)
            args.output = f"{base}_tflm.png" if base != "/home/singalsu/tmp/mtrace" else "/home/singalsu/tmp/tflm_mtrace.png"

    hops, preds, detects, kpb_triggers, summaries = parse_mtrace(args.input)

    title = args.title
    if title is None and args.input != "-":
        title = f"TFLM Diagnostics — {os.path.basename(args.input)}"

    print(f"Parsed {len(hops)} hops, {len(preds)} inferences, {len(detects)} detections from {args.input}")
    for d in detects:
        print(f"  [Detection Event] timestamp={d['t']}s, class={d['class']}, confidence={d['confidence']}%")
    for s in summaries:
        print(f"  [Shutdown Summary] inferences={s['inferences']}, events={s['events_str']}, kpb_triggers={s['kpb_triggers']}")

    plot_tflm_diagnostics(
        hops=hops,
        preds=preds,
        detects=detects,
        kpb_triggers=kpb_triggers,
        summaries=summaries,
        output_path=args.output,
        title=title,
        threshold=args.threshold,
        raw_units=args.raw_units,
        show_vad=not args.no_vad,
        dpi=args.dpi,
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
