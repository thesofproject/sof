#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Bridge Zephyr shell PTY traffic to a framed shell/probe stream.

This tool is intentionally transport-agnostic for the probe side: it reads and
writes framed packets via files/FIFOs so it can be paired with any compressed
PCM bridge implementation.

Typical setup:
1. Start a process that converts probe compressed PCM <-> framed byte stream.
2. Point this tool to those two files/FIFOs.
3. It will bridge shell PTY traffic to/from those framed streams.
"""

from __future__ import annotations

import argparse
import os
import queue
import select
import subprocess
import sys
import termios
import threading
import time
from typing import Optional

from shell_probe_framing import (
    FLAG_EOC,
    TYPE_CREDIT,
    TYPE_DATA,
    TYPE_RESET,
    Frame,
    chunk_bytes,
    pack_frame,
    read_frames,
)


def _find_cavstool_path(dut: str, preferred: str) -> str:
    cmd = (
        "python3 - <<'PY'\n"
        "import os, shutil\n"
        "cands=["
        f"{preferred!r},"
        "'/usr/local/bin/cavstool.py','/usr/bin/cavstool.py','/usr/sbin/cavstool.py']\n"
        "for p in cands:\n"
        "  if p and os.path.isfile(p):\n"
        "    print(p); raise SystemExit(0)\n"
        "w=shutil.which('cavstool.py')\n"
        "print(w or '')\n"
        "PY"
    )
    out = subprocess.check_output(["ssh", f"root@{dut}", cmd], text=True).strip()
    if not out:
        raise RuntimeError("cavstool.py not found on DUT")
    return out


def _open_shell_pty(dut: str, cavstool: str, timeout_s: int = 20):
    ssh_cmd = [
        "ssh",
        "-o",
        "StrictHostKeyChecking=no",
        f"root@{dut}",
        f"python3 {cavstool} --log-only --shell-pty --no-history",
    ]
    proc = subprocess.Popen(
        ssh_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    pty_path = None
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        line = proc.stdout.readline()
        if not line:
            break
        if "shell PTY at:" in line:
            pty_path = line.split("shell PTY at:", 1)[1].strip()
            break

    if not pty_path:
        proc.terminate()
        raise RuntimeError("cavstool did not report shell PTY path")

    return proc, pty_path


def _open_remote_pty_fd(dut: str, pty_path: str):
    # Keep a dedicated SSH process that proxies PTY bytes to local stdio.
    # stdin -> PTY write ; PTY read -> stdout
    script = (
        "python3 - <<'PY'\n"
        "import os, pty, select, sys, termios\n"
        f"p={pty_path!r}\n"
        "fd=os.open(p, os.O_RDWR|os.O_NOCTTY)\n"
        "attr=termios.tcgetattr(fd)\n"
        "attr[3] &= ~(termios.ECHO|termios.ICANON|termios.ISIG)\n"
        "attr[6][termios.VMIN]=0\n"
        "attr[6][termios.VTIME]=0\n"
        "termios.tcsetattr(fd, termios.TCSANOW, attr)\n"
        "while True:\n"
        "  r,_,_=select.select([fd, sys.stdin.buffer], [], [], 0.1)\n"
        "  if fd in r:\n"
        "    b=os.read(fd,4096)\n"
        "    if b:\n"
        "      sys.stdout.buffer.write(b); sys.stdout.buffer.flush()\n"
        "  if sys.stdin.buffer in r:\n"
        "    b=os.read(sys.stdin.fileno(),4096)\n"
        "    if not b:\n"
        "      break\n"
        "    os.write(fd,b)\n"
        "PY"
    )

    proc = subprocess.Popen(
        ["ssh", f"root@{dut}", script],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return proc


class Bridge:
    def __init__(
        self,
        pty_proc: subprocess.Popen,
        framed_in_path: str,
        framed_out_path: str,
        max_payload: int,
        initial_credits: int,
    ):
        self.pty_proc = pty_proc
        self.max_payload = max_payload
        self.tx_seq = 0
        self.rx_seq = 0
        self.tx_credits = initial_credits
        self.tx_q: "queue.Queue[bytes]" = queue.Queue()

        self.framed_in = open(framed_in_path, "rb", buffering=0)
        self.framed_out = open(framed_out_path, "wb", buffering=0)

        self.stop = threading.Event()

    def _send_frame(self, frame: Frame):
        self.framed_out.write(pack_frame(frame))
        self.framed_out.flush()

    def _tx_from_shell(self):
        fd = self.pty_proc.stdout.fileno()
        while not self.stop.is_set():
            r, _, _ = select.select([fd], [], [], 0.1)
            if fd not in r:
                continue
            data = os.read(fd, 4096)
            if not data:
                self.stop.set()
                return

            for chunk in chunk_bytes(data, self.max_payload):
                self.tx_q.put(chunk)

    def _rx_to_shell(self):
        for frame in read_frames(self.framed_in):
            if self.stop.is_set():
                return

            if frame.frame_type == TYPE_DATA:
                if frame.seq != self.rx_seq:
                    # Resync on sequence mismatch by requesting reset.
                    self._send_frame(Frame(frame_type=TYPE_RESET, seq=self.rx_seq))
                    continue
                self.rx_seq = (self.rx_seq + 1) & 0xFFFFFFFF

                if frame.payload:
                    self.pty_proc.stdin.write(frame.payload)
                    self.pty_proc.stdin.flush()

                consumed = len(frame.payload)
                if consumed:
                    self._send_frame(Frame(frame_type=TYPE_CREDIT, credits=consumed))

            elif frame.frame_type == TYPE_CREDIT:
                self.tx_credits += frame.credits

            elif frame.frame_type == TYPE_RESET:
                self.tx_seq = 0
                self.rx_seq = 0
                self.tx_credits = 0
                self._send_frame(Frame(frame_type=TYPE_CREDIT, credits=0))

    def _tx_loop(self):
        while not self.stop.is_set():
            try:
                chunk = self.tx_q.get(timeout=0.1)
            except queue.Empty:
                continue

            while self.tx_credits < len(chunk) and not self.stop.is_set():
                time.sleep(0.002)

            if self.stop.is_set():
                return

            self.tx_credits -= len(chunk)
            self._send_frame(
                Frame(
                    frame_type=TYPE_DATA,
                    seq=self.tx_seq,
                    payload=chunk,
                    flags=FLAG_EOC if chunk.endswith(b"\n") else 0,
                )
            )
            self.tx_seq = (self.tx_seq + 1) & 0xFFFFFFFF

    def run(self):
        t1 = threading.Thread(target=self._tx_from_shell, daemon=True)
        t2 = threading.Thread(target=self._rx_to_shell, daemon=True)
        t3 = threading.Thread(target=self._tx_loop, daemon=True)

        t1.start()
        t2.start()
        t3.start()

        try:
            while not self.stop.is_set():
                if self.pty_proc.poll() is not None:
                    self.stop.set()
                    break
                time.sleep(0.1)
        finally:
            self.stop.set()
            t1.join(timeout=1)
            t2.join(timeout=1)
            t3.join(timeout=1)


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description="Bridge shell PTY traffic to framed shell/probe transport streams",
    )
    ap.add_argument("--dut", default="spider", help="DUT hostname (default: spider)")
    ap.add_argument(
        "--cavstool",
        default="/usr/local/bin/cavstool.py",
        help="Path to cavstool.py on DUT",
    )
    ap.add_argument(
        "--framed-in",
        required=True,
        help="Input file/FIFO for framed bytes heading to shell",
    )
    ap.add_argument(
        "--framed-out",
        required=True,
        help="Output file/FIFO for framed bytes coming from shell",
    )
    ap.add_argument(
        "--max-payload",
        type=int,
        default=512,
        help="Maximum payload bytes per data frame (default: 512)",
    )
    ap.add_argument(
        "--initial-credits",
        type=int,
        default=16384,
        help="Initial TX credits before remote credit frames arrive (default: 16384)",
    )
    return ap.parse_args()


def main() -> int:
    args = parse_args()

    cavstool_path = _find_cavstool_path(args.dut, args.cavstool)
    cavstool_proc, pty_path = _open_shell_pty(args.dut, cavstool_path)
    pty_io_proc = _open_remote_pty_fd(args.dut, pty_path)

    sys.stderr.write(f"[bridge] shell PTY path: {pty_path}\n")
    sys.stderr.flush()

    bridge = Bridge(
        pty_proc=pty_io_proc,
        framed_in_path=args.framed_in,
        framed_out_path=args.framed_out,
        max_payload=args.max_payload,
        initial_credits=args.initial_credits,
    )

    try:
        bridge.run()
    finally:
        for p in (pty_io_proc, cavstool_proc):
            try:
                p.terminate()
            except Exception:
                pass

    return 0


if __name__ == "__main__":
    sys.exit(main())
