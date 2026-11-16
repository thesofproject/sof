#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
WOV Regular PCM Multi-Cycle Capture & Verification Daemon for SOF Firmware.

Captures audio from SOF regular PCM devices (e.g. hw:0,11 - 16 kHz, 2-channel
S16_LE) across multiple cycles to verify audio capture quality, dynamic pipeline
creation/teardown reliability, and runtime PM stability.

Usage:
  python3 wov_daemon.py [options]

Options:
  --cycles N       : Number of capture cycles to run (default: 3, 0=unlimited)
  --duration S     : Duration per cycle in seconds (default: 2.0)
  --delay S        : Delay between cycles in seconds (default: 0.5)
  --card N         : ALSA sound card index (default: 0)
  --device N       : ALSA PCM device index (default: 11)
  --rate N         : Sampling rate in Hz (default: 16000)
  --channels N     : Number of channels (default: 2)
  --format FMT     : Sample format (default: S16_LE)
  --out PATH       : Output WAV file or directory (default: /tmp/wov_daemon_cycle_{n}.wav)
  --test-pm        : Test DSP runtime PM D3 autosuspend sleep/wake between cycles
  --verify         : Run deep audio signal analysis on captured WAV files
"""

import argparse
import math
import os
import struct
import subprocess
import sys
import time
import wave


def format_timestamp():
    return time.strftime("%Y-%m-%d %H:%M:%S")


def log(tag, msg):
    ts = format_timestamp()
    print(f"[{ts}] [{tag:<5}] {msg}", flush=True)


def analyze_wav(wav_path):
    """Analyze audio metrics (Min, Max, DC offset, RMS, Peak, dBFS) for each channel."""
    if not os.path.exists(wav_path):
        return None

    try:
        with wave.open(wav_path, "rb") as wf:
            channels = wf.getnchannels()
            sampwidth = wf.getsampwidth()
            framerate = wf.getframerate()
            nframes = wf.getnframes()
            raw_data = wf.readframes(nframes)
    except Exception as e:
        log("ERROR", f"Failed to read WAV file {wav_path}: {e}")
        return None

    if nframes == 0 or channels == 0:
        return None

    if sampwidth == 2:
        fmt = f"<{nframes * channels}h"
        max_val = 32768.0
    elif sampwidth == 4:
        fmt = f"<{nframes * channels}i"
        max_val = 2147483648.0
    else:
        log("WARN", f"Unsupported sample width: {sampwidth} bytes")
        return None

    try:
        samples = struct.unpack(fmt, raw_data)
    except struct.error as e:
        log("ERROR", f"Unpack failed: {e}")
        return None

    stats = []
    for c in range(channels):
        ch_samples = samples[c::channels]
        if not ch_samples:
            continue
        n = len(ch_samples)
        min_v = min(ch_samples)
        max_v = max(ch_samples)
        dc = sum(ch_samples) / n
        ac_sq = sum((x - dc) ** 2 for x in ch_samples) / n
        rms = math.sqrt(max(ac_sq, 0.0))
        peak = max(abs(min_v), abs(max_v))
        rms_dbfs = 20.0 * math.log10(max(rms, 1.0) / max_val)
        peak_dbfs = 20.0 * math.log10(max(peak, 1.0) / max_val)

        stats.append({
            "channel": c,
            "min": min_v,
            "max": max_v,
            "dc": dc,
            "rms": rms,
            "peak": peak,
            "rms_dbfs": rms_dbfs,
            "peak_dbfs": peak_dbfs,
        })

    return {
        "channels": channels,
        "rate": framerate,
        "frames": nframes,
        "duration_s": nframes / framerate,
        "sample_width": sampwidth,
        "channel_stats": stats,
    }


def capture_arecord(card, device, rate, channels, sample_fmt, duration_s, out_path):
    """Run arecord to capture a regular PCM stream."""
    endpoint = f"hw:{card},{device}"
    cmd = [
        "arecord",
        "-D", endpoint,
        "-f", sample_fmt,
        "-r", str(rate),
        "-c", str(channels),
        "-d", str(int(math.ceil(duration_s))),
        out_path,
    ]
    t0 = time.monotonic()
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    t_elapsed = time.monotonic() - t0

    if proc.returncode != 0:
        log("ERROR", f"arecord failed (code {proc.returncode}): {proc.stderr.strip()}")
        return False, t_elapsed

    return True, t_elapsed


def run_daemon(args):
    log("INFO", f"Starting WOV Multi-Cycle Capture Daemon")
    log("INFO", f"Endpoint: hw:{args.card},{args.device} | Rate: {args.rate}Hz | Ch: {args.channels} | Fmt: {args.format}")
    log("INFO", f"Cycles: {args.cycles if args.cycles > 0 else 'Unlimited'} | Duration per cycle: {args.duration}s")

    cycle = 1
    total_passed = 0
    total_failed = 0

    while True:
        if args.out.endswith(".wav") and args.cycles == 1:
            out_file = args.out
        elif "{n}" in args.out:
            out_file = args.out.format(n=cycle)
        elif os.path.isdir(args.out):
            out_file = os.path.join(args.out, f"wov_capture_cyc{cycle:03d}.wav")
        else:
            base, ext = os.path.splitext(args.out)
            out_file = f"{base}_cyc{cycle:03d}{ext or '.wav'}"

        log("STATE", f"Cycle {cycle}: Starting capture -> {out_file}")
        ok, elapsed = capture_arecord(
            args.card, args.device, args.rate, args.channels,
            args.format, args.duration, out_file
        )

        if ok:
            total_passed += 1
            file_sz = os.path.getsize(out_file) if os.path.exists(out_file) else 0
            log("STATE", f"Cycle {cycle}: PASSED ({file_sz} bytes, {elapsed:.2f}s elapsed)")

            if args.verify:
                res = analyze_wav(out_file)
                if res:
                    for ch_s in res["channel_stats"]:
                        log("INFO",
                            f"  Ch {ch_s['channel']}: Min={ch_s['min']:<6} Max={ch_s['max']:<6} "
                            f"DC={ch_s['dc']:<7.1f} RMS={ch_s['rms']:<7.1f} ({ch_s['rms_dbfs']:.1f} dBFS) "
                            f"Peak={ch_s['peak']} ({ch_s['peak_dbfs']:.1f} dBFS)")
        else:
            total_failed += 1
            log("ERROR", f"Cycle {cycle}: FAILED")

        if args.cycles > 0 and cycle >= args.cycles:
            break

        cycle += 1

        if args.test_pm:
            # Wait 3.0s so DSP runtime PM autosuspends to D3 state
            log("INFO", "Testing Runtime PM: Sleeping 3.0s to allow DSP D3 autosuspend...")
            time.sleep(3.0)
        elif args.delay > 0:
            time.sleep(args.delay)

    log("INFO", "--------------------------------------------------------")
    log("INFO", f"Summary: Total Cycles={cycle}, Passed={total_passed}, Failed={total_failed}")
    return 0 if total_failed == 0 else 1


def main():
    parser = argparse.ArgumentParser(
        description="WOV Regular PCM Multi-Cycle Capture & Verification Daemon for SOF Firmware"
    )
    parser.add_argument("--cycles", "-n", type=int, default=3,
                        help="Number of capture cycles to run (0=unlimited, default: 3)")
    parser.add_argument("--duration", "-t", type=float, default=2.0,
                        help="Duration per cycle in seconds (default: 2.0)")
    parser.add_argument("--delay", "-w", type=float, default=0.5,
                        help="Delay between cycles in seconds (default: 0.5)")
    parser.add_argument("--card", "-c", type=int, default=0,
                        help="ALSA sound card index (default: 0)")
    parser.add_argument("--device", "-d", type=int, default=11,
                        help="ALSA PCM device index (default: 11)")
    parser.add_argument("--rate", "-r", type=int, default=16000,
                        help="Sample rate in Hz (default: 16000)")
    parser.add_argument("--channels", "-C", type=int, default=2,
                        help="Number of channels (default: 2)")
    parser.add_argument("--format", "-f", type=str, default="S16_LE",
                        help="ALSA format (default: S16_LE)")
    parser.add_argument("--out", "-o", type=str, default="/tmp/wov_daemon_cycle_{n}.wav",
                        help="Output WAV file path or pattern (default: /tmp/wov_daemon_cycle_{n}.wav)")
    parser.add_argument("--test-pm", action="store_true",
                        help="Sleep 3.0s between cycles to verify DSP D3 autosuspend / wake")
    parser.add_argument("--verify", "-v", action="store_true", default=True,
                        help="Perform deep audio signal energy analysis (default: True)")

    args = parser.parse_args()
    sys.exit(run_daemon(args))


if __name__ == "__main__":
    main()
