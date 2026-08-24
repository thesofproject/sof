#!/usr/bin/env python3
"""
WOV multi-cycle re-arm daemon for spider (TGL/CAVS2.5, pipeline 100 VAD).

Keeps comprC0D11 open throughout (no pipeline close/reopen for re-arm).
Polls vad_gate_status_100 kcontrol to detect silence; drains and re-arms
firmware by waiting for KPBs to empty after VAD silence.

Usage:
  python3 wov_daemon.py [--cycles N] [--out FILE] [--threshold THR]

  --cycles N      : number of WOV events to capture (default: unlimited)
  --out FILE      : output file for PCM (default: /tmp/wov_daemon.raw)
  --threshold THR : VAD threshold to write at startup (e.g. 300000000)
  --compress N    : compress PCM device number (default: 11)
  --card N        : sound card number (default: 0)
"""
import argparse
import ctypes
import fcntl
import os
import struct
import subprocess
import sys
import tempfile
import threading
import time

CARD_PATH = "/dev/snd/controlC{card}"

CFG_NUMID    = 8    # vad_gate_cfg_100
STATUS_NUMID = 9    # vad_gate_status_100

TLV_READ_IOC  = 0xC008551A
TLV_WRITE_IOC = 0xC008551B

SOF_CTRL_CMD_BINARY = 3
SOF_IPC4_ABI_MAGIC  = 0x34464F53
PARAM_SET_CFG = 1
ABI_HDR_SIZE  = 32
CFG_SIZE      = 12
STATUS_SIZE   =  8

POLL_INTERVAL_S = 0.2    # kcontrol poll rate while streaming
DRAIN_IDLE_S    = 0.5    # declare drain done after this many seconds of no new data
DRAIN_TIMEOUT_S = 5.0    # max seconds waiting for KPB drain after VAD silence
FRAGMENT_BYTES  = 65536
IDLE_TIMEOUT_MS = 3000   # wov_capture -i value


def _ctl_open(card):
    return os.open(CARD_PATH.format(card=card), os.O_RDWR)


def read_status(card):
    inner_payload = 8 + ABI_HDR_SIZE + STATUS_SIZE
    buf = ctypes.create_string_buffer(
        struct.pack("<II", STATUS_NUMID, inner_payload) + bytes(inner_payload))
    fd = _ctl_open(card)
    try:
        fcntl.ioctl(fd, TLV_READ_IOC, buf)
    except OSError:
        return None, None
    finally:
        os.close(fd)
    magic = struct.unpack_from("<I", buf, 16)[0]
    if magic != SOF_IPC4_ABI_MAGIC:
        return None, None
    energy, vad_active = struct.unpack_from("<IB", buf, 16 + ABI_HDR_SIZE)
    return energy, bool(vad_active)


def write_threshold(card, threshold, onset=5, hangover=100, shift=0):
    payload    = struct.pack("<iHHBxxx", threshold, onset, hangover, shift)
    abi_hdr    = struct.pack("<IIII16s", SOF_IPC4_ABI_MAGIC, PARAM_SET_CFG,
                             len(payload), 0, bytes(16))
    inner_data = abi_hdr + payload
    inner_tlv  = struct.pack("<II", SOF_CTRL_CMD_BINARY, len(inner_data)) + inner_data
    outer_tlv  = struct.pack("<II", CFG_NUMID, len(inner_tlv)) + inner_tlv
    buf = ctypes.create_string_buffer(bytes(outer_tlv))
    fd = _ctl_open(card)
    try:
        fcntl.ioctl(fd, TLV_WRITE_IOC, buf)
        return True
    except OSError as e:
        print(f"[vad] write_threshold error: {e}", file=sys.stderr)
        return False
    finally:
        os.close(fd)


def run_wov_cycle(card, device, outfile, cycle_num):
    """
    Run one WOV capture cycle using wov_capture writing to a temp file.

    Returns bytes written for this cycle (0 means no trigger).
    """
    print(f"[cycle {cycle_num}] starting wov_capture (blocks until WOV trigger)...",
          flush=True)

    tmpf = tempfile.NamedTemporaryFile(delete=False, suffix=".raw",
                                       prefix=f"/tmp/wov_cyc{cycle_num}_")
    tmppath = tmpf.name
    tmpf.close()

    cmd = [
        "wov_capture",
        "-c", str(card),
        "-d", str(device),
        "-b", str(FRAGMENT_BYTES),
        "-i", str(IDLE_TIMEOUT_MS),
        "-n", "1",
        tmppath,
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    # Forward wov_capture log lines while monitoring VAD
    triggered     = False
    silence_seen  = False
    drain_start_t = None
    last_size     = 0
    last_data_t   = time.monotonic()

    log_lines = []
    log_lock  = threading.Lock()

    def log_reader():
        for line in proc.stdout:
            txt = line.decode(errors="replace").rstrip()
            with log_lock:
                log_lines.append(txt)

    lt = threading.Thread(target=log_reader, daemon=True)
    lt.start()

    while True:
        # Print any new log lines
        with log_lock:
            while log_lines:
                print(f"  [wov] {log_lines.pop(0)}", flush=True)

        # Check temp file size
        try:
            sz = os.path.getsize(tmppath)
        except OSError:
            sz = 0

        if sz > last_size:
            last_size  = sz
            last_data_t = time.monotonic()
            if not triggered:
                triggered = True
                print(f"[cycle {cycle_num}] WOV triggered, receiving PCM...", flush=True)

        # Poll VAD status once triggered
        if triggered and not silence_seen:
            e, active = read_status(card)
            if e is not None and not active:
                print(f"[cycle {cycle_num}] VAD silence (energy={e}), draining...",
                      flush=True)
                silence_seen  = True
                drain_start_t = time.monotonic()

        # Drain complete?
        if silence_seen:
            idle_s  = time.monotonic() - last_data_t
            drain_s = time.monotonic() - drain_start_t
            if idle_s >= DRAIN_IDLE_S:
                print(f"[cycle {cycle_num}] drain complete ({drain_s:.1f}s), "
                      f"{last_size} bytes captured", flush=True)
                break
            if drain_s >= DRAIN_TIMEOUT_S:
                print(f"[cycle {cycle_num}] drain timeout, "
                      f"{last_size} bytes captured", flush=True)
                break

        # Process ended?
        if proc.poll() is not None:
            lt.join(timeout=1)
            with log_lock:
                while log_lines:
                    print(f"  [wov] {log_lines.pop(0)}", flush=True)
            if not triggered:
                print(f"[cycle {cycle_num}] wov_capture exited without trigger "
                      f"(rc={proc.returncode})", flush=True)
            break

        time.sleep(0.1)

    proc.terminate()
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

    # Append cycle data to main output file
    bytes_written = os.path.getsize(tmppath) if os.path.exists(tmppath) else 0
    if bytes_written > 0:
        mode = "ab" if os.path.exists(outfile) else "wb"
        with open(outfile, mode) as out, open(tmppath, "rb") as src:
            out.write(src.read())
    try:
        os.unlink(tmppath)
    except OSError:
        pass

    return bytes_written


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cycles",    "-n", type=int, default=0,
                    help="Number of WOV events to capture (0=unlimited)")
    ap.add_argument("--out",       "-o", default="/tmp/wov_daemon.raw",
                    help="Output PCM file")
    ap.add_argument("--threshold", "-t", type=int, default=None,
                    help="VAD threshold to write at startup (e.g. 300000000)")
    ap.add_argument("--compress",  "-d", type=int, default=11,
                    help="Compress PCM device number (default: 11)")
    ap.add_argument("--card",      "-c", type=int, default=0,
                    help="Sound card number (default: 0)")
    args = ap.parse_args()

    card   = args.card
    device = args.compress

    print(f"WOV daemon: card={card} device={device} out={args.out}")
    print(f"  kcontrols: vad_gate_cfg_100 (numid={CFG_NUMID}), "
          f"vad_gate_status_100 (numid={STATUS_NUMID})")

    if args.threshold is not None:
        print(f"  writing threshold={args.threshold}...")
        if write_threshold(card, args.threshold):
            print(f"  threshold written OK")
        else:
            print(f"  WARNING: threshold write failed (pipeline not yet active?)")

    total_cycles = args.cycles if args.cycles > 0 else float("inf")
    cycle_num    = 0
    total_bytes  = 0

    print(f"\nStarting capture loop (cycles={'unlimited' if args.cycles==0 else args.cycles})...")
    print("Press Ctrl+C to stop.\n")

    try:
        while cycle_num < total_cycles:
            cycle_num += 1
            b = run_wov_cycle(card, device, args.out, cycle_num)
            total_bytes += b
            if b == 0 and cycle_num == 1:
                print("[daemon] No data on first cycle — exiting.")
                break
            print(f"[cycle {cycle_num}] done. Total: {total_bytes} bytes → {args.out}",
                  flush=True)
            if cycle_num < total_cycles:
                print(f"[cycle {cycle_num}] re-arming for next WOV trigger...\n",
                      flush=True)
                time.sleep(0.3)
    except KeyboardInterrupt:
        print("\n[daemon] stopped by user")

    print(f"\nDone. {cycle_num} cycle(s), {total_bytes} total bytes → {args.out}")
    if total_bytes > 0:
        print(f"Inspect: sox -r 16000 -e signed -b 32 -c 1 {args.out} {args.out}.wav")


if __name__ == "__main__":
    main()
