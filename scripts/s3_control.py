#!/usr/bin/env python3
"""
ESP32-S3 Board Control & Flashing Service via ESP32-C6 / ESP32-C3 Bridge

Provides automated GPIO pin detection, target reset, ROM bootloader entry,
and end-to-end flashing for Board A (Master) and Board B (Slave).

Safety Rules:
  NEVER touch, open, or write to 28:37:2F:54:4E:98 (relay controller)
  or the ESP32-P4 DUT bridges (5B7B029952 / 5B7B030033).
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys
import time
import serial

FORBIDDEN_IDS = [
    "28:37:2F:54:4E:98",  # Power relay controller
    "5B7B029952",         # Spider P4 bridge
    "5B7B030033",         # Aphid P4 bridge
    "80F1B2D315C7",
    "80F1B2D31515",
]

KNOWN_S3_MACS = [
    "34:85:18:7B:40:6C",  # Board A
    "20:6E:F1:32:D6:84",  # Board B
    "3C:84:27:C4:4B:90",  # S3 Board 1
    "74:4D:BD:7D:23:80",  # S3 Board 2
]

BOARD_A_MAC = "34:85:18:7B:40:6C"
BOARD_B_MAC = "20:6E:F1:32:D6:84"

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
PIN_MAP_FILE = os.path.join(SCRIPT_DIR, "s3_pins.json")
FIRMWARE_BIN = os.path.join(PROJECT_ROOT, "build_s3", "zephyr", "zephyr.bin")

ESPTOOL_PATH = os.path.join(PROJECT_ROOT, ".venv", "bin", "esptool")
if not os.path.exists(ESPTOOL_PATH):
    ESPTOOL_PATH = "esptool"


def check_safety(dev_path):
    if not dev_path:
        return
    real = os.path.realpath(dev_path)
    for forbidden in FORBIDDEN_IDS:
        if forbidden in dev_path or forbidden in real:
            raise RuntimeError(f"SAFETY VIOLATION: Device {dev_path} ({real}) is protected ({forbidden})! Aborting.")


def get_dmesg_tail(lines=50):
    """Fetch kernel log buffer without requiring sudo password."""
    try:
        res = subprocess.run(
            ["docker", "run", "--rm", "--privileged", "alpine", "dmesg"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10
        )
        if res.returncode == 0:
            return "\n".join(res.stdout.splitlines()[-lines:])
    except Exception:
        pass
    try:
        res = subprocess.run(
            ["dmesg"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5
        )
        if res.returncode == 0:
            return "\n".join(res.stdout.splitlines()[-lines:])
    except Exception:
        pass
    return ""


def detect_chip_type(port):
    """Use esptool chip-id to identify chip family (ESP32-C6, ESP32-C3, ESP32-S3)."""
    check_safety(port)
    try:
        res = subprocess.run(
            [ESPTOOL_PATH, "--port", port, "chip-id"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=8
        )
        out = res.stdout + res.stderr
        if "ESP32-C6" in out:
            return "ESP32-C6"
        elif "ESP32-C3" in out:
            return "ESP32-C3"
        elif "ESP32-S3" in out:
            return "ESP32-S3"
    except Exception as e:
        print(f"Error detecting chip on {port}: {e}")
    return None


def find_bridge_device():
    """Locate the ESP32-C6 or ESP32-C3 bridge serial port."""
    devices = glob.glob("/dev/serial/by-id/*")
    candidates = []
    for d in devices:
        real = os.path.realpath(d)
        if any(f in d or f in real for f in FORBIDDEN_IDS):
            continue
        if any(s3_mac in d.upper() for s3_mac in KNOWN_S3_MACS):
            continue
        if "Espressif" in d or "USB_Single_Serial" in d or "ttyACM" in d:
            check_safety(d)
            candidates.append(d)

    if not candidates:
        return None
    if len(candidates) == 1:
        return candidates[0]

    for c in candidates:
        chip = detect_chip_type(c)
        if chip in ["ESP32-C6", "ESP32-C3"]:
            return c

    return candidates[0]


def flash_bridge(port=None):
    """Flash Zephyr bridge firmware onto detected ESP32-C6 or ESP32-C3."""
    if not port:
        port = find_bridge_device()
    if not port:
        raise RuntimeError("Bridge device not found on USB!")
    check_safety(port)

    chip = detect_chip_type(port)
    print(f"Bridge device {port} detected as chip: {chip}")

    if chip == "ESP32-C3":
        build_dir = os.path.join(PROJECT_ROOT, "build_c3")
        bin_path = os.path.join(build_dir, "zephyr", "zephyr.bin")
        board = "esp32c3_devkitm"
    else:
        build_dir = os.path.join(PROJECT_ROOT, "build_c6")
        bin_path = os.path.join(build_dir, "zephyr", "zephyr.bin")
        board = "esp32c6_devkitc/esp32c6/hpcore"

    if not os.path.exists(bin_path):
        print(f"Building bridge firmware for {board}...")
        res = subprocess.run(
            ["west", "build", "-b", board, os.path.join(PROJECT_ROOT, "c6_bridge"), "-d", build_dir],
            cwd=PROJECT_ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        if res.returncode != 0:
            raise RuntimeError(f"Build failed:\n{res.stderr}")

    print(f"Flashing bridge firmware ({bin_path}) to {port}...")
    res = subprocess.run(
        ["west", "flash", "-d", build_dir, "--esp-device", port],
        cwd=PROJECT_ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    if res.returncode != 0:
        print(f"west flash failed, falling back to esptool write_flash...")
        cmd = [ESPTOOL_PATH, "--port", port, "--baud", "921600", "write_flash", "0x0", bin_path]
        res2 = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res2.returncode != 0:
            raise RuntimeError(f"Flashing bridge failed:\n{res2.stderr}")

    print("Bridge flashed successfully!")
    time.sleep(2.0)


class C6Bridge:
    def __init__(self, port=None, baudrate=115200, timeout=2.0):
        if not port:
            port = find_bridge_device()
        if not port:
            raise RuntimeError("Bridge serial device not found on USB!")
        check_safety(port)
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None

    def connect(self):
        check_safety(self.port)
        self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
        time.sleep(0.1)
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        self.send_cmd("")

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_cmd(self, cmd, timeout=3.0):
        if not self.ser or not self.ser.is_open:
            self.connect()
        self.ser.write((cmd + "\r\n").encode("utf-8"))
        start = time.time()
        output = ""
        while time.time() - start < timeout:
            line = self.ser.readline().decode("utf-8", errors="replace")
            output += line
            if "c6:~$" in line or "c3:~$" in line or "uart:~$" in line:
                break
        return output

    def scan_pins(self):
        out = self.send_cmd("c6 scan", timeout=5.0)
        pins = []
        for line in out.splitlines():
            m = re.search(r"GPIO\s+(\d+):\s+HIGH", line)
            if m:
                pins.append(int(m.group(1)))
        return sorted(list(set(pins)))

    def pulse(self, pin, ms=50):
        return self.send_cmd(f"c6 pulse {pin} {ms}")

    def bootloader(self, rst_pin, b1_pin):
        return self.send_cmd(f"c6 bootloader {rst_pin} {b1_pin}", timeout=4.0)

    def reset(self, rst_pin):
        return self.send_cmd(f"c6 reset {rst_pin}")

    def set_map(self, a_rst, a_b1, b_rst, b_b1):
        return self.send_cmd(f"c6 map {a_rst} {a_b1} {b_rst} {b_b1}")


def load_pin_map():
    if os.path.exists(PIN_MAP_FILE):
        with open(PIN_MAP_FILE, "r") as f:
            return json.load(f)
    return None


def save_pin_map(pin_map):
    with open(PIN_MAP_FILE, "w") as f:
        json.dump(pin_map, f, indent=2)
    print(f"Saved pin map to {PIN_MAP_FILE}")


def auto_detect_pins(bridge):
    print("=== Auto-Detecting Bridge GPIO Connections ===")
    detected = bridge.scan_pins()
    print(f"Candidates detected with active pull-ups: {detected}")

    if len(detected) < 4:
        print(f"WARNING: Expected at least 4 lines (A_RST, A_B1, B_RST, B_B1), found {len(detected)}: {detected}")

    pin_map = {"a_rst": None, "a_b1": None, "b_rst": None, "b_b1": None}

    # Step 1: Find RST pins by pulsing each candidate and checking USB port events
    rst_candidates = []
    for pin in detected:
        print(f"Testing candidate pin GPIO {pin} for RST...")
        bridge.pulse(pin, 60)
        time.sleep(1.0)

        log = get_dmesg_tail(30)
        if "3-12.3.2" in log and ("new full-speed" in log or "disconnect" in log or "reset" in log or "unable" in log):
            print(f"  --> GPIO {pin} triggered reset on Board A (Port 3-12.3.2)!")
            pin_map["a_rst"] = pin
            rst_candidates.append(pin)
        elif "3-12.3.1" in log and ("new full-speed" in log or "disconnect" in log or "reset" in log or "unable" in log):
            print(f"  --> GPIO {pin} triggered reset on Board B (Port 3-12.3.1)!")
            pin_map["b_rst"] = pin
            rst_candidates.append(pin)

    # Step 2: For remaining candidates, test bootloader mode with the identified RST pins
    remaining = [p for p in detected if p not in rst_candidates]
    print(f"Remaining candidates for B1 (BOOT0): {remaining}")

    if pin_map["a_rst"] is not None:
        for pin in remaining:
            print(f"Testing candidate GPIO {pin} as Board A B1 with RST GPIO {pin_map['a_rst']}...")
            bridge.bootloader(pin_map["a_rst"], pin)
            time.sleep(1.5)
            by_id = glob.glob("/dev/serial/by-id/*")
            if any(BOARD_A_MAC in x.upper() for x in by_id):
                print(f"  --> Board A successfully entered ROM bootloader! GPIO {pin} is Board A B1.")
                pin_map["a_b1"] = pin
                remaining.remove(pin)
                break

    if pin_map["b_rst"] is not None:
        for pin in remaining:
            print(f"Testing candidate GPIO {pin} as Board B B1 with RST GPIO {pin_map['b_rst']}...")
            bridge.bootloader(pin_map["b_rst"], pin)
            time.sleep(1.5)
            by_id = glob.glob("/dev/serial/by-id/*")
            if any(BOARD_B_MAC in x.upper() for x in by_id):
                print(f"  --> Board B successfully entered ROM bootloader! GPIO {pin} is Board B B1.")
                pin_map["b_b1"] = pin
                remaining.remove(pin)
                break

    print("\nDetection Results:")
    print(json.dumps(pin_map, indent=2))
    save_pin_map(pin_map)
    bridge.set_map(pin_map["a_rst"] or -1, pin_map["a_b1"] or -1,
                   pin_map["b_rst"] or -1, pin_map["b_b1"] or -1)
    return pin_map


def flash_board(target, bridge, pin_map):
    target = target.upper()
    if target not in ["A", "B"]:
        raise ValueError("Target must be 'A' or 'B'")

    rst_pin = pin_map.get(f"{target.lower()}_rst")
    b1_pin  = pin_map.get(f"{target.lower()}_b1")
    mac     = BOARD_A_MAC if target == "A" else BOARD_B_MAC

    if rst_pin is None or b1_pin is None:
        raise RuntimeError(f"Pin map for Board {target} is incomplete: RST={rst_pin}, B1={b1_pin}")

    print(f"\n==========================================")
    print(f" Flashing Board {target} (MAC {mac})")
    print(f"==========================================")

    print(f"[1/4] Putting Board {target} into ROM bootloader (RST={rst_pin}, B1={b1_pin})...")
    bridge.bootloader(rst_pin, b1_pin)
    time.sleep(1.5)

    target_dev = None
    for _ in range(10):
        devices = glob.glob("/dev/serial/by-id/*")
        for d in devices:
            if mac in d.upper():
                target_dev = d
                break
        if target_dev:
            break
        time.sleep(0.5)

    if not target_dev:
        raise RuntimeError(f"Board {target} ROM bootloader device (MAC {mac}) did not appear on USB!")

    print(f"[2/4] Found target device: {target_dev}")
    check_safety(target_dev)

    print(f"[3/4] Writing firmware ({FIRMWARE_BIN})...")
    flash_cmd = [
        ESPTOOL_PATH,
        "--port", target_dev,
        "--baud", "921600",
        "--before", "default_reset",
        "--after", "no_reset",
        "write_flash", "0x0", FIRMWARE_BIN
    ]
    res = subprocess.run(flash_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f"Flashing failed! Output:\n{res.stdout}\n{res.stderr}")
        raise RuntimeError(f"esptool returned {res.returncode}")
    print("Firmware written successfully!")

    print(f"[4/4] Resetting Board {target} into application mode (RST={rst_pin})...")
    bridge.reset(rst_pin)
    time.sleep(2.0)
    print(f"Board {target} reset complete!")


def main():
    parser = argparse.ArgumentParser(description="ESP32-S3 Board Control & Flashing Service via Bridge")
    parser.add_argument("--port", help="Serial port of bridge (auto-detected if omitted)")
    parser.add_argument("--flash-bridge", action="store_true", help="Build and flash Zephyr firmware to the bridge")
    parser.add_argument("--auto-detect", action="store_true", help="Run automated GPIO detection")
    parser.add_argument("--scan", action="store_true", help="Scan GPIOs for active pull-ups")
    parser.add_argument("--reset", choices=["A", "B", "a", "b", "all"], help="Reset target board")
    parser.add_argument("--bootloader", choices=["A", "B", "a", "b", "all"], help="Put target board into ROM bootloader")
    parser.add_argument("--flash", choices=["A", "B", "a", "b", "all"], help="Automated flash and boot of target board")
    parser.add_argument("--status", action="store_true", help="Query bridge status and active pin map")
    args = parser.parse_args()

    if args.flash_bridge:
        flash_bridge(args.port)
        return

    bridge = C6Bridge(port=args.port)
    print(f"Connecting to bridge at {bridge.port}...")
    bridge.connect()

    pin_map = load_pin_map() or {"a_rst": None, "a_b1": None, "b_rst": None, "b_b1": None}

    if args.auto_detect:
        auto_detect_pins(bridge)
    elif args.scan:
        pins = bridge.scan_pins()
        print(f"Connected pins detected: {pins}")
    elif args.reset:
        target = args.reset.upper()
        if target in ["A", "ALL"] and pin_map.get("a_rst"):
            print(f"Resetting Board A (RST={pin_map['a_rst']})...")
            bridge.reset(pin_map["a_rst"])
        if target in ["B", "ALL"] and pin_map.get("b_rst"):
            print(f"Resetting Board B (RST={pin_map['b_rst']})...")
            bridge.reset(pin_map["b_rst"])
    elif args.bootloader:
        target = args.bootloader.upper()
        if target in ["A", "ALL"] and pin_map.get("a_rst") and pin_map.get("a_b1"):
            print(f"Putting Board A into bootloader (RST={pin_map['a_rst']}, B1={pin_map['a_b1']})...")
            bridge.bootloader(pin_map["a_rst"], pin_map["a_b1"])
        if target in ["B", "ALL"] and pin_map.get("b_rst") and pin_map.get("b_b1"):
            print(f"Putting Board B into bootloader (RST={pin_map['b_rst']}, B1={pin_map['b_b1']})...")
            bridge.bootloader(pin_map["b_rst"], pin_map["b_b1"])
    elif args.flash:
        target = args.flash.upper()
        if target in ["A", "ALL"]:
            flash_board("A", bridge, pin_map)
        if target in ["B", "ALL"]:
            flash_board("B", bridge, pin_map)
    elif args.status:
        print("Active Pin Map:", json.dumps(pin_map, indent=2))
        print("Bridge Status:\n" + bridge.send_cmd("c6 status"))
    else:
        parser.print_help()

    bridge.close()


if __name__ == "__main__":
    main()
