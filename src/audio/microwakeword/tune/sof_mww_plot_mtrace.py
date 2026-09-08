#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2026, Intel Corporation. All rights reserved.

"""
Visualize microWakeWord (MWW) streaming diagnostics from SOF mtrace logs.

Parses log lines produced by CONFIG_COMP_MWW_DEBUG_TRACE:
  [MWW DBG hop <n>] vad=<0|1> E=<energy> Ne=<noise_energy>
                    mel_min=<min> mel_max=<max>
                    f_min=<min> f_max=<max> agc_q23=<gain>
and model inference / detection lines:
  MWW probability=<pct>
  MWW keyword detected: probability=<pct>

Generates a multi-panel PNG displaying:
  1. Detection probability (%) and keyword detection trigger events
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

# Regex patterns for SOF mtrace log messages
HOP_RE = re.compile(
    r"\[\s*([0-9.]+)\]\s*.*\[MWW DBG hop\s+(\d+)\]\s+"
    r"vad=(\d+)\s+E=(-?\d+)\s+Ne=(-?\d+)\s+"
    r"mel_min=(-?\d+)\s+mel_max=(-?\d+)\s+"
    r"f_min=(-?\d+)\s+f_max=(-?\d+)\s+agc_q23=(-?\d+)"
)

PROB_RE = re.compile(
    r"\[\s*([0-9.]+)\]\s*.*MWW probability=(\d+)"
)

DETECT_RE = re.compile(
    r"\[\s*([0-9.]+)\]\s*.*MWW keyword detected:\s*probability=(\d+)"
)

KPB_TRIGGER_RE = re.compile(
    r"\[\s*([0-9.]+)\]\s*.*MWW keyword trigger -> notifying KPB to begin draining"
)

SUMMARY_RE = re.compile(
    r"\[MWW STREAM SHUTDOWN SUMMARY\]\s+Total Inferences=(\d+)\s*\|\s*"
    r"VAD Gated=(\d+)\s*\|\s*Detections=(\d+)\s*\|\s*KPB Triggers=(\d+)\s*\|\s*"
    r"Arena Used=(\d+)/(\d+)\s*B"
)


def parse_mtrace(file_path):
    """Parse MWW diagnostic and detection events from mtrace text log."""
    hops = []
    probs = []
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

    try:
        for line in f:
            m = HOP_RE.search(line)
            if m:
                t, hop, vad, e, ne, m_min, m_max, f_min, f_max, agc = m.groups()
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

            m = PROB_RE.search(line)
            if m:
                probs.append({
                    "t": float(m.group(1)),
                    "prob": int(m.group(2))
                })
                continue

            m = DETECT_RE.search(line)
            if m:
                detects.append({
                    "t": float(m.group(1)),
                    "prob": int(m.group(2))
                })
                continue

            m = KPB_TRIGGER_RE.search(line)
            if m:
                kpb_triggers.append(float(m.group(1)))
                continue

            m = SUMMARY_RE.search(line)
            if m:
                inf, vad_gated, det, kpb_trig, arena_used, arena_cap = m.groups()
                summaries.append({
                    "inferences": int(inf),
                    "vad_gated": int(vad_gated),
                    "detections": int(det),
                    "kpb_triggers": int(kpb_trig),
                    "arena_used": int(arena_used),
                    "arena_cap": int(arena_cap),
                })
    finally:
        if file_path != "-":
            f.close()

    return hops, probs, detects, kpb_triggers, summaries


def plot_mww_diagnostics(hops, probs, detects, kpb_triggers, summaries,
                         output_path, title=None, threshold=65,
                         raw_units=False, show_vad=True, dpi=150):
    """Plot MWW diagnostics across 5 synchronized subplots."""
    if not hops:
        print("Error: No 'MWW DBG hop' records found in input.", file=sys.stderr)
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

    t_probs = np.array([p["t"] - t0 for p in probs]) if probs else np.array([])
    prob_vals = np.array([p["prob"] for p in probs]) if probs else np.array([])

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
    # Subplot 1: Model Probability & Detections
    # -------------------------------------------------------------
    ax1 = axes[0]
    if len(t_probs) > 0:
        ax1.plot(t_probs, prob_vals, color="#c62828", lw=2,
                 marker="o", markersize=3, label="MWW Probability (%)", zorder=3)
    ax1.axhline(threshold, color="#757575", linestyle="--", lw=1.2,
                label=f"Detection Threshold ({threshold}%)")
    ax1.set_ylim(-5, 105)
    ax1.set_ylabel("Probability (%)", fontweight="bold", fontsize=10)

    # Annotate detections
    detect_labeled = False
    for d in detects:
        dt = d["t"] - t0
        dp = d["prob"]
        lbl = "Keyword Detected" if not detect_labeled else None
        detect_labeled = True
        ax1.plot(dt, dp, marker="*", markersize=18, color="#ffd700",
                 markeredgecolor="#b71c1c", markeredgewidth=1.5, zorder=5, label=lbl)
        ax1.annotate(
            f"Detected: {dp}%", xy=(dt, dp),
            xytext=(dt - 0.45, dp - 22 if dp > 50 else dp + 15),
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
    ax1.legend(loc="upper left", framealpha=0.9)

    # Plot title
    if title is None:
        title = "microWakeWord (MWW) Streaming Diagnostics"
    stats_str = f"Hops: {len(hops)} | Inferences: {len(probs)} | Detections: {len(detects)}"
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

    plt.tight_layout()
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    fig.savefig(output_path, dpi=dpi)
    plt.close(fig)
    print(f"Visualization saved to: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Visualize microWakeWord (MWW) hop metrics and detections from SOF mtrace logs."
    )
    parser.add_argument(
        "input", nargs="?", default="-",
        help="Path to mtrace log file (default: '-' for stdin)"
    )
    parser.add_argument(
        "-o", "--output", default=None,
        help="Output PNG image path (default: <input_stem>_mww.png or mww_mtrace.png for stdin)"
    )
    parser.add_argument(
        "-t", "--threshold", type=int, default=65,
        help="Wake word detection probability threshold in percent (default: 65)"
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
            args.output = "mww_mtrace.png"
        else:
            base, _ = os.path.splitext(args.input)
            args.output = f"{base}_mww.png"

    hops, probs, detects, kpb_triggers, summaries = parse_mtrace(args.input)

    title = args.title
    if title is None and args.input != "-":
        title = f"MWW Diagnostics — {os.path.basename(args.input)}"

    print(f"Parsed {len(hops)} hops, {len(probs)} inferences, {len(detects)} detections from {args.input}")
    for d in detects:
        print(f"  [Detection Event] timestamp={d['t']}s, probability={d['prob']}%")
    for s in summaries:
        print(f"  [Shutdown Summary] inferences={s['inferences']}, detections={s['detections']}, arena={s['arena_used']}/{s['arena_cap']} B")

    plot_mww_diagnostics(
        hops=hops,
        probs=probs,
        detects=detects,
        kpb_triggers=kpb_triggers,
        summaries=summaries,
        output_path=args.output,
        title=title,
        threshold=args.threshold,
        raw_units=args.raw_units,
        show_vad=not args.no_vad,
        dpi=args.dpi
    )


if __name__ == "__main__":
    main()
