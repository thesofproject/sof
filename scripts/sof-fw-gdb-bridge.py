#!/usr/bin/env python3
"""Bridge a TCP port to SOF fw_gdb debugfs endpoint.

Run this on the DUT. It forwards GDB remote protocol bytes between
TCP clients and /sys/kernel/debug/sof/fw_gdb.
"""

import argparse
import os
import select
import socket
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SOF fw_gdb TCP bridge")
    parser.add_argument(
        "--fw-path",
        default="/sys/kernel/debug/sof/fw_gdb",
        help="Path to fw_gdb debugfs file",
    )
    parser.add_argument(
        "--host",
        default="0.0.0.0",
        help="TCP listen host",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=1235,
        help="TCP listen port",
    )
    return parser.parse_args()


def bridge_one_client(conn: socket.socket, fwfd: int) -> None:
    conn.setblocking(False)

    while True:
        readable, _, _ = select.select([conn, fwfd], [], [], 1.0)

        if conn in readable:
            try:
                data = conn.recv(4096)
            except BlockingIOError:
                data = None

            if data == b"":
                return

            if data:
                os.write(fwfd, data)

        if fwfd in readable:
            try:
                data = os.read(fwfd, 4096)
            except BlockingIOError:
                data = None

            if data:
                conn.sendall(data)


def main() -> int:
    args = parse_args()

    if not os.path.exists(args.fw_path):
        print(f"fw_gdb path not found: {args.fw_path}", file=sys.stderr)
        return 1

    fwfd = os.open(args.fw_path, os.O_RDWR | os.O_NONBLOCK)

    listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listen_sock.bind((args.host, args.port))
    listen_sock.listen(1)

    print(f"listening on {args.host}:{args.port}, fw={args.fw_path}", flush=True)

    try:
        while True:
            conn, addr = listen_sock.accept()
            print(f"client connected: {addr}", flush=True)
            try:
                bridge_one_client(conn, fwfd)
            finally:
                conn.close()
                print("client disconnected", flush=True)
    finally:
        os.close(fwfd)
        listen_sock.close()


if __name__ == "__main__":
    raise SystemExit(main())