#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

# We need to calculate ELF section addresses of an LLEXT module and use them to
# run the linker. We could just use Python to calculate addresses and pass them
# back to cmake to have it call the linker. However, there doesn't seem to be a
# portable way to do that. Therefore we pass the linker path and all the command
# line parameters to this script and call the linker directly.

import argparse
import subprocess
from elftools.elf.elffile import ELFFile

args = None
def parse_args():
	global args

	parser = argparse.ArgumentParser(description='Helper utility to run a linker command '
                                         'with calculated ELF section addresses')

	parser.add_argument('command', type=str, help='Linker command to execute')
	parser.add_argument('params', nargs='+', help='Additional linker parameters')
	parser.add_argument("-f", "--file", required=True, type=str,
						help='Object file name')
	parser.add_argument("-t", "--text-addr", required=True, type=str,
						help='.text section address')

	args = parser.parse_args()

def main():
	parse_args()

	elf = ELFFile(open(args.file, 'rb'))

	text_addr = int(args.text_addr, 0)

	text_offset = elf.get_section_by_name('.text').header.sh_offset
	rodata_offset = elf.get_section_by_name('.rodata').header.sh_offset
	data_offset = elf.get_section_by_name('.data').header.sh_offset

	upper = rodata_offset - text_offset + text_addr + 0xfff
	rodata_addr = upper - (upper % 0x1000)

	upper = data_offset - rodata_offset + rodata_addr + 0xf
	data_addr = upper - (upper % 0x10)

	command = [args.command,
		   f'-Wl,-Ttext=0x{text_addr:x}',
		   f'-Wl,--section-start=.rodata=0x{rodata_addr:x}',
		   f'-Wl,-Tdata=0x{data_addr:x}']
	command.extend(args.params)

	subprocess.run(command)

if __name__ == "__main__":
	main()
