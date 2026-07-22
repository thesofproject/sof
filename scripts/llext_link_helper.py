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
import fcntl

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
		f_size_file = open(args.size_file, 'a+')
		fcntl.flock(f_size_file, fcntl.LOCK_EX)
		f_size_file.seek(0)
		content = f_size_file.read().strip()
		if content:
			size = int(content, base=0)
		else:
			size = 0
	except OSError:
		size = 0
		f_size_file = None

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
			writable.append(section)
			continue

		else:
			if s_name == '.coldrodata':
				readonly_dram.append(section)
			else:
				readonly.append(section)

	if not text_found:
		raise RuntimeError('No .text section found in the object file')

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

	# Patch the sh_addr and st_value in the output ELF file created by LLD
	import struct

	def do_align_up(addr, align):
		if align <= 1:
			return addr
		upper = addr + align - 1
		return upper - (upper % align)

	addr_map = {'.text': text_addr}
	dram_addr2 = text_addr if (text_size == 0 and executable) else 0

	tmp_sec_map = {}
	try:
		tmp_elf = ELFFile(open(f'{args.file}.tmp', 'rb'))
		tmp_sec_map = {s.name: s for s in tmp_elf.iter_sections() if s.header['sh_flags'] & SH_FLAGS.SHF_ALLOC}
	except Exception:
		pass

	for section in executable:
		s_obj = tmp_sec_map.get(section.name, section)
		dram_addr2 = do_align_up(dram_addr2, s_obj.header['sh_addralign'])
		addr_map[section.name] = dram_addr2
		dram_addr2 += s_obj.header['sh_size']

	for section in readonly_dram:
		s_obj = tmp_sec_map.get(section.name, section)
		dram_addr2 = do_align_up(dram_addr2, s_obj.header['sh_addralign'])
		addr_map[section.name] = dram_addr2
		dram_addr2 += s_obj.header['sh_size']

	if text_size == 0 and executable:
		ro_start = do_align_up(dram_addr2, 0x1000)
	else:
		ro_start = do_align_up(text_addr + text_size, 0x1000)
	for section in readonly:
		s_obj = tmp_sec_map.get(section.name, section)
		ro_start = do_align_up(ro_start, s_obj.header['sh_addralign'])
		addr_map[section.name] = ro_start
		ro_start += s_obj.header['sh_size']

	wr_start = do_align_up(ro_start, 0x1000)
	for section in writable:
		s_obj = tmp_sec_map.get(section.name, section)
		wr_start = do_align_up(wr_start, s_obj.header['sh_addralign'])
		addr_map[section.name] = wr_start
		wr_start += s_obj.header['sh_size']

	if addr_map:
		out_path = f'{args.file}.tmp'
		with open(out_path, 'r+b') as f:
			raw = bytearray(f.read())
			e_ident = raw[:16]
			little_endian = e_ident[5] == 1
			bits32 = e_ident[4] == 1
			endian = '<' if little_endian else '>'

			if bits32:
				e_shoff     = struct.unpack_from(f'{endian}I', raw, 0x20)[0]
				e_shentsize = struct.unpack_from(f'{endian}H', raw, 0x2e)[0]
				e_shnum     = struct.unpack_from(f'{endian}H', raw, 0x30)[0]
				e_shstrndx  = struct.unpack_from(f'{endian}H', raw, 0x32)[0]
				sh_addr_offset = 0x0C
				sh_name_offset = 0x00
			else:
				e_shoff     = struct.unpack_from(f'{endian}Q', raw, 0x28)[0]
				e_shentsize = struct.unpack_from(f'{endian}H', raw, 0x3a)[0]
				e_shnum     = struct.unpack_from(f'{endian}H', raw, 0x3c)[0]
				e_shstrndx  = struct.unpack_from(f'{endian}H', raw, 0x3e)[0]
				sh_addr_offset = 0x10
				sh_name_offset = 0x00

			shstr_shdr_off = e_shoff + e_shstrndx * e_shentsize
			if bits32:
				shstr_off  = struct.unpack_from(f'{endian}I', raw, shstr_shdr_off + 0x10)[0]
			else:
				shstr_off  = struct.unpack_from(f'{endian}Q', raw, shstr_shdr_off + 0x18)[0]

			patched = 0
			for i in range(e_shnum):
				shdr_off = e_shoff + i * e_shentsize
				sh_name_idx = struct.unpack_from(f'{endian}I', raw, shdr_off + sh_name_offset)[0]
				name_end = raw.index(0, shstr_off + sh_name_idx)
				s_name = raw[shstr_off + sh_name_idx:name_end].decode('utf-8', errors='replace')

				if s_name in addr_map:
					new_addr = addr_map[s_name]
					if bits32:
						new_addr_u32 = new_addr & 0xFFFFFFFF
						struct.pack_into(f'{endian}I', raw, shdr_off + sh_addr_offset, new_addr_u32)
					else:
						struct.pack_into(f'{endian}Q', raw, shdr_off + sh_addr_offset, new_addr)
					patched += 1

			sect_idx_to_addr = {}
			for i2 in range(e_shnum):
				shdr_off2 = e_shoff + i2 * e_shentsize
				sh_name_idx2 = struct.unpack_from(f'{endian}I', raw, shdr_off2 + sh_name_offset)[0]
				name_end2 = raw.index(0, shstr_off + sh_name_idx2)
				s_name2 = raw[shstr_off + sh_name_idx2:name_end2].decode('utf-8', errors='replace')
				if s_name2 in addr_map:
					sect_idx_to_addr[i2] = addr_map[s_name2]

			sym_patched = 0
			for i in range(e_shnum):
				shdr_off = e_shoff + i * e_shentsize
				sh_type_val = struct.unpack_from(f'{endian}I', raw, shdr_off + 0x04)[0]
				if sh_type_val != 2:
					continue
				sym_sh_offset = struct.unpack_from(f'{endian}I', raw, shdr_off + 0x10)[0]
				sym_sh_size   = struct.unpack_from(f'{endian}I', raw, shdr_off + 0x14)[0]
				if bits32:
					sym_entsize = struct.unpack_from(f'{endian}I', raw, shdr_off + 0x24)[0]
				else:
					sym_entsize = struct.unpack_from(f'{endian}Q', raw, shdr_off + 0x38)[0]
				if sym_entsize == 0:
					continue
				for j in range(sym_sh_size // sym_entsize):
					sym_off = sym_sh_offset + j * sym_entsize
					if bits32:
						st_info  = raw[sym_off + 0x0C]
						st_shndx = struct.unpack_from(f'{endian}H', raw, sym_off + 0x0E)[0]
						st_value_off = sym_off + 0x04
					else:
						st_info  = raw[sym_off + 0x04]
						st_shndx = struct.unpack_from(f'{endian}H', raw, sym_off + 0x06)[0]
						st_value_off = sym_off + 0x08
					sym_type = st_info & 0xF
					if sym_type == 3 and st_shndx in sect_idx_to_addr:
						new_val = sect_idx_to_addr[st_shndx] & 0xFFFFFFFF
						if bits32:
							struct.pack_into(f'{endian}I', raw, st_value_off, new_val)
						else:
							struct.pack_into(f'{endian}Q', raw, st_value_off, sect_idx_to_addr[st_shndx])
						sym_patched += 1

			f.seek(0)
			f.write(raw)
			print(f'llext_link_helper: patched sh_addr for {patched} sections and st_value for {sym_patched} STT_SECTION symbols')

	copy_command = [args.copy]

	if first_dram_text:
		copy_command.extend(['--set-section-alignment', f'{first_dram_text}=4096'])
	if first_dram_rodata:
		copy_command.extend(['--set-section-alignment', f'{first_dram_rodata}=4096'])

	copy_command.extend([f'{args.file}.tmp', f'{args.output}'])
	subprocess.run(copy_command)

	if f_size_file:
		# Calculate new size based on the output file
		start = 0xffffffff
		min_start = 0x08000000
		end = 0
		with open(f'{args.output}', 'rb') as f_elf:
			out_elf = ELFFile(f_elf)
			for section in out_elf.iter_sections():
				s_flags = section.header['sh_flags']
				if not s_flags & SH_FLAGS.SHF_ALLOC:
					continue
				if section.header['sh_addr'] < min_start:
					continue
				if section.header['sh_addr'] < start:
					start = section.header['sh_addr']
				if section.header['sh_addr'] + section.header['sh_size'] > end:
					end = section.header['sh_addr'] + section.header['sh_size']
		if start < 0xffffffff:
			elf_size = end - start
		else:
			elf_size = 0
		size += elf_size + 0xfff
		size &= ~0xfff
		f_size_file.seek(0)
		f_size_file.truncate()
		f_size_file.write(f'0x{size:x}\n')
		f_size_file.flush()
		fcntl.flock(f_size_file, fcntl.LOCK_UN)
		f_size_file.close()

if __name__ == "__main__":
	main()
