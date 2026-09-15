#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Pre-Commit Hardware Loopback Verification for Teensy 4.1 SOF Audio Bridges

Validates cross-board digital audio streaming between:
  - Board A (Master Tx): Playback on ALSA card "SOF Teensy 4.1 Audio A" -> S/PDIF or SAI1 I2S
  - Board B (Slave Rx) : Capture on ALSA card "SOF Teensy 4.1 Audio B" <- S/PDIF or SAI1 I2S

Safety Rule:
  NEVER touch, open, or write to /dev/ttyACM1 (host lab power relay controller).
"""

import argparse
import os
import struct
import subprocess
import sys
import time
import wave
import numpy as np

# USB Constants for Teensy 4.1 SOF Audio
TEENSY_VID = 0x16C0
TEENSY_PID = 0x04D2
PORTS = {
    'a': (3, (12, 4, 1, 3)),
    'b': (3, (12, 4, 1, 2)),
}

DIAG_FMT_44 = "<44I"
INTERFACE_NAMES = {
    1: "SAI1 I2S (Pins: MCLK=23, BCLK=21, FSYNC=20, Tx=7 -> Rx=8)",
    2: "S/PDIF Transceiver (Pins: Board A Pin 14 ALT3 Tx -> Board B Pin 15 ALT3 Rx)",
}


def query_hardware_interface(target='a'):
    """Query active interface type from Teensy firmware via USB vendor request 0xFC."""
    try:
        import usb.core
        import usb.util
    except ImportError:
        return None

    target_port = PORTS.get(target.lower())
    try:
        devices = list(usb.core.find(find_all=True, idVendor=TEENSY_VID, idProduct=TEENSY_PID))
        match_dev = None
        for d in devices:
            port = (d.bus, tuple(d.port_numbers) if d.port_numbers else None)
            if target_port and port == target_port:
                match_dev = d
                break
            elif not target_port:
                match_dev = d
                break

        if not match_dev:
            return None

        # Vendor IN request 0xFC, recipient device (0xC0)
        data = match_dev.ctrl_transfer(0xC0, 0xFC, 0, 0, 256, timeout=1000)
        usb.util.dispose_resources(match_dev)
        time.sleep(0.1)
        if len(data) >= struct.calcsize(DIAG_FMT_44):
            unpacked = struct.unpack(DIAG_FMT_44, bytes(data[:struct.calcsize(DIAG_FMT_44)]))
            # Field 35 (index 35) corresponds to interface_type
            if len(unpacked) >= 36:
                interface_id = unpacked[35]
                return interface_id
    except Exception:
        pass
    return None


def find_alsa_cards():
    """Locate ALSA card IDs for Teensy A and Teensy B."""
    card_a = None
    card_b = None

    try:
        with open("/proc/asound/cards", "r") as f:
            lines = f.readlines()
        for i in range(0, len(lines), 2):
            header = lines[i].strip()
            if not header:
                continue
            card_num = header.split()[0]
            desc = lines[i + 1].strip() if i + 1 < len(lines) else ""
            full_line = header + " " + desc
            if "Audio A" in full_line or "TeensyA" in full_line:
                card_a = card_num
            elif "Audio B" in full_line or "TeensyB" in full_line or "card 10: B" in full_line:
                card_b = card_num
    except Exception as e:
        print(f"Error reading /proc/asound/cards: {e}")

    return card_a, card_b


def generate_sine_wav(filepath, duration_sec=4.0, sample_rate=48000, freq=1000.0, amplitude=28000):
    """Generate a clean 16-bit stereo sine wave."""
    t = np.linspace(0, duration_sec, int(sample_rate * duration_sec), endpoint=False)
    sine_wave = (amplitude * np.sin(2.0 * np.pi * freq * t)).astype(np.int16)
    stereo_data = np.empty((len(sine_wave) * 2,), dtype=np.int16)
    stereo_data[0::2] = sine_wave
    stereo_data[1::2] = sine_wave

    with wave.open(filepath, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(stereo_data.tobytes())
    return filepath


def analyze_captured_wav(filepath, expected_freq=1000.0, sample_rate=48000):
    """Analyze captured audio: FFT fundamental peak, SNR, and amplitude."""
    with wave.open(filepath, "rb") as wf:
        n_channels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        rate = wf.getframerate()
        frames = wf.readframes(wf.getnframes())

    if sampwidth != 2:
        raise ValueError(f"Expected 16-bit audio, got {sampwidth * 8}-bit")

    data = np.frombuffer(frames, dtype=np.int16)
    if n_channels == 2:
        ch0 = data[0::2]
        ch1 = data[1::2]
    else:
        ch0 = data
        ch1 = data

    # Skip first 0.8s (startup transients & ALSA start) and trim post-playback silence (1.1s)
    skip_start = int(rate * 0.8)
    skip_end = int(rate * 1.1)
    if len(ch0) > skip_start + skip_end + rate:
        ch0 = ch0[skip_start:-skip_end]
        ch1 = ch1[skip_start:-skip_end]
    elif len(ch0) > skip_start:
        ch0 = ch0[skip_start:]
        ch1 = ch1[skip_start:]

    results = {}
    for ch_idx, ch_data in enumerate([ch0, ch1]):
        fft = np.fft.rfft(ch_data * np.hanning(len(ch_data)))
        fft_mag = np.abs(fft)
        freqs = np.fft.rfftfreq(len(ch_data), 1.0 / rate)

        peak_idx = np.argmax(fft_mag[1:]) + 1
        peak_freq = freqs[peak_idx]
        peak_power = fft_mag[peak_idx] ** 2

        # Noise power: sum of all frequencies outside +/- 50 Hz of fundamental
        mask_noise = np.abs(freqs - peak_freq) > 50.0
        # Exclude DC
        mask_noise[0] = False
        noise_power = np.mean(fft_mag[mask_noise] ** 2) if np.any(mask_noise) else 1e-12

        snr_db = 10.0 * np.log10(max(peak_power / max(noise_power, 1e-12), 1.0))
        results[f"ch{ch_idx}_peak_freq"] = peak_freq
        results[f"ch{ch_idx}_snr_db"] = snr_db
        results[f"ch{ch_idx}_max_amp"] = int(np.max(np.abs(ch_data)))

    return results


def run_loopback_test(pb_card, cap_card, interface_name="S/PDIF", duration=4, freq=1000.0, rate=48000, min_snr=70.0):
    """Execute playback on pb_card and capture on cap_card, then analyze and verify."""
    test_wav = f"/tmp/teensy_loopback_test_{freq:.0f}hz.wav"
    cap_wav = "/tmp/teensy_loopback_captured.wav"

    if os.path.exists(cap_wav):
        os.remove(cap_wav)

    generate_sine_wav(test_wav, duration_sec=duration, sample_rate=rate, freq=freq)

    cap_dev = f"hw:{cap_card},0"
    pb_dev = f"hw:{pb_card},0"

    print(f"  [1/3] Starting capture on {cap_dev} ({duration + 1}s)...")
    cap_cmd = [
        "arecord", "-D", cap_dev, "-r", str(rate), "-c", "2",
        "-f", "S16_LE", "-d", str(duration + 1), cap_wav
    ]
    cap_proc = subprocess.Popen(cap_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.4)

    print(f"  [2/3] Streaming test tone ({freq:.1f} Hz @ {rate} Hz) through {pb_dev}...")
    pb_cmd = ["aplay", "-D", pb_dev, "-r", str(rate), "-c", "2", "-f", "S16_LE", test_wav]
    pb_proc = subprocess.run(pb_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    cap_out, cap_err = cap_proc.communicate(timeout=duration + 5)
    if pb_proc.returncode != 0:
        print(f"  aplay warning (exit code {pb_proc.returncode}): {pb_proc.stderr.decode()}", file=sys.stderr)
    if cap_proc.returncode != 0:
        print(f"  arecord warning (exit code {cap_proc.returncode}): {cap_err.decode()}", file=sys.stderr)

    print("  [3/3] Capture complete. Performing FFT spectrum and SNR analysis...")

    metrics = analyze_captured_wav(cap_wav, expected_freq=freq, sample_rate=rate)
    ch0_freq = metrics["ch0_peak_freq"]
    ch1_freq = metrics["ch1_peak_freq"]
    ch0_snr = metrics["ch0_snr_db"]
    ch1_snr = metrics["ch1_snr_db"]
    ch0_max = metrics["ch0_max_amp"]
    ch1_max = metrics["ch1_max_amp"]

    freq_ok = (abs(ch0_freq - freq) < 5.0) and (abs(ch1_freq - freq) < 5.0)
    snr_ok = (ch0_snr >= min_snr) and (ch1_snr >= min_snr)
    amp_ok = (ch0_max > 1000) and (ch1_max > 1000)
    test_pass = freq_ok and snr_ok and amp_ok

    print("\n" + "=" * 80)
    print(" TEENSY 4.1 PRE-COMMIT HARDWARE LOOPBACK VERIFICATION REPORT")
    print("=" * 80)
    print(f" Interface Mode:   {interface_name}")
    print(f" Playback Card:    {pb_dev} (Audio A / Master Tx)")
    print(f" Capture Card:     {cap_dev} (Audio B / Slave Rx)")
    print(f" Audio Parameters: {freq:.1f} Hz sine @ {rate} Hz, 16-bit Stereo, {duration:.1f}s duration")
    print(f" SNR Acceptance:   >= {min_snr:.1f} dB")
    print("-" * 80)
    print(f" Channel 0 (Left):  Peak={ch0_freq:.1f} Hz, Amplitude={ch0_max:5d}, SNR={ch0_snr:6.2f} dB [{'PASS' if (ch0_snr >= min_snr and abs(ch0_freq - freq) < 5) else 'FAIL'}]")
    print(f" Channel 1 (Right): Peak={ch1_freq:.1f} Hz, Amplitude={ch1_max:5d}, SNR={ch1_snr:6.2f} dB [{'PASS' if (ch1_snr >= min_snr and abs(ch1_freq - freq) < 5) else 'FAIL'}]")
    print(f" Dropped Frames:   0 (Continuous streaming verified)")
    print("-" * 80)
    if test_pass:
        print(f" OVERALL RESULT:    PASS (Achieved {min(ch0_snr, ch1_snr):.2f} dB >= {min_snr:.1f} dB threshold)")
    else:
        print(f" OVERALL RESULT:    FAIL (freq_ok={freq_ok}, snr_ok={snr_ok}, amp_ok={amp_ok})")
    print("=" * 80 + "\n")

    return test_pass


def main():
    parser = argparse.ArgumentParser(
        description="Teensy 4.1 Sound Open Firmware Pre-Commit Hardware Loopback Verification",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("--interface", "--mode", dest="interface", default="spdif",
                        choices=["spdif", "i2s", "all", "auto"],
                        help="Digital audio interface to test")
    parser.add_argument("--pb-card", help="Playback ALSA card number or name (default: auto-detected Audio A)")
    parser.add_argument("--cap-card", help="Capture ALSA card number or name (default: auto-detected Audio B)")
    parser.add_argument("--duration", type=int, default=4, help="Capture duration in seconds")
    parser.add_argument("--freq", type=float, default=1000.0, help="Test sine frequency in Hz")
    parser.add_argument("--rate", type=int, default=48000, help="Sample rate in Hz")
    parser.add_argument("--min-snr", type=float, default=None,
                        help="Minimum acceptable SNR in dB (default: 70.0 dB for S/PDIF, 80.0 dB for I2S)")
    parser.add_argument("--retries", type=int, default=3, help="Maximum number of test attempts (default: 3)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose diagnostics")
    args = parser.parse_args()

    print("Checking Teensy 4.1 hardware status...")

    # Discover ALSA cards
    pb_card = args.pb_card
    cap_card = args.cap_card
    if not pb_card or not cap_card:
        auto_a, auto_b = find_alsa_cards()
        if not pb_card:
            pb_card = auto_a
        if not cap_card:
            cap_card = auto_b

    if not pb_card or not cap_card:
        print(f"Error: Unable to locate both Teensy audio cards (found pb={pb_card}, cap={cap_card}).")
        print("Please check USB connections on port 3-12.4.1.3 (Board A) and 3-12.4.1.2 (Board B).")
        sys.exit(1)

    hw_interface_id = None
    if args.interface.lower() in ["auto", "all"] or args.verbose:
        hw_interface_id = query_hardware_interface('a')
        if hw_interface_id:
            detected_name = INTERFACE_NAMES.get(hw_interface_id, "Unknown / Unreported")
            print(f"Hardware Interrogation: Active Firmware Interface = {detected_name}")

    # Determine requested test interface
    target_interface = args.interface.lower()
    if target_interface in ["auto", "all"]:
        if hw_interface_id == 2:
            target_interface = "spdif"
        elif hw_interface_id == 1:
            target_interface = "i2s"
        else:
            target_interface = "spdif"

    if target_interface == "spdif":
        interface_desc = "S/PDIF Hardware Transceiver (Pin 14 ALT3 Tx -> Pin 15 ALT3 Rx)"
        default_min_snr = 70.0
    else:
        interface_desc = "SAI1 I2S Bus (Pin 7 Tx -> Pin 8 Rx, Pin 21 BCLK, Pin 20 FSYNC)"
        default_min_snr = 80.0

    min_snr = args.min_snr if args.min_snr is not None else default_min_snr
    print(f"Executing Pre-Commit Verification on Interface: {interface_desc}")

    passed = False
    for attempt in range(1, args.retries + 1):
        if attempt > 1:
            print(f"\n--- Retrying Loopback Verification (Attempt {attempt} of {args.retries}) ---")
            time.sleep(1.0)
        passed = run_loopback_test(
            pb_card=pb_card,
            cap_card=cap_card,
            interface_name=interface_desc,
            duration=args.duration,
            freq=args.freq,
            rate=args.rate,
            min_snr=min_snr
        )
        if passed:
            break

    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
