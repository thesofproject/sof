#!/usr/bin/env python3
"""
SOF QEMU Host-to-DSP IPC Bridge Client

This script connects to a running QEMU instance via its TCP monitor socket
and provides a programmatic interface to send/receive IPC messages to the 
simulated Zephyr firmware.

Usage Example:
  # Start QEMU with a TCP monitor socket:
  $ qemu-system-xtensa -machine adsp_ace30 ... -monitor tcp:localhost:1025,server,nowait
  
  # Check status:
  $ python3 sof-qemu-ipc.py --status

  # Send a message (e.g. IPC4 Pipeline state change):
  $ python3 sof-qemu-ipc.py --primary 0x01004000 --extension 0x0
"""

import sys
import time
import socket
import argparse
import re

class SOFQemuIPC:
    def __init__(self, host='localhost', port=1025):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(2.0)
        
    def connect(self):
        try:
            self.sock.connect((self.host, self.port))
            # Fast-forward past the QEMU welcome banner
            self._read_until(b"(qemu)")
            print(f"[+] Connected to QEMU HMP on {self.host}:{self.port}")
        except Exception as e:
            print(f"[-] Connection failed: {e}")
            print(f"    Ensure QEMU is running with: -monitor tcp:{self.host}:{self.port},server,nowait")
            sys.exit(1)

    def _read_until(self, delimiter):
        data = b""
        while delimiter not in data:
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                data += chunk
            except socket.timeout:
                break
        return data.decode('utf-8', errors='ignore')

    def send_cmd(self, cmd):
        """Sends a raw HMP command and returns the textual output string."""
        # Note: the prompt is actually '(qemu) ' usually, we tolerate missing trailing space
        self.sock.send(f"{cmd}\n".encode('utf-8'))
        output = self._read_until(b"(qemu)")
        # Clean up output parsing bounds
        output = output.replace(f"{cmd}\r\n", "").strip()
        if output.endswith("(qemu)"):
            output = output[:-6].strip()
        return output

    def get_ipc_status(self):
        """Abstracts parsing of 'info ace-ipc' into a status dictionary."""
        res = self.send_cmd("info ace-ipc")
        status = {}
        for line in res.splitlines():
            line = line.strip()
            if "XTDR (Command)" in line:
                m = re.search(r'0x([0-9a-fA-F]+)\s+\[BUSY=(\d)\]', line)
                if m:
                    status['xtdr_val'] = int(m.group(1), 16)
                    status['xtdr_busy'] = int(m.group(2))
            elif "XTDA (Response)" in line:
                m = re.search(r'0x([0-9a-fA-F]+)\s+\[BUSY=(\d)\]', line)
                if m:
                    status['xtda_val'] = int(m.group(1), 16)
                    status['xtda_busy'] = int(m.group(2))
        return status

    def send_ipc(self, primary, extension=0, payload2=None, payload3=None):
        """Dispatches an IPC and synchronously polls for the DONE/BUSY completion state."""
        cmd = f"ace-ipc-tx {primary} {extension}"
        if payload2 is not None:
            cmd += f" {payload2}"
            if payload3 is not None:
                cmd += f" {payload3}"
                
        status = self.get_ipc_status()
        if status.get('xtdr_busy', 0) == 1:
            print("[-] Cannot send: previous IPC message still BUSY (DSP has not acked XTDR)")
            return False

        print(f"[*] Sending IPC Message to DSP: Primary=0x{primary:08x}, Extension=0x{extension:08x}")
        res = self.send_cmd(cmd)
        
        # Poll the host-facing IPC registers until firmware signals completion
        print("[*] Waiting for Zephyr DSP to process...")
        timeout = 5.0
        start = time.time()
        while time.time() - start < timeout:
            status = self.get_ipc_status()
            # Firmware consumes the request by clearing XTDR_BUSY
            if status.get('xtdr_busy', 1) == 0:
                # Firmware posts response tracking to XTDA 
                if status.get('xtda_busy', 1) == 0:
                    resp = status.get('xtda_val', 0)
                    print(f"[+] IPC Transaction Complete. Response Object (XTDA): 0x{resp:08x}")
                    return resp
            
            time.sleep(0.1)
            
        print("[-] Timeout waiting for DSP firmware response.")
        return None

    def send_load_library(self, dma_id, lib_id, file_path, load_offset=0):
        """Builds and dispatches the SOF_IPC4_GLB_LOAD_LIBRARY firmware message for dynamic LLEXT injection."""
        # IPC4 msg_tgt=1 (FW_GEN_MSG), type=24 (LOAD_LIBRARY), rsp=0 (MSG_REQUEST)
        
        print(f"[*] Mapping local file to QEMU HD/A DMA Stream {dma_id}: {file_path}")
        dma_res = self.send_cmd(f"ace-dma-in {dma_id} {file_path}")
        if "Error" in dma_res:
            print(f"[-] QEMU DMA Binding Failed: {dma_res}")
            return None
        
        primary = (1 << 30) | (24 << 24) | ((lib_id & 0xF) << 16) | (dma_id & 0x1F)
        extension = load_offset & 0x3FFFFFFF
        print(f"[*] Preparing LLEXT Load Request: lib_id={lib_id}, dma_id={dma_id}, offset={load_offset}")
        return self.send_ipc(primary, extension)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="SOF QEMU Host-to-DSP IPC Bridge")
    parser.add_argument("--port", type=int, default=1025, help="QEMU HMP TCP Port (default: 1025)")
    parser.add_argument("--primary", type=lambda x: int(x,0), help="Primary IPC Message Header (Hex)", required=False)
    parser.add_argument("--extension", type=lambda x: int(x,0), default=0, help="Extension Header / Data (Hex)")
    parser.add_argument("--load-llext", nargs=3, metavar=('DMA_ID', 'LIB_ID', 'FILE'), help="Load a Zephyr LLEXT library instance from FILE via DMA_ID (e.g., --load-llext 0 1 ./my_ext.o)")
    parser.add_argument("--load-offset", type=lambda x: int(x,0), default=0, help="Offset for LLEXT unpacking")
    parser.add_argument("--start-core", type=int, metavar="CORE_ID", help="Start (D0 transition) a specific auxiliary DSP core via IPC4 SET_DX (e.g., --start-core 1)")
    parser.add_argument("--status", action="store_true", help="Dump complete raw architectural IPC status")
    args = parser.parse_args()

    ipc = SOFQemuIPC(port=args.port)
    ipc.connect()
    
    if args.status:
        print("\n" + ipc.send_cmd("info ace-ipc"))
    elif args.load_llext:
        dma_id = int(args.load_llext[0], 0)
        lib_id = int(args.load_llext[1], 0)
        ipc.send_load_library(dma_id=dma_id, lib_id=lib_id, file_path=args.load_llext[2], load_offset=args.load_offset)
    elif args.start_core is not None:
        core_id = args.start_core
        print(f"[*] Constructing SOF_IPC4_MOD_SET_DX for Core {core_id}...")
        
        # Primary: MODULE_MSG (msg_tgt=1), SET_DX (type=7), msg_rsp=0
        primary = (1 << 30) | (7 << 24)
        
        # Payload struct: ipc4_dx_state_info requires [core_mask] [dx_mask]
        core_mask = (1 << core_id)
        dx_mask = (1 << core_id)  # D0 = 1, D3 = 0
        
        res = ipc.send_ipc(primary, core_mask, payload2=dx_mask)
        if res:
            print(f"[+] Core {core_id} power state transitioned successfully.")
    elif args.primary is not None:
        ipc.send_ipc(args.primary, args.extension)
    else:
        print("\nTip: Pass --primary 0x... to inject a message, or --status to view registers.")
        parser.print_help()
