#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
sof-qemu-func-decode.py - Zephyr Xtensa Function Trace Decoder

Parses a QEMU -d func log stream, looks up the PC addresses in the ELF objdump,
and dynamically appends the function names to the FUNC ENTRY / FUNC RET output.

Dependencies:
    - python3
    - binutils for your target architecture (e.g. xtensa-zephyr-elf-objdump)
"""

import sys
import re
import argparse
import subprocess
import bisect
import os
import json
import shlex
import shutil

def get_objdump_output(elf_path, objdump_cmd):
    try:
        result = subprocess.run([objdump_cmd, "-d", "-S", "-l", elf_path],
                                capture_output=True, text=True, check=True)
        return result.stdout
    except FileNotFoundError:
        print(f"Error: {objdump_cmd} not found.", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"Error running objdump: {e}", file=sys.stderr)
        sys.exit(1)

def build_address_map(objdump_text):
    current_func = "<unknown>"
    address_map = {}
    func_re = re.compile(r'^([0-9a-fA-F]+)\s+<([^>]+)>:$')
    asm_re = re.compile(r'^\s*([0-9a-fA-F]+):\s+.*$')

    for line in objdump_text.splitlines():
        line = line.rstrip()
        if not line or line.startswith("Disassembly of section"):
            continue

        m_func = func_re.match(line)
        if m_func:
            current_func = m_func.group(2)
            continue
            
        m_asm = asm_re.match(line)
        if m_asm:
            addr = int(m_asm.group(1), 16)
            address_map[addr] = current_func

    return address_map

def find_closest_function(addr, address_map, sorted_addresses):
    if addr in address_map:
        return address_map[addr]

    # Extract lower 29 bits for physical address mappings on Xtensa
    physical = addr & 0x1FFFFFFF
    if physical in address_map:
        return address_map[physical]

    idx = bisect.bisect_right(sorted_addresses, physical)
    if idx > 0:
        closest = sorted_addresses[idx-1]
        if physical - closest < 4096:
            return address_map[closest]

    return "<unknown>"

def main():
    parser = argparse.ArgumentParser(description="Decode Xtensa/Zephyr QEMU -d func logs.")
    parser.add_argument("--elf", required=False, help="Path to the ELF file. Overridden if --build-dir is provided.")
    parser.add_argument("--build-dir", required=False, help="Path to the Zephyr build directory.")
    parser.add_argument("--log", default="-", help="Path to the qemu log. Default is '-' for stdin.")
    parser.add_argument("--objdump", default="xtensa-zephyr-elf-objdump", help="Objdump command to use. e.g. xtensa-zephyr-elf-objdump")
    args = parser.parse_args()

    objdump_cmd = args.objdump

    if args.build_dir:
        default_elf = os.path.join(args.build_dir, "zephyr", "zephyr.elf")
        if os.path.isfile(default_elf):
            args.elf = default_elf

        cc_path = os.path.join(args.build_dir, "compile_commands.json")
        if not os.path.isfile(cc_path):
            cc_path = os.path.join(args.build_dir, "compile_commands.txt")

        if os.path.isfile(cc_path):
            try:
                with open(cc_path, 'r') as f:
                    cc_data = json.load(f)
                    if cc_data and len(cc_data) > 0 and 'command' in cc_data[0]:
                        cmd_tokens = shlex.split(cc_data[0]['command'])
                        compiler_path = cmd_tokens[0]
                        if compiler_path.endswith('gcc') or compiler_path.endswith('g++') or compiler_path.endswith('cc'):
                            new_cmd = re.sub(r'(g?cc|g\+\+)$', 'objdump', compiler_path)
                            if os.path.isfile(new_cmd) and os.access(new_cmd, os.X_OK):
                                objdump_cmd = new_cmd
            except Exception:
                pass

    if not args.elf:
        print("Error: --elf or --build-dir must be provided.", file=sys.stderr)
        sys.exit(1)

    if not os.path.isfile(args.elf):
        print(f"Cannot find ELF file: {args.elf}", file=sys.stderr)
        sys.exit(1)
        
    if not os.path.isfile(objdump_cmd) and not shutil.which(objdump_cmd):
        alt_cmds = [
            "xtensa-sof-zephyr-elf-objdump",
            "xtensa-intel-elf-objdump",
            "zephyr-sdk/xtensa-zephyr-elf-objdump",
            "objdump"
        ]
        for alt in alt_cmds:
            if shutil.which(alt):
                objdump_cmd = alt
                break

    print(f"[sof-qemu-func-decode] Parsing objdump from {args.elf}...", file=sys.stderr)
    objdump_text = get_objdump_output(args.elf, objdump_cmd)
    address_map = build_address_map(objdump_text)
    sorted_addresses = sorted(address_map.keys())
    print("[sof-qemu-func-decode] Done! Waiting for functional traces...\n", file=sys.stderr)

    if args.log == "-":
        infile = sys.stdin
    else:
        if not os.path.isfile(args.log):
            print(f"Cannot find log file: {args.log}", file=sys.stderr)
            sys.exit(1)
        infile = open(args.log, 'r')

    trace_re = re.compile(r'^(.*FUNC (?:ENTRY|RET):\s*pc=(0x[0-9a-fA-F]+))(.*)$')
    int_re = re.compile(r'^(.*xtensa_cpu_do_interrupt\(\d+\)\s*pc\s*=\s*)([0-9a-fA-F]+)(.*)$')

    indent = 0
    try:
        for line in infile:
            m = trace_re.match(line)
            if m:
                prefix = m.group(1)
                pc = int(m.group(2), 16)
                suffix = m.group(3)
                
                func_name = find_closest_function(pc, address_map, sorted_addresses)
                
                # We strip the newline from suffix, print our output, then newline
                suffix = suffix.rstrip('\n')
                
                if "FUNC RET" in prefix:
                    indent = max(0, indent - 1)
                    
                print(f"{prefix}{suffix} -> {' ' * indent}<{func_name}>")
                
                if "FUNC ENTRY" in prefix:
                    indent += 1
            else:
                m_int = int_re.match(line)
                if m_int:
                    prefix = m_int.group(1)
                    pc = int(m_int.group(2), 16)
                    suffix = m_int.group(3)
                    func_name = find_closest_function(pc, address_map, sorted_addresses)
                    suffix = suffix.rstrip('\n')
                    print(f"{prefix}{m_int.group(2)}{suffix} -> {' ' * indent}<{func_name}>")
                else:
                    print(line, end="")
    except KeyboardInterrupt:
        pass
    except BrokenPipeError:
        devnull = os.open(os.devnull, os.O_WRONLY)
        os.dup2(devnull, sys.stdout.fileno())
        sys.exit(0)
    finally:
        if args.log != "-":
            infile.close()

if __name__ == '__main__':
    main()
