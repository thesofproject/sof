#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

HOST="spider"
PORT="1235"
ELF_PATH=""
GDB_BIN=${GDB_BIN:-/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-intel_tgl_adsp_zephyr-elf/bin/xtensa-intel_tgl_adsp_zephyr-elf-gdb}
REMOTE_BRIDGE_PATH="/tmp/sof-fw-gdb-bridge.py"
REMOTE_LOG_PATH="/tmp/sof-fw-gdb-bridge.log"
REMOTE_PID_PATH="/tmp/sof-fw-gdb-bridge.pid"

usage() {
    cat <<EOF
Usage: $0 --elf <path> [options]

Required:
  --elf <path>           Path to firmware ELF with symbols

Options:
  --host <hostname>      DUT hostname (default: spider)
  --port <port>          Bridge TCP port (default: 1235)
  --gdb <path>           GDB binary path (default: $GDB_BIN)
    --smoke                Non-interactive smoke test: info registers + bt + quit
    --cleanup-bridge       Stop remote bridge after GDB exits
  --no-start-bridge      Do not install/start remote bridge
  --keep-bridge          Do not stop existing bridge before start
  -h, --help             Show this help

Environment:
  GDB_BIN                Default GDB path override

Examples:
  $0 --elf /home/lrg/work/sof-tgl/build-tgl-gdb-llvm/zephyr/zephyr.elf
  $0 --host spider --port 1235 --elf build-tgl-gdb-llvm/zephyr/zephyr.elf
EOF
}

START_BRIDGE=1
KILL_EXISTING=1
SMOKE=0
CLEANUP_BRIDGE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --host)
            HOST="$2"
            shift 2
            ;;
        --port)
            PORT="$2"
            shift 2
            ;;
        --elf)
            ELF_PATH="$2"
            shift 2
            ;;
        --gdb)
            GDB_BIN="$2"
            shift 2
            ;;
        --smoke)
            SMOKE=1
            shift
            ;;
        --cleanup-bridge)
            CLEANUP_BRIDGE=1
            shift
            ;;
        --no-start-bridge)
            START_BRIDGE=0
            shift
            ;;
        --keep-bridge)
            KILL_EXISTING=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            exit 2
            ;;
    esac
done

if [[ -z "$ELF_PATH" ]]; then
    echo "--elf is required" >&2
    usage
    exit 2
fi

if [[ ! -f "$ELF_PATH" ]]; then
    echo "ELF not found: $ELF_PATH" >&2
    exit 2
fi

if [[ ! -x "$GDB_BIN" ]]; then
    echo "GDB binary not executable: $GDB_BIN" >&2
    exit 2
fi

SSH=(ssh -o BatchMode=yes -o ConnectTimeout=8 "root@$HOST")

kill_remote_bridges() {
    "${SSH[@]}" "
        if [ -f '$REMOTE_PID_PATH' ]; then
            pid=\$(cat '$REMOTE_PID_PATH' 2>/dev/null || true)
            [ -n \"\$pid\" ] && kill \"\$pid\" 2>/dev/null || true
            rm -f '$REMOTE_PID_PATH'
        fi
        extra_pids=\$(ps -eo pid=,comm=,args= | awk '\$2==\"python3\" && (\$0 ~ /python3 -u \\/tmp\\/sof-fw-gdb-bridge.py/ || \$0 ~ /python3 \\/tmp\\/fw_gdb_bridge.py/) { print \$1 }')
        [ -n \"\$extra_pids\" ] && kill \$extra_pids 2>/dev/null || true
    "
}

cleanup_remote_bridge() {
    echo "Cleaning up remote bridge"
    kill_remote_bridges
}

echo "Checking DUT connectivity and fw_gdb node on $HOST"
"${SSH[@]}" "hostname; ls -l /sys/kernel/debug/sof/fw_gdb"

if [[ "$START_BRIDGE" -eq 1 ]]; then
    echo "Installing remote bridge script: $REMOTE_BRIDGE_PATH"
    "${SSH[@]}" "cat > '$REMOTE_BRIDGE_PATH'" < "$SCRIPT_DIR/sof-fw-gdb-bridge.py"
    "${SSH[@]}" "chmod +x '$REMOTE_BRIDGE_PATH'"

    if [[ "$KILL_EXISTING" -eq 1 ]]; then
        echo "Stopping existing bridge, if any"
        kill_remote_bridges
    fi

    echo "Starting remote bridge on $HOST:$PORT"
    "${SSH[@]}" "nohup python3 -u '$REMOTE_BRIDGE_PATH' --port '$PORT' > '$REMOTE_LOG_PATH' 2>&1 & echo \$! > '$REMOTE_PID_PATH'"

    for _ in $(seq 1 20); do
        if "${SSH[@]}" "ss -ltn 2>/dev/null | grep -q ':$PORT '"; then
            echo "Remote bridge is listening on port $PORT"
            break
        fi
        sleep 0.2
    done

    if ! "${SSH[@]}" "ss -ltn 2>/dev/null | grep -q ':$PORT '"; then
        echo "Bridge did not start on $HOST:$PORT" >&2
        echo "Recent bridge log:" >&2
        "${SSH[@]}" "tail -n 60 '$REMOTE_LOG_PATH' || true" >&2
        exit 1
    fi
fi

echo "Launching GDB: $GDB_BIN"
echo "Target: $HOST:$PORT"

if [[ "$CLEANUP_BRIDGE" -eq 1 ]]; then
    trap cleanup_remote_bridge EXIT
fi

if [[ "$SMOKE" -eq 1 ]]; then
    "$GDB_BIN" "$ELF_PATH" \
        -q \
        -ex "set pagination off" \
        -ex "set confirm off" \
        -ex "set remotetimeout 8" \
        -ex "target remote $HOST:$PORT" \
        -ex "info registers" \
        -ex "bt" \
        -ex "quit"
else
    "$GDB_BIN" "$ELF_PATH" \
        -ex "set pagination off" \
        -ex "set remotetimeout 8" \
        -ex "target remote $HOST:$PORT"
fi