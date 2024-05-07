#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

# We need to calculate ELF section addresses of an LLEXT module and use them to
# run the linker. We could just use Python to calculate addresses and pass them
# back to cmake to have it call the linker. However, there doesn't seem to be a
# portable way to do that. Therefore we pass the linker path and all the command
# line parameters to this script and call the linker directly.

import os
import argparse
import subprocess
from elftools.elf.elffile import ELFFile
import re

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
	p = re.compile(r'(^lib|\.so$)')
	module = p.sub('', args.file)

	if elf.num_segments() != 0:
		# A shared object type image, it contains segments
		sections = ['.text', '.rodata', '.data', '.bss']
		alignment = [0x1000, 0x1000, 0x10, 0x1000]
	else:
		# A relocatable object, need to handle all sections separately
		sections = ['.text',
			    f'._log_const.static.log_const_{module}_',
			    '.static_uuids', '.z_init_APPLICATION90_0_', '.module',
			    '.mod_buildinfo', '.data', '.trace_ctx', '.bss']
		alignment = [0x1000, 0x1000, 0x0, 0x0, 0x0, 0x0, 0x10, 0x0, 0x1000]

	last_increment = 0

	command = [args.command]

	for i in range(len(sections)):
		try:
			offset = elf.get_section_by_name(sections[i]).header.sh_offset
			size = elf.get_section_by_name(sections[i]).header.sh_size
		except AttributeError:
			print("section " + sections[i] + " not found in " + args.file)
			continue

		if last_increment == 0:
			# first section must be .text and it must be successful
			if i != 0 or sections[i] != '.text':
                                break

			address = text_addr
		elif alignment[i] != 0:
			upper = offset + last_increment + alignment[i] - 1
			address = upper - (upper % alignment[i])
		else:
			address = offset + last_increment

		last_increment = address - offset

		if sections[i] == '.text':
			command.append(f'-Wl,-Ttext=0x{text_addr:x}')
		elif sections[i] == '.data':
			command.append(f'-Wl,-Tdata=0x{address:x}')
		else:
			command.append(f'-Wl,--section-start={sections[i]}=0x{address:x}')

	command.extend(args.params)

	subprocess.run(command)

if __name__ == "__main__":
	main()
