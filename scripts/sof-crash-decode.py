#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
decode_crash.py - Zephyr Xtensa Crash Dump Decoder

Parses a Zephyr crash dump, extracts CPU registers/backtraces, and correlates them
to source code files and function names using the `objdump` output of the ELF file.

Dependencies:
    - python3
    - binutils for your target architecture (e.g. xtensa-zephyr-elf-objdump)
    - Optional: `xclip`, `xsel`, or `wl-paste` (for --clipboard support on Linux)

Usage Examples:
    # 1. Provide the ELF and read crash from stdin
    cat crash.txt | ./sof-crash-decode.py --elf zephyr.elf

    # 2. Automatically locate ELF/objdump from a Zephyr build directory, read crash from file
    ./sof-crash-decode.py --build-dir build-qemu_xtensa/ --dump crash.txt

    # 3. Read directly from the system clipboard
    ./sof-crash-decode.py --build-dir build-qemu_xtensa/ --clipboard

    # 4. Pipe a live trace to the decoder
    tail -f log.txt | ./sof-crash-decode.py --build-dir build_dir/

"""

import sys
import re
import argparse
import subprocess
import bisect
import os
import json
import shlex

XTENSA_EXCCAUSE = {
    0: "No Error (or IllegalInstruction)",
    1: "Syscall",
    2: "InstructionFetchError",
    3: "LoadStoreError",
    4: "Level1Interrupt",
    5: "Alloca",
    6: "IntegerDivideByZero",
    8: "Privileged",
    9: "LoadStoreAlignment",
    12: "InstrPIFDataError",
    13: "LoadStorePIFDataError",
    14: "InstrPIFAddrError",
    15: "LoadStorePIFAddrError",
    16: "InstTLBMiss",
    17: "InstTLBMultiHit",
    18: "InstFetchPrivilege",
    20: "InstFetchProhibited",
    24: "LoadStoreTLBMiss",
    25: "LoadStoreTLBMultiHit",
    26: "LoadStorePrivilege",
    28: "LoadStoreProhibited",
    32: "Coprocessor0Disabled",
    33: "Coprocessor1Disabled",
    34: "Coprocessor2Disabled",
    35: "Coprocessor3Disabled",
    36: "Coprocessor4Disabled",
    37: "Coprocessor5Disabled",
    38: "Coprocessor6Disabled",
    39: "Coprocessor7Disabled",
}

def parse_crash_log(content):

    registers = {}
    backtraces = []

    # Detect QEMU format
    if re.search(r'\bPC=[0-9a-fA-F]+\b', content):
        reg_pattern = re.compile(r'\b([A-Z0-9]+)=([0-9a-fA-F]+)\b')
        for match in reg_pattern.finditer(content):
            reg = match.group(1)
            val = int(match.group(2), 16)

            if reg == 'EXCVADDR':
                reg = 'VADDR'
            elif re.match(r'^A\d{2}$', reg):
                reg = f"A{int(reg[1:])}"
            elif re.match(r'^AR\d{2}$', reg):
                reg = f"AR{int(reg[2:])}"

            if re.match(r'^(PC|PR|SP|A\d+|AR\d+|EXCCAUSE|VADDR|LBEG|LEND|SAR|EPC\d+|EPS\d+|PS)$', reg):
                registers[reg] = val
    else:
        # Standard format
        # regex for registers: we want standalone pairs like PC 0x123 or A0 0x123 or EXCCAUSE 9
        reg_pattern = re.compile(r'\b([A-Z0-9]+)\s+(0x[0-9a-fA-F]+|\d+|(?:nil))\b')
        for match in reg_pattern.finditer(content):
            reg = match.group(1)
            val_str = match.group(2)
            if val_str == "(nil)":
                val = 0
            elif val_str.startswith("0x"):
                val = int(val_str, 16)
            else:
                val = int(val_str)

            # Keep only known registers or likely candidates
            if re.match(r'^(PC|PR|SP|A\d+|AR\d+|EXCCAUSE|VADDR|LBEG|LEND|SAR|EPC\d+|EPS\d+|PS)$', reg):
                registers[reg] = val

    # Backtrace parsing
    bt_idx = content.find("Backtrace:")
    if bt_idx != -1:
        bt_line = content[bt_idx:content.find('\n', bt_idx)]
        bt_pattern = re.compile(r'(0x[0-9a-fA-F]+):(0x[0-9a-fA-F]+)')
        for match in bt_pattern.finditer(bt_line):
            pc = int(match.group(1), 16)
            sp = int(match.group(2), 16)
            backtraces.append((pc, sp))

    return registers, backtraces, content

def get_objdump_output(elf_path, objdump_cmd):
    print(f"Running {objdump_cmd} -d -S -l \"{elf_path}\" ...")
    try:
        # Check if objdump exists
        result = subprocess.run([objdump_cmd, "-d", "-S", "-l", elf_path],
                                capture_output=True, text=True, check=True)
        return result.stdout
    except FileNotFoundError:
        print(f"Error: {objdump_cmd} not found. Please provide the correct objdump command using --objdump.")
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"Error running objdump: {e}")
        sys.exit(1)

def parse_linker_cmd(filepath):
    regions = []
    try:
        with open(filepath, 'r') as f:
            content = f.read()
            m_block = re.search(r'MEMORY\s*\{(.*?)\}', content, re.DOTALL)
            if m_block:
                for line in m_block.group(1).splitlines():
                    line = line.strip()
                    if not line or ':' not in line: continue
                    name, rest = line.split(':', 1)
                    name = name.strip()
                    m_org = re.search(r'org\s*=\s*(.*?),', rest)
                    m_len = re.search(r'len\s*=\s*(.*)', rest)
                    if m_org and m_len:
                        org_expr = m_org.group(1).strip()
                        len_expr = m_len.group(1).strip()
                        try:
                            org_val = eval(org_expr)
                            len_val = eval(len_expr)
                            # Ignore debug regions
                            if not (name.startswith('.debug') or name.startswith('.stab')):
                                regions.append({'name': name, 'start': org_val, 'end': org_val + len_val})
                        except Exception:
                            pass
    except Exception as e:
        print(f"Warning: Failed to parse {filepath}: {e}")
    return regions

def parse_zephyr_stat(filepath):
    sections = []
    try:
        with open(filepath, 'r') as f:
            for line in f:
                m = re.match(r'^\s*\[\s*\d+\]\s+(\S+)\s+[A-Z0-9]+\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)', line)
                if m:
                    name = m.group(1)
                    start = int(m.group(2), 16)
                    size = int(m.group(3), 16)
                    # Ignore debug sections
                    if size > 0 and not (name.startswith('.debug') or name.startswith('.stab')):
                        sections.append({'name': name, 'start': start, 'end': start + size})
    except Exception as e:
        print(f"Warning: Failed to parse {filepath}: {e}")
    return sections

def parse_zephyr_dts(filepath):
    regions = []
    try:
        with open(filepath, 'r') as f:
            lines = f.read().splitlines()

        current_node_path = []
        for line in lines:
            line = line.strip()
            # Simple node match:  node_name: some_name@addr {
            m_node = re.match(r'^(?:[a-zA-Z0-9_]+:\s*)?([a-zA-Z0-9_\-]+(?:@[0-9a-fA-Fx]+)?)\s*\{', line)
            if m_node:
                node_name = m_node.group(1)
                current_node_path.append(node_name)
                continue

            if line == "};":
                if current_node_path:
                    current_node_path.pop()
                continue

            # match reg = < ... >
            m_reg = re.match(r'^reg\s*=\s*<\s*(.*?)\s*>;', line)
            if m_reg and current_node_path:
                reg_vals = m_reg.group(1).split()
                if len(reg_vals) >= 2:
                    try:
                        addr = int(reg_vals[0], 16) if reg_vals[0].startswith('0x') else int(reg_vals[0])
                        size = int(reg_vals[1], 16) if reg_vals[1].startswith('0x') else int(reg_vals[1])
                        if size > 0:
                            node_name = current_node_path[-1]
                            regions.append({'name': node_name, 'start': addr, 'end': addr + size})
                    except ValueError:
                        pass
    except Exception as e:
        print(f"Warning: Failed to parse {filepath}: {e}")
    return regions

def build_address_map(objdump_text):
    current_func = "<unknown>"
    current_context = []
    last_was_asm = False
    address_map = {}

    func_re = re.compile(r'^([0-9a-fA-F]+)\s+<([^>]+)>:$')
    asm_re = re.compile(r'^\s*([0-9a-fA-F]+):\s+(.*)$')

    for line in objdump_text.splitlines():
        line = line.rstrip()
        if not line or line.startswith("Disassembly of section"):
            continue

        m_func = func_re.match(line)
        if m_func:
            current_func = m_func.group(2)
            current_context = []
            last_was_asm = False
            continue

        m_asm = asm_re.match(line)
        if m_asm:
            addr = int(m_asm.group(1), 16)
            address_map[addr] = {
                'func': current_func,
                'context': list(current_context),
                'asm': m_asm.group(2)
            }
            last_was_asm = True
            continue

        if last_was_asm:
            current_context = []
            last_was_asm = False
        current_context.append(line)

    return address_map

def find_closest_instruction(addr, address_map, sorted_addresses):
    if addr in address_map:
        return addr, address_map[addr]

    # Extract lower 29 bits for physical address mappings on Xtensa
    physical = addr & 0x1FFFFFFF
    if physical in address_map:
        return physical, address_map[physical]

    idx = bisect.bisect_right(sorted_addresses, physical)
    if idx > 0:
        closest = sorted_addresses[idx-1]
        # Return if within 16 bytes (typical small instruction offset)
        if physical - closest < 16:
            return closest, address_map[closest]

    return addr, None

def decode_ps_bits(val):
    intlevel = val & 0xF
    excm = (val >> 4) & 1
    um = (val >> 5) & 1
    ring = (val >> 6) & 3
    owb = (val >> 8) & 0xF
    callinc = (val >> 16) & 3
    woe = (val >> 18) & 1

    flags = []
    flags.append(f"INTLEVEL:{intlevel}")
    if excm: flags.append("EXCM")
    flags.append(f"UM:{um}")
    flags.append(f"RING:{ring}")
    flags.append(f"OWB:{owb}")
    flags.append(f"CALLINC:{callinc}")
    flags.append(f"WOE:{woe}")

    return " | ".join(flags)

def main():
    # Set default color explicitly at start
    print("\x1b[0m", end='', flush=True)

    parser = argparse.ArgumentParser(description="Decode Xtensa/Zephyr crash dump using objdump.")
    parser.add_argument("--elf", required=False, help="Path to the ELF file. Overridden if --build-dir is provided.")
    parser.add_argument("--build-dir", required=False, help="Path to the Zephyr build directory.")
    parser.add_argument("--dump", default="-", help="Path to the crash dump file. Default is '-' for stdin.")
    parser.add_argument("--clipboard", action="store_true", help="Read crash dump from the clipboard instead of file/stdin.")
    parser.add_argument("--objdump", default="xtensa-sof-zephyr-elf-objdump", help="Objdump command to use. e.g. xtensa-zephyr-elf-objdump")
    args = parser.parse_args()

    objdump_cmd = args.objdump

    if args.build_dir:
        # Resolve zephyr.elf
        default_elf = os.path.join(args.build_dir, "zephyr", "zephyr.elf")
        if os.path.isfile(default_elf):
            args.elf = default_elf

        # Try to find objdump from compile_commands
        cc_path = os.path.join(args.build_dir, "compile_commands.json")
        if not os.path.isfile(cc_path):
            cc_path = os.path.join(args.build_dir, "compile_commands.txt")

        if os.path.isfile(cc_path):
            try:
                with open(cc_path, 'r') as f:
                    cc_data = json.load(f)
                    if cc_data and len(cc_data) > 0 and 'command' in cc_data[0]:
                        # The command might contain arguments, we extract the first token
                        cmd_tokens = shlex.split(cc_data[0]['command'])
                        compiler_path = cmd_tokens[0]
                        # Replace gcc, g++, clang, etc. with objdump
                        if compiler_path.endswith('gcc') or compiler_path.endswith('g++') or compiler_path.endswith('cc'):
                            new_cmd = re.sub(r'(g?cc|g\+\+)$', 'objdump', compiler_path)
                            if os.path.isfile(new_cmd) and os.access(new_cmd, os.X_OK):
                                objdump_cmd = new_cmd
            except Exception as e:
                print(f"Warning: Failed to parse {cc_path} to deduce objdump: {e}")

    linker_regions = []
    stat_sections = []
    dts_regions = []
    if args.build_dir:
        linker_cmd_path = os.path.join(args.build_dir, "zephyr", "linker.cmd")
        zephyr_stat_path = os.path.join(args.build_dir, "zephyr", "zephyr.stat")
        zephyr_dts_path = os.path.join(args.build_dir, "zephyr", "zephyr.dts")

        if os.path.isfile(linker_cmd_path):
            linker_regions = parse_linker_cmd(linker_cmd_path)
        if os.path.isfile(zephyr_stat_path):
            stat_sections = parse_zephyr_stat(zephyr_stat_path)
        if os.path.isfile(zephyr_dts_path):
            dts_regions = parse_zephyr_dts(zephyr_dts_path)

    if not args.elf:
        print("Error: --elf or --build-dir must be provided.")
        sys.exit(1)

    if not os.path.isfile(args.elf):
        print(f"Cannot find ELF file: {args.elf}")
        sys.exit(1)

    if args.clipboard:
        try:
            dump_content = subprocess.check_output(['xclip', '-o', '-selection', 'clipboard'], text=True)
        except (subprocess.CalledProcessError, FileNotFoundError):
            try:
                dump_content = subprocess.check_output(['xsel', '--clipboard', '--output'], text=True)
            except (subprocess.CalledProcessError, FileNotFoundError):
                try:
                    dump_content = subprocess.check_output(['wl-paste'], text=True)
                except (subprocess.CalledProcessError, FileNotFoundError):
                    print("Error: Could not read from clipboard. Make sure xclip, xsel, or wl-paste is installed.")
                    sys.exit(1)
    elif args.dump == "-":
        dump_content = sys.stdin.read()
    else:
        if not os.path.isfile(args.dump):
            print(f"Cannot find Dump file: {args.dump}")
            sys.exit(1)
        with open(args.dump, 'r') as f:
            dump_content = f.read()

    registers, backtraces, raw_content = parse_crash_log(dump_content)

    print(f"Found {len(registers)} registers and {len(backtraces)} backtrace elements in crash dump.")

    print("Parsing objdump (this may take a few seconds)...")

    # Actually, many systems might use standard xtensa-zephyr-elf-objdump
    # We can try to dynamically choose if the user just provided a prefix or left default

    # Try running the objdump to ensure it exists
    import shutil
    if not os.path.isfile(objdump_cmd) and not shutil.which(objdump_cmd) and "zephyr" in objdump_cmd:
        # try without sof if user has a different one
        alt_cmds = [
            "xtensa-zephyr-elf-objdump",
            "xtensa-intel-elf-objdump",
            "zephyr-sdk/xtensa-zephyr-elf-objdump",
            "objdump"
        ]
        for alt in alt_cmds:
            if shutil.which(alt):
                print(f"Warning: {objdump_cmd} not found, falling back to {alt}")
                objdump_cmd = alt
                break

    objdump_text = get_objdump_output(args.elf, objdump_cmd)
    address_map = build_address_map(objdump_text)
    sorted_addresses = sorted(address_map.keys())

    print("\n--- Summary ---")
    print("PS Register Legend:")
    print("  INTLEVEL : Interrupt Level          EXCM : Exception Mode")
    print("  UM       : User Mode (1=User)       RING : Privilege Ring")
    print("  OWB      : Old Window Base          WOE  : Window Overflow Enable")
    print("  CALLINC  : Call Increment")
    print()

    def print_decoded(name, val):
        if val == 0:
            print(f"{name:5}: 0x00000000 -> (nil)")
            return

        addr, info = find_closest_instruction(val, address_map, sorted_addresses)
        if info:
            print(f"{name:5}: 0x{val:08x} -> <{info['func']}>")
            for ctx in info['context']:
                ctx_strip = ctx.strip()
                if re.match(r'^[^ \t:]+:\d+', ctx_strip):
                    print(f"      \x1b[35m{ctx_strip}\x1b[0m")
                else:
                    print(f"      \x1b[93m{ctx_strip}\x1b[0m")
            print(f"      \x1b[93m{addr:08x}: {info['asm']}\x1b[0m")
            print()
        else:
            dts_str = ""
            for d in dts_regions:
                if d['start'] <= val < d['end']:
                    dts_str = f", DT: {d['name']}"
                    break
            region_str = ""
            for r in linker_regions:
                if r['start'] <= val < r['end']:
                    region_str = f", Region: {r['name']}"
                    break
            sec_str = ""
            for s in stat_sections:
                if s['start'] <= val < s['end']:
                    sec_str = f", Section: {s['name']}"
                    break

            if dts_str or region_str or sec_str:
                print(f"{name:5}: 0x{val:08x} -> <unknown{dts_str}{region_str}{sec_str}>")
            else:
                print(f"{name:5}: 0x{val:08x} -> <unknown/no code section>")

    # Prioritize specific registers
    for reg in ['PC', 'EXCCAUSE', 'VADDR', 'SP', 'PS']:
        if reg in registers:
            if reg == 'EXCCAUSE':
                cause_code = registers[reg]
                cause_str = XTENSA_EXCCAUSE.get(cause_code, "Unknown/Unassigned")
                print(f"EXCCAUSE: {cause_code} ({cause_str})")
            elif reg == 'VADDR':
                print(f"{reg:5}: 0x{registers[reg]:08x}")
            elif reg == 'PS':
                print(f"{reg:5}: 0x{registers[reg]:08x} -> [{decode_ps_bits(registers[reg])}]\n")
            else:
                print_decoded(reg, registers[reg])

    for i in range(1, 8):
        reg = f"EPC{i}"
        if reg in registers:
            print_decoded(reg, registers[reg])

    print()
    for i in range(2, 8):
        reg = f"EPS{i}"
        if reg in registers:
            print(f"{reg:5}: 0x{registers[reg]:08x} -> [{decode_ps_bits(registers[reg])}]")

    print("\n--- Physical Windowed Registers (A) ---")
    for i in range(16):
        reg = f"A{i}"
        if reg in registers:
            print_decoded(reg, registers[reg])

    print("\n--- Saved Stack Registers (AR) ---")
    for i in range(64):
        reg = f"AR{i}"
        if reg in registers:
            print_decoded(reg, registers[reg])

    print("\n--- Backtrace Decode ---")
    # Backtraces:
    for i, (pc, sp) in enumerate(backtraces):
        print(f"Frame {i}: SP = 0x{sp:08x}")
        print_decoded("PC", pc)

if __name__ == '__main__':
    main()
