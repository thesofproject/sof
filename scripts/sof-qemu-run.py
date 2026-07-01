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
    parser.add_argument("--cores", type=int, default=None, help="Number of SMP cores to emulate in QEMU.")
    parser.add_argument("--mtrace-log", help="Path to MTrace log file for ADSP ACE30.")
    parser.add_argument("--tcp-monitor", type=int, nargs="?", const=1025, default=None, help="Start the QEMU TCP monitor socket. Optionally specify the port (default: 1025).")
    parser.add_argument("--qemu-d", help="Pass -d flags to QEMU.")
    parser.add_argument("--exec-log", help="Pass -D log file to QEMU.")
    parser.add_argument("--rebuild", action="store_true", help="Rebuild before running.")
    parser.add_argument("--ztest", action="store_true", help="Build and run with ZTest overlay.")
    parser.add_argument("--test-fw-standard", action="store_true", help="Build and run standard FW with ZTest enabled.")
    parser.add_argument("--interactive", action="store_true", help="Run QEMU directly in interactive mode (disables crash monitor).")
    args = parser.parse_args()

    # Clean up old log files before starting
    for log_path in [args.mtrace_log, args.exec_log]:
        if log_path and os.path.exists(log_path):
            try:
                os.remove(log_path)
                print(f"[sof-qemu-run] Cleaned up old log: {log_path}")
            except Exception as e:
                print(f"[sof-qemu-run] Warning: Could not delete {log_path}: {e}")
    
    extra_qemu_flags = []
    if args.cores:
        extra_qemu_flags.append(f"-smp {args.cores}")
        
    if args.tcp_monitor:
        extra_qemu_flags.append(f"-monitor tcp:localhost:{args.tcp_monitor},server,nowait")

    if args.mtrace_log:
        # For ADSP boards, mtrace-file is a machine parameter. 
        # We append it to a -machine flag. West will append this to its own flags.
        extra_qemu_flags.append(f"-machine adsp_ace30,mtrace-file={args.mtrace_log}")
        os.environ["QEMU_ACE_MTRACE_FILE"] = args.mtrace_log
        print(f"[sof-qemu-run] Setting QEMU_ACE_MTRACE_FILE: {args.mtrace_log}")

    if args.exec_log:
        extra_qemu_flags.append(f"-D {args.exec_log}")

    if args.qemu_d:
        extra_qemu_flags.append(f"-d {args.qemu_d}")

    if extra_qemu_flags:
        existing_flags = os.environ.get("QEMU_EXTRA_FLAGS", "")
        os.environ["QEMU_EXTRA_FLAGS"] = f"{existing_flags} {' '.join(extra_qemu_flags)}".strip()
        print(f"[sof-qemu-run] QEMU_EXTRA_FLAGS: {os.environ['QEMU_EXTRA_FLAGS']}")

    # Make absolute path just in case
    build_dir = os.path.abspath(args.build_dir)
    
    print(f"Starting QEMU test runner (Build Dir: {args.build_dir})...")

    west_path = shutil.which("west")

    # Detect the board configuration from CMakeCache.txt
    is_native_sim = False
    board = "unknown"
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
    elif args.test_fw_standard:
        print("\n\033[32;1m[sof-qemu-run] STANDARD FIRMWARE + ZTEST ENABLED: Tests attached to normal IPC boot hook without standalone overlay limits.\033[0m")
        if args.rebuild:
            print("\033[32;1m[sof-qemu-run] Recompiling standard Zephyr firmware natively alongside unit testing modules...\033[0m")
            # Inject standard rimage build directory directly into PATH so `west sign` mathematically authenticates Zephyr.elf into Zephyr.ri directly seamlessly.
            sof_workspace = os.environ.get("SOF_WORKSPACE", os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
            optional_rimage_path = os.path.join(sof_workspace, "build-rimage")
            if os.path.isdir(optional_rimage_path) and optional_rimage_path not in os.environ.get("PATH", ""):
                os.environ["PATH"] = f"{optional_rimage_path}{os.pathsep}{os.environ.get('PATH', '')}"
                print(f"[sof-qemu-run] Injected Rimage Path: {optional_rimage_path}")

            # Force fully-functional topology builds by injecting testing parameters strictly via commandline arguments natively
            subprocess.run([west_path, "build", "-d", build_dir, "-p", "auto", "--", "-DCONFIG_ZTEST=y", "-DCONFIG_SOF_USERSPACE_LL=y", "-DCONFIG_COMP_SRC=y", "-DCONFIG_COMP_COPIER=y", "-DCONFIG_COMP_VOLUME=y", "-DCONFIG_COMP_MIXIN_MIXOUT=y", "-DCONFIG_MAX_THREAD_BYTES=4"], check=True)
            print("\033[32;1m[sof-qemu-run] Standard Compilation Successful.\033[0m\n")
        else:
            print("\033[32;1m[sof-qemu-run] Skipping compilation/rebuild, using previously generated binaries.\033[0m\n")

    print(f"Starting QEMU test runner (Build Dir: {args.build_dir}, Timeout: {args.timeout}s)...")

    west_path = shutil.which("west")
    if not west_path:
        print("[sof-qemu-run] Error: 'west' command not found in PATH.")
        sys.exit(1)

    # Determine execution command
    runs = []
    
    if "ace30" in board.lower() or "ptl" in board.lower() or "wcl" in board.lower():
        qemu_bin_path = os.environ.get("QEMU_BIN_PATH", os.path.join(os.environ.get("SOF_WORKSPACE", os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))), "qemu", "build"))
        qemu_exe = os.path.join(qemu_bin_path, "qemu-system-xtensa")
        print(f"[sof-qemu-run] Bypassing west run explicitly for ACE30 target. Using QEMU: {qemu_exe}")
        
        # Enable dynamic recursive ZTest execution if isolated zephyr.elf doesn't exist
        fw_images = []
        default_ri = os.path.join(build_dir, "zephyr", "zephyr.ri")
        default_elf = os.path.join(build_dir, "zephyr", "zephyr.elf")
        if os.path.isfile(default_ri):
            fw_images.append(default_ri)
        elif os.path.isfile(default_elf):
            fw_images.append(default_elf)
        else:
            for root, dirs, files in os.walk(build_dir):
                if "zephyr.elf" in files:
                    fw_images.append(os.path.join(root, "zephyr.elf"))
                    
        if not fw_images:
            print(f"[sof-qemu-run] Error: No Zephyr firmware generated natively (missing zephyr.elf) within {build_dir}")
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
