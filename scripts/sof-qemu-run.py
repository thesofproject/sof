#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
sof-qemu-run.py - Automated QEMU test runner and crash analyzer

This script runs `west -v build -t run` for Zephyr, monitors the output, and
waits for 2 seconds after the last log event. If a Zephyr/QEMU crash occurs,
it decodes it using `sof-crash-decode.py`. If no crash occurred, it enters
the QEMU monitor (Ctrl-A c), issues `info registers`, and passes the output
to the crash decoder.
"""

import sys
import pexpect
import subprocess
import argparse
import os
import time

def check_external_logs_active(rcmd):
    files_to_check = []
    if "-D" in rcmd:
        try:
            d_idx = rcmd.index("-D")
            files_to_check.append(rcmd[d_idx + 1])
        except (ValueError, IndexError):
            pass
    if "QEMU_ACE_MTRACE_FILE" in os.environ:
        files_to_check.append(os.environ["QEMU_ACE_MTRACE_FILE"])
        
    for fp in files_to_check:
        if os.path.isfile(fp):
            if time.time() - os.path.getmtime(fp) < 2.0:
                return True
    return False

import re

# ANSI Color Codes
COLOR_RED = "\x1b[31;1m"
COLOR_YELLOW = "\x1b[33;1m"
COLOR_RESET = "\x1b[0m"

def colorize_line(line):
    """Colorize significant Zephyr log items."""
    if "<err>" in line or "[ERR]" in line or "Fatal fault" in line or "Oops" in line:
        return COLOR_RED + line + COLOR_RESET
    elif "<wrn>" in line or "[WRN]" in line:
        return COLOR_YELLOW + line + COLOR_RESET
    return line

def check_for_crash(output):
    """
    Check if the collected output indicates a crash.
    Look for known Zephyr/Xtensa oops or exceptions, or QEMU crash register dumps.
    """
    crash_keywords = [
        "Fatal fault",
        "Oops",
        "ZEPHYR FATAL ERROR",
        "Exception",
        "PC=",  # QEMU PC output format
        "EXCCAUSE=",
        "Backtrace:"
    ]
    for keyword in crash_keywords:
        if keyword in output:
            return True
    return False

def run_sof_crash_decode(build_dir, dump_content):
    """
    Invokes sof-crash-decode.py and feeds it the crash dump via stdin.
    """
    # Find sof-crash-decode.py in the same directory as this script, or fallback to relative path
    decoder_script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sof-crash-decode.py")
    if not os.path.isfile(decoder_script):
        decoder_script = "sof-crash-decode.py"

    print("\n====================================")
    print("Running sof-crash-decode.py Analysis")
    print("====================================\n")

    cmd = [sys.executable, decoder_script, "--build-dir", build_dir]

    try:
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
        proc.communicate(input=dump_content.encode('utf-8'))
    except Exception as e:
        print(f"Failed to run sof-crash-decode.py: {e}")

def main():
    parser = argparse.ArgumentParser(description="Run QEMU via west and automatically decode crashes.")
    parser.add_argument("--build-dir", default="build", help="Path to the build directory containing zephyr.elf, linker.cmd, etc. Defaults to 'build'.")
    parser.add_argument("--log-file", default="qemu-run.log", help="Path to save the QEMU output log. Defaults to 'qemu-run.log'.")
    parser.add_argument("--valgrind", action="store_true", help="Run the executable under Valgrind (only valid for native_sim).")
    parser.add_argument("--interactive", action="store_true", help="Drop into the interactive QEMU monitor after execution completes instead of quitting natively.")
    parser.add_argument("--qemu-d", default="in_asm,nochain,int", help="Options to pass to QEMU's -d flag. Defaults to 'in_asm,nochain,int'.")
    parser.add_argument("--ztest", action="store_true", help="Automatically compile the firmware image with ztest_overlay.conf prior to booting.")
    parser.add_argument("--rebuild", action="store_true", help="Rebuild the firmware before running; otherwise, assumes firmware is already built.")
    args = parser.parse_args()

    # Make absolute path just in case
    # The shell script cd's into `args.build_dir` before executing us, so `args.build_dir` might be relative to the shell script's pwd.
    # We resolve it relative to the python script's original invocation cwd.
    build_dir = os.path.abspath(args.build_dir)

    print(f"Starting QEMU test runner. Monitoring for crashes (Build Dir: {args.build_dir})...")

    # We will use pexpect to spawn the west command to get PTY features
    import shutil
    west_path = shutil.which("west")
    if not west_path:
        print("[sof-qemu-run] Error: 'west' command not found in PATH.")
        print("Please ensure you have sourced the Zephyr environment (e.g., source zephyr-env.sh).")
        sys.exit(1)

    # Detect the board configuration from CMakeCache.txt
    is_native_sim = False

    cmake_cache = os.path.join(build_dir, "CMakeCache.txt")

    if os.path.isfile(cmake_cache):
        with open(cmake_cache, "r") as f:
            for line in f:
                if line.startswith("CACHED_BOARD:STRING=") or line.startswith("BOARD:STRING="):
                    board = line.split("=", 1)[1].strip()
                    if "native_sim" in board:
                        is_native_sim = True
                        break

    if args.ztest:
        print("\n\033[32;1m[sof-qemu-run] ZTEST ENABLED: Mathematics and firmware testing configured.\033[0m")
        if args.rebuild:
            print("\033[32;1m[sof-qemu-run] Recompiling Zephyr firmware with testing overlays natively...\033[0m")
            # Inject standard rimage build directory directly into PATH so `west sign` mathematically authenticates Zephyr.elf into Zephyr.ri directly seamlessly.
            sof_workspace = os.environ.get("SOF_WORKSPACE", os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
            optional_rimage_path = os.path.join(sof_workspace, "build-rimage")
            if os.path.isdir(optional_rimage_path) and optional_rimage_path not in os.environ.get("PATH", ""):
                os.environ["PATH"] = f"{optional_rimage_path}{os.pathsep}{os.environ.get('PATH', '')}"
                print(f"[sof-qemu-run] Injected Rimage Path: {optional_rimage_path}")

            # Ensure pristine builds trigger CMake re-configuration loading the new overlay arguments cleanly:
            subprocess.run([west_path, "build", "-d", build_dir, "-p", "auto", "--", "-DOVERLAY_CONFIG=ztest_overlay.conf"], check=True)
            print("\033[32;1m[sof-qemu-run] Compilation Successful.\033[0m\n")
        else:
            print("\033[32;1m[sof-qemu-run] Skipping compilation/rebuild, using previously generated binaries.\033[0m\n")

    # Determine execution command
    runs = []
    
    if "ptl" in build_dir.lower() or "wcl" in build_dir.lower():
        qemu_bin = os.environ.get("QEMU_BIN_PATH", os.path.join(os.environ.get("SOF_WORKSPACE", os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))), "qemu", "build"))
        qemu_exe = os.path.join(qemu_bin, "qemu-system-xtensa")
        print(f"[sof-qemu-run] Bypassing west run explicitly for ACE30 target. Using QEMU: {qemu_exe}")
        os.environ["QEMU_ACE_MTRACE_FILE"] = "/tmp/ace-mtrace.log"
        
        # Enable dynamic recursive ZTest execution if isolated zephyr.ri/elf doesn't exist
        fw_images = []
        default_ri = os.path.join(build_dir, "zephyr", "zephyr.ri")
        default_elf = os.path.join(build_dir, "zephyr", "zephyr.elf")
        if os.path.isfile(default_ri):
            fw_images.append(default_ri)
        elif os.path.isfile(default_elf):
            fw_images.append(default_elf)
        else:
            for root, dirs, files in os.walk(build_dir):
                if "zephyr.ri" in files:
                    fw_images.append(os.path.join(root, "zephyr.ri"))
                elif "zephyr.elf" in files:
                    fw_images.append(os.path.join(root, "zephyr.elf"))
                    
        if not fw_images:
            print(f"[sof-qemu-run] Error: No Zephyr firmware generated natively (missing zephyr.ri or zephyr.elf) within {build_dir}")
            sys.exit(1)

        print("\n[sof-qemu-run] \033[36;1m💡 Quick Tip: Monitor logs in real-time across another terminal window:\033[0m")
        print("    tail -f /tmp/qemu-exec*.log")
        print("    tail -f /tmp/ace-mtrace.log\n")

        for fw in fw_images:
            run_cmd = [
                qemu_exe,
                "-machine", "adsp_ace30",
                "-kernel", fw,
                "-nographic",
                "-icount", "shift=5,align=off"
            ]
            if args.qemu_d:
                run_cmd.extend(["-d", args.qemu_d])
                
            log_key = os.path.basename(os.path.dirname(os.path.dirname(fw))) if len(fw_images) > 1 else "default"
            run_cmd.extend(["-D", f"/tmp/qemu-exec-{log_key}.log"])
            runs.append((fw, run_cmd))
            
    else:
        if not args.rebuild and is_native_sim and not args.valgrind:
            run_cmd = [os.path.join(build_dir, "zephyr", "zephyr.exe")]
        else:
            run_cmd = [west_path, "-v", "build", "-d", build_dir, "-t", "run"]
        if args.valgrind:
            if not is_native_sim:
                print("[sof-qemu-run] Error: --valgrind is only supported for the native_sim board.")
                sys.exit(1)

            if args.rebuild:
                print("[sof-qemu-run] Rebuilding before valgrind...")
                subprocess.run([west_path, "build", "-d", build_dir], check=True)

            valgrind_path = shutil.which("valgrind")
            if not valgrind_path:
                sys.exit(1)

            exe_path = os.path.join(build_dir, "zephyr", "zephyr.exe")
            run_cmd = [valgrind_path, exe_path]
            
        runs.append((build_dir, run_cmd))

    # Master Batch Execution Loop traversing standard runner pipelines identically
    for idx, (fw_target, rcmd) in enumerate(runs):
        if len(runs) > 1:
            print(f"\n\033[32;1m========================================================================\033[0m")
            print(f"\033[32;1m[sof-qemu-run] BATCH EXECUTE [{idx+1}/{len(runs)}]: {fw_target}\033[0m")
            print(f"\033[32;1m========================================================================\033[0m\n")

        if args.interactive:
            print("\n[sof-qemu-run] Starting QEMU directly in interactive mode. Automatic crash analysis is disabled.")
            subprocess.run(rcmd)
            continue

        child = pexpect.spawn(rcmd[0], rcmd[1:], encoding='utf-8')
        full_output = ""

        # Suffix distinct files appropriately if chained
        active_log = args.log_file + (f".{idx}" if len(runs) > 1 else "")
        
        with open(active_log, "w") as log_file:
            try:
                while True:
                    try:
                        index = child.expect([r'\r\n', pexpect.TIMEOUT, pexpect.EOF], timeout=2)
                        if index == 0:
                            line = child.before + '\n'
                            clean_line = re.sub(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])', '', line)
                            log_file.write(clean_line)
                            log_file.flush()

                            colored_line = colorize_line(line)
                            sys.stdout.write(colored_line)
                            sys.stdout.flush()

                            full_output += line
                        elif index == 1: # TIMEOUT
                            if check_external_logs_active(rcmd):
                                continue
                            print("\n\n[sof-qemu-run] 2 seconds passed since last log event. Checking status...")
                            break
                        elif index == 2: # EOF
                            print("\n\n[sof-qemu-run] QEMU process terminated.")
                            break

                    except pexpect.TIMEOUT:
                        if check_external_logs_active(rcmd):
                            continue
                        print("\n\n[sof-qemu-run] 2 seconds passed since last log event. Checking status...")
                        break
                    except pexpect.EOF:
                        print("\n\n[sof-qemu-run] QEMU process terminated.")
                        break

            except KeyboardInterrupt:
                print("\n[sof-qemu-run] Interrupted by user.")

        crashed = check_for_crash(full_output)

        if crashed:
            print("\n[sof-qemu-run] Detected crash signature in standard output!")
            if child.isalive():
                child.sendline("\x01x") # Ctrl-A x to quit qemu
                child.close(force=True)

            run_sof_crash_decode(build_dir, full_output)
        else:
            if is_native_sim:
                print("\n[sof-qemu-run] No crash detected. (Skipping QEMU monitor interaction for native_sim)")
            else:
                print("\n[sof-qemu-run] No crash detected. Interacting with QEMU Monitor to grab registers...")

                if child.isalive():
                    child.send("\x01c") # Ctrl-A c
                    try:
                        child.expect(r"\(qemu\)", timeout=5)
                        child.sendline("info registers")
                        child.expect(r"\(qemu\)", timeout=5)

                        info_regs_output = child.before
                        print("\n[sof-qemu-run] Successfully extracted registers from QEMU monitor.\n")

                        run_sof_crash_decode(build_dir, info_regs_output)
                    except pexpect.TIMEOUT:
                        print("\n[sof-qemu-run] Timed out waiting for QEMU monitor. Is it running?")
                        child.close(force=True)
                    except pexpect.EOF:
                        print("\n[sof-qemu-run] QEMU terminated before we could run monitor commands.")
                else:
                    print("\n[sof-qemu-run] Process is no longer alive, cannot extract registers.")

if __name__ == "__main__":
    main()
