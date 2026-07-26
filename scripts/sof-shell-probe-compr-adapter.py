#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Bridge framed shell bytes to compressed PCM transport commands.

This helper connects the framed byte streams used by sof-shell-probe-bridge.py
to any pair of user-provided processes that read/write probe compressed PCM
streams.

Direction summary:
- framed-in  -> playback command stdin  (host -> FW)
- capture command stdout -> framed-out  (FW -> host)
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import threading
import time


def _format_cmd(template: str, dev: str) -> list[str]:
    return shlex.split(template.format(dev=dev))


def _pump_reader_to_writer(reader, writer, stop_evt: threading.Event, label: str):
    try:
        while not stop_evt.is_set():
            chunk = reader.read(4096)
            if not chunk:
                stop_evt.set()
                return
            writer.write(chunk)
            writer.flush()
    except BrokenPipeError:
        stop_evt.set()
    except Exception as e:
        stop_evt.set()
        raise RuntimeError(f"{label} pump failed: {e}") from e


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description="Bridge framed shell bytes to compressed PCM commands",
    )
    ap.add_argument(
        "--framed-in",
        required=True,
        help="Path to framed input stream to send to playback command",
    )
    ap.add_argument(
        "--framed-out",
        required=True,
        help="Path to framed output stream read from capture command",
    )
    ap.add_argument(
        "--playback-dev",
        required=True,
        help="Playback compressed device path (e.g. /dev/snd/comprC1D0)",
    )
    ap.add_argument(
        "--capture-dev",
        required=True,
        help="Capture compressed device path (e.g. /dev/snd/comprC1D1)",
    )
    ap.add_argument(
        "--playback-cmd",
        required=True,
        help=(
            "Playback command template. Must consume stdin and write to {dev}. "
            "Example: 'my_playback_tool --device {dev} --stdin'"
        ),
    )
    ap.add_argument(
        "--capture-cmd",
        required=True,
        help=(
            "Capture command template. Must read from {dev} and write to stdout. "
            "Example: 'my_capture_tool --device {dev} --stdout'"
        ),
    )
    return ap.parse_args()


def main() -> int:
    args = parse_args()

    playback_cmd = _format_cmd(args.playback_cmd, args.playback_dev)
    capture_cmd = _format_cmd(args.capture_cmd, args.capture_dev)

    print(f"[adapter] playback cmd: {' '.join(playback_cmd)}")
    print(f"[adapter] capture cmd : {' '.join(capture_cmd)}")

    play_proc = subprocess.Popen(playback_cmd, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL)
    cap_proc = subprocess.Popen(capture_cmd, stdout=subprocess.PIPE, stdin=subprocess.DEVNULL)

    stop_evt = threading.Event()

    # Open framed streams in binary mode with buffering disabled.
    # Use read-write mode to avoid FIFO open order deadlocks.
    framed_in = open(args.framed_in, "r+b", buffering=0)
    framed_out = open(args.framed_out, "r+b", buffering=0)

    t_in = threading.Thread(
        target=_pump_reader_to_writer,
        args=(framed_in, play_proc.stdin, stop_evt, "framed->playback"),
        daemon=True,
    )
    t_out = threading.Thread(
        target=_pump_reader_to_writer,
        args=(cap_proc.stdout, framed_out, stop_evt, "capture->framed"),
        daemon=True,
    )

    t_in.start()
    t_out.start()

    try:
        while not stop_evt.is_set():
            if play_proc.poll() is not None:
                print(f"[adapter] playback process exited with {play_proc.returncode}")
                stop_evt.set()
                break
            if cap_proc.poll() is not None:
                print(f"[adapter] capture process exited with {cap_proc.returncode}")
                stop_evt.set()
                break
            time.sleep(0.1)
    finally:
        stop_evt.set()
        t_in.join(timeout=1)
        t_out.join(timeout=1)
        for p in (play_proc, cap_proc):
            try:
                p.terminate()
            except Exception:
                pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
