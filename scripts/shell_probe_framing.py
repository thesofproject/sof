#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Shell/probe transport framing helpers.

This module defines a small binary frame format suitable for carrying shell
traffic over probe transports that may need explicit chunking, sequencing and
credit-based flow control.
"""

from __future__ import annotations

import dataclasses
import io
import struct
from typing import Generator, Iterable

MAGIC = 0x53504252  # 'SPBR' = Shell Probe BRidge
VERSION = 1

TYPE_DATA = 1
TYPE_CREDIT = 2
TYPE_RESET = 3

FLAG_EOC = 0x0001  # End-of-command / logical message marker

_HEADER = struct.Struct(">IBBHIHH")
_HEADER_SIZE = _HEADER.size  # 16 bytes


@dataclasses.dataclass
class Frame:
    frame_type: int
    seq: int = 0
    payload: bytes = b""
    flags: int = 0
    credits: int = 0


def pack_frame(frame: Frame) -> bytes:
    payload = frame.payload or b""
    if len(payload) > 0xFFFF:
        raise ValueError("payload too large for single frame")
    hdr = _HEADER.pack(
        MAGIC,
        VERSION,
        frame.frame_type & 0xFF,
        frame.flags & 0xFFFF,
        frame.seq & 0xFFFFFFFF,
        len(payload),
        frame.credits & 0xFFFF,
    )
    return hdr + payload


def unpack_frame(buf: bytes) -> Frame:
    if len(buf) < _HEADER_SIZE:
        raise ValueError("buffer too short for frame header")

    magic, version, frame_type, flags, seq, length, credits = _HEADER.unpack(
        buf[:_HEADER_SIZE]
    )

    if magic != MAGIC:
        raise ValueError(f"bad magic: 0x{magic:08x}")
    if version != VERSION:
        raise ValueError(f"unsupported version: {version}")

    end = _HEADER_SIZE + length
    if len(buf) < end:
        raise ValueError("buffer too short for frame payload")

    payload = buf[_HEADER_SIZE:end]
    return Frame(
        frame_type=frame_type,
        seq=seq,
        payload=payload,
        flags=flags,
        credits=credits,
    )


def read_frames(stream: io.BufferedReader) -> Generator[Frame, None, None]:
    """Yield frames from a byte stream until EOF.

    The stream may return partial reads; this function handles reassembly.
    """

    while True:
        hdr = stream.read(_HEADER_SIZE)
        if not hdr:
            return
        if len(hdr) != _HEADER_SIZE:
            raise EOFError("truncated frame header")

        magic, version, frame_type, flags, seq, length, credits = _HEADER.unpack(hdr)

        if magic != MAGIC:
            raise ValueError(f"bad magic: 0x{magic:08x}")
        if version != VERSION:
            raise ValueError(f"unsupported version: {version}")

        payload = stream.read(length)
        if len(payload) != length:
            raise EOFError("truncated frame payload")

        yield Frame(
            frame_type=frame_type,
            seq=seq,
            payload=payload,
            flags=flags,
            credits=credits,
        )


def chunk_bytes(data: bytes, max_payload: int) -> Iterable[bytes]:
    if max_payload <= 0:
        raise ValueError("max_payload must be > 0")
    for i in range(0, len(data), max_payload):
        yield data[i : i + max_payload]
