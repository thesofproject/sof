#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation.
"""Report platform headers that nothing includes and platform macros that nothing uses.

The platform headers in src/kernel/platform/<platform>/include and
src/platform/<platform>/include give the generic SOF code its view of a
platform. Over time they collected headers and macros that no code reads
any more. This script finds them in the source tree, without building:

- a header is unused when no code that can be built for its platform can
  include it (following #include from the platform's sources, with the
  platform's include path; #if is not evaluated);
- a macro is unused when its name appears nowhere in code that can be
  built together with its platform's headers: the generic SOF code
  (src/ outside the platform and vendor driver directories), the Zephyr
  glue (zephyr/, app/) and the platform's own platform and vendor driver
  directories. The host library platform sees the host-only code
  (posix/, tools/, test/cmocka/, the host arch) instead of the Zephyr
  glue, and the posix platform also sees the ztest unit tests. Text
  matching counts every #if branch, so the result does not depend on the
  configuration. Macros only used by unused macros are unused too.

Not reported: include guards, names built by token pasting, the _BASE or
_SIZE (_START or _END) partner of a used macro, the other macros of a
mailbox window with a used member, and the vendor-generated Xtensa core
headers in include/arch/.

The platforms, the directories they share and their vendor driver
directories are listed in scripts/platform_headers.txt. Its [baseline]
section lists the known findings. By default the script only reports.
With --strict it exits with an error on a finding that is not in the
baseline, so new unused headers and macros fail CI; the baseline may
only shrink. Baseline entries that are no longer found are reported so
that they can be dropped. --update-baseline rewrites the baseline with
the current findings.

--contract prints, for each platform header include name, the macros
that generic code uses from it: what a platform has to provide.

Usage:
    check_platform_headers.py [--strict] [--summary]
    check_platform_headers.py --update-baseline
    check_platform_headers.py --contract
"""

import argparse
import fnmatch
import os
import posixpath
import re
import subprocess
import sys
from collections import defaultdict

SOF_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_MANIFEST = os.path.join(SOF_ROOT, 'scripts', 'platform_headers.txt')

KP = 'src/kernel/platform/'
UP = 'src/platform/'
HOST = ('posix/', 'tools/', 'test/cmocka/', 'src/kernel/arch/host/', 'src/kernel/drivers/host/')
CODE_EXTS = ('.c', '.h', '.cpp', '.cc', '.hpp', '.S', '.ld', '.ld.in', '.x.in')
SOURCE_EXTS = ('.c', '.cpp', '.cc', '.S')
# include directories that every build has, besides the platform ones, in the order the
# builds search them: the zephyr/include and posix/include wrappers come first
FW_INCLUDE = ['zephyr/include', 'src/include', 'src/kernel/include', 'third_party/include',
	      'src/kernel/arch/xtensa/include', 'tools/rimage/src/include']
HOST_INCLUDE = ['posix/include', 'src/include', 'src/kernel/include', 'third_party/include',
		'src/kernel/arch/host/include', 'tools/testbench/include']

IDENT = re.compile(r'[A-Za-z_]\w*')
DEFINE = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)')
IFNDEF = re.compile(r'^\s*#\s*(?:ifndef|undef)\s+([A-Za-z_]\w*)')
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)
# a name prefix pasted with ## ("PREFIX_ ## x"); a bare parameter ("x ## _suffix") is not one
PASTE = re.compile(r'([A-Za-z_]\w*_)\s*##')
PARTNERS = (('_BASE', '_SIZE'), ('_SIZE', '_BASE'), ('_START', '_END'), ('_END', '_START'))


def parse_manifest(path):
	"""Return {section: [line, ...]} from a '[section]' + one entry per line file."""
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
				sys.exit(f'{path}: entry "{line}" outside of a [section]')
			sections[section].append(line)
	return sections


def git_files():
	out = subprocess.run(['git', 'ls-files'], cwd=SOF_ROOT, check=True,
			     capture_output=True, text=True).stdout
	return [f for f in out.splitlines()
		if f.endswith(CODE_EXTS) and os.path.isfile(os.path.join(SOF_ROOT, f))]


class Source:
	"""Macro definitions, identifier uses and includes of one file, comments removed."""

	def __init__(self, path):
		with open(os.path.join(SOF_ROOT, path), encoding='utf-8', errors='replace') as f:
			raw = f.read()
		text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), raw, flags=re.S)
		text = re.sub(r'//[^\n]*', '', text)
		self.includes = INCLUDE.findall(text)
		self.defines = defaultdict(list)	# name -> [line]
		self.bodies = defaultdict(str)		# name -> body text
		self.uses = []				# (name, macro whose body has it, or None)
		self.guards = set()
		self.pasted = set()			# prefixes pasted with ##
		lines = text.split('\n')
		cur = None
		for i, line in enumerate(lines, 1):
			m = DEFINE.match(line)
			if m:
				cur = m.group(1)
				self.defines[cur].append(i)
				prev = next((l for l in reversed(lines[:i - 1]) if l.strip()), '')
				if re.match(r'\s*#\s*ifndef\s+' + cur + r'\b', prev):
					self.guards.add(cur)
				body = line[m.end():]
			else:
				body = line
			if cur:
				self.bodies[cur] += body + '\n'
			self.pasted.update(p for p in PASTE.findall(body) if len(p) >= 3)
			skip = IFNDEF.match(line)
			for t in IDENT.findall(body):
				self.uses.append((t, '#ifndef' if skip and t == skip.group(1) else cur))
			if not line.rstrip().endswith('\\'):
				cur = None


class Tree:
	def __init__(self, manifest):
		self.m = parse_manifest(manifest)
		self.files = git_files()
		self.fileset = set(self.files)
		self.cache = {}
		self.dirs = sorted({re.sub(r'/include/.*', '', f[len(p):]) for f in self.files
				    for p in (KP, UP) if f.startswith(p) and '/include/' in f})
		self.groups = {}
		shared = set()
		for line in self.m['groups']:
			name, dirs = line.split(':', 1)
			self.groups[name.strip()] = dirs.split()
			shared.update(dirs.split())
		for d in self.dirs:
			if d not in shared and d not in self.groups:
				self.groups[d] = [d]
		self.vendor = []	# (group name pattern, driver directory)
		for line in self.m['vendor']:
			pat, d = line.split(':', 1)
			self.vendor.append((pat.strip(), d.strip().rstrip('/') + '/'))
		self.host = set(self.m['host'])
		self.posix = set(self.m['posix'])
		self.vendor_dirs = tuple(sorted({d for _, d in self.vendor}))
		self.index = None

	def src(self, path):
		if path not in self.cache:
			self.cache[path] = Source(path)
		return self.cache[path]

	def own(self, f, g):
		return any(f.startswith((KP + d + '/', UP + d + '/')) for d in self.groups[g])

	def generic(self, f):
		return f.startswith(('src/', 'zephyr/', 'app/')) and \
			not f.startswith((KP, UP) + self.vendor_dirs + HOST)

	def visible(self, f, g):
		"""Can f be built together with the headers of platform group g?"""
		if self.own(f, g):
			return True
		if any(fnmatch.fnmatchcase(g, pat) and f.startswith(d) for pat, d in self.vendor):
			return True
		if g in self.host:
			return (self.generic(f) and not f.startswith(('zephyr/', 'app/'))) or f.startswith(HOST)
		if g in self.posix and f.startswith('test/ztest/'):
			return True
		return self.generic(f)

	def headers(self, g):
		return [f for f in self.files if self.own(f, g) and f.endswith('.h') and '/include/arch/' not in f]

	def include_path(self, g):
		dirs = []
		for d in self.groups[g]:
			dirs += [KP + d + '/include', UP + d + '/include']
		return dirs + (HOST_INCLUDE if g in self.host else FW_INCLUDE)

	def unreachable(self, g):
		path = self.include_path(g)
		seen = set()
		stack = [f for f in self.files if f.endswith(SOURCE_EXTS) and self.visible(f, g)]
		while stack:
			f = stack.pop()
			if f in seen:
				continue
			seen.add(f)
			for q, name in self.src(f).includes:
				cands = [posixpath.normpath(posixpath.join(posixpath.dirname(f), name))] if q == '"' else []
				cands += [posixpath.normpath(posixpath.join(d, name)) for d in path]
				for c in cands:
					if c in self.fileset:
						stack.append(c)
						break
		return [h for h in self.headers(g) if h not in seen]

	def uses_index(self):
		"""{platform macro name: [(file, enclosing macro)]} over the whole tree"""
		if self.index is None:
			names = set()
			for f in self.files:
				if f.startswith((KP, UP)) and f.endswith('.h'):
					names.update(self.src(f).defines)
			self.index = defaultdict(list)
			for f in self.files:
				for t, enc in self.src(f).uses:
					if t in names:
						self.index[t].append((f, enc))
		return self.index

	def macros(self, g):
		"""-> ({unused name: [header]}, used names, {name: [header]}) for group g"""
		hdrs = self.headers(g)
		cand = defaultdict(list)
		for h in hdrs:
			s = self.src(h)
			for n in s.defines:
				if n not in s.guards:
					cand[n].append(h)
		vis = {f for f in self.files if self.visible(f, g)}
		pasted = set()
		for f in vis:
			pasted |= self.src(f).pasted
		uses = defaultdict(list)
		index = self.uses_index()
		for n in cand:
			for f, enc in index[n]:
				if f not in vis:
					continue
				if enc == '#ifndef':
					if n in self.src(f).defines:
						continue	# a guard or a duplicate-definition guard
					enc = None
				uses[n].append(enc)
		alive = {n for n in cand if any(n.startswith(p) for p in pasted)}
		changed = True
		while changed:
			changed = False
			for n in cand:
				if n not in alive and any(e is None or (e != n and (e not in cand or e in alive))
							 for e in uses[n]):
					alive.add(n)
					changed = True
		dead = set(cand) - alive
		# keep the partner of a used macro, and the rest of a used mailbox window,
		# and anything a kept macro needs
		kept = set()
		for n in dead:
			for a, b in PARTNERS:
				if n.endswith(a) and n[:-len(a)] + b in alive:
					kept.add(n)
			if any('/lib/mailbox' in h for h in cand[n]):
				w = re.sub(r'_(BASE|SIZE|OFFSET)$', '', n)
				if any(re.sub(r'_(BASE|SIZE|OFFSET)$', '', x) == w for x in alive):
					kept.add(n)
		changed = True
		while changed:
			changed = False
			for k in list(kept):
				for h in cand[k]:
					for t in IDENT.findall(self.src(h).bodies.get(k, '')):
						if t in dead and t not in kept:
							kept.add(t)
							changed = True
		dead -= kept
		return {n: cand[n] for n in dead}, alive, cand

	def findings(self):
		hdr_groups, unused_in = defaultdict(set), defaultdict(set)
		def_groups, dead_in = defaultdict(set), defaultdict(set)
		for g in self.groups:
			for h in self.headers(g):
				hdr_groups[h].add(g)
			for h in self.unreachable(g):
				unused_in[h].add(g)
			dead, _, cand = self.macros(g)
			for n, hs in cand.items():
				for h in hs:
					def_groups[(h, n)].add(g)
			for n, hs in dead.items():
				for h in hs:
					dead_in[(h, n)].add(g)
		# a header or macro shared by several platforms is only unused if it is in all of them
		hdrs = {h for h, gs in unused_in.items() if gs == hdr_groups[h]}
		macros = {f'{h}:{n}' for (h, n), gs in dead_in.items() if gs == def_groups[(h, n)] and h not in hdrs}
		return hdrs | macros

	def contract(self):
		"""{include name: (macros firmware code uses, macros only host code uses)}"""
		def alive_in(files):
			"""names used by plain code in files, or by a macro of files that is"""
			uses = defaultdict(list)
			for f in files:
				src = self.src(f)
				for t, enc in src.uses:
					if enc == '#ifndef':
						if t in src.defines:
							continue
						enc = None
					uses[t].append(enc)
			alive = {t for t, es in uses.items() if None in es}
			changed = True
			while changed:
				changed = False
				for t, es in uses.items():
					if t not in alive and any(e in alive for e in es):
						alive.add(t)
						changed = True
			return alive

		fw = alive_in([f for f in self.files if self.generic(f)])
		host = alive_in([f for f in self.files
				 if (self.generic(f) and not f.startswith(('zephyr/', 'app/'))) or f.startswith(HOST)])
		res = defaultdict(lambda: (set(), set()))
		for g in self.groups:
			for h in self.headers(g):
				name = h.split('/include/', 1)[1]
				src = self.src(h)
				for n in src.defines:
					if n in src.guards:
						continue
					if n in fw:
						res[name][0].add(n)
					elif n in host:
						res[name][1].add(n)
		return res


def write_baseline(manifest, findings):
	with open(manifest, encoding='utf-8') as f:
		text = f.read()
	head = text.split('[baseline]', 1)[0]
	body = '\n'.join(sorted(findings))
	with open(manifest, 'w', encoding='utf-8') as f:
		f.write(head + '[baseline]\n' + body + ('\n' if body else ''))


def main():
	ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
	ap.add_argument('--strict', action='store_true', help='fail on findings that are not in the baseline')
	ap.add_argument('--summary', action='store_true', help='only print the counts')
	ap.add_argument('--update-baseline', action='store_true', help='rewrite the baseline with the current findings')
	ap.add_argument('--contract', action='store_true', help='print the macros generic code uses, per header')
	ap.add_argument('--manifest', default=DEFAULT_MANIFEST)
	args = ap.parse_args()

	tree = Tree(args.manifest)
	if args.contract:
		for name, (fw, host) in sorted(tree.contract().items()):
			print(f'{name}: {" ".join(sorted(fw)) or "-"}' + (f'; host only: {" ".join(sorted(host))}' if host else ''))
		return 0

	found = tree.findings()
	if args.update_baseline:
		write_baseline(args.manifest, found)
		print(f'{len(found)} findings written to the baseline')
		return 0

	baseline = set(tree.m['baseline'])
	new = sorted(found - baseline)
	stale = sorted(baseline - found)
	if not args.summary:
		for f in new:
			kind = 'macro' if ':' in f else 'header'
			print(f'unused platform {kind}: {f}')
		for f in stale:
			print(f'no longer found, drop from the baseline: {f}')
	print(f'{len(found)} unused platform headers and macros, {len(new)} not in the baseline, '
	      f'{len(stale)} baseline entries no longer found')
	return 1 if args.strict and new else 0


if __name__ == '__main__':
	sys.exit(main())
