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
                    if "native_sim" in line.split("=", 1)[1].strip():
                        is_native_sim = True
                        break

    # Determine execution command
    if "ptl" in build_dir.lower() or "wcl" in build_dir.lower():
        qemu_bin = os.environ.get("QEMU_BIN_PATH", os.path.join(os.environ.get("SOF_WORKSPACE", os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))), "qemu", "build"))
        qemu_exe = os.path.join(qemu_bin, "qemu-system-xtensa")
        print(f"[sof-qemu-run] Bypassing west run explicitly for ACE30 target. Using QEMU: {qemu_exe}")
        os.environ["QEMU_ACE_MTRACE_FILE"] = "/tmp/ace-mtrace.log"
        fw_image = os.path.join(build_dir, "zephyr", "zephyr.ri")
        run_cmd = [qemu_exe, "-machine", "adsp_ace30", "-kernel", fw_image, "-nographic"]
    else:
        run_cmd = [west_path, "-v", "build", "-d", build_dir, "-t", "run"]

    if args.valgrind:
        if not is_native_sim:
            print("[sof-qemu-run] Error: --valgrind is only supported for the native_sim board.")
            sys.exit(1)

        print("[sof-qemu-run] Rebuilding before valgrind...")
        subprocess.run([west_path, "build", "-d", build_dir], check=True)

        valgrind_path = shutil.which("valgrind")
        if not valgrind_path:
            print("[sof-qemu-run] Error: 'valgrind' command not found in PATH.")
            sys.exit(1)

        exe_path = os.path.join(build_dir, "zephyr", "zephyr.exe")
        if not os.path.isfile(exe_path):
            print(f"[sof-qemu-run] Error: Executable not found at {exe_path}")
            sys.exit(1)

        run_cmd = [valgrind_path, exe_path]

    child = pexpect.spawn(run_cmd[0], run_cmd[1:], encoding='utf-8')

    # We will accumulate output to check for crashes
    full_output = ""

    with open(args.log_file, "w") as log_file:
        try:
            # Loop reading output until EOF or a timeout occurs
            qemu_started = False
            while True:
                try:
                    # Read character by character or line by line
                    # Pexpect's readline() doesn't consistently trigger timeout on idle
                    # We can use read_nonblocking and an explicit exceptTIMEOUT
                    index = child.expect([r'\r\n', pexpect.TIMEOUT, pexpect.EOF], timeout=2)
                    if index == 0:
                        line = child.before + '\n'
                        # Strip ANSI escape codes from output to write raw text to log file
                        clean_line = re.sub(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])', '', line)
                        log_file.write(clean_line)
                        log_file.flush()

                        colored_line = colorize_line(line)
                        sys.stdout.write(colored_line)
                        sys.stdout.flush()

                        full_output += line
                        if not qemu_started and ("Booting Zephyr OS" in line or "To exit from QEMU" in line or "qemu-system-" in line):
                            qemu_started = True
                    elif index == 1: # TIMEOUT
                        if qemu_started or check_for_crash(full_output):
                            print("\n\n[sof-qemu-run] 2 seconds passed since last log event. Checking status...")
                            break
                        else:
                            # Still building or loading, continue waiting
                            pass
                    elif index == 2: # EOF
                        print("\n\n[sof-qemu-run] QEMU process terminated.")
                        break

                except pexpect.TIMEOUT:
                    if qemu_started or check_for_crash(full_output):
                        print("\n\n[sof-qemu-run] 2 seconds passed since last log event. Checking status...")
                        break
                    else:
                        # Still building or loading, continue waiting
                        pass
                except pexpect.EOF:
                    print("\n\n[sof-qemu-run] QEMU process terminated.")
                    break

        except KeyboardInterrupt:
            print("\n[sof-qemu-run] Interrupted by user.")
            # Proceed with what we have

    crashed = check_for_crash(full_output)

    if crashed:
        print("\n[sof-qemu-run] Detected crash signature in standard output!")
        # Stop QEMU if it's still running
        if child.isalive():
            child.sendline("\x01x") # Ctrl-A x to quit qemu
            child.close(force=True)

        run_sof_crash_decode(build_dir, full_output)
    else:
        if is_native_sim:
            print("\n[sof-qemu-run] No crash detected. (Skipping QEMU monitor interaction for native_sim)")
        else:
            print("\n[sof-qemu-run] No crash detected. Interacting with QEMU Monitor to grab registers...")

            # We need to send Ctrl-A c to enter the monitor
            if child.isalive():
                child.send("\x01c") # Ctrl-A c
                try:
                    # Wait for (qemu) prompt
                    child.expect(r"\(qemu\)", timeout=5)
                    # Send "info registers"
                    child.sendline("info registers")
                    # Wait for the next prompt
                    child.expect(r"\(qemu\)", timeout=5)

                    info_regs_output = child.before
                    print("\n[sof-qemu-run] Successfully extracted registers from QEMU monitor.\n")

                    # Quit qemu safely
                    child.sendline("quit")
                    child.expect(pexpect.EOF, timeout=2)
                    child.close()

                    # Run the decoder on the intercepted register output
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
