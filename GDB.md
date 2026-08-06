# SOF Firmware GDB Architecture and Usage

This document explains how SOF firmware GDB debugging works on Intel ADSP platforms, how the Linux SOF GDB client bridges to the DSP, and how to connect a host GDB session to a running firmware instance.

It focuses on the Tiger Lake IPC4 flow used on Spider.

## Quickstart (Spider)

Use this as a minimal bring-up path once firmware and kernel artifacts are built.

### DUT checks

~~~bash
ssh root@spider 'modprobe snd_sof_fw_gdb || true'
ssh root@spider 'lsmod | grep snd_sof_fw_gdb'
ssh root@spider 'find /sys/kernel/debug -maxdepth 4 -type f -name fw_gdb'
~~~

### Verify firmware GDB slot

~~~bash
# From your shell transport on Spider, confirm gdb_stub appears in slot table
sof dbgwin dump
~~~

### Start bridge on Spider

~~~bash
ssh root@spider 'socat TCP-LISTEN:1235,reuseaddr,fork OPEN:/sys/kernel/debug/sof/fw_gdb,rdwr'
~~~

If `socat` is not installed on Spider, use the repo bridge helper script:

~~~bash
# copy once
scp sof/scripts/sof-fw-gdb-bridge.py root@spider:/tmp/

# run on DUT
ssh root@spider 'python3 /tmp/sof-fw-gdb-bridge.py --port 1235'
~~~

### Connect host GDB

~~~bash
xt-gdb /home/lrg/work/sof-tgl/build-tgl-shell-llvm/zephyr/zephyr.elf
~~~

In the GDB prompt:

~~~text
target remote spider:1235
monitor reset
info registers
bt
~~~

Or run the one-command helper from the SOF repo root:

~~~bash
./sof/scripts/sof-fw-gdb-attach.sh --elf build-tgl-gdb-llvm/zephyr/zephyr.elf

# non-interactive smoke test
./sof/scripts/sof-fw-gdb-attach.sh --elf build-tgl-gdb-llvm/zephyr/zephyr.elf --smoke

# smoke test and auto-stop remote bridge on exit
./sof/scripts/sof-fw-gdb-attach.sh --elf build-tgl-gdb-llvm/zephyr/zephyr.elf --smoke --cleanup-bridge
~~~

VS Code task shortcut:

1. Run task SOF FW GDB Smoke Attach (Spider) from the workspace task list.

If attach fails:

1. Re-check that snd_sof_fw_gdb is loaded.
2. Re-check that gdb_stub is present in sof dbgwin dump output.
3. Ensure only one process has fw_gdb open.

### Troubleshooting Matrix

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| modprobe snd_sof_fw_gdb fails | Kernel built without CONFIG_SND_SOC_SOF_DEBUG_FW_GDB | Enable CONFIG_SND_SOC_SOF_DEBUG_FW_GDB, rebuild/deploy snd-sof-fw-gdb.ko, run depmod |
| /sys/kernel/debug/sof/fw_gdb missing | Module not loaded, SOF debugfs not present, or probe failed | Load module, verify debugfs mount, check dmesg for SOF/fw_gdb probe errors |
| GDB attach hangs or disconnects | Bridge process exited or fw_gdb not returning data | Keep socat in foreground first, verify fw_gdb path and permissions, retry target remote |
| GDB commands return no firmware data | Firmware image lacks CONFIG_GDBSTUB | Rebuild firmware with debug overlay or CONFIG_GDBSTUB=y and redeploy |
| fw_gdb opens but transport fails | No gdb_stub slot in debug window | Verify slot table with sof dbgwin dump and deploy a GDBSTUB-capable firmware build |
| GDB reports device busy | Another process already opened fw_gdb | Stop competing process and reconnect with a single bridge/client |
| Attach works but breakpoints fail | Software breakpoints unsupported in this path | Use hardware breakpoints, for example hbreak |
| Behavior is flaky after module reload | DSP or client state not clean | Reboot DUT, reload modules, and start bridge before attach |

## 1. Big Picture

SOF firmware debugging uses a layered transport:

1. Host GDB speaks the Remote Serial Protocol packets.
2. Linux kernel SOF GDB client exposes a debugfs endpoint named fw_gdb.
3. The client proxies bytes into DSP shared-memory ring buffers in the debug window.
4. Firmware GDB stub consumes those packets, inspects DSP state, and returns replies.

The debugfs file open operation also sends an ENTER_GDB IPC message to firmware.

~~~mermaid
flowchart LR
    A[Host GDB] --> B[Host bridge process]
    B --> C[/sys/kernel/debug/sof/fw_gdb]
    C --> D[Linux snd_sof_fw_gdb client]
    D --> E[Debug window ring buffers]
    E --> F[Firmware GDB stub]
    F --> G[DSP registers and memory]
~~~

## 2. Firmware Side Architecture

### 2.1 GDB Stub Core

The firmware contains a GDB protocol parser and exception handling integration.

Main behavior:

1. Exception or breakpoint enters GDB handler.
2. Firmware exchanges packets in remote protocol format.
3. Commands include register and memory read/write, continue, single-step, and breakpoints.

### 2.2 Transport Backend on Intel ADSP

On Intel ADSP Zephyr platforms, the GDB backend uses shared SRAM rings located in the debug window region.

Transport details:

1. Two circular rings are used: host-to-fw and fw-to-host.
2. Ring metadata includes head and tail pointers.
3. A debug window slot type is marked as GDB_STUB.

### 2.3 IPC Trigger into GDB Mode

For IPC4, firmware handles ENTER_GDB global message and sets an internal enter-gdb flag.

Kernel client sends this message when fw_gdb is opened.

~~~mermaid
sequenceDiagram
    participant K as Linux fw_gdb client
    participant I as IPC4 global message
    participant F as SOF firmware
    participant S as GDB stub

    K->>I: ENTER_GDB request
    I->>F: global message dispatch
    F->>F: set enter-gdb flag
    F->>S: route execution to stub path
~~~

## 3. Linux Kernel Side Architecture

### 3.1 Module and Registration

Kernel feature flag:

1. CONFIG_SND_SOC_SOF_DEBUG_FW_GDB enables the fw_gdb client.
2. Module produced is snd-sof-fw-gdb.
3. SOF core registers an auxiliary client named fw_gdb.

### 3.2 Debugfs Endpoint

The client creates:

1. /sys/kernel/debug/sof/fw_gdb

Important semantics:

1. Only one opener at a time.
2. Open sends ENTER_GDB IPC to firmware.
3. Read and write move bytes via debug-window rings.
4. Poll support is present for readability and writability.

### 3.3 Ring Access Constraints

The kernel client expects:

1. Firmware debug window has a GDB_STUB slot.
2. Slot offsets are discoverable through IPC4 debug slot helpers.

If no GDB_STUB slot exists, reads and writes fail.

## 4. End-to-End Runtime Data Flow

~~~mermaid
sequenceDiagram
    participant G as Host GDB
    participant S as Socat bridge
    participant D as debugfs fw_gdb
    participant K as snd_sof_fw_gdb
    participant R as DSP ring buffers
    participant F as FW GDB stub

    G->>S: target remote connect
    S->>D: open fw_gdb
    D->>K: file open callback
    K->>F: ENTER_GDB IPC
    G->>S: packet, for example read registers
    S->>D: write packet bytes
    D->>K: write callback
    K->>R: host-to-fw ring write
    F->>R: parse request and prepare reply
    K->>R: fw-to-host ring read
    D->>S: read callback returns bytes
    S->>G: reply packet bytes
~~~

## 5. Spider Prerequisites Checklist

Before trying to attach GDB, verify all of the following:

1. Firmware is built with GDB stub enabled.
2. Firmware debug window reports a GDB_STUB slot.
3. Linux has snd_sof_fw_gdb module available and loaded.
4. debugfs is mounted and /sys/kernel/debug/sof/fw_gdb exists.
5. DSP firmware is running and SOF stack is healthy.

Quick checks on DUT:

~~~bash
uname -r
lsmod | grep snd_sof_fw_gdb
find /sys/kernel/debug -maxdepth 4 -type f -name fw_gdb
~~~

Quick slot inspection via shell path:

~~~bash
# Use existing shell transport and run:
sof dbgwin dump
# Confirm a line with name gdb_stub is present
~~~

## 6. Build and Deploy Flow

### 6.1 Firmware with GDB Stub

Use a build overlay that enables GDBSTUB. In this tree, debug_overlay.conf already sets:

1. CONFIG_GDBSTUB=y
2. CONFIG_GDBSTUB_ENTER_IMMEDIATELY=n

Build and deploy firmware to the IPC4 path used by Spider.

### 6.2 Kernel fw_gdb Client

Ensure Linux config enables:

1. CONFIG_SND_SOC_SOF_DEBUG_FW_GDB=m or y

Then build SOF kernel modules and deploy snd-sof-fw-gdb to Spider module tree, followed by depmod and modprobe.

## 7. Host Connection Procedure

Run a bridge on Spider from debugfs to TCP:

~~~bash
socat TCP-LISTEN:1235,reuseaddr,fork OPEN:/sys/kernel/debug/sof/fw_gdb,rdwr
~~~

If `socat` is unavailable on DUT:

~~~bash
python3 /tmp/sof-fw-gdb-bridge.py --port 1235
~~~

On host machine, launch Xtensa GDB with firmware ELF:

~~~bash
xt-gdb path/to/zephyr.elf
~~~

Inside GDB:

~~~text
target remote spider:1235
~~~

Then use normal remote debugging commands:

1. info registers
2. bt
3. x and p memory/register reads
4. continue
5. stepi
6. hbreak for hardware breakpoints

## 8. Expected Failure Modes and Fixes

### 8.1 snd_sof_fw_gdb module missing

Symptom:

1. modprobe snd_sof_fw_gdb fails with module not found.

Fix:

1. Enable CONFIG_SND_SOC_SOF_DEBUG_FW_GDB.
2. Rebuild and deploy module.

### 8.2 fw_gdb file missing

Symptom:

1. /sys/kernel/debug/sof/fw_gdb does not exist.

Fix:

1. Load snd_sof_fw_gdb.
2. Verify SOF core debugfs root exists.
3. Confirm SOF probe completed without crash.

### 8.3 No gdb_stub slot in debug window

Symptom:

1. Kernel client cannot locate slot offsets and communication fails.

Fix:

1. Rebuild firmware with CONFIG_GDBSTUB enabled.
2. Reboot and re-check slot table.

### 8.4 GDB disconnects quickly

Potential causes:

1. Bridge process exits.
2. No data returned due to transport misconfiguration.
3. Firmware not in GDB path.

Fix:

1. Keep socat running in foreground first.
2. Check dmesg and SOF logs.
3. Re-open fw_gdb and retry attach.

## 9. Relationship to SOF Shell TTY Path

The shell transport and GDB transport are different clients:

1. Shell uses serial client and ttysof0.
2. GDB uses fw_gdb debugfs client and GDB_STUB ring buffers.

Both can coexist in a debug build, but they are independent endpoints.

## 10. Practical Bring-up Plan

Use this order to minimize churn:

1. Enable and deploy fw_gdb kernel module.
2. Build and deploy GDBSTUB firmware image.
3. Verify gdb_stub debug-window slot appears.
4. Verify fw_gdb debugfs node exists.
5. Start socat bridge and connect GDB.
6. Validate basic commands: registers, memory read, continue, break, backtrace.

~~~mermaid
flowchart TD
    A[Deploy fw_gdb kernel module] --> B[Deploy GDBSTUB firmware]
    B --> C{gdb_stub slot visible}
    C -- no --> B
    C -- yes --> D{fw_gdb debugfs exists}
    D -- no --> A
    D -- yes --> E[Start socat bridge]
    E --> F[Connect host GDB target remote]
    F --> G[Run smoke debug commands]
~~~
