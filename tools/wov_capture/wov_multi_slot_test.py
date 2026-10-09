#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Multi-Slot WOV Automated Trigger & State Verification Test Suite
Tests 3 WOV detector slots (101 -> Slot 1, 102 -> Slot 2, 103 -> Slot 3),
momentary switch auto-reset, and active_slot enum transition to Listening (0).
"""

import argparse
import math
import os
import re
import subprocess
import sys
import time


def get_kcontrol(name, card=0):
    """Query an ALSA mixer control value using amixer."""
    cmd = ["amixer", "-c", str(card), "cget", f"name={name}"]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"amixer cget {name} failed: {proc.stderr.strip()}")
    
    for line in proc.stdout.splitlines():
        if ": values=" in line:
            val_str = line.split(": values=")[1].strip()
            return val_str
    raise RuntimeError(f"Could not parse values from: {proc.stdout}")


def set_kcontrol(name, val, card=0):
    """Set an ALSA mixer control value using amixer."""
    cmd = ["amixer", "-c", str(card), "cset", f"name={name}", str(val)]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"amixer cset {name} {val} failed: {proc.stderr.strip()}")


def run_cycle(cycle_num, target_slot, card=0, device=11, channels=4, rate=16000, duration=3):
    """
    Run a single test cycle:
      1. Verify wov_active_slot is '0' (Listening) before start.
      2. Start arecord on hw:{card},{device}.
      3. Trigger wov_test_{100 + target_slot}.
      4. Verify wov_active_slot transitions to target_slot.
      5. Verify test switch auto-resets to 'off'.
      6. Wait for arecord to complete.
      7. Verify wov_active_slot returns to '0' (Listening).
    """
    slot_names = {1: "Slot 1 (wov_test_101)", 2: "Slot 2 (wov_test_102)", 3: "Slot 3 (wov_test_103)"}
    sw_name = f"wov_test_{100 + target_slot}"
    out_wav = f"/tmp/wov_slot_test_cyc{cycle_num}_{target_slot}.wav"

    print(f"\n[Cycle {cycle_num:02d}] Testing {slot_names[target_slot]}...")

    # Step 1: Initial state
    initial_slot = get_kcontrol("wov_active_slot", card=card)
    if initial_slot != "0":
        print(f"  [WARN] Initial slot was '{initial_slot}', expected '0' (Listening)")

    # Step 2: Start capture
    arecord_cmd = [
        "arecord", "-D", f"hw:{card},{device}", "-f", "S16_LE", "-r", str(rate),
        "-c", str(channels), "-d", str(duration), out_wav
    ]
    t0 = time.monotonic()
    proc = subprocess.Popen(arecord_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.5)

    # Step 3: Trigger test switch
    set_kcontrol(sw_name, 1, card=card)
    time.sleep(0.2)

    # Step 4: Verify active slot
    active_slot = get_kcontrol("wov_active_slot", card=card)
    print(f"  Active Slot during trigger : {active_slot} (Expected: {target_slot})")

    # Step 5: Verify switch reset
    sw_state = get_kcontrol(sw_name, card=card)
    print(f"  Test Switch state          : {sw_state} (Expected: off)")

    # Step 6: Wait for capture completion
    proc.wait()
    t_elapsed = time.monotonic() - t0
    file_sz = os.path.getsize(out_wav) if os.path.exists(out_wav) else 0

    # Step 7: Verify post-stop slot reset
    post_slot = get_kcontrol("wov_active_slot", card=card)
    print(f"  Active Slot after stop     : {post_slot} (Expected: 0)")
    print(f"  Captured WAV File          : {out_wav} ({file_sz} bytes, {t_elapsed:.2f}s)")

    passed = (
        active_slot == str(target_slot) and
        sw_state == "off" and
        post_slot == "0" and
        file_sz > 0
    )

    if passed:
        print(f"  [Cycle {cycle_num:02d} - Slot {target_slot}] => PASSED")
    else:
        print(f"  [Cycle {cycle_num:02d} - Slot {target_slot}] => FAILED")

    return passed


def main():
    parser = argparse.ArgumentParser(description="Multi-Slot WOV Automated Trigger & State Verification Test Suite")
    parser.add_argument("--card", type=int, default=0, help="ALSA sound card index (default: 0)")
    parser.add_argument("--device", type=int, default=11, help="ALSA PCM device index (default: 11)")
    parser.add_argument("--channels", "-c", type=int, default=4, help="Number of channels (default: 4)")
    parser.add_argument("--rate", "-r", type=int, default=16000, help="Sampling rate in Hz (default: 16000)")
    parser.add_argument("--duration", "-d", type=int, default=3, help="Duration per cycle in seconds (default: 3)")
    parser.add_argument("--cycles", type=int, default=10, help="Number of test cycles (default: 10)")
    args = parser.parse_args()

    total_tests = 0
    passed_tests = 0

    print("================================================================")
    print(f"  WOV 3-Slot Automated Trigger & Arbitration {args.cycles}-Cycle Suite")
    print(f"  Target: hw:{args.card},{args.device} | {args.channels} Channels @ {args.rate} Hz")
    print("================================================================")

    for cycle in range(1, args.cycles + 1):
        # Round-robin through slot 1, slot 2, slot 3
        slot = ((cycle - 1) % 3) + 1
        total_tests += 1
        if run_cycle(cycle, slot, card=args.card, device=args.device,
                     channels=args.channels, rate=args.rate, duration=args.duration):
            passed_tests += 1
        time.sleep(0.5)

    print("\n================================================================")
    print(f"  RESULTS: {passed_tests}/{total_tests} passed ({(passed_tests/total_tests)*100:.1f}%)")
    print("================================================================")

    sys.exit(0 if passed_tests == total_tests else 1)


if __name__ == "__main__":
    main()
