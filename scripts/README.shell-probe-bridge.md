# Shell Probe Bridge

`sof-shell-probe-bridge.py` bridges SOF shell PTY traffic to a framed byte stream
that can be carried by probe/compressed PCM transport.

## Why

The existing shell path uses ADSP memory-window transport. This bridge isolates
shell framing/chunking/flow-control so compressed PCM transport can be wired in
without changing shell command parsing behavior.

## Frame format

Frame encoding is implemented in `shell_probe_framing.py`:

- Magic: `SPBR`
- Version: `1`
- Types: `DATA`, `CREDIT`, `RESET`
- Sequence number per `DATA` frame
- 16-bit payload length
- Credit-based flow control for large output

## Bridge usage

The bridge is transport-agnostic and expects two files/FIFOs:

- `--framed-in`: bytes heading toward shell input
- `--framed-out`: bytes coming from shell output

Example:

```bash
mkfifo /tmp/shell_probe.in /tmp/shell_probe.out
./sof/scripts/sof-shell-probe-bridge.py \
  --dut spider \
  --framed-in /tmp/shell_probe.in \
  --framed-out /tmp/shell_probe.out
```

A companion process should connect these FIFOs to compressed PCM endpoints.

## Compressed PCM adapter

`sof-shell-probe-compr-adapter.py` provides that companion process. It does not
hardcode tinycompress ioctls; instead it supervises helper commands so existing
or future compressed-PCM tools can be reused.

Supported placeholders in helper command templates:

- `{capture_dev}`
- `{playback_dev}`
- `{frame_in}`
- `{frame_out}`

Example:

```bash
mkfifo /tmp/shell_probe.in /tmp/shell_probe.out
./sof/scripts/sof-shell-probe-compr-adapter.py \
  --frame-in /tmp/shell_probe.in \
  --frame-out /tmp/shell_probe.out \
  --capture-dev /dev/snd/comprC1D0 \
  --playback-dev /dev/snd/comprC1D1 \
  --rx-cmd "my_capture_tool --dev {capture_dev} --stdout" \
  --tx-cmd "my_playback_tool --dev {playback_dev} --stdin"
```

## Framed transport validator

`sof-shell-probe-validate.py` validates command/response handling over the
framed shell/probe transport.

```bash
./sof/scripts/sof-shell-probe-validate.py --dut spider
```

It uses the same command-pass/fail style as `sof-shell-validate.py` and checks
that chunking plus credit-based flow control work while running shell commands.

## UART + probe mirror workflow

With `CONFIG_SOF_SHELL_PROBE_MIRROR=y`, shell output is mirrored to probe
extraction packets while UART shell interaction stays active.

Typical usage:

1. Terminal A (interactive shell on UART):

```bash
picocom /dev/ttysof0
```

2. Terminal B (capture and decode mirrored shell output from probes):

```bash
<probe-capture-command> | ./sof/scripts/sof-shell-probe-packet-decode.py
```

`<probe-capture-command>` is any command that reads bytes from the probe
compressed capture endpoint and writes them to stdout.
