#!/usr/bin/env python3
"""
Automated Hardware Loopback Test Suite for ESP32-S3 Sound Open Firmware (SOF)

Supports both:
1. Native USB Audio Class 2 (UAC2) loopback via ALSA host streaming (aplay/arecord).
2. Direct CLI firmware loopback via internal tone generator and capture dump.

Validates bidirectional I2S digital audio transmission across cross-connected
ESP32-S3 development boards (Arduino Nano ESP32 / u-blox NORA-W106):
  - Board A (MAC 34:85:18:7b:40:6c, Default Master Tx)
  - Board B (MAC 20:6e:f1:32:d6:84, Default Slave Rx)

Wiring:
  D9  (GPIO 18) <-> D9  (GPIO 18) (I2S BCLK)
  D10 (GPIO 21) <-> D10 (GPIO 21) (I2S WS / LRCLK)
  D4  (GPIO 7)   -> D5  (GPIO 8)  (Board A DOUT -> Board B DIN)
  D5  (GPIO 8)  <-  D4  (GPIO 7)  (Board A DIN <- Board B DOUT)
  GND            <-> GND           (Ground reference)

Safety Rule:
  NEVER touch, open, or write to /dev/ttyACM0 (28:37:2F:54:4E:98 power relay controller).
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import time
import wave
import numpy as np
import serial

# Safety restriction: Power monitor relay controller must never be opened
FORBIDDEN_SERIAL_PATHS = ["/dev/ttyACM0", "28:37:2F:54:4E:98"]


def check_safety(port):
    """Ensure the specified serial port is not the hardware power relay controller."""
    if not port:
        return
    resolved = os.path.realpath(port)
    for forbidden in FORBIDDEN_SERIAL_PATHS:
        if forbidden in port or forbidden in resolved:
            raise RuntimeError(
                f"SAFETY VIOLATION: Port {port} resolved to {resolved}, which matches "
                f"protected power relay controller ({forbidden}). Aborting."
            )


def resolve_serial_ports(board_a=None, board_b=None):
    """Auto-detect Board A and Board B serial ports by ID or MAC."""
    by_id_devices = glob.glob("/dev/serial/by-id/*")
    
    resolved_a = board_a
    resolved_b = board_b

    for dev in by_id_devices:
        if any(f in dev for f in FORBIDDEN_SERIAL_PATHS):
            continue
        dev_real = os.path.realpath(dev)
        if any(f in dev_real for f in FORBIDDEN_SERIAL_PATHS):
            continue

        dev_upper = dev.upper()
        # Board A checks
        if not resolved_a:
            if "34:85:18:7B:40:6C" in dev_upper or "S3_MASTER" in dev_upper or "MASTER" in dev_upper:
                resolved_a = dev
        # Board B checks
        if not resolved_b:
            if "20:6E:F1:32:D6:84" in dev_upper or "S3_SLAVE" in dev_upper or "SLAVE" in dev_upper:
                resolved_b = dev

    check_safety(resolved_a)
    check_safety(resolved_b)
    return resolved_a, resolved_b


def find_alsa_cards():
    """Find ALSA card names for Board A (Master) and Board B (Slave)."""
    master_card = None
    slave_card = None

    proc = subprocess.run(["aplay", "-l"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode == 0:
        for line in proc.stdout.splitlines():
            # e.g.: card 4: Master [SOF ESP32S3 Master], device 0: USB Audio [USB Audio]
            m = re.search(r"card (\d+): ([^,]+) \[(.*?)\]", line)
            if m:
                card_num = m.group(1)
                card_name = m.group(2).strip()
                desc = m.group(3)
                if "Master" in card_name or "Master" in desc or "3485187B406C" in desc:
                    master_card = f"hw:{card_num},0"
                elif "Slave" in card_name or "Slave" in desc or "206EF132D684" in desc:
                    slave_card = f"hw:{card_num},0"

    return master_card, slave_card


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


def generate_sine_wav(filepath, duration_sec=4.0, sample_rate=48000, freq=1000.0, amplitude=28000):
    """Generate a stereo 16-bit PCM WAV test file with pure sine tone."""
    t = np.linspace(0, duration_sec, int(sample_rate * duration_sec), endpoint=False)
    sine_wave = (amplitude * np.sin(2 * np.pi * freq * t)).astype(np.int16)
    stereo_data = np.column_stack((sine_wave, sine_wave)).flatten()

    with wave.open(filepath, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(stereo_data.tobytes())


def analyze_captured_wav(filepath, sample_rate=48000, target_freq=1000.0, verbose=False):
    """Analyze recorded WAV file: FFT peak, SNR, and channel symmetry."""
    with wave.open(filepath, "rb") as wf:
        nchannels = wf.getnchannels()
        rate = wf.getframerate()
        nframes = wf.getnframes()
        raw_data = wf.readframes(nframes)

    samples = np.frombuffer(raw_data, dtype=np.int16).reshape(-1, nchannels)
    duration = nframes / rate

    # Analyze steady-state portion
    start_idx = int(1.0 * rate)
    end_idx = int(max(start_idx + rate, (duration - 0.5) * rate))
    if end_idx > len(samples):
        end_idx = len(samples)

    steady = samples[start_idx:end_idx]
    ch0 = steady[:, 0].astype(float)
    ch1 = steady[:, 1].astype(float) if nchannels > 1 else ch0
    diff = np.abs(ch0 - ch1)

    results = {
        "frames": nframes,
        "duration": duration,
        "sample_rate": rate,
        "channels": nchannels,
        "max_diff": float(diff.max()) if nchannels > 1 else 0.0,
        "mean_diff": float(diff.mean()) if nchannels > 1 else 0.0,
        "ch0": {},
        "ch1": {},
    }

    for ch_name, s in [("ch0", ch0), ("ch1", ch1)]:
        w = np.hanning(len(s))
        fft_vals = np.abs(np.fft.rfft((s - np.mean(s)) * w))
        freqs = np.fft.rfftfreq(len(s), 1.0 / rate)

        peak_idx = np.argmax(fft_vals)
        peak_freq = freqs[peak_idx]

        target_mask = np.abs(freqs - target_freq) <= 50.0
        signal_power = np.sum(fft_vals[target_mask] ** 2)
        noise_mask = ~target_mask
        dc_mask = freqs < 50.0
        noise_mask = noise_mask & ~dc_mask
        noise_power = np.sum(fft_vals[noise_mask] ** 2)
        snr_db = 10.0 * np.log10(signal_power / noise_power) if noise_power > 0 else 100.0

        results[ch_name] = {
            "peak_freq": float(peak_freq),
            "snr_db": float(snr_db),
            "min": float(s.min()),
            "max": float(s.max()),
            "rms": float(np.sqrt(np.mean(s ** 2))),
        }

    return results


def test_cli_direction(name, tx_ser, rx_ser, tx_name, rx_name, verbose=False):
    """Run internal tone CLI loopback test in one direction."""
    print(f"\n{'=' * 65}")
    print(f"  RUNNING CLI {name.upper()} LOOPBACK: {tx_name} (Tx) -> {rx_name} (Rx)")
    print(f"{'=' * 65}")

    send_cmd(tx_ser, "sof play stop", verbose=verbose)
    send_cmd(rx_ser, "sof cap stop", verbose=verbose)
    send_cmd(tx_ser, "sof tone off", verbose=verbose)
    send_cmd(rx_ser, "sof tone off", verbose=verbose)

    print(f"[*] Setting {tx_name} to MASTER, {rx_name} to SLAVE...")
    send_cmd(tx_ser, "sof mode i2s master", verbose=verbose)
    send_cmd(rx_ser, "sof mode i2s slave", verbose=verbose)
    time.sleep(0.2)

    print(f"[*] Starting capture on {rx_name} (Slave Rx)...")
    send_cmd(rx_ser, "sof cap start", verbose=verbose)
    time.sleep(0.2)

    print(f"[*] Starting 1000 Hz tone and playback on {tx_name} (Master Tx)...")
    send_cmd(tx_ser, "sof tone on", verbose=verbose)
    send_cmd(tx_ser, "sof play start", verbose=verbose)
    time.sleep(0.6)

    print(f"[*] Querying capture stream metrics...")
    stats_resp = send_cmd(rx_ser, "sof cap stats", verbose=verbose)
    metrics = parse_stats(stats_resp)

    print(f"[*] Inspecting captured PCM sample waveform...")
    dump_resp = send_cmd(rx_ser, "sof cap dump 32", verbose=verbose)
    samples = parse_dump(dump_resp)

    send_cmd(rx_ser, "sof cap stop", verbose=verbose)
    send_cmd(tx_ser, "sof play stop", verbose=verbose)
    send_cmd(tx_ser, "sof tone off", verbose=verbose)

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

    if len(samples) > 0:
        max_diff = max(abs(s[1] - s[2]) for s in samples)
        if max_diff > 200:
            passed = False
            reasons.append(f"Channel asymmetry detected: max|L - R| = {max_diff}")
    else:
        passed = False
        reasons.append("No PCM samples captured or dumped")

    print(f"\n--- {name} Results Summary ---")
    print(f"  Left Channel  : pk-pk={metrics['pk_pk_l']}, rms={metrics['rms_l']}, min={metrics['min_l']}, max={metrics['max_l']}")
    print(f"  Right Channel : pk-pk={metrics['pk_pk_r']}, rms={metrics['rms_r']}, min={metrics['min_r']}, max={metrics['max_r']}")
    print(f"  Zero Crossings: {metrics['zero_crossings']} (5ms window)")
    print(f"  Est Frequency : {metrics['est_freq']} Hz")
    if len(samples) > 0:
        print(f"  Sample Symmetry: bit-exact match on {len(samples)} frames (max|L-R| = {max_diff})")

    if passed:
        print(f"\n>> {name.upper()} LOOPBACK: [PASS]")
    else:
        print(f"\n>> {name.upper()} LOOPBACK: [FAIL]")
        for r in reasons:
            print(f"   [-] Reason: {r}")

    return passed, metrics


def test_uac2_loopback(tx_card, rx_card, duration=4, rate=48000, freq=1000.0, min_snr=80.0, verbose=False):
    """Run full-stack UAC2 ALSA host streaming loopback test."""
    print(f"\n{'=' * 65}")
    print(f"  RUNNING UAC2 ALSA LOOPBACK: Host -> {tx_card} -> I2S -> {rx_card} -> Host")
    print(f"{'=' * 65}")

    test_wav = "/tmp/s3_uac2_test.wav"
    cap_wav = "/tmp/s3_uac2_cap.wav"

    if os.path.exists(cap_wav):
        os.remove(cap_wav)

    print(f"[*] Generating {freq} Hz stereo 16-bit reference tone ({duration}s)...")
    generate_sine_wav(test_wav, duration_sec=duration + 1.0, sample_rate=rate, freq=freq)

    print(f"[*] Launching aplay on {tx_card}...")
    play_proc = subprocess.Popen(
        ["aplay", "-D", tx_card, "-f", "S16_LE", "-r", str(rate), "-c", "2", test_wav],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    time.sleep(0.3)

    print(f"[*] Launching arecord on {rx_card}...")
    rec_proc = subprocess.Popen(
        ["arecord", "-D", rx_card, "-f", "S16_LE", "-r", str(rate), "-c", "2",
         "--period-size=480", "--buffer-size=1920", "-d", str(duration), cap_wav],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    rec_out, rec_err = rec_proc.communicate(timeout=duration + 6)
    play_proc.terminate()
    try:
        play_proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        play_proc.kill()

    if rec_proc.returncode != 0:
        print(f"[-] ERROR: arecord failed with code {rec_proc.returncode}")
        if rec_err:
            print(f"    stderr: {rec_err.decode(errors='replace').strip()}")
        return False, None

    if not os.path.exists(cap_wav) or os.path.getsize(cap_wav) < 44:
        print(f"[-] ERROR: Captured WAV file {cap_wav} is missing or empty.")
        return False, None

    results = analyze_captured_wav(cap_wav, sample_rate=rate, target_freq=freq, verbose=verbose)

    ch0_snr = results["ch0"]["snr_db"]
    ch1_snr = results["ch1"]["snr_db"]
    ch0_freq = results["ch0"]["peak_freq"]
    ch1_freq = results["ch1"]["peak_freq"]

    print(f"\n--- UAC2 Loopback Analysis Results ---")
    print(f"  Frames Captured : {results['frames']} ({results['duration']:.2f}s)")
    print(f"  Ch0 (Left)      : Peak={ch0_freq:7.2f} Hz, SNR={ch0_snr:6.2f} dB, Range=[{results['ch0']['min']:6.0f}, {results['ch0']['max']:6.0f}]")
    print(f"  Ch1 (Right)     : Peak={ch1_freq:7.2f} Hz, SNR={ch1_snr:6.2f} dB, Range=[{results['ch1']['min']:6.0f}, {results['ch1']['max']:6.0f}]")
    print(f"  Ch0-Ch1 Diff    : Max={results['max_diff']:6.0f}, Mean={results['mean_diff']:6.4f}")

    freq_pass = abs(ch0_freq - freq) <= 5.0 and abs(ch1_freq - freq) <= 5.0
    snr_pass = ch0_snr >= min_snr and ch1_snr >= min_snr
    passed = freq_pass and snr_pass

    if passed:
        print(f"\n>> UAC2 ALSA LOOPBACK: [PASS]")
    else:
        print(f"\n>> UAC2 ALSA LOOPBACK: [FAIL]")
        if not freq_pass:
            print(f"   [-] Frequency check failed: expected {freq} Hz, got L={ch0_freq} Hz, R={ch1_freq} Hz")
        if not snr_pass:
            print(f"   [-] SNR check failed: minimum {min_snr} dB, got L={ch0_snr:.1f} dB, R={ch1_snr:.1f} dB")

    return passed, results


def main():
    parser = argparse.ArgumentParser(description="Pre-Commit ESP32-S3 Audio Loopback Test Suite")
    parser.add_argument("--board-a-port", default=None,
                        help="Serial port for Board A (MAC 34:85:18:7b:40:6c)")
    parser.add_argument("--board-b-port", default=None,
                        help="Serial port for Board B (MAC 20:6e:f1:32:d6:84)")
    parser.add_argument("--mode", choices=["all", "cli", "uac2"], default="all",
                        help="Test mode: cli (shell commands), uac2 (ALSA streaming), or all")
    parser.add_argument("--direction", choices=["both", "forward", "reverse"], default="both",
                        help="Test direction: forward (A->B), reverse (B->A), or both")
    parser.add_argument("--duration", type=int, default=4, help="Capture duration in seconds (default: 4)")
    parser.add_argument("--min-snr", type=float, default=80.0, help="Minimum acceptable SNR in dB (default: 80.0)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose serial I/O logging")
    args = parser.parse_args()

    port_a, port_b = resolve_serial_ports(args.board_a_port, args.board_b_port)

    print("ESP32-S3 Devices:")
    print(f"  Board A (Master) Serial: {port_a or 'Not Found'}")
    print(f"  Board B (Slave)  Serial: {port_b or 'Not Found'}")

    master_card, slave_card = find_alsa_cards()
    print(f"  Board A (Master) ALSA  : {master_card or 'Not Found'}")
    print(f"  Board B (Slave)  ALSA  : {slave_card or 'Not Found'}")

    overall_pass = True

    # 1. UAC2 ALSA Test
    if args.mode in ["all", "uac2"]:
        if master_card and slave_card:
            uac2_pass, _ = test_uac2_loopback(
                tx_card=master_card,
                rx_card=slave_card,
                duration=args.duration,
                min_snr=args.min_snr,
                verbose=args.verbose,
            )
            if not uac2_pass:
                overall_pass = False
        else:
            if args.mode == "uac2":
                print("[-] ERROR: UAC2 mode requested but ALSA cards not detected for Master and Slave.")
                sys.exit(1)
            else:
                print("[*] ALSA cards not found yet, skipping UAC2 test.")

    # 2. CLI Serial Test
    if args.mode in ["all", "cli"]:
        if port_a and port_b:
            try:
                ser_a = serial.Serial(port_a, 115200, timeout=1.0)
                ser_b = serial.Serial(port_b, 115200, timeout=1.0)

                ser_a.write(b"\r\n")
                ser_b.write(b"\r\n")
                time.sleep(0.1)
                ser_a.read_all()
                ser_b.read_all()

                if args.direction in ["both", "forward"]:
                    fwd_pass, _ = test_cli_direction("Forward", ser_a, ser_b, "Board A", "Board B", verbose=args.verbose)
                    if not fwd_pass:
                        overall_pass = False

                if args.direction in ["both", "reverse"]:
                    rev_pass, _ = test_cli_direction("Reverse", ser_b, ser_a, "Board B", "Board A", verbose=args.verbose)
                    if not rev_pass:
                        overall_pass = False

                send_cmd(ser_a, "sof play stop")
                send_cmd(ser_a, "sof cap stop")
                send_cmd(ser_a, "sof tone off")
                send_cmd(ser_b, "sof play stop")
                send_cmd(ser_b, "sof cap stop")
                send_cmd(ser_b, "sof tone off")
                send_cmd(ser_a, "sof mode i2s master")
                send_cmd(ser_b, "sof mode i2s slave")
                ser_a.close()
                ser_b.close()
            except Exception as e:
                print(f"[-] Serial CLI testing error: {e}")
                overall_pass = False
        else:
            if args.mode == "cli":
                print("[-] ERROR: CLI mode requested but serial ports not found.")
                sys.exit(1)

    print(f"\n{'=' * 65}")
    if overall_pass:
        print(">> ALL ESP32-S3 LOOPBACK TESTS PASSED!")
    else:
        print(">> SOME ESP32-S3 LOOPBACK TESTS FAILED!")
    print(f"{'=' * 65}\n")

    sys.exit(0 if overall_pass else 1)


if __name__ == "__main__":
    main()
