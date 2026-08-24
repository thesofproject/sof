#!/usr/bin/env python3
"""
VAD gate calibration for spider (TGL/CAVS2.5).

Reads live energy from firmware via vad_gate_status_100 kcontrol,
and writes threshold via vad_gate_cfg_100 kcontrol.

Must be run WHILE a WOV compress stream is active (use_count > 0),
otherwise the IPC4 to firmware is silently dropped.

Usage:
  # Terminal 1 – start WOV stream in background:
  #   wov_capture -d comprC0D11 -b 65536 -o /dev/null &
  # Wait ~9s for detect_test to fire, then:
  # Terminal 2 – run this script:
  #   python3 vad_calibrate.py [--threshold N] [--read-only]
"""
import argparse
import ctypes
import fcntl
import os
import struct
import sys
import time

CARD = "/dev/snd/controlC0"

CFG_NUMID    = 8    # vad_gate_cfg_100    (R/W config)
STATUS_NUMID = 9    # vad_gate_status_100 (volatile RO status)

TLV_READ_IOC  = 0xC008551A   # SNDRV_CTL_IOCTL_TLV_READ  (_IOWR 'U' 0x1a)
TLV_WRITE_IOC = 0xC008551B   # SNDRV_CTL_IOCTL_TLV_WRITE (_IOWR 'U' 0x1b)

SOF_CTRL_CMD_BINARY = 3
SOF_IPC4_ABI_MAGIC  = 0x34464F53   # "SOF4"
PARAM_SET_CFG    = 1   # IPC4_VAD_GATE_SET_CONFIG
PARAM_GET_STATUS = 2   # IPC4_VAD_GATE_GET_STATUS
ABI_HDR_SIZE = 32
CFG_SIZE     = 12   # sizeof(ipc4_vad_gate_config)
STATUS_SIZE  =  8   # sizeof(ipc4_vad_gate_status)


def _open():
    return os.open(CARD, os.O_RDWR)


def read_status():
    """Query firmware for current energy + vad_active via volatile RO kcontrol.

    Buffer layout (56 bytes total):
      [0..7]   outer snd_ctl_tlv: numid=STATUS_NUMID, length=48  (request)
      [8..15]  inner snd_ctl_tlv: numid=3, length=40             (filled by kernel)
      [16..47] sof_abi_hdr: magic, type=2, size=8, abi=0, reserved  (filled by kernel)
      [48..55] ipc4_vad_gate_status: energy(4) vad_active(1) pad(3) (from firmware)
    """
    inner_payload = 8 + ABI_HDR_SIZE + STATUS_SIZE  # inner_hdr + abi_hdr + status = 48
    buf = ctypes.create_string_buffer(
        struct.pack("<II", STATUS_NUMID, inner_payload) + bytes(inner_payload))
    fd = _open()
    try:
        fcntl.ioctl(fd, TLV_READ_IOC, buf)
    except OSError as e:
        print(f"[read_status] ioctl error: {e}", file=sys.stderr)
        return None, None
    finally:
        os.close(fd)
    # ABI header starts at offset 16 (outer_hdr=8 + inner_hdr=8)
    magic = struct.unpack_from("<I", buf, 16)[0]
    if magic != SOF_IPC4_ABI_MAGIC:
        print(f"[read_status] bad magic 0x{magic:08x}", file=sys.stderr)
        return None, None
    # Status payload at offset 48 (16 + ABI_HDR_SIZE=32)
    energy, vad_active = struct.unpack_from("<IB", buf, 16 + ABI_HDR_SIZE)
    return energy, bool(vad_active)


def read_config():
    """Read current vad_gate_config from kernel-cached kcontrol data.

    Buffer layout (60 bytes total):
      [0..7]   outer snd_ctl_tlv: numid=CFG_NUMID, length=52   (request)
      [8..15]  inner snd_ctl_tlv: numid=3, length=44           (filled by kernel)
      [16..47] sof_abi_hdr: magic, type=1, size=12, ...        (from cached cdata)
      [48..59] ipc4_vad_gate_config: threshold(4) onset(2) hangover(2) shift(1) pad(3)
    """
    inner_payload = 8 + ABI_HDR_SIZE + CFG_SIZE  # 8+32+12 = 52
    buf = ctypes.create_string_buffer(
        struct.pack("<II", CFG_NUMID, inner_payload) + bytes(inner_payload))
    fd = _open()
    try:
        fcntl.ioctl(fd, TLV_READ_IOC, buf)
    except OSError as e:
        print(f"[read_config] ioctl error: {e}", file=sys.stderr)
        return None
    finally:
        os.close(fd)
    magic = struct.unpack_from("<I", buf, 16)[0]
    if magic != SOF_IPC4_ABI_MAGIC:
        print(f"[read_config] bad magic 0x{magic:08x}", file=sys.stderr)
        return None
    threshold, onset, hangover, shift = struct.unpack_from("<iHHB", buf,
                                                            16 + ABI_HDR_SIZE)
    return {"threshold": threshold, "onset": onset,
            "hangover": hangover, "shift": shift}


def write_config(threshold, onset=5, hangover=100, shift=0):
    """Write vad_gate_config to firmware via TLV_WRITE (must be streaming)."""
    payload  = struct.pack("<iHHBxxx", threshold, onset, hangover, shift)
    abi_hdr  = struct.pack("<IIII16s", SOF_IPC4_ABI_MAGIC, PARAM_SET_CFG,
                           len(payload), 0, bytes(16))
    inner_data = abi_hdr + payload                          # 32+12 = 44 B
    inner_tlv  = struct.pack("<II", SOF_CTRL_CMD_BINARY,
                              len(inner_data)) + inner_data # 8+44 = 52 B
    outer_tlv  = struct.pack("<II", CFG_NUMID,
                              len(inner_tlv)) + inner_tlv   # 8+52 = 60 B
    buf = ctypes.create_string_buffer(bytes(outer_tlv))
    fd = _open()
    try:
        fcntl.ioctl(fd, TLV_WRITE_IOC, buf)   # 0xC008551B — the WRITE code
        print(f"  wrote: threshold={threshold} onset={onset} "
              f"hangover={hangover} shift={shift}")
        return True
    except OSError as e:
        print(f"  write error: {e}", file=sys.stderr)
        return False
    finally:
        os.close(fd)


def monitor(count=20, interval=0.5):
    """Sample energy repeatedly and print, to calibrate threshold."""
    print(f"{'time':>6}  {'energy':>10}  {'active':>6}")
    t0 = time.time()
    for _ in range(count):
        e, a = read_status()
        if e is None:
            print(f"{time.time()-t0:6.1f}  {'(error)':>10}  {'?':>6}")
        else:
            print(f"{time.time()-t0:6.1f}  {e:>10}  {str(a):>6}")
        time.sleep(interval)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--monitor", "-m", action="store_true",
                    help="Sample energy every 0.5s (20 samples)")
    ap.add_argument("--threshold", "-t", type=int, default=None,
                    help="Write this threshold to firmware")
    ap.add_argument("--onset",    type=int, default=5)
    ap.add_argument("--hangover", type=int, default=100)
    ap.add_argument("--shift",    type=int, default=0)
    ap.add_argument("--read-config", "-r", action="store_true",
                    help="Print cached kcontrol config")
    args = ap.parse_args()

    if args.read_config:
        cfg = read_config()
        if cfg:
            print(f"cached config: {cfg}")
        else:
            print("could not read config")

    if args.monitor:
        monitor()

    if args.threshold is not None:
        write_config(args.threshold, args.onset, args.hangover, args.shift)
        time.sleep(0.2)
        e, a = read_status()
        if e is not None:
            print(f"  energy after write: {e}  vad_active={a}")

    if not (args.monitor or args.threshold or args.read_config):
        # Default: single status read
        e, a = read_status()
        if e is not None:
            print(f"energy={e}  vad_active={a}")
        else:
            print("Error: pipeline may not be active (need streaming use_count>0)")


if __name__ == "__main__":
    main()
