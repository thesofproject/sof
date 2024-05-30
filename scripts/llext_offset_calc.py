#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

# Add rounded new file size to accumulated module size cache

import argparse
import pathlib
import os

args = None

def parse_args():
	global args

	parser = argparse.ArgumentParser(description='Add a file size to a sum in a file')

	parser.add_argument("-i", "--input", required=True, type=str,
						help='Object file name')
	parser.add_argument("-s", "--size-file", required=True, type=str,
						help='File to store accumulated size')

	args = parser.parse_args()

def main():
	global args

	parse_args()

	f_output = pathlib.Path(args.size_file)

	try:
		with open(f_output, 'r') as f_size:
			size = int(f_size.read().strip(), base = 0)
	except OSError:
		size = 0

	# Failure will raise an exception
	f_size = open(f_output, "w")

	# align to a page border
	size += os.path.getsize(args.input) + 0xfff
	size &= ~0xfff

	f_size.write(f'0x{size:x}\n')

if __name__ == "__main__":
	main()
