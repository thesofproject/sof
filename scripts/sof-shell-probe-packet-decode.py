#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Decode probe extraction packets and print mirrored shell payload.

Reads a raw probe extraction byte stream from stdin and writes shell payload
bytes to stdout for packets with PROBE_SHELL_BUFFER_ID.
"""

from __future__ import annotations

import argparse
import struct
import sys

PROBE_EXTRACT_SYNC_WORD = 0xBABEBEBA
PROBE_LOGGING_BUFFER_ID = 0x01000000
PROBE_SHELL_BUFFER_ID = 0x01000001

HDR = struct.Struct("<6I")
HDR_SIZE = HDR.size
CHECKSUM_SIZE = 8


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="Decode shell payload from probe packets")
    ap.add_argument(
        "--include-logging",
        action="store_true",
        help="Also print PROBE_LOGGING_BUFFER_ID packets",
    )
    return ap.parse_args()


def _read_exact(reader, size: int) -> bytes | None:
    data = bytearray()
    while len(data) < size:
        chunk = reader.read(size - len(data))
        if not chunk:
            return None
        data.extend(chunk)
    return bytes(data)


def main() -> int:
    args = parse_args()

    targets = {PROBE_SHELL_BUFFER_ID}
    if args.include_logging:
        targets.add(PROBE_LOGGING_BUFFER_ID)

    r = sys.stdin.buffer
    w = sys.stdout.buffer

    while True:
        hdr = _read_exact(r, HDR_SIZE)
        if hdr is None:
            return 0

        sync, buffer_id, _fmt, _ts_lo, _ts_hi, data_len = HDR.unpack(hdr)

        if sync != PROBE_EXTRACT_SYNC_WORD:
            # Resync by shifting one byte and retrying.
            carry = hdr[1:]
            nxt = _read_exact(r, 1)
            if nxt is None:
                return 0
            blob = carry + nxt
            # Try immediate decode of shifted window; if still invalid, continue loop.
            try:
                sync2, buffer_id, _fmt, _ts_lo, _ts_hi, data_len = HDR.unpack(blob)
            except struct.error:
                continue
            if sync2 != PROBE_EXTRACT_SYNC_WORD:
                continue

        payload = _read_exact(r, data_len)
        if payload is None:
            return 0

        checksum = _read_exact(r, CHECKSUM_SIZE)
        if checksum is None:
            return 0

        if buffer_id in targets and payload:
            w.write(payload)
            w.flush()


if __name__ == "__main__":
    raise SystemExit(main())
