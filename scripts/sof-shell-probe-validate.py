#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""Validate SOF shell commands through framed probe transport bridge.

This validator exercises shell commands through the shell/probe framing path:
validator <-> framed stream <-> sof-shell-probe-bridge.py <-> cavstool shell PTY

It verifies command responses and error patterns similarly to sof-shell-validate.py.
"""

from __future__ import annotations

import argparse
import json
import os
import selectors
import shutil
import subprocess
import sys
import tempfile
import time

from shell_probe_framing import (
    TYPE_CREDIT,
    TYPE_DATA,
    Frame,
    chunk_bytes,
    pack_frame,
    unpack_frame,
)

COMMANDS = [
    ("sof version", ["SOF Version"]),
    ("sof pipeline list", ["ID"]),
    ("sof module status", ["No components found", "comp_id"]),
    ("sof core status", ["core", "enabled"]),
    ("sof ipc stats", ["rx_count", "tx_count"]),
    ("sof buffer list", ["Audio buffers"]),
    ("sof dma status", ["No DMA controllers", "DMA controller"]),
]

ERROR_PATTERNS = [
    "shell: command not found",
    "shell: unknown command",
    "-EINVAL",
    "-ENOMEM",
    "Traceback",
]

SHELL_PROMPT = "~$ "


class FrameClient:
    def __init__(self, tx_path: str, rx_path: str, max_payload: int):
        self.max_payload = max_payload
        self.tx_seq = 0

        # Use nonblocking IO + selector to avoid deadlocks with FIFOs.
        self.tx_fd = os.open(tx_path, os.O_RDWR | os.O_NONBLOCK)
        self.rx_fd = os.open(rx_path, os.O_RDWR | os.O_NONBLOCK)
        self.sel = selectors.DefaultSelector()
        self.sel.register(self.rx_fd, selectors.EVENT_READ)

        self._rx_buf = bytearray()

    def close(self):
        self.sel.close()
        os.close(self.tx_fd)
        os.close(self.rx_fd)

    def _write_frame(self, frame: Frame):
        data = pack_frame(frame)
        off = 0
        while off < len(data):
            try:
                n = os.write(self.tx_fd, data[off:])
            except BlockingIOError:
                time.sleep(0.002)
                continue
            off += n

    def send_text(self, text: str):
        raw = text.encode("utf-8", errors="replace")
        for chunk in chunk_bytes(raw, self.max_payload):
            self._write_frame(
                Frame(frame_type=TYPE_DATA, seq=self.tx_seq, payload=chunk, flags=0)
            )
            self.tx_seq = (self.tx_seq + 1) & 0xFFFFFFFF

    def recv_until_prompt(self, timeout_s: float) -> str:
        deadline = time.time() + timeout_s
        collected = bytearray()

        while time.time() < deadline:
            events = self.sel.select(timeout=0.2)
            if not events:
                continue

            for _key, _mask in events:
                try:
                    chunk = os.read(self.rx_fd, 4096)
                except BlockingIOError:
                    continue

                if not chunk:
                    continue

                self._rx_buf.extend(chunk)

                while True:
                    if len(self._rx_buf) < 16:
                        break

                    payload_len = (self._rx_buf[12] << 8) | self._rx_buf[13]
                    frame_len = 16 + payload_len
                    if len(self._rx_buf) < frame_len:
                        break

                    frame_bytes = bytes(self._rx_buf[:frame_len])
                    del self._rx_buf[:frame_len]

                    frame = unpack_frame(frame_bytes)
                    if frame.frame_type == TYPE_DATA:
                        collected.extend(frame.payload)
                        # Grant credits for consumed bytes so sender can continue.
                        if frame.payload:
                            self._write_frame(Frame(frame_type=TYPE_CREDIT, credits=len(frame.payload)))

            if SHELL_PROMPT.encode() in collected:
                return collected.decode("utf-8", errors="replace")

        return collected.decode("utf-8", errors="replace")


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="Validate shell over framed probe transport")
    ap.add_argument("--dut", default="spider", help="DUT hostname (default: spider)")
    ap.add_argument(
        "--bridge-script",
        default="sof/scripts/sof-shell-probe-bridge.py",
        help="Path to sof-shell-probe-bridge.py",
    )
    ap.add_argument(
        "--cavstool",
        default="/usr/local/bin/cavstool.py",
        help="Path to cavstool.py on DUT",
    )
    ap.add_argument("--timeout", type=float, default=5.0, help="Per-command timeout in seconds")
    ap.add_argument("--max-payload", type=int, default=512, help="Max bytes per frame payload")
    ap.add_argument(
        "--initial-credits",
        type=int,
        default=16384,
        help="Initial tx credits for bridge side",
    )
    return ap.parse_args()


def _normalize_output(cmd: str, collected: str) -> str:
    out = collected
    for s in [cmd, SHELL_PROMPT, "\r\n", "\r", "\n\n"]:
        out = out.replace(s, "\n")
    return out.strip()


def main() -> int:
    args = parse_args()

    bridge_script = args.bridge_script
    if not os.path.isfile(bridge_script):
        candidates = [
            bridge_script,
            os.path.join("sof", "scripts", "sof-shell-probe-bridge.py"),
            os.path.join("scripts", "sof-shell-probe-bridge.py"),
        ]
        bridge_script = next((p for p in candidates if os.path.isfile(p)), "")
        if not bridge_script and shutil.which(args.bridge_script):
            bridge_script = args.bridge_script
        if not bridge_script:
            print(f"[error] bridge script not found: {args.bridge_script}")
            return 2

    with tempfile.TemporaryDirectory(prefix="sof-shell-probe-") as td:
        framed_in = os.path.join(td, "framed.in")
        framed_out = os.path.join(td, "framed.out")
        os.mkfifo(framed_in)
        os.mkfifo(framed_out)

        bridge_cmd = [
            sys.executable,
            bridge_script,
            "--dut",
            args.dut,
            "--cavstool",
            args.cavstool,
            "--framed-in",
            framed_in,
            "--framed-out",
            framed_out,
            "--max-payload",
            str(args.max_payload),
            "--initial-credits",
            str(args.initial_credits),
        ]

        bridge = subprocess.Popen(bridge_cmd)

        # Give bridge some time to establish SSH and shell PTY.
        time.sleep(2.0)
        if bridge.poll() is not None:
            print("[error] bridge exited before validation started")
            return 2

        client = FrameClient(tx_path=framed_in, rx_path=framed_out, max_payload=args.max_payload)
        results = []

        try:
            # Prime shell prompt.
            client.send_text("\r\n")
            client.recv_until_prompt(timeout_s=args.timeout)

            for cmd, patterns in COMMANDS:
                client.send_text(cmd + "\r\n")
                collected = client.recv_until_prompt(timeout_s=args.timeout)

                has_error = any(ep in collected for ep in ERROR_PATTERNS)
                has_match = (not patterns) or any(p.lower() in collected.lower() for p in patterns)

                results.append(
                    {
                        "cmd": cmd,
                        "pass": has_match and not has_error,
                        "output": _normalize_output(cmd, collected),
                    }
                )
        finally:
            client.close()
            bridge.terminate()

        passed = sum(1 for r in results if r["pass"])
        total = len(results)

        print(f"[summary] PASS {passed}/{total}")
        print(json.dumps(results, indent=2))

        return 0 if passed == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
