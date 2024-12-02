#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

# Add rounded new file size to accumulated module size cache

import argparse
import pathlib
import os
from elftools.elf.elffile import ELFFile
from elftools.elf.constants import SH_FLAGS

args = None

def parse_args():
	global args

	parser = argparse.ArgumentParser(description='Add a file size to a sum in a file')

	parser.add_argument("-i", "--input", required=True, type=str,
						help='Object file name')
	parser.add_argument("-s", "--size-file", required=True, type=str,
						help='File to store accumulated size')

	args = parser.parse_args()

def get_elf_size(elf_name):
	start = 0xffffffff
	# SOF_MODULE_DRAM_LINK_END
	min_start = 0x08000000
	end = 0
	with open(elf_name, 'rb') as f_elf:
		elf = ELFFile(f_elf)

		for section in elf.iter_sections():
			s_flags = section.header['sh_flags']

			if not s_flags & SH_FLAGS.SHF_ALLOC:
				continue

			# Ignore detached sections, to be used in DRAM, their addresses
			# are below min_start
			if section.header['sh_addr'] < min_start:
				continue

			if section.header['sh_addr'] < start:
				start = section.header['sh_addr']
			if section.header['sh_addr'] + section.header['sh_size'] > end:
				end = section.header['sh_addr'] + section.header['sh_size']

		size = end - start

	return size

def main():
	global args

	parse_args()

	f_output = pathlib.Path(args.size_file)

	try:
		with open(f_output, 'r') as f_size:
			size = int(f_size.read().strip(), base = 0)
	except OSError:
		size = 0

	size += get_elf_size(args.input) + 0xfff
	# align to a page border
	size &= ~0xfff

	# Failure will raise an exception
	f_size = open(f_output, "w")
	f_size.write(f'0x{size:x}\n')

if __name__ == "__main__":
	main()
