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
from elftools.elf.constants import SH_FLAGS
import re
import pathlib

args = None
def parse_args():
	global args

	parser = argparse.ArgumentParser(description='Helper utility to run a linker command '
                                         'with calculated ELF section addresses')

	parser.add_argument('command', type=str, help='Linker command to execute')
	parser.add_argument('params', nargs='+', help='Additional linker parameters')
	parser.add_argument("-f", "--file", required=True, type=str,
						help='Object file name')
	parser.add_argument("-c", "--copy", required=True, type=str,
						help='Objcopy command')
	parser.add_argument("-o", "--output", required=True, type=str,
						help='Output file name')
	parser.add_argument("-t", "--text-addr", required=True, type=str,
						help='.text section address')
	parser.add_argument("-s", "--size-file", required=True, type=str,
						help='File with stored accumulated size')

	args = parser.parse_args()

def align_up(addr, align):
	upper = addr + align - 1
	return upper - (upper % align)

def max_alignment(addr, align1, align2):
	if align2 > align1:
		align1 = align2

	upper = addr + align1 - 1
	return upper - (upper % align1)

def main():
	global args

	parse_args()

	# Get the size of the previous module, if this isn't the first one.
	# It is used to automatically calculate starting address of the current
	# module.
	try:
		with open(args.size_file, 'r') as f_size:
			size = int(f_size.read().strip(), base = 0)
	except OSError:
		size = 0

	text_addr = int(args.text_addr, 0) + size
	text_size = 0

	# File names differ when building shared or relocatable objects
	if args.file[:-3] == '.so':
		p = re.compile(r'(^lib|\.so$)')
		fname = args.file
	else:
		fpath = pathlib.Path(args.file)
		fname = fpath.name
		p = re.compile(r'(^lib|_llext_lib\.obj$)')
	module = p.sub('', fname)

	command = [args.command]

	executable = []
	writable = []
	readonly = []
	readonly_dram = []

	text_found = False

	elf = ELFFile(open(args.file, 'rb'))

	# Create an object file with sections grouped by their properties,
	# similar to how program segments are created: all executable sections,
	# then all read-only data sections, and eventually all writable data
	# sections like .data and .bss. Each group is aligned on a page boundary
	# (0x1000) to make dynamic memory mapping possible. The resulting object
	# file will either be a shared library or a relocatable (partially
	# linked) object, depending on the build configuration.
	for section in elf.iter_sections():
		s_flags = section.header['sh_flags']
		s_type = section.header['sh_type']
		s_name = section.name
		s_size = section.header['sh_size']
		s_alignment = section.header['sh_addralign']

		if not s_flags & SH_FLAGS.SHF_ALLOC:
			continue

		if (s_flags & (SH_FLAGS.SHF_ALLOC | SH_FLAGS.SHF_EXECINSTR) ==
                    SH_FLAGS.SHF_ALLOC | SH_FLAGS.SHF_EXECINSTR and
		    s_type == 'SHT_PROGBITS'):
			# An executable section.
			if s_name == '.text':
				text_found = True
				text_addr = max_alignment(text_addr, 0x1000, s_alignment)
				text_size = s_size
				command.append(f'-Wl,-Ttext=0x{text_addr:x}')
			else:
				executable.append(section)

			continue

		if (s_flags & (SH_FLAGS.SHF_WRITE | SH_FLAGS.SHF_ALLOC) ==
                    SH_FLAGS.SHF_WRITE | SH_FLAGS.SHF_ALLOC):
			# .data, .bss or other writable sections
			writable.append(section)
			continue

		if s_type == 'SHT_PROGBITS' and s_flags & SH_FLAGS.SHF_ALLOC:
			# .rodata or other read-only sections
			if s_name == '.coldrodata':
				readonly_dram.append(section)
			else:
				readonly.append(section)

	if not text_found:
		raise RuntimeError('No .text section found in the object file')

	# The original LLEXT support in SOF linked all LLEXT modules with pre-
	# calculated addresses. Such modules can only be used at those exact
	# addresses, so we map memory buffers for such modules to those
	# addresses and copy them there.
	# Now we also need to be able to re-link parts of modules at run-time to
	# run at arbitrary memory locations. One of the use-cases is running
	# parts of the module directly in DRAM - sacrificing performance but
	# saving scarce SRAM. We achieve this by placing non-performance
	# critical functions in a .cold ELF section, read-only data in a
	# .coldrodata ELF section, etc. When compiling and linking such
	# functions, an additional .cold.literal section is automatically
	# created. Note, that for some reason the compiler also marks .cold as
	# executable.
	# This script links those sections at address 0. We could hard-code
	# section names, but so far we choose to only link .text the "original"
	# way and all other executable sections we link at 0. For data sections
	# we accept only the .coldrodata name for now.

	dram_addr = 0
	first_dram_text = None
	first_dram_rodata = None

	for section in executable:
		s_alignment = section.header['sh_addralign']
		s_name = section.name

		if not first_dram_text:
			first_dram_text = s_name

		dram_addr = align_up(dram_addr, s_alignment)

		command.append(f'-Wl,--section-start={s_name}=0x{dram_addr:x}')

		dram_addr += section.header['sh_size']

	for section in readonly_dram:
		s_alignment = section.header['sh_addralign']
		s_name = section.name

		if not first_dram_rodata:
			first_dram_rodata = s_name

		dram_addr = align_up(dram_addr, s_alignment)

		command.append(f'-Wl,--section-start={s_name}=0x{dram_addr:x}')

		dram_addr += section.header['sh_size']

	start_addr = align_up(text_addr + text_size, 0x1000)

	for section in readonly:
		s_alignment = section.header['sh_addralign']
		s_name = section.name

		start_addr = align_up(start_addr, s_alignment)

		command.append(f'-Wl,--section-start={s_name}=0x{start_addr:x}')

		start_addr += section.header['sh_size']

	start_addr = align_up(start_addr, 0x1000)

	for section in writable:
		s_alignment = section.header['sh_addralign']
		s_name = section.name

		start_addr = align_up(start_addr, s_alignment)

		if s_name == '.data':
			command.append(f'-Wl,-Tdata=0x{start_addr:x}')
		else:
			command.append(f'-Wl,--section-start={s_name}=0x{start_addr:x}')

		start_addr += section.header['sh_size']

	command.extend(['-o', f'{args.file}.tmp'])
	command.extend(args.params)

	subprocess.run(command)

	copy_command = [args.copy]

	if first_dram_text:
		copy_command.extend(['--set-section-alignment', f'{first_dram_text}=4096'])
	if first_dram_rodata:
		copy_command.extend(['--set-section-alignment', f'{first_dram_rodata}=4096'])

	copy_command.extend([f'{args.file}.tmp', f'{args.output}'])
	subprocess.run(copy_command)

if __name__ == "__main__":
	main()
