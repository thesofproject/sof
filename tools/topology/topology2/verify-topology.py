#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Verify PCM and backend-link properties in a compiled topology."""

import argparse
import struct
import sys
from pathlib import Path


TPLG_MAGIC = 0x41536F43
TPLG_ABI = 5
TYPE_PCM = 7
TYPE_BACKEND_LINK = 10
TUPLE_TYPE_UUID = 0
TUPLE_TYPE_STRING = 1
TUPLE_TYPE_BOOL = 2
TUPLE_TYPE_BYTE = 3
TUPLE_TYPE_WORD = 4
TUPLE_TYPE_SHORT = 5
HEADER = struct.Struct("<9I")
STREAM_CAPS_SIZE = 104
PCM_SIZE = 912
LINK_SIZE = 1656
HW_CONFIG_SIZE = 120
TUPLE_ELEMENT_SIZES = {
	TUPLE_TYPE_UUID: 20,
	TUPLE_TYPE_STRING: 48,
	TUPLE_TYPE_BOOL: 8,
	TUPLE_TYPE_BYTE: 8,
	TUPLE_TYPE_WORD: 8,
	TUPLE_TYPE_SHORT: 8,
}

DEFAULT_PCMS = (
	("PCM", 0, 1, 1),
	("PCM Deep Buffer", 1, 1, 0),
)

DEFAULT_LINK = {
	"name": "SSP2-Codec",
	"hw_config_id": 0,
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
	"sample_bits": 24,
}


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


def verify_pcm(record: bytes, expected, rate: int, channels: int):
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
		if (rate_min, rate_max, channels_min, channels_max) != (
			rate,
			rate,
			channels,
			channels,
		):
			raise VerificationError(
				f"{pcm_name} {direction} is not fixed at {rate} Hz and "
				f"{channels} channels"
			)


def verify_link(record: bytes, expected):
	"""Verify one backend link and its hardware configuration."""
	if c_string(record, 8) != expected["name"]:
		raise VerificationError(f"unexpected backend link {c_string(record, 8)!r}")
	if u32(record, 1636) != 1:
		raise VerificationError(
			f"{expected['name']} must contain exactly one hardware configuration"
		)
	if u32(record, 1640) != expected["hw_config_id"]:
		raise VerificationError(
			f"{expected['name']} default hardware configuration must be ID "
			f"{expected['hw_config_id']}"
		)

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
	expected_hw = {
		key: expected[key]
		for key in actual
	}
	if actual != expected_hw:
		differences = ", ".join(
			f"{key}={actual[key]!r} (expected {value!r})"
			for key, value in expected_hw.items()
			if actual[key] != value
		)
		raise VerificationError(
			f"invalid {expected['name']} hardware configuration: {differences}"
		)

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
		try:
			element_size = TUPLE_ELEMENT_SIZES[tuple_type]
		except KeyError as error:
			raise VerificationError(
				f"unsupported SSP2 vendor tuple type {tuple_type}"
			) from error
		if 12 + count * element_size != array_size:
			raise VerificationError("invalid SSP2 vendor array element count")
		for element in range(count):
			element_offset = offset + 12 + element * element_size
			token = u32(private, element_offset)
			if tuple_type == TUPLE_TYPE_WORD and token == 502:
				sample_bits.append(u32(private, element_offset + 4))
		offset += array_size
	if sample_bits != [expected["sample_bits"]]:
		raise VerificationError(
			f"{expected['name']} valid sample bits must be "
			f"{expected['sample_bits']}, found {sample_bits}"
		)


def verify(path: Path, expected_pcms, expected_link, rate: int, channels: int) -> None:
	"""Verify the requested topology invariants in a compiled artifact."""
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
			f"expected one {expected_link['name']} backend link, "
			f"found {len(link_records)}"
		)
	if len(pcm_records) != len(expected_pcms):
		raise VerificationError(
			f"expected {len(expected_pcms)} PCMs, found {len(pcm_records)}"
		)

	verify_link(link_records[0], expected_link)
	for record, expected in zip(pcm_records, expected_pcms):
		verify_pcm(record, expected, rate, channels)


def integer(value: str) -> int:
	"""Parse a decimal or prefixed integer for command-line options."""
	try:
		return int(value, 0)
	except ValueError as error:
		raise argparse.ArgumentTypeError(f"invalid integer: {value}") from error


def pcm(value: str):
	"""Parse NAME:ID:DIRECTIONS into the topology PCM tuple."""
	try:
		name, pcm_id, direction_list = value.rsplit(":", 2)
	except ValueError as error:
		raise argparse.ArgumentTypeError(
			"PCM must use NAME:ID:playback,capture syntax"
		) from error

	directions = set(direction_list.split(","))
	if not name or not directions or not directions <= {"playback", "capture"}:
		raise argparse.ArgumentTypeError(
			"PCM directions must be playback, capture, or playback,capture"
		)

	return (
		name,
		integer(pcm_id),
		int("playback" in directions),
		int("capture" in directions),
	)


def add_link_arguments(parser: argparse.ArgumentParser) -> None:
	"""Add overridable backend-link expectations with Yoga Book defaults."""
	parser.add_argument("--link-name", default=DEFAULT_LINK["name"])
	parser.add_argument(
		"--hw-config-id", type=integer, default=DEFAULT_LINK["hw_config_id"]
	)
	parser.add_argument("--format", type=integer, default=DEFAULT_LINK["format"])
	for name in (
		"invert-bclk",
		"invert-fsync",
		"bclk-provider",
		"fsync-provider",
		"mclk-direction",
	):
		parser.add_argument(
			f"--{name}",
			type=int,
			choices=(0, 1),
			default=DEFAULT_LINK[name.replace("-", "_")],
		)
	for name in (
		"mclk-rate",
		"bclk-rate",
		"fsync-rate",
		"tdm-slots",
		"tdm-slot-width",
		"tx-slots",
		"rx-slots",
		"sample-bits",
	):
		parser.add_argument(
			f"--{name}",
			type=integer,
			default=DEFAULT_LINK[name.replace("-", "_")],
		)


def main() -> int:
	"""Parse arguments and report a concise verification result."""
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("topology", type=Path, help="compiled topology file")
	parser.add_argument(
		"--pcm",
		action="append",
		type=pcm,
		metavar="NAME:ID:DIRECTIONS",
		help="expected PCM; repeat for each PCM in topology order",
	)
	parser.add_argument("--rate", type=integer, default=48_000)
	parser.add_argument("--channels", type=integer, default=2)
	add_link_arguments(parser)
	args = parser.parse_args()
	expected_pcms = args.pcm if args.pcm is not None else DEFAULT_PCMS
	expected_link = dict(DEFAULT_LINK)
	expected_link["name"] = args.link_name
	for key in DEFAULT_LINK.keys() - {"name"}:
		expected_link[key] = getattr(args, key)
	try:
		verify(args.topology, expected_pcms, expected_link, args.rate, args.channels)
	except (OSError, UnicodeDecodeError, struct.error, VerificationError) as error:
		print(f"FAIL: {error}", file=sys.stderr)
		return 1
	print(f"PASS: {args.topology} matches the requested backend link and PCM contract")
	return 0


if __name__ == "__main__":
	sys.exit(main())
