#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Automated 10x S0 and 10x D0i3 WoV capture test runner on hardware.
"""

import os
import re
import subprocess
import sys
import time

def find_wov_pcm(card=0):
    """Auto-detect DMIC Multi-WOV capture PCM device index."""
    try:
        with open("/proc/asound/pcm") as f:
            for line in f:
                if "DMIC Multi-WOV" in line:
                    m = re.match(r"^(\d+)-(\d+):", line)
                    if m and int(m.group(1)) == card:
                        return int(m.group(2))
    except Exception:
        pass
    return 11

def find_wov_slots(card=0):
    """Auto-detect slot controls (e.g. wovdebug_101 or wovdebug_111)."""
    try:
        cmd = ["amixer", "-c", str(card), "controls"]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if "wovdebug_101" in res.stdout:
            return [
                (1, "wovdebug_101"),
                (2, "wovdebug_102"),
                (3, "wovdebug_103"),
            ]
    except Exception:
        pass
    return [
        (1, "wovdebug_111"),
        (2, "wovdebug_112"),
        (3, "wovdebug_113"),
    ]

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

def reset_all_controls(card=0, slots=None):
    if slots is None:
        slots = find_wov_slots(card)
    set_kcontrol("wov_active_slot", 0, card=card)
    for _, ctl in slots:
        set_kcontrol(ctl, 0, card=card)

def get_dsp_power_state():
    """Check most recent DSP power state from dmesg."""
    try:
        out = subprocess.check_output('dmesg | grep "Current DSP power state:" | tail -n 5', shell=True, text=True)
        states = re.findall(r"Current DSP power state:\s*(\w+)", out)
        if states:
            return states[-1].upper()
    except Exception:
        pass
    return "UNKNOWN"

def check_d0i3_transition(t_start):
    """Check if D0I3 was entered after t_start using dmesg timestamps."""
    try:
        out = subprocess.check_output('dmesg | grep "Current DSP power state:" | tail -n 10', shell=True, text=True)
        for line in out.splitlines():
            m = re.search(r"\[\s*([0-9]+\.[0-9]+)\]\s+.*Current DSP power state:\s*(\w+)", line)
            if m:
                ts = float(m.group(1))
                state = m.group(2).upper()
                if ts >= t_start and state == "D0I3":
                    return True
    except Exception:
        pass
    return False

def run_s0_iteration(iteration, slot, ctl, card=0, device=None):
    if device is None:
        device = find_wov_pcm(card)
    reset_all_controls(card)
    out_wav = f"/tmp/wov_s0_run{iteration}_slot{slot}.wav"
    subprocess.run(["rm", "-f", out_wav])
    app = "/usr/local/bin/wov_blocking_read"
    dev_str = f"hw:{card},{device}"
    cmd = [app, dev_str, ctl, "0", out_wav, "8000"]
    
    t0 = time.monotonic()
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=20)
    elapsed = time.monotonic() - t0
    
    file_sz = os.path.getsize(out_wav) if os.path.exists(out_wav) else 0
    
    # Wait up to 1.5s for async control updates to settle
    post_slot = None
    for _ in range(15):
        post_slot = get_kcontrol("wov_active_slot", card=card)
        if post_slot == "0":
            break
        time.sleep(0.1)
        
    post_ctl = None
    for _ in range(10):
        post_ctl = get_kcontrol(ctl, card=card)
        if post_ctl == "off":
            break
        time.sleep(0.1)
    
    trig_slot = None
    m = re.search(r"triggered active_slot=(\d+)", p.stdout)
    if m:
        trig_slot = int(m.group(1))

    passed = (p.returncode == 0) and (file_sz > 0) and (post_slot == "0") and (post_ctl == "off") and (trig_slot == slot)
    return {
        "iteration": iteration,
        "mode": "S0",
        "slot": slot,
        "trig_slot": trig_slot,
        "ctl": ctl,
        "elapsed": elapsed,
        "size": file_sz,
        "post_slot": post_slot,
        "post_ctl": post_ctl,
        "passed": passed,
    }

def run_d0i3_iteration(iteration, slot, ctl, card=0, device=None):
    if device is None:
        device = find_wov_pcm(card)
    reset_all_controls(card)
    out_wav = f"/tmp/wov_d0i3_run{iteration}_slot{slot}.wav"
    subprocess.run(["rm", "-f", out_wav])
    app = "/usr/local/bin/wov_blocking_read"
    dev_str = f"hw:{card},{device}"
    # Wait 7s idle in stream so DSP cleanly reaches D0I3 before trigger fires
    cmd = [app, dev_str, ctl, "7", out_wav, "8000"]
    
    t_start = time.clock_gettime(time.CLOCK_BOOTTIME)
    t0 = time.monotonic()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    # Poll for DSP D0I3 state between 5.7s and 6.8s
    time.sleep(5.7)
    d0i3_state = "UNKNOWN"
    for _ in range(12):
        if check_d0i3_transition(t_start):
            d0i3_state = "D0I3"
            break
        time.sleep(0.1)
    
    stdout, stderr = proc.communicate(timeout=25)
    elapsed = time.monotonic() - t0
    
    file_sz = os.path.getsize(out_wav) if os.path.exists(out_wav) else 0
    
    # Wait up to 1.5s for async control updates to settle
    post_slot = None
    for _ in range(15):
        post_slot = get_kcontrol("wov_active_slot", card=card)
        if post_slot == "0":
            break
        time.sleep(0.1)
        
    post_ctl = None
    for _ in range(10):
        post_ctl = get_kcontrol(ctl, card=card)
        if post_ctl == "off":
            break
        time.sleep(0.1)
        
    post_pstate = get_dsp_power_state()
    
    trig_slot = None
    m = re.search(r"triggered active_slot=(\d+)", stdout)
    if m:
        trig_slot = int(m.group(1))

    passed = (proc.returncode == 0) and (file_sz > 0) and (post_slot == "0") and (post_ctl == "off") and (d0i3_state == "D0I3") and (trig_slot == slot)
    return {
        "iteration": iteration,
        "mode": "D0i3",
        "slot": slot,
        "trig_slot": trig_slot,
        "ctl": ctl,
        "elapsed": elapsed,
        "size": file_sz,
        "d0i3_state": d0i3_state,
        "post_slot": post_slot,
        "post_ctl": post_ctl,
        "post_pstate": post_pstate,
        "passed": passed,
    }

def main():
    print("=" * 70)
    print("  SOF WOV HARDWARE VALIDATION: 10x S0 and 10x D0i3 CAPTURE TESTS")
    print("=" * 70)
    
    card = 0
    slots = find_wov_slots(card)
    device = find_wov_pcm(card)
    print(f"Target: hw:{card},{device}, Controls: {[ctl for _, ctl in slots]}")
    
    # Ensure dynamic debug is enabled for DSP power state messages
    subprocess.run('echo "file hda-dsp.c +p" > /sys/kernel/debug/dynamic_debug/control 2>/dev/null', shell=True)
    
    # Initial cleanup
    reset_all_controls(card=card, slots=slots)
    time.sleep(1.0)
    
    s0_results = []
    print("\n--- PHASE 1: Running 10x S0 WoV Capture Tests ---")
    for i in range(1, 11):
        slot, ctl = slots[(i - 1) % len(slots)]
        res = run_s0_iteration(i, slot, ctl, card=card, device=device)
        s0_results.append(res)
        status_str = "PASS" if res["passed"] else "FAIL"
        print(f"  [S0 Run {i:02d}/10] Slot {slot} ({ctl}): {status_str} in {res['elapsed']:.2f}s, trig_slot={res['trig_slot']}, size={res['size']}B, slot_reset={res['post_slot']}, ctl_reset={res['post_ctl']}")
        sys.stdout.flush()
        time.sleep(3.0)
        
    d0i3_results = []
    print("\n--- PHASE 2: Running 10x D0i3 WoV Idle Delay & Wake Tests ---")
    for i in range(1, 11):
        slot, ctl = slots[(i - 1) % len(slots)]
        res = run_d0i3_iteration(i, slot, ctl, card=card, device=device)
        d0i3_results.append(res)
        status_str = "PASS" if res["passed"] else "FAIL"
        print(f"  [D0i3 Run {i:02d}/10] Slot {slot} ({ctl}): {status_str} in {res['elapsed']:.2f}s, trig_slot={res['trig_slot']}, d0i3={res['d0i3_state']}, size={res['size']}B, slot_reset={res['post_slot']}, ctl_reset={res['post_ctl']}")
        sys.stdout.flush()
        time.sleep(6.0)
        
    print("\n" + "=" * 70)
    print("  FINAL VALIDATION SUMMARY")
    print("=" * 70)
    s0_pass = sum(1 for r in s0_results if r["passed"])
    d0i3_pass = sum(1 for r in d0i3_results if r["passed"])
    print(f"  S0 WoV Capture Tests   : {s0_pass:2d}/10 PASSED")
    print(f"  D0i3 WoV Wake Tests    : {d0i3_pass:2d}/10 PASSED")
    print("=" * 70)
    
    if s0_pass == 10 and d0i3_pass == 10:
        print("\n>>> ALL 20 HARDWARE TESTS PASSED (100% SUCCESS) <<<")
        sys.exit(0)
    else:
        print("\n>>> SOME TESTS FAILED <<<")
        sys.exit(1)

if __name__ == "__main__":
    main()
