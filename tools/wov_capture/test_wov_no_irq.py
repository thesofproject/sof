#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
WOV Test Suite using no_irq (no_period_wakeup) and mmap modes with test trigger kcontrols.
Verifies WoV detection and capture in both S0 and D0i3 states without modifying kernel wait_time.
"""

import os
import re
import subprocess
import sys
import threading
import time


def get_kcontrol(name, card=0):
    cmd = ["amixer", "-c", str(card), "cget", f"name={name}"]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        return None
    for line in res.stdout.splitlines():
        if ": values=" in line:
            return line.split(": values=")[1].strip()
    return None


def set_kcontrol(name, val, card=0):
    cmd = ["amixer", "-c", str(card), "cset", f"name={name}", str(val)]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return res.returncode == 0


def get_dsp_power_state():
    """Check DSP power state from dmesg."""
    try:
        out = subprocess.check_output("dmesg | tail -n 25", shell=True, text=True)
        states = re.findall(r"Current DSP power state:\s*(\w+)", out)
        if states:
            return states[-1]
    except Exception:
        pass
    return "UNKNOWN"


def monitor_active_slot(stop_evt, observed_slots, card=0):
    """Poll wov_active_slot every 50ms while capture is active."""
    while not stop_evt.is_set():
        v = get_kcontrol("wov_active_slot", card=card)
        if v is not None and (not observed_slots or observed_slots[-1] != v):
            observed_slots.append(v)
        time.sleep(0.05)


def test_s0_slot(slot, ctl, mode="alsa_no_irq", card=0, device=12):
    print(f"\n==================================================")
    print(f"[S0 Test] Slot {slot} ({ctl}) - Mode: {mode}")
    print(f"==================================================")

    init_slot = get_kcontrol("wov_active_slot", card=card)
    print(f"  1. Initial wov_active_slot : {init_slot} (Expected: 0)")

    if mode == "alsa_no_irq":
        app = "/usr/local/bin/wov_blocking_read"
    elif mode == "tinyalsa_mmap_noirq":
        app = "/usr/local/bin/wov_blocking_read_tinyalsa"
    else:
        raise ValueError(f"Unknown mode: {mode}")

    dev_str = f"hw:{card},{device}"
    print(f"  2. Starting {os.path.basename(app)} on {dev_str} with trigger {ctl}...")
    t0 = time.monotonic()

    observed_slots = []
    stop_evt = threading.Event()
    mon_thread = threading.Thread(target=monitor_active_slot, args=(stop_evt, observed_slots, card))
    mon_thread.start()

    p = subprocess.Popen([app, dev_str, ctl], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        stdout, stderr = p.communicate(timeout=15)
        elapsed = time.monotonic() - t0
        stop_evt.set()
        mon_thread.join()
        print(f"  3. Capture completed in    : {elapsed:.2f}s")
        print(f"     App output: {stdout.strip()}")
    except subprocess.TimeoutExpired:
        p.kill()
        stop_evt.set()
        mon_thread.join()
        print(f"  [ERROR] Capture timed out after 15s!")
        return False

    post_slot = None
    for _ in range(15):
        post_slot = get_kcontrol("wov_active_slot", card=card)
        if post_slot == "0":
            break
        time.sleep(0.1)
    post_ctl = get_kcontrol(ctl, card=card)
    print(f"  4. Observed slots during run: {' -> '.join(observed_slots)}")
    print(f"     Post-stop active slot   : {post_slot} (Expected: 0)")
    print(f"     Post-stop trigger state : {post_ctl} (Expected: off)")

    wake_seen = str(slot) in observed_slots
    passed = wake_seen and (post_slot == "0") and (post_ctl == "off") and (p.returncode == 0)
    print(f"  --> RESULT: {'PASSED' if passed else 'FAILED'}")
    return passed


def test_d0i3_slot(slot, ctl, card=0, device=12):
    print(f"\n==================================================")
    print(f"[D0i3 Test] Slot {slot} ({ctl}) - Idle D0i3 -> Wake -> Capture")
    print(f"==================================================")

    out_wav = f"/tmp/wov_d0i3_slot{slot}.wav"
    subprocess.run(["rm", "-f", out_wav])

    init_slot = get_kcontrol("wov_active_slot", card=card)
    print(f"  1. Initial wov_active_slot : {init_slot} (Expected: 0)")

    app = "/usr/local/bin/wov_blocking_read"
    dev_str = f"hw:{card},{device}"
    cmd = [app, dev_str, ctl, "6", out_wav, "8000"]
    print(f"  2. Launching {os.path.basename(app)} on {dev_str} (6s D0i3 idle delay, trigger {ctl})...")
    t0 = time.monotonic()

    observed_slots = []
    stop_evt = threading.Event()
    mon_thread = threading.Thread(target=monitor_active_slot, args=(stop_evt, observed_slots, card))
    mon_thread.start()

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # Check DSP power state after 5.5s idle
    time.sleep(5.5)
    pstate = get_dsp_power_state()
    print(f"  3. DSP power state after 5.5s stream idle: {pstate} (Expected: D0I3)")

    try:
        stdout, stderr = proc.communicate(timeout=25)
        elapsed = time.monotonic() - t0
        stop_evt.set()
        mon_thread.join()
        print(f"  4. Capture process exited in: {elapsed:.2f}s")
        print(f"     App output: {stdout.strip()}")
    except subprocess.TimeoutExpired:
        proc.kill()
        stop_evt.set()
        mon_thread.join()
        print(f"  [ERROR] Capture timed out after 25s!")
        return False

    post_pstate = get_dsp_power_state()
    file_sz = os.path.getsize(out_wav) if os.path.exists(out_wav) else 0
    post_slot = None
    for _ in range(15):
        post_slot = get_kcontrol("wov_active_slot", card=card)
        if post_slot == "0":
            break
        time.sleep(0.1)
    sw_state = get_kcontrol(ctl, card=card)

    print(f"  5. Verification:")
    print(f"     Observed slots during run: {' -> '.join(observed_slots)}")
    print(f"     Post-stop active slot   : {post_slot} (Expected: 0)")
    print(f"     Captured WAV file       : {out_wav} ({file_sz} bytes)")
    print(f"     Test switch reset       : {sw_state} (Expected: off)")
    print(f"     DSP power state after   : {post_pstate}")

    wake_seen = str(slot) in observed_slots
    passed = (file_sz > 0) and wake_seen and (post_slot == "0") and (sw_state == "off") and (proc.returncode == 0)
    print(f"  --> RESULT: {'PASSED' if passed else 'FAILED'}")
    return passed


def main():
    print("****************************************************************")
    print("  SOF WOV S0 & D0i3 Test Runner (no_irq & mmap modes)")
    print("  Unmodified Kernel Driver (substream->wait_time = 500ms)")
    print("****************************************************************")

    # Slot mappings:
    # Slot 1 -> wovdebug_111 (mww.111.1, Pipeline 111, slot_id 0)
    # Slot 2 -> wovdebug_112 (mww.112.1, Pipeline 112, slot_id 1)
    # Slot 3 -> wovdebug_113 (mww.113.1, Pipeline 113, slot_id 2)
    slots = [
        (1, "wovdebug_111"),
        (2, "wovdebug_112"),
        (3, "wovdebug_113"),
    ]

    results = {}

    # Test 1: S0 with ALSA no_irq (snd_pcm_hw_params_set_period_wakeup 0)
    for slot, ctl in slots:
        test_name = f"S0_Slot_{slot}_no_irq"
        results[test_name] = test_s0_slot(slot, ctl, mode="alsa_no_irq")
        time.sleep(1.0)

    # Test 2: S0 with tinyalsa MMAP + NOIRQ
    test_name = "S0_Slot_1_tinyalsa_mmap_noirq"
    results[test_name] = test_s0_slot(1, "wovdebug_111", mode="tinyalsa_mmap_noirq")
    time.sleep(1.0)

    # Test 3: D0i3 with arecord -M (mmap mode)
    for slot, ctl in slots:
        test_name = f"D0i3_Slot_{slot}_mmap"
        results[test_name] = test_d0i3_slot(slot, ctl)
        time.sleep(1.0)

    print("\n****************************************************************")
    print("  FINAL TEST SUMMARY")
    print("****************************************************************")
    all_passed = True
    for name, res in results.items():
        status = "PASSED" if res else "FAILED"
        print(f"  {name:<32}: {status}")
        if not res:
            all_passed = False

    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
