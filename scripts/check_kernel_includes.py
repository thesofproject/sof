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

By default it only reports. With --strict it exits with an error when a
user-space object includes a kernel-only header, unless its source is
listed in the manifest's [leak-baseline] section. The baseline lists the
sources that still have leaks to remove; it may only shrink, so new
leaks fail the build. Baseline sources that no longer leak in the
checked builds are reported, so that they can be dropped from the list
once they are clean in every configuration.

With --sources it checks the source tree instead of build directories:
kernel-only headers in src/kernel/include are included as
<sof/kernel/...>, and no user-space source or header may include one.
This covers code that no checked build compiles, and has no baseline.

Usage:
    check_kernel_includes.py [--strict] [--summary] <build-dir> [<build-dir>...]
    check_kernel_includes.py --sources [--strict]
"""

import argparse
import fnmatch
import os
import re
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
	"""Return ({user source: set(kernel headers)}, set(user sources)) for one
	build directory."""
	user_src = manifest['user-sources']
	not_user = manifest['not-user-sources']
	kernel_hdr = manifest['kernel-headers']
	zephyr_hdr = manifest['zephyr-kernel-headers']

	leaks = defaultdict(set)
	built = set()
	if not os.path.exists(os.path.join(build_dir, 'build.ninja')):
		sys.exit(f'{build_dir}: not a ninja build directory')

	for _, deps in ninja_deps(build_dir):
		src = next((d for d in deps if d.endswith(SOURCE_EXTS)), None)
		rel_src = relative_to(src, sof_root) if src else None
		if rel_src is None or not matches(rel_src, user_src) or matches(rel_src, not_user):
			continue
		built.add(rel_src)
		for dep in deps:
			rel = relative_to(dep, sof_root)
			if rel is not None and matches(rel, kernel_hdr):
				leaks[rel_src].add(rel)
				continue
			if zephyr_base:
				rel = relative_to(dep, zephyr_base)
				if rel is not None and matches(rel, zephyr_hdr):
					leaks[rel_src].add('zephyr/' + rel)
	return leaks, built


KERNEL_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"](sof/kernel/[^>"]+)[>"]', re.M)


def source_files(sof_root):
	"""Return the files of the SOF source tree, relative to it."""
	try:
		out = subprocess.run(['git', '-C', sof_root, 'ls-files'],
				     check=True, capture_output=True, text=True).stdout
		return out.splitlines()
	except (OSError, subprocess.CalledProcessError):
		files = []
		for top, dirs, names in os.walk(sof_root):
			dirs[:] = [d for d in dirs if d != '.git' and not d.startswith('build')]
			files += [os.path.relpath(os.path.join(top, n), sof_root).replace(os.sep, '/')
				  for n in names]
		return files


def check_sources(manifest, sof_root):
	"""Return {user-space file: [kernel header includes]} from the source tree."""
	user_src = manifest['user-sources']
	not_user = manifest['not-user-sources']
	user_hdr = manifest['user-headers']
	kernel_hdr = manifest['kernel-headers']

	found = {}
	for rel in source_files(sof_root):
		if rel.endswith(SOURCE_EXTS):
			if not matches(rel, user_src) or matches(rel, not_user):
				continue
		elif not (rel.endswith(('.h', '.hpp')) and matches(rel, user_hdr)) or \
		     matches(rel, not_user) or matches(rel, kernel_hdr):
			continue
		path = os.path.join(sof_root, rel)
		if not os.path.isfile(path):
			continue
		with open(path, encoding='utf-8', errors='replace') as f:
			incs = KERNEL_INCLUDE.findall(f.read())
		if incs:
			found[rel] = incs
	return found


def main():
	parser = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
	parser.add_argument('build_dirs', nargs='*', metavar='build-dir')
	parser.add_argument('--manifest', default=DEFAULT_MANIFEST)
	parser.add_argument('--sof-root', default=SOF_ROOT,
			    help='SOF source directory the build directories were built from')
	parser.add_argument('--zephyr-base', default=os.environ.get('ZEPHYR_BASE',
				os.path.join(os.path.dirname(SOF_ROOT), 'zephyr')))
	parser.add_argument('--strict', action='store_true',
			    help='exit with an error if a source outside of the baseline '
				 'includes a kernel-only header')
	parser.add_argument('--summary', action='store_true',
			    help='only print per-header counts, not every source file')
	parser.add_argument('--sources', action='store_true',
			    help='check the source tree for <sof/kernel/...> includes from '
				 'user-space code instead of build directories')
	args = parser.parse_args()

	manifest = parse_manifest(args.manifest)

	if args.sources:
		if args.build_dirs:
			parser.error('--sources cannot be combined with build directories')
		found = check_sources(manifest, args.sof_root)
		for src in sorted(found):
			print(f'{src}:')
			for inc in found[src]:
				print(f'\t{inc}')
		print(f'{len(found)} user-space files include headers from src/kernel/include')
		return 1 if args.strict and found else 0
	if not args.build_dirs:
		parser.error('build directories or --sources are required')
	zephyr_base = args.zephyr_base if os.path.isdir(args.zephyr_base) else None

	leaks = defaultdict(set)
	built = set()
	for build_dir in args.build_dirs:
		build_leaks, build_srcs = check_build(build_dir, manifest, args.sof_root,
						      zephyr_base)
		for src, hdrs in build_leaks.items():
			leaks[src] |= hdrs
		built |= build_srcs

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

	baseline = manifest['leak-baseline']
	new_leaks = sorted(src for src in leaks if not matches(src, baseline))
	fixed = sorted(src for src in built - leaks.keys() if matches(src, baseline))

	if fixed:
		print('\nBaseline sources built without leaks (drop them from '
		      '[leak-baseline] once they are clean in every configuration):')
		for src in fixed:
			print(f'\t{src}')

	if new_leaks:
		print('\nUser-space sources including kernel-only headers that are not in '
		      '[leak-baseline]:')
		for src in new_leaks:
			print(f'{src}:')
			for hdr in sorted(leaks[src]):
				print(f'\t{hdr}')

	if args.strict and new_leaks:
		return 1
	return 0


if __name__ == '__main__':
	sys.exit(main())
