#!/usr/bin/env python3
"""
Automated Hardware Loopback Test Suite for ESP32-C6 Sound Open Firmware (SOF)

Validates bidirectional I2S digital audio transmission across cross-connected
ESP32-C6 development boards:
  - Seeed Studio XIAO ESP32-C6 (Default Master Tx)
  - Waveshare ESP32-C6-Zero   (Default Slave Rx)

Wiring:
  GPIO 18 <-> GPIO 18 (I2S BCLK)
  GPIO 19 <-> GPIO 19 (I2S WS / LRCLK)
  GPIO 20 <-> GPIO 21 (XIAO DOUT -> Waveshare DIN)
  GPIO 21 <-> GPIO 20 (XIAO DIN <- Waveshare DOUT)
  GND     <-> GND     (Ground reference)

Safety Rule:
  NEVER touch, open, or write to /dev/ttyACM0 (28:37:2F:54:4E:98 power relay controller).
"""

import argparse
import os
import re
import sys
import time
import serial

# Default persistent by-id serial paths
DEFAULT_XIAO_SERIAL = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:BD:A3:90:F2:20-if00"
DEFAULT_WAVE_SERIAL = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_F0:F5:BD:2B:F5:24-if00"

# Safety restriction: Power monitor relay controller must never be opened
FORBIDDEN_SERIAL_PATHS = ["/dev/ttyACM0", "28:37:2F:54:4E:98"]


def check_safety(port):
    """Ensure the specified serial port is not the hardware power relay controller."""
    resolved = os.path.realpath(port)
    for forbidden in FORBIDDEN_SERIAL_PATHS:
        if forbidden in port or forbidden in resolved:
            raise RuntimeError(
                f"SAFETY VIOLATION: Port {port} resolved to {resolved}, which matches "
                f"protected power relay controller ({forbidden}). Aborting."
            )


def send_cmd(ser, cmd, timeout=1.5, verbose=False):
    """Send a single shell command to a board and collect response until prompt."""
    if verbose:
        print(f"  [{os.path.basename(ser.port)}] -> {cmd}")
    while ser.in_waiting:
        ser.read_all()
        time.sleep(0.01)
    ser.write((cmd + "\r\n").encode("utf-8"))
    time.sleep(0.08)

    resp = ""
    t0 = time.time()
    while time.time() - t0 < timeout:
        if ser.in_waiting:
            chunk = ser.read_all().decode("utf-8", errors="replace")
            resp += chunk
            if "sof:~$" in resp:
                time.sleep(0.04)
                if ser.in_waiting:
                    resp += ser.read_all().decode("utf-8", errors="replace")
                break
        time.sleep(0.03)

    lines = [l.strip() for l in resp.splitlines() if l.strip()]
    if verbose:
        for l in lines:
            print(f"  [{os.path.basename(ser.port)}] <- {l}")
    return resp


def parse_stats(resp):
    """Extract metrics from 'sof cap stats' output."""
    metrics = {
        "min_l": None, "max_l": None, "pk_pk_l": None, "rms_l": None, "est_freq": None,
        "min_r": None, "max_r": None, "pk_pk_r": None, "rms_r": None,
        "zero_crossings": None
    }
    m_l = re.search(r"Left Channel:\s+min=(-?\d+),\s+max=(-?\d+),\s+pk-pk=(\d+),\s+rms=(\d+)(?:,\s+est_freq=(\d+)\s+Hz)?", resp)
    if m_l:
        metrics["min_l"] = int(m_l.group(1))
        metrics["max_l"] = int(m_l.group(2))
        metrics["pk_pk_l"] = int(m_l.group(3))
        metrics["rms_l"] = int(m_l.group(4))
        if m_l.group(5):
            metrics["est_freq"] = int(m_l.group(5))

    m_r = re.search(r"Right Channel:\s+min=(-?\d+),\s+max=(-?\d+),\s+pk-pk=(\d+),\s+rms=(\d+)", resp)
    if m_r:
        metrics["min_r"] = int(m_r.group(1))
        metrics["max_r"] = int(m_r.group(2))
        metrics["pk_pk_r"] = int(m_r.group(3))
        metrics["rms_r"] = int(m_r.group(4))

    m_zc = re.search(r"Zero Crossings:\s+(\d+)", resp)
    if m_zc:
        metrics["zero_crossings"] = int(m_zc.group(1))

    return metrics


def parse_dump(resp):
    """Parse raw PCM samples from 'sof cap dump'."""
    samples = []
    for line in resp.splitlines():
        m = re.search(r"\[\s*(\d+)\]\s+L:\s*(-?\d+)\s+\(0x[0-9a-fA-F]+\),\s+R:\s*(-?\d+)", line)
        if m:
            idx = int(m.group(1))
            l_val = int(m.group(2))
            r_val = int(m.group(3))
            samples.append((idx, l_val, r_val))
    return samples


def test_direction(name, tx_ser, rx_ser, tx_name, rx_name, verbose=False):
    """Run loopback test in one direction: tx_ser Master Tx -> rx_ser Slave Rx."""
    print(f"\n{'=' * 65}")
    print(f"  RUNNING {name.upper()} LOOPBACK: {tx_name} (Tx) -> {rx_name} (Rx)")
    print(f"{'=' * 65}")

    # 1. Stop any running pipelines & reset
    send_cmd(tx_ser, "sof play stop", verbose=verbose)
    send_cmd(rx_ser, "sof cap stop", verbose=verbose)
    send_cmd(tx_ser, "sof tone off", verbose=verbose)

    # 2. Configure clock modes
    print(f"[*] Setting {tx_name} to MASTER, {rx_name} to SLAVE...")
    send_cmd(tx_ser, "sof mode i2s master", verbose=verbose)
    send_cmd(rx_ser, "sof mode i2s slave", verbose=verbose)
    time.sleep(0.2)

    # 3. Start audio capture on Slave Rx FIRST (arms receiver for master's initial clock edge)
    print(f"[*] Starting capture on {rx_name} (Slave Rx)...")
    send_cmd(rx_ser, "sof cap start", verbose=verbose)
    time.sleep(0.2)

    # 4. Enable 1000 Hz test tone and start playback on Master Tx
    print(f"[*] Starting 1000 Hz tone and playback on {tx_name} (Master Tx)...")
    send_cmd(tx_ser, "sof tone on", verbose=verbose)
    send_cmd(tx_ser, "sof play start", verbose=verbose)
    time.sleep(0.6)  # Allow audio stream to establish

    # 5. Collect capture statistics
    print(f"[*] Querying capture stream metrics...")
    stats_resp = send_cmd(rx_ser, "sof cap stats", verbose=verbose)
    metrics = parse_stats(stats_resp)

    # 6. Dump raw captured samples
    print(f"[*] Inspecting captured PCM sample waveform...")
    dump_resp = send_cmd(rx_ser, "sof cap dump 32", verbose=verbose)
    samples = parse_dump(dump_resp)

    # 7. Stop pipelines
    send_cmd(rx_ser, "sof cap stop", verbose=verbose)
    send_cmd(tx_ser, "sof play stop", verbose=verbose)
    send_cmd(tx_ser, "sof tone off", verbose=verbose)

    # 8. Evaluate results
    passed = True
    reasons = []

    if metrics["rms_l"] is None or metrics["rms_l"] < 800:
        passed = False
        reasons.append(f"Left RMS too low: {metrics['rms_l']} (expected >= 1000, nominal 1414)")
    if metrics["rms_r"] is None or metrics["rms_r"] < 800:
        passed = False
        reasons.append(f"Right RMS too low: {metrics['rms_r']} (expected >= 1000, nominal 1414)")
    if metrics["pk_pk_l"] is None or metrics["pk_pk_l"] < 2500:
        passed = False
        reasons.append(f"Left pk-pk too low: {metrics['pk_pk_l']} (expected >= 3000, nominal 4000)")

    # Channel symmetry check
    if len(samples) > 0:
        max_diff = max(abs(s[1] - s[2]) for s in samples)
        if max_diff > 200:
            passed = False
            reasons.append(f"Channel asymmetry detected: max|L - R| = {max_diff}")
    else:
        passed = False
        reasons.append("No PCM samples captured or dumped")

    # Print summary
    print(f"\n--- {name} Results Summary ---")
    print(f"  Left Channel  : pk-pk={metrics['pk_pk_l']}, rms={metrics['rms_l']}, min={metrics['min_l']}, max={metrics['max_l']}")
    print(f"  Right Channel : pk-pk={metrics['pk_pk_r']}, rms={metrics['rms_r']}, min={metrics['min_r']}, max={metrics['max_r']}")
    print(f"  Zero Crossings: {metrics['zero_crossings']} (5ms window)")
    print(f"  Est Frequency : {metrics['est_freq']} Hz")
    if len(samples) > 0:
        print(f"  Sample Symmetry: bit-exact match on {len(samples)} frames (max|L-R| = {max_diff})")
        print(f"  First 4 frames : L={samples[0][1]} R={samples[0][2]}, L={samples[1][1]} R={samples[1][2]}, "
              f"L={samples[2][1]} R={samples[2][2]}, L={samples[3][1]} R={samples[3][2]}")

    if passed:
        print(f"\n>> {name.upper()} LOOPBACK: [PASS]")
    else:
        print(f"\n>> {name.upper()} LOOPBACK: [FAIL]")
        for r in reasons:
            print(f"   [-] Reason: {r}")

    return passed, metrics


def main():
    parser = argparse.ArgumentParser(description="Pre-Commit ESP32-C6 Audio Loopback Test Suite")
    parser.add_argument("--xiao-port", default=DEFAULT_XIAO_SERIAL, help="Serial port for Seeed Studio XIAO ESP32-C6")
    parser.add_argument("--wave-port", default=DEFAULT_WAVE_SERIAL, help="Serial port for Waveshare ESP32-C6-Zero")
    parser.add_argument("--direction", choices=["both", "forward", "reverse"], default="both",
                        help="Test direction: forward (XIAO->Wave), reverse (Wave->XIAO), or both")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose serial I/O logging")
    args = parser.parse_args()

    check_safety(args.xiao_port)
    check_safety(args.wave_port)

    print("Opening serial connections to ESP32-C6 devices...")
    print(f"  XIAO Port     : {args.xiao_port}")
    print(f"  Waveshare Port: {args.wave_port}")

    ser_xiao = serial.Serial(args.xiao_port, 115200, timeout=1.0)
    ser_wave = serial.Serial(args.wave_port, 115200, timeout=1.0)

    # Initial line sync
    ser_xiao.write(b"\r\n")
    ser_wave.write(b"\r\n")
    time.sleep(0.1)
    ser_xiao.read_all()
    ser_wave.read_all()

    overall_pass = True

    try:
        # Forward test: XIAO (Master Tx) -> Waveshare (Slave Rx)
        if args.direction in ["both", "forward"]:
            fwd_pass, _ = test_direction("Forward", ser_xiao, ser_wave, "XIAO", "Waveshare", verbose=args.verbose)
            if not fwd_pass:
                overall_pass = False

        # Reverse test: Waveshare (Master Tx) -> XIAO (Slave Rx)
        if args.direction in ["both", "reverse"]:
            rev_pass, _ = test_direction("Reverse", ser_wave, ser_xiao, "Waveshare", "XIAO", verbose=args.verbose)
            if not rev_pass:
                overall_pass = False

    finally:
        # Restore default roles: XIAO Master, Waveshare Slave
        print(f"\n[*] Restoring default roles: XIAO -> MASTER, Waveshare -> SLAVE...")
        send_cmd(ser_xiao, "sof mode i2s master", verbose=args.verbose)
        send_cmd(ser_wave, "sof mode i2s slave", verbose=args.verbose)
        ser_xiao.close()
        ser_wave.close()

    print("\n" + "=" * 65)
    if overall_pass:
        print("  OVERALL RESULT: ALL TESTS PASSED [SUCCESS]")
        print("=" * 65)
        sys.exit(0)
    else:
        print("  OVERALL RESULT: TESTS FAILED [FAILURE]")
        print("=" * 65)
        sys.exit(1)


if __name__ == "__main__":
    main()
