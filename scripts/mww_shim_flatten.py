#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.

"""Flatten -ffunction-sections naming in the microwakeword libc shim objects.

The shim is assembled by extracting prebuilt members out of the toolchain's
libc.a/libm.a. Those members were compiled without -mlongcalls, so their calls
to the libgcc soft-float helpers (__addsf3, __mulsf3, __adddf3, ...) are plain
CALL8 instructions with a +/-512KB reach.

Zephyr's Xtensa linker script places plain `.text` (where libgcc's helpers live)
via `*(.literal .text)`, i.e. at the very front of the .text output section, and
per-function `.text.*` via the later `*(.literal.* .text.*)` rule, i.e. at the
very back. On a large image (ptl) the two ends are more than 512KB apart and the
link fails with:

    dangerous relocation: call8: call target out of range: __addsf3

Renaming the shim's `.text.<fn>`/`.literal.<fn>` back to plain `.text`/`.literal`
puts these few functions in the same early cluster as libgcc, so the calls stay
in range. Order within a single `*(...)` wildcard follows input order, so each
object's `.literal` still precedes its `.text` and l32r reach is unaffected.
"""

import pathlib
import struct
import subprocess
import sys


def section_names(path):
	"""Return the section names of a 32-bit little-endian ELF object."""
	data = path.read_bytes()
	if data[:4] != b'\x7fELF' or data[4] != 1 or data[5] != 1:
		return []

	e_shoff, = struct.unpack_from('<I', data, 0x20)
	e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', data, 0x2e)
	if not e_shoff or not e_shnum:
		return []

	strtab_off, = struct.unpack_from('<I', data, e_shoff + e_shstrndx * e_shentsize + 0x10)

	names = []
	for i in range(e_shnum):
		sh_name, = struct.unpack_from('<I', data, e_shoff + i * e_shentsize)
		start = strtab_off + sh_name
		end = data.index(b'\0', start)
		names.append(data[start:end].decode())
	return names


def main():
	if len(sys.argv) != 3:
		sys.exit(f'usage: {sys.argv[0]} <objcopy> <dir>')

	objcopy, directory = sys.argv[1], pathlib.Path(sys.argv[2])

	for obj in sorted(directory.glob('*.o')):
		args = []
		for name in section_names(obj):
			for prefix in ('.text.', '.literal.'):
				if name.startswith(prefix):
					args += ['--rename-section',
						 f'{name}={prefix.rstrip(".")}']
					break
		if args:
			subprocess.run([objcopy] + args + [str(obj)], check=True)


if __name__ == '__main__':
	main()
