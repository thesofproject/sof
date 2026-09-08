#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Verify the compiled Cherry Trail RT5677 topology contract."""

import argparse
import struct
import sys
from pathlib import Path


TPLG_MAGIC = 0x41536F43
TPLG_ABI = 5
TYPE_PCM = 7
TYPE_BACKEND_LINK = 10
HEADER = struct.Struct("<9I")
STREAM_CAPS_SIZE = 104
PCM_SIZE = 912
LINK_SIZE = 1656
HW_CONFIG_SIZE = 120


class VerificationError(Exception):
	"""Report a topology contract violation."""


def u32(data: bytes, offset: int) -> int:
	"""Read one little-endian unsigned 32-bit value."""
	return struct.unpack_from("<I", data, offset)[0]


def c_string(data: bytes, offset: int, size: int = 44) -> str:
	"""Read one fixed-width, NUL-terminated topology string."""
	return data[offset : offset + size].split(b"\0", 1)[0].decode("ascii")


def blocks(data: bytes):
	"""Yield validated topology block headers and payloads."""
	offset = 0
	while offset < len(data):
		if len(data) - offset < HEADER.size:
			raise VerificationError(f"truncated block header at offset {offset}")
		magic, abi, _, block_type, size, _, payload_size, _, count = HEADER.unpack_from(
			data, offset
		)
		if magic != TPLG_MAGIC:
			raise VerificationError(f"invalid topology magic at offset {offset}")
		if abi != TPLG_ABI:
			raise VerificationError(f"expected topology ABI {TPLG_ABI}, found {abi}")
		if size != HEADER.size:
			raise VerificationError(f"unexpected block header size {size}")
		payload_start = offset + size
		payload_end = payload_start + payload_size
		if payload_end > len(data):
			raise VerificationError(f"truncated block payload at offset {offset}")
		yield block_type, count, data[payload_start:payload_end]
		offset = payload_end

	if offset != len(data):
		raise VerificationError("topology has trailing bytes")


def records(payload: bytes, count: int, fixed_size: int):
	"""Yield records whose private data follows the fixed topology structure."""
	offset = 0
	for _ in range(count):
		if len(payload) - offset < fixed_size:
			raise VerificationError("truncated topology record")
		record_size = u32(payload, offset)
		if record_size != fixed_size:
			raise VerificationError(
				f"unexpected topology record size {record_size}, expected {fixed_size}"
			)
		private_size = u32(payload, offset + fixed_size - 4)
		end = offset + fixed_size + private_size
		if end > len(payload):
			raise VerificationError("truncated topology private data")
		yield payload[offset:end]
		offset = end

	if offset != len(payload):
		raise VerificationError("topology block contains trailing record data")


def verify_pcm(record: bytes, expected):
	"""Verify a PCM identity, directions, and its active stream capabilities."""
	pcm_name, pcm_id, playback, capture = expected
	actual_name = c_string(record, 4)
	actual_id = u32(record, 92)
	actual_playback = u32(record, 100)
	actual_capture = u32(record, 104)
	if (actual_name, actual_id, actual_playback, actual_capture) != expected:
		raise VerificationError(
			f"unexpected PCM: {(actual_name, actual_id, actual_playback, actual_capture)}"
		)

	for direction, enabled, offset in (
		("playback", playback, 692),
		("capture", capture, 692 + STREAM_CAPS_SIZE),
	):
		caps_size = u32(record, offset)
		if not enabled:
			if caps_size != 0:
				raise VerificationError(f"{pcm_name} unexpectedly enables {direction}")
			continue
		if caps_size != STREAM_CAPS_SIZE:
			raise VerificationError(f"{pcm_name} has invalid {direction} capabilities")
		rate_min = u32(record, offset + 60)
		rate_max = u32(record, offset + 64)
		channels_min = u32(record, offset + 68)
		channels_max = u32(record, offset + 72)
		if (rate_min, rate_max, channels_min, channels_max) != (48000, 48000, 2, 2):
			raise VerificationError(
				f"{pcm_name} {direction} is not fixed at 48 kHz stereo"
			)


def verify_link(record: bytes):
	"""Verify the single SSP2 hardware configuration."""
	if c_string(record, 8) != "SSP2-Codec":
		raise VerificationError(f"unexpected backend link {c_string(record, 8)!r}")
	if u32(record, 1636) != 1:
		raise VerificationError("SSP2-Codec must contain exactly one hardware configuration")
	if u32(record, 1640) != 0:
		raise VerificationError("SSP2-Codec default hardware configuration must be ID 0")

	hw = record[676 : 676 + HW_CONFIG_SIZE]
	if u32(hw, 0) != HW_CONFIG_SIZE or u32(hw, 4) != 0:
		raise VerificationError("invalid SSP2 hardware configuration header")
	actual = {
		"format": u32(hw, 8),
		"invert_bclk": hw[13],
		"invert_fsync": hw[14],
		"bclk_provider": hw[15],
		"fsync_provider": hw[16],
		"mclk_direction": hw[17],
		"mclk_rate": u32(hw, 20),
		"bclk_rate": u32(hw, 24),
		"fsync_rate": u32(hw, 28),
		"tdm_slots": u32(hw, 32),
		"tdm_slot_width": u32(hw, 36),
		"tx_slots": u32(hw, 40),
		"rx_slots": u32(hw, 44),
	}
	expected = {
		"format": 5,  # SND_SOC_DAI_FORMAT_DSP_B
		"invert_bclk": 1,
		"invert_fsync": 0,
		"bclk_provider": 1,
		"fsync_provider": 1,
		"mclk_direction": 1,
		"mclk_rate": 19_200_000,
		"bclk_rate": 4_800_000,
		"fsync_rate": 48_000,
		"tdm_slots": 4,
		"tdm_slot_width": 25,
		"tx_slots": 0x3,
		"rx_slots": 0x3,
	}
	if actual != expected:
		differences = ", ".join(
			f"{key}={actual[key]!r} (expected {value!r})"
			for key, value in expected.items()
			if actual[key] != value
		)
		raise VerificationError(f"invalid SSP2 hardware configuration: {differences}")

	private_size = u32(record, LINK_SIZE - 4)
	private = record[LINK_SIZE : LINK_SIZE + private_size]
	offset = 0
	sample_bits = []
	while offset < len(private):
		if len(private) - offset < 12:
			raise VerificationError("truncated SSP2 vendor array")
		array_size, tuple_type, count = struct.unpack_from("<III", private, offset)
		if array_size < 12 or offset + array_size > len(private):
			raise VerificationError("invalid SSP2 vendor array size")
		element_size = 48 if tuple_type == 1 else 8
		if 12 + count * element_size != array_size:
			raise VerificationError("invalid SSP2 vendor array element count")
		for element in range(count):
			element_offset = offset + 12 + element * element_size
			token = u32(private, element_offset)
			if token == 502:
				sample_bits.append(u32(private, element_offset + 4))
		offset += array_size
	if sample_bits != [24]:
		raise VerificationError(f"SSP2 valid sample bits must be 24, found {sample_bits}")


def verify(path: Path) -> None:
	"""Verify all Yoga Book topology invariants in a compiled artifact."""
	data = path.read_bytes()
	if not data:
		raise VerificationError("topology artifact is empty")

	pcm_records = []
	link_records = []
	for block_type, count, payload in blocks(data):
		if block_type == TYPE_PCM:
			pcm_records.extend(records(payload, count, PCM_SIZE))
		elif block_type == TYPE_BACKEND_LINK:
			link_records.extend(records(payload, count, LINK_SIZE))

	if len(link_records) != 1:
		raise VerificationError(
			f"expected one SSP2-Codec backend link, found {len(link_records)}"
		)
	if len(pcm_records) != 2:
		raise VerificationError(f"expected two PCMs, found {len(pcm_records)}")

	verify_link(link_records[0])
	verify_pcm(pcm_records[0], ("PCM", 0, 1, 1))
	verify_pcm(pcm_records[1], ("PCM Deep Buffer", 1, 1, 0))


def main() -> int:
	"""Parse arguments and report a concise verification result."""
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("topology", type=Path, help="compiled sof-cht-rt5677.tplg")
	args = parser.parse_args()
	try:
		verify(args.topology)
	except (OSError, UnicodeDecodeError, struct.error, VerificationError) as error:
		print(f"FAIL: {error}", file=sys.stderr)
		return 1
	print(f"PASS: {args.topology} matches the Yoga Book SSP2 and stereo PCM contract")
	return 0


if __name__ == "__main__":
	sys.exit(main())
