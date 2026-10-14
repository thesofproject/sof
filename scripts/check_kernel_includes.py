#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation.
"""Report kernel-only headers included by user-space SOF code.

SOF is being partitioned into DSP kernel code (Zephyr supervisor mode:
hardware, interrupts, threads, memory domains, power management, DMA and
DAI driver internals) and user-space code (pipelines, modules and IPC
processing that can run in Zephyr user threads). User-space code must
not depend on kernel-only headers, but today most of it does through
transitive includes.

This script measures that. It reads the dependency information that the
build already recorded (``ninja -t deps``), so what is checked is exactly
what was compiled, and matches every object built from a user-space
source against the kernel-only header patterns in
scripts/kernel_user_partition.txt.

By default it only reports. With --strict it exits with an error when any
user-space object includes a kernel-only header; that is meant for the
later phases of the partition work, once the leaks have been removed.

Usage:
    check_kernel_includes.py [--strict] [--summary] <build-dir> [<build-dir>...]
"""

import argparse
import fnmatch
import os
import subprocess
import sys
from collections import defaultdict

SOF_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_MANIFEST = os.path.join(SOF_ROOT, 'scripts', 'kernel_user_partition.txt')
SOURCE_EXTS = ('.c', '.cpp', '.cc', '.S')


def parse_manifest(path):
	"""Return {section: [glob, ...]} from a simple '[section]' + glob list file."""
	sections = defaultdict(list)
	section = None
	with open(path, encoding='utf-8') as f:
		for line in f:
			line = line.split('#', 1)[0].strip()
			if not line:
				continue
			if line.startswith('[') and line.endswith(']'):
				section = line[1:-1]
				continue
			if section is None:
				sys.exit(f'{path}: pattern "{line}" outside of a [section]')
			sections[section].append(line)
	return sections


def matches(relpath, patterns):
	return any(fnmatch.fnmatchcase(relpath, p) for p in patterns)


def ninja_deps(build_dir):
	"""Yield (object, [dependencies]) from the ninja dependency log."""
	try:
		out = subprocess.run(['ninja', '-C', build_dir, '-t', 'deps'],
				     check=True, capture_output=True, text=True).stdout
	except (OSError, subprocess.CalledProcessError) as e:
		sys.exit(f'{build_dir}: cannot read ninja dependency log: {e}')

	obj, deps = None, []
	for line in out.splitlines():
		if line.startswith((' ', '\t')):
			if obj is not None and line.strip():
				deps.append(line.strip())
		elif ': #deps' in line:
			if obj is not None:
				yield obj, deps
			obj, deps = line.split(': #deps', 1)[0], []
	if obj is not None:
		yield obj, deps


def relative_to(path, root):
	path = os.path.realpath(path)
	root = os.path.realpath(root)
	if path == root or path.startswith(root + os.sep):
		return os.path.relpath(path, root)
	return None


def check_build(build_dir, manifest, sof_root, zephyr_base):
	"""Return {user source: set(kernel headers)} for one build directory."""
	user_src = manifest['user-sources']
	not_user = manifest['not-user-sources']
	kernel_hdr = manifest['kernel-headers']
	zephyr_hdr = manifest['zephyr-kernel-headers']

	leaks = defaultdict(set)
	if not os.path.exists(os.path.join(build_dir, 'build.ninja')):
		sys.exit(f'{build_dir}: not a ninja build directory')

	for _, deps in ninja_deps(build_dir):
		src = next((d for d in deps if d.endswith(SOURCE_EXTS)), None)
		rel_src = relative_to(src, sof_root) if src else None
		if rel_src is None or not matches(rel_src, user_src) or matches(rel_src, not_user):
			continue
		for dep in deps:
			rel = relative_to(dep, sof_root)
			if rel is not None and matches(rel, kernel_hdr):
				leaks[rel_src].add(rel)
				continue
			if zephyr_base:
				rel = relative_to(dep, zephyr_base)
				if rel is not None and matches(rel, zephyr_hdr):
					leaks[rel_src].add('zephyr/' + rel)
	return leaks


def main():
	parser = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
	parser.add_argument('build_dirs', nargs='+', metavar='build-dir')
	parser.add_argument('--manifest', default=DEFAULT_MANIFEST)
	parser.add_argument('--sof-root', default=SOF_ROOT,
			    help='SOF source directory the build directories were built from')
	parser.add_argument('--zephyr-base', default=os.environ.get('ZEPHYR_BASE',
				os.path.join(os.path.dirname(SOF_ROOT), 'zephyr')))
	parser.add_argument('--strict', action='store_true',
			    help='exit with an error if any kernel-only header is included')
	parser.add_argument('--summary', action='store_true',
			    help='only print per-header counts, not every source file')
	args = parser.parse_args()

	manifest = parse_manifest(args.manifest)
	zephyr_base = args.zephyr_base if os.path.isdir(args.zephyr_base) else None

	leaks = defaultdict(set)
	for build_dir in args.build_dirs:
		for src, hdrs in check_build(build_dir, manifest, args.sof_root,
						 zephyr_base).items():
			leaks[src] |= hdrs

	by_header = defaultdict(set)
	for src, hdrs in leaks.items():
		for hdr in hdrs:
			by_header[hdr].add(src)

	if not args.summary:
		for src in sorted(leaks):
			print(f'{src}:')
			for hdr in sorted(leaks[src]):
				print(f'\t{hdr}')
		print()

	print('Kernel-only headers included by user-space sources:')
	for hdr, srcs in sorted(by_header.items(), key=lambda kv: (-len(kv[1]), kv[0])):
		print(f'{len(srcs):5d}  {hdr}')
	print(f'{len(leaks)} user-space source files include {len(by_header)} '
	      f'kernel-only headers')

	if args.strict and leaks:
		return 1
	return 0


if __name__ == '__main__':
	sys.exit(main())
