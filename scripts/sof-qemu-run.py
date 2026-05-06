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

import time
import shutil

# ANSI Color Codes
COLOR_RED = "\x1b[31;1m"
COLOR_YELLOW = "\x1b[33;1m"
COLOR_CYAN = "\x1b[36;1m"
COLOR_RESET = "\x1b[0m"

def colorize_line(line):
    """Colorize significant Zephyr log items."""
    if "<err>" in line or "[ERR]" in line or "Fatal fault" in line or "Oops" in line:
        return COLOR_RED + line + COLOR_RESET
    elif "<wrn>" in line or "[WRN]" in line:
        return COLOR_YELLOW + line + COLOR_RESET
    elif "[sof-qemu-run]" in line:
        return COLOR_CYAN + line + COLOR_RESET
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
        "Backtrace:",
        "halting system"
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
    parser.add_argument("--timeout", type=int, default=2, help="Seconds of silence before assuming QEMU has hung or finished. Defaults to 2.")
    parser.add_argument("--valgrind", action="store_true", help="Run with valgrind (native_sim only).")
    parser.add_argument("--cores", type=int, help="Number of SMP cores for QEMU.")
    parser.add_argument("--mtrace-log", help="Path to MTrace log file for ADSP ACE30.")
    parser.add_argument("--tcp-monitor", type=int, help="Expose QEMU monitor on local TCP port.")
    parser.add_argument("--qemu-d", help="Pass -d flags to QEMU.")
    parser.add_argument("--exec-log", help="Pass -D log file to QEMU.")
    parser.add_argument("--rebuild", action="store_true", help="Rebuild before running.")
    parser.add_argument("--interactive", action="store_true", help="Run QEMU directly in interactive mode (disables crash monitor).")
    args = parser.parse_args()

    # Make absolute path just in case
    build_dir = os.path.abspath(args.build_dir)
    
    # Detection for native_sim board
    is_native_sim = False
    if os.path.isfile(os.path.join(build_dir, "zephyr", "zephyr.exe")):
        is_native_sim = True

    print(f"Starting QEMU test runner (Build Dir: {args.build_dir}, Timeout: {args.timeout}s)...")

    west_path = shutil.which("west")
    if not west_path:
        print("[sof-qemu-run] Error: 'west' command not found in PATH.")
        sys.exit(1)

    # Determine what we are actually running
    runs = []
    
    # Check for multiple firmware images in the build directory (chained boot tests)
    fw_images = []
    zephyr_elf = os.path.join(build_dir, "zephyr", "zephyr.elf")
    if os.path.isfile(zephyr_elf):
        fw_images.append(zephyr_elf)
        
    # Also check for .bin images if elf isn't there or if we have multi-image
    if not is_native_sim:
        qemu_exe = shutil.which("qemu-system-xtensa")
        if not qemu_exe:
            print("[sof-qemu-run] Error: 'qemu-system-xtensa' not found in PATH.")
            sys.exit(1)

        for fw in fw_images:
            run_cmd = [qemu_exe]
            if args.mtrace_log:
                run_cmd.extend(["-machine", f"adsp_ace30,mtrace-file={args.mtrace_log}"])
            else:
                run_cmd.extend(["-machine", "adsp_ace30"])

            run_cmd.extend([
                "-kernel", fw,
                "-display", "none",
                "-serial", "stdio",
                "-icount", "shift=5,align=off"
            ])
            if args.cores:
                run_cmd.extend(["-smp", str(args.cores)])
            if args.tcp_monitor:
                run_cmd.extend(["-monitor", f"tcp:localhost:{args.tcp_monitor},server,nowait"])
            if args.qemu_d:
                run_cmd.extend(["-d", args.qemu_d])
                
            log_key = os.path.basename(os.path.dirname(os.path.dirname(fw))) if len(fw_images) > 1 else "default"
            exec_log = args.exec_log if args.exec_log else f"/tmp/qemu-exec-{log_key}.log"
            run_cmd.extend(["-D", exec_log])
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
                print("[sof-qemu-run] Error: 'valgrind' command not found in PATH.")
                sys.exit(1)

            exe_path = os.path.join(build_dir, "zephyr", "zephyr.exe")
            run_cmd = [valgrind_path, exe_path]
            
        runs.append((build_dir, run_cmd))

    print("\n[sof-qemu-run] \033[36;1m💡 Quick Tip: Monitor logs in real-time across another terminal window:\033[0m")
    print(f"    tail -f /tmp/qemu-exec*.log")
    print(f"    tail -f {args.mtrace_log if args.mtrace_log else '/tmp/ace-mtrace.log'}")
    
    if args.tcp_monitor:
        print("\n[sof-qemu-run] \033[36;1m💡 Quick Tip: Automate Out-Of-Band IPC Triggers (Requires sof/qemu codebase mapping):\033[0m")
        print(f"    python3 scripts/sof-qemu-ipc.py --port {args.tcp_monitor} --status\n")
    else:
        print()

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
        mtrace_file = os.environ.get("QEMU_ACE_MTRACE_FILE")
        mtrace_fd = None
        last_active_time = time.time()
        
        with open(active_log, "w") as log_file:
            try:
                while True:
                    try:
                        index = child.expect([r'\r\n', pexpect.TIMEOUT, pexpect.EOF], timeout=0.5)
                        if index == 0:
                            last_active_time = time.time()
                            line = child.before + '\n'
                            clean_line = re.sub(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])', '', line)
                            log_file.write(clean_line)
                            log_file.flush()

                            colored_line = colorize_line(line)
                            sys.stdout.write(colored_line)
                            sys.stdout.flush()

                            full_output += line
                        elif index == 2: # EOF
                            print("\n\n[sof-qemu-run] QEMU process terminated.")
                            break

                    except pexpect.TIMEOUT:
                        pass
                    except pexpect.EOF:
                        print("\n\n[sof-qemu-run] QEMU process terminated.")
                        break
                        
                    if mtrace_file and os.path.isfile(mtrace_file):
                        if not mtrace_fd:
                            try:
                                mtrace_fd = open(mtrace_file, "r", encoding="utf-8", errors="ignore")
                            except Exception:
                                pass
                        
                        if mtrace_fd:
                            new_data = mtrace_fd.read()
                            if new_data:
                                last_active_time = time.time()
                                full_output += new_data
                                # Also write to log file and stdout
                                log_file.write(new_data)
                                log_file.flush()
                                sys.stdout.write(new_data)
                                sys.stdout.flush()
                                if "halting system" in new_data:
                                    print("\n\n[sof-qemu-run] Detected 'halting system' in mtrace log! Breaking...")
                                    break

                    if time.time() - last_active_time >= args.timeout:
                        print(f"\n\n[sof-qemu-run] {args.timeout} seconds passed since last log event. Checking status...")
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
                print("\n[sof-qemu-run] No crash detected.")
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
