#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Teensy 4.1 USB Flasher Tool

Supports:
- Distinguishing multiple Teensy boards by serial number or USB path
- Automatic soft-reboot from Teensyduino (RawHID / SerEmu / CDC-ACM) to HalfKay bootloader
- Parsing standard Zephyr Intel HEX files (mapping 0x60000000 -> 0x00000000)
- Programming flash via HalfKay USB HID SET_REPORT control transfers
- Booting the newly flashed firmware
"""

import sys
import time
import argparse
import usb.core
import usb.util

TEENSY_VID = 0x16C0
HALFKAY_PID = 0x0478
RAWHID_PID = 0x0486
CDC_PID = 0x0483
SEREMU_PID = 0x0487

BLOCK_SIZE = 1024
FLASH_SIZE = 8126464  # Teensy 4.1 8MB flash (minus HalfKay area)


def parse_intel_hex(hex_path):
    """
    Parses an Intel HEX file and returns a bytearray of firmware data
    mapped to flash offset 0.
    """
    image = bytearray(b'\xFF' * FLASH_SIZE)
    max_addr = 0
    ext_addr = 0

    with open(hex_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line.startswith(':'):
                continue
            length = int(line[1:3], 16)
            addr = int(line[3:7], 16)
            rtype = int(line[7:9], 16)
            data_hex = line[9:9 + length * 2]

            if rtype == 0:  # Data Record
                full_addr = ext_addr + addr
                if full_addr >= 0x60000000 and full_addr < 0x60000000 + FLASH_SIZE:
                    flash_addr = full_addr - 0x60000000
                elif full_addr < FLASH_SIZE:
                    flash_addr = full_addr
                else:
                    continue  # Ignore addresses outside flash

                for i, byte in enumerate(bytes.fromhex(data_hex)):
                    if flash_addr + i < FLASH_SIZE:
                        image[flash_addr + i] = byte
                        if flash_addr + i > max_addr:
                            max_addr = flash_addr + i

            elif rtype == 1:  # EOF
                break
            elif rtype == 2:  # Extended Segment Address
                ext_addr = int(data_hex, 16) << 4
            elif rtype == 4:  # Extended Linear Address
                ext_addr = int(data_hex, 16) << 16

    print(f"Loaded HEX '{hex_path}': {max_addr + 1} bytes (up to block {(max_addr // BLOCK_SIZE) + 1})")
    return image, max_addr + 1


def send_soft_reboot(dev):
    """Sends soft reboot command to Teensy running Teensyduino firmware."""
    for iface in [0, 1]:
        try:
            if dev.is_kernel_driver_active(iface):
                dev.detach_kernel_driver(iface)
        except Exception:
            pass

    try:
        # SerEmu reboot token (Teensy 4.0 / 4.1 RawHID interface 1 feature report 0)
        reboot_token = bytes([0xA9, 0x45, 0xC2, 0x6B])
        dev.ctrl_transfer(0x21, 0x09, 0x0300, 1, reboot_token, timeout=200)
        print("Sent SerEmu reboot token (0x0300, iface 1)")
        return True
    except Exception:
        pass

    try:
        # CDC-ACM line coding reboot (134 baud)
        reboot_cdc = bytes([0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08])  # 134 bps
        dev.ctrl_transfer(0x21, 0x20, 0, 0, reboot_cdc, timeout=200)
        print("Sent CDC 134 baud reboot request")
        return True
    except Exception:
        pass

    try:
        # SOF Teensy vendor reboot request (0xFB)
        dev.ctrl_transfer(0x40, 0xFB, 0, 0, None, timeout=200)
        print("Sent SOF Teensy vendor reboot request (0xFB)")
        return True
    except Exception:
        pass

    return False


def find_teensy(target_serial=None, target_port=None, bootloader_only=False):
    """Finds matching Teensy device."""
    devices = list(usb.core.find(find_all=True, idVendor=TEENSY_VID))
    matches = []
    for d in devices:
        try:
            s = d.serial_number
        except Exception:
            s = None
        port = (d.bus, tuple(d.port_numbers) if d.port_numbers else None)

        if d.idProduct == HALFKAY_PID:
            matches.append(('halfkay', d, s, port))
        elif not bootloader_only:
            matches.append(('firmware', d, s, port))

    if target_port:
        filtered = [m for m in matches if m[3] == target_port]
        return filtered[0] if filtered else None

    if target_serial:
        filtered = [m for m in matches if m[2] == str(target_serial)]
        return filtered[0] if filtered else None

    if matches:
        return matches[0]
    return None


KNOWN_BOARDS = {
    '17854640': (3, (12, 4, 1, 3)),
    '17861730': (3, (12, 4, 1, 2)),
    'a': (3, (12, 4, 1, 3)),
    'b': (3, (12, 4, 1, 2)),
    'teensya': (3, (12, 4, 1, 3)),
    'teensyb': (3, (12, 4, 1, 2)),
}


def write_block(dev, buf, timeout_sec=2.0):
    start = time.time()
    while time.time() - start < timeout_sec:
        try:
            ret = dev.ctrl_transfer(0x21, 9, 0x0200, 0, buf, timeout=1000)
            if ret == len(buf):
                return True
        except usb.core.USBError:
            time.sleep(0.01)
    return False


def program_device(dev, image, total_bytes):
    """Programs a Teensy 4.1 device currently in HalfKay bootloader mode."""
    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
    except Exception:
        pass

    num_blocks = (total_bytes + BLOCK_SIZE - 1) // BLOCK_SIZE
    buf = bytearray(1088)

    for b in range(num_blocks):
        addr = b * BLOCK_SIZE
        chunk = image[addr:addr + BLOCK_SIZE]

        if b > 0 and all(x == 0xFF for x in chunk):
            continue

        buf[0] = addr & 0xFF
        buf[1] = (addr >> 8) & 0xFF
        buf[2] = (addr >> 16) & 0xFF
        buf[3:64] = b'\x00' * 61
        buf[64:64 + BLOCK_SIZE] = chunk

        timeout_sec = 45.0 if b <= 4 else 2.0
        if not write_block(dev, buf, timeout_sec):
            raise IOError(f"Failed to write block {b} at addr 0x{addr:08x} after {timeout_sec}s")

        if (b % 32 == 0) or (b == num_blocks - 1):
            pct = (b + 1) / num_blocks * 100.0
            print(f"Programming: block {b + 1}/{num_blocks} ({pct:.1f}%)")

    print("Programming complete. Sending reboot token...")
    buf[0] = 0xFF
    buf[1] = 0xFF
    buf[2] = 0xFF
    buf[3:] = b'\x00' * (1088 - 3)
    time.sleep(0.1)
    if not write_block(dev, buf, timeout_sec=5.0):
        print("Warning: Reboot token transfer timed out")
    else:
        print("Teensy 4.1 rebooted into new firmware successfully!")


def flash_teensy(hex_path, target_serial=None, timeout=30):
    """Flashes an Intel HEX firmware image to Teensy 4.1."""
    image, total_bytes = parse_intel_hex(hex_path)

    print(f"Searching for Teensy (Target Serial: {target_serial or 'any'})...")
    start = time.time()
    dev_entry = None
    target_port = None
    if target_serial and str(target_serial).lower() in KNOWN_BOARDS:
        target_port = KNOWN_BOARDS[str(target_serial).lower()]
        print(f"Target serial '{target_serial}' mapped to USB port {target_port}")

    while time.time() - start < timeout:
        dev_entry = find_teensy(target_serial=target_serial, target_port=target_port, bootloader_only=False)
        if dev_entry:
            mode, dev, serial, port = dev_entry
            target_port = port
            print(f"Found Teensy in '{mode}' mode (Serial: {serial}, Port: {port}, VID:PID: {dev.idVendor:04x}:{dev.idProduct:04x})")
            if mode == 'firmware':
                print("Triggering soft reboot into HalfKay bootloader...")
                send_soft_reboot(dev)
                time.sleep(1.0)
                continue
            elif mode == 'halfkay':
                break
        time.sleep(0.5)

    if not dev_entry or dev_entry[0] != 'halfkay':
        raise RuntimeError(
            "Could not connect to Teensy HalfKay bootloader. "
            "Please press the pushbutton on the Teensy board."
        )

    mode, dev, serial, port = dev_entry
    print("Attached to HalfKay bootloader. Starting programming...")
    program_device(dev, image, total_bytes)


def auto_flash(timeout=600):
    """Monitors USB bus and flashes whichever Teensy board enters HalfKay."""
    hex_a = "build-teensya/zephyr/zephyr.hex"
    hex_b = "build-teensyb/zephyr/zephyr.hex"
    img_a, bytes_a = parse_intel_hex(hex_a)
    img_b, bytes_b = parse_intel_hex(hex_b)

    flashed = set()
    start = time.time()
    last_reboot = 0
    print("=== Auto-Flasher Active ===")
    print(f"Waiting up to {timeout}s for Teensy boards to enter HalfKay...")
    print("Press the pushbutton on Board A (Port 12.4.1.3) or Board B (Port 12.4.1.2) at any time.")

    while time.time() - start < timeout:
        try:
            # Check for HalfKay bootloader devices
            devices = list(usb.core.find(find_all=True, idVendor=TEENSY_VID, idProduct=HALFKAY_PID))
            for dev in devices:
                port = (dev.bus, tuple(dev.port_numbers) if dev.port_numbers else None)
                if port == (3, (12, 4, 1, 3)) and 'A' not in flashed:
                    print(f"\n[AUTO-FLASH] Detected Board A in HalfKay on port {port}!", flush=True)
                    program_device(dev, img_a, bytes_a)
                    flashed.add('A')
                    time.sleep(2.0)
                elif port == (3, (12, 4, 1, 2)) and 'B' not in flashed:
                    print(f"\n[AUTO-FLASH] Detected Board B in HalfKay on port {port}!", flush=True)
                    program_device(dev, img_b, bytes_b)
                    flashed.add('B')
                    time.sleep(2.0)
                elif port not in [(3, (12, 4, 1, 3)), (3, (12, 4, 1, 2))]:
                    print(f"\n[AUTO-FLASH] Detected Teensy HalfKay on port {port} (VID:PID {dev.idVendor:04x}:{dev.idProduct:04x})", flush=True)

            if len(flashed) == 2:
                print("\n===> BOTH BOARDS FLASHED SUCCESSFULLY! <===", flush=True)
                return True

            # Try soft-rebooting any responsive firmware devices every 5 seconds
            now = time.time()
            if now - last_reboot > 5.0:
                last_reboot = now
                fw_devices = list(usb.core.find(find_all=True, idVendor=TEENSY_VID))
                for dev in fw_devices:
                    if dev.idProduct != HALFKAY_PID:
                        port = (dev.bus, tuple(dev.port_numbers) if dev.port_numbers else None)
                        target = 'A' if port == (3, (12, 4, 1, 3)) else ('B' if port == (3, (12, 4, 1, 2)) else None)
                        if target and target not in flashed:
                            send_soft_reboot(dev)

        except Exception as e:
            pass

        time.sleep(0.5)

    print(f"Auto-flasher timed out after {timeout}s (flashed: {flashed})", flush=True)
    return len(flashed) > 0


def main():
    parser = argparse.ArgumentParser(description="Teensy 4.1 USB Flasher")
    parser.add_argument("--hex", help="Path to Intel HEX file")
    parser.add_argument("--serial", help="Target device serial number (e.g. 17854640, a, b)")
    parser.add_argument("--timeout", type=int, default=30, help="Wait timeout in seconds")
    parser.add_argument("--auto", action="store_true", help="Auto-flash daemon for both boards")
    args = parser.parse_args()

    if args.auto:
        auto_flash(timeout=args.timeout if args.timeout != 30 else 600)
    elif args.hex:
        flash_teensy(args.hex, target_serial=args.serial, timeout=args.timeout)
    else:
        parser.error("Either --hex or --auto is required.")


if __name__ == "__main__":
    main()
